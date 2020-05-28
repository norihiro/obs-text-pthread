#include <obs-module.h>
#include <pango/pangocairo.h>
#include "plugin-macros.generated.h"
#include "obs-text-pthread.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define tp_data_get_color(s, c) tp_data_get_color2(s, c, c".alpha")
static inline uint32_t tp_data_get_color2(obs_data_t *settings, const char *color, const char *alpha)
{
	return
		((uint32_t)obs_data_get_int(settings, color) & 0xFFFFFF) |
		((uint32_t)obs_data_get_int(settings, alpha) & 0xFF) << 24;
}

#define tp_data_add_color(props, c, t) { \
	obs_properties_add_color(props, c, t); \
	obs_properties_add_int_slider(props, c".alpha", obs_module_text("Alpha"), 0, 255, 1); \
}

static const char *tp_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);

	return obs_module_text("Text Pthread");
}

static void tp_update(void *data, obs_data_t *settings);

static void *tp_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(source);
	struct tp_source *src = bzalloc(sizeof(struct tp_source));

	pthread_mutex_init(&src->config_mutex, NULL);
	pthread_mutex_init(&src->tex_mutex, NULL);

	tp_update(src, settings);

	tp_thread_start(src);

	return src;
}

static void tp_destroy(void *data)
{
	struct tp_source *src = data;

	tp_thread_end(src);

	tp_config_destroy_member(&src->config);

	if (src->textures) free_texture(src->textures);
	if (src->tex_new) free_texture(src->tex_new);

	pthread_mutex_destroy(&src->tex_mutex);
	pthread_mutex_destroy(&src->config_mutex);

	bfree(src);
}

static void tp_update(void *data, obs_data_t *settings)
{
	struct tp_source *src = data;

	pthread_mutex_lock(&src->config_mutex);

	obs_data_t *font_obj = obs_data_get_obj(settings, "font");
	if (font_obj) {
		BFREE_IF_NONNULL(src->config.font_name);
		src->config.font_name = bstrdup(obs_data_get_string(font_obj, "face"));

		BFREE_IF_NONNULL(src->config.font_style);
		src->config.font_style = bstrdup(obs_data_get_string(font_obj, "style"));

		src->config.font_size = (uint32_t)obs_data_get_int(font_obj, "size");
		src->config.font_flags = (uint32_t)obs_data_get_int(font_obj, "flags");

		obs_data_release(font_obj);
	}

	BFREE_IF_NONNULL(src->config.text_file);
	src->config.text_file = bstrdup(obs_data_get_string(settings, "text_file"));

	src->config.color = tp_data_get_color(settings, "color");

	src->config.width = (uint32_t)obs_data_get_int(settings, "width");
	src->config.height = (uint32_t)obs_data_get_int(settings, "height");
	src->config.shrink_size = obs_data_get_bool(settings, "shrink_size");
	src->config.align = obs_data_get_int(settings, "align");
	src->config.wrapmode = obs_data_get_int(settings, "wrapmode");
	src->config.spacing = obs_data_get_int(settings, "spacing");

	src->config.outline = obs_data_get_bool(settings, "outline");
	src->config.outline_color = tp_data_get_color(settings, "outline_color");
	src->config.outline_width = obs_data_get_int(settings, "outline_width");
	src->config.outline_blur = obs_data_get_int(settings, "outline_blur");

	src->config.shadow = obs_data_get_bool(settings, "shadow");
	src->config.shadow_color = tp_data_get_color(settings, "shadow_color");
	src->config.shadow_x = obs_data_get_int(settings, "shadow_x");
	src->config.shadow_y = obs_data_get_int(settings, "shadow_y");

	src->config_updated = true;

	pthread_mutex_unlock(&src->config_mutex);
}

static void tp_get_defaults(obs_data_t *settings)
{
	obs_data_t *font_obj = obs_data_create();
	// obs_data_set_default_string(font_obj, "face", DEFAULT_FACE); // TODO: if not set, I expect pango can choose something.
	obs_data_set_default_int(font_obj, "size", 256);
	obs_data_set_default_obj(settings, "font", font_obj);
	obs_data_release(font_obj);

	obs_data_set_default_int(settings, "color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "color.alpha", 0xFF);

	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
	obs_data_set_default_bool(settings, "shrink_size", true);
	obs_data_set_default_int(settings, "wrapmode", PANGO_WRAP_WORD);
	obs_data_set_default_int(settings, "spacing", 0);

	obs_data_set_default_int(settings, "outline_color.alpha", 0xFF);

	obs_data_set_default_int(settings, "shadow_x", 2);
	obs_data_set_default_int(settings, "shadow_y", 3);
	obs_data_set_default_int(settings, "shadow_color.alpha", 0xFF);
}

#define tp_set_visible(props, name, en) \
{ \
	obs_property_t *prop = obs_properties_get(props, name); \
	if (prop) obs_property_set_visible(prop, en); \
}

static bool tp_prop_outline_changed(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *prop;

	bool en = settings ? obs_data_get_bool(settings, "outline") : false;
	tp_set_visible(props, "outline_color", en);
	tp_set_visible(props, "outline_color.alpha", en);
	tp_set_visible(props, "outline_width", en);
	tp_set_visible(props, "outline_blur", en);
}

static bool tp_prop_shadow_changed(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);
	obs_property_t *prop;

	bool en = settings ? obs_data_get_bool(settings, "shadow") : false;
	tp_set_visible(props, "shadow_color", en);
	tp_set_visible(props, "shadow_color.alpha", en);
	tp_set_visible(props, "shadow_x", en);
	tp_set_visible(props, "shadow_y", en);
}

static obs_properties_t *tp_get_properties(void *unused)
{
	UNUSED_PARAMETER(unused);
	obs_properties_t *props;
	obs_property_t *prop;
	props = obs_properties_create();

	obs_properties_add_font(props, "font", obs_module_text("Font"));

	obs_properties_add_path(props, "text_file", obs_module_text("Text file"), OBS_PATH_FILE, NULL, NULL);

	tp_data_add_color(props, "color", obs_module_text("Color"));

	obs_properties_add_int(props, "width", obs_module_text("Width"), 1, 16384, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 1, 16384, 1);
	obs_properties_add_bool(props, "shrink_size", obs_module_text("Automatically shrink size"));

	prop = obs_properties_add_list(props, "align", obs_module_text("Alignment"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Left"), ALIGN_LEFT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Center"), ALIGN_CENTER);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Right"), ALIGN_RIGHT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Left.Justify"), ALIGN_LEFT | ALIGN_JUSTIFY);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Center.Justify"), ALIGN_CENTER | ALIGN_JUSTIFY);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Right.Justify"), ALIGN_RIGHT | ALIGN_JUSTIFY);

	prop = obs_properties_add_list(props, "wrapmode", obs_module_text("Wrap text"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Wrapmode.Word"), PANGO_WRAP_WORD);
	obs_property_list_add_int(prop, obs_module_text("Wrapmode.Char"), PANGO_WRAP_CHAR);
	obs_property_list_add_int(prop, obs_module_text("Wrapmode.WordChar"), PANGO_WRAP_WORD_CHAR);

	obs_properties_add_int(props, "spacing", obs_module_text("Line spacing"), -65536, +65536, 1);

	// TODO: ellipsize
	// TODO: auto_dir
	// TODO: single-paragraph?
	// TODO: vertical
	// TODO: markup-option (use set_text or set_markup)

	prop = obs_properties_add_bool(props, "outline", obs_module_text("Outline"));
	obs_property_set_modified_callback(prop, tp_prop_outline_changed);
	tp_data_add_color(props, "outline_color", obs_module_text("Outline color"));
	obs_properties_add_int(props, "outline_width", obs_module_text("Outline width"), 0, 65536, 1);
	obs_properties_add_int(props, "outline_blur", obs_module_text("Outline blur"), 0, 65536, 1);

	prop = obs_properties_add_bool(props, "shadow", obs_module_text("Shadow"));
	obs_property_set_modified_callback(prop, tp_prop_shadow_changed);
	tp_data_add_color(props, "shadow_color", obs_module_text("Shadow color"));
	obs_properties_add_int(props, "shadow_x", obs_module_text("Shadow offset x"), -65536, 65536, 1);
	obs_properties_add_int(props, "shadow_y", obs_module_text("Shadow offset y"), -65536, 65536, 1);


	return props;
}

static uint32_t tp_get_width(void *data)
{
	struct tp_source *src = data;

	uint32_t w = 0;
	struct tp_texture *t = src->textures;
	while (t) {
		if (w < t->width) w = t->width;
		t = t->next;
	}

	return w;
}

static uint32_t tp_get_height(void *data)
{
	struct tp_source *src = data;

	uint32_t h = 0;
	struct tp_texture *t = src->textures;
	while (t) {
		if (h < t->height) h = t->height;
		t = t->next;
	}

	return h;
}

static void tp_surface_to_texture(struct tp_texture *t)
{
	if(t->surface) {
		obs_enter_graphics();
		if (t->tex) gs_texture_destroy(t->tex);
		t->tex = gs_texture_create(t->width, t->height, GS_BGRA, 1, (const uint8_t**)&t->surface, 0);
		obs_leave_graphics();

		bfree(t->surface);
		t->surface = NULL;
	}

	if(t->next)
		tp_surface_to_texture(t->next);
}

static void tp_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct tp_source *src = data;

	// TODO: implement to draw all textures with alpha.

	if (src->textures) {
		struct tp_texture *t = src->textures;

		gs_reset_blend_state();
		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), t->tex);
		gs_draw_sprite(t->tex, 0, t->width, t->height);
	}
}

static void tp_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct tp_source *src = data;

	if (pthread_mutex_trylock(&src->tex_mutex)==0) {
		if (src->tex_new) {
			if (src->textures) free_texture(src->textures);
			src->textures = src->tex_new;
			src->tex_new = NULL;

			tp_surface_to_texture(src->textures);
		}
		pthread_mutex_unlock(&src->tex_mutex);
	}
}

static struct obs_source_info tp_src_info = {
	.id = "obs_text_pthread_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = tp_get_name,
	.create = tp_create,
	.destroy = tp_destroy,
	.update = tp_update,
	.get_defaults = tp_get_defaults,
	.get_properties = tp_get_properties,
	.get_width = tp_get_width,
	.get_height = tp_get_height,
	.video_render = tp_render,
	.video_tick = tp_tick,
};

bool obs_module_load(void)
{
	obs_register_source(&tp_src_info);

	blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, "plugin unloaded");
}
