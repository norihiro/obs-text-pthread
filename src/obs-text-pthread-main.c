#include <obs-module.h>
#include <pango/pangocairo.h>
#include "plugin-macros.generated.h"
#include "obs-text-pthread.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

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

	if (src->text_file) bfree(src->text_file);
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

	if (src->text_file) bfree(src->text_file);
	src->text_file = bstrdup(obs_data_get_string(settings, "text_file"));

	pthread_mutex_unlock(&src->config_mutex);
}

static void tp_get_defaults(obs_data_t *settings)
{
}

static obs_properties_t *tp_get_properties(void *unused)
{
	UNUSED_PARAMETER(unused);
	obs_properties_t *props;
	props = obs_properties_create();

	obs_properties_add_path(props, "text_file", obs_module_text("text file"), OBS_PATH_FILE, NULL, NULL);

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

	if (pthread_mutex_trylock(&src->tex_mutex)==0) {
		if (src->tex_new) {
			if (src->textures) free_texture(src->textures);
			src->textures = src->tex_new;
			src->tex_new = NULL;

			tp_surface_to_texture(src->textures);
		}
		pthread_mutex_unlock(&src->tex_mutex);
	}

	if (src->textures) {
		struct tp_texture *t = src->textures;

		gs_reset_blend_state();
		gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), t->tex);
		gs_draw_sprite(t->tex, 0, t->width, t->height);
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
