#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <pango/pangocairo.h>
#include "plugin-macros.generated.h"
#include "obs-text-pthread.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

#define debug(format, ...)
// #define debug(format, ...) fprintf(stderr, format, ##__VA_ARGS__)

static inline uint64_t max_u64(uint64_t a, uint64_t b)
{
	if (a > b)
		return a;
	return b;
}

static gs_effect_t *textalpha_effect = NULL;

#define tp_data_get_color(s, c) tp_data_get_color2(s, c, c ".alpha")
static inline uint32_t tp_data_get_color2(obs_data_t *settings, const char *color, const char *alpha)
{
	return ((uint32_t)obs_data_get_int(settings, color) & 0xFFFFFF) |
	       ((uint32_t)obs_data_get_int(settings, alpha) & 0xFF) << 24;
}

#define tp_data_add_color(props, c, t)                                                                 \
	{                                                                                              \
		obs_properties_add_color(props, c, t);                                                 \
		obs_properties_add_int_slider(props, c ".alpha", obs_module_text("Alpha"), 0, 255, 1); \
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

	obs_enter_graphics();
	if (!textalpha_effect) {
		char *f = obs_module_file("textalpha.effect");
		textalpha_effect = gs_effect_create_from_file(f, NULL);
		if (!textalpha_effect)
			blog(LOG_ERROR, "Cannot load '%s'", f);
		bfree(f);
	}
	obs_leave_graphics();

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

	if (src->textures)
		free_texture(src->textures);
	if (src->tex_new)
		free_texture(src->tex_new);

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

	src->config.from_file = obs_data_get_bool(settings, "from_file");
	BFREE_IF_NONNULL(src->config.text);
	BFREE_IF_NONNULL(src->config.text_file);
	if (!src->config.from_file) {
		src->config.text = bstrdup(obs_data_get_string(settings, "text"));
	}
	else {
		src->config.text_file = bstrdup(obs_data_get_string(settings, "text_file"));
	}
	src->config.markup = obs_data_get_bool(settings, "markup");

	src->config.color = tp_data_get_color(settings, "color");

	src->config.width = (uint32_t)obs_data_get_int(settings, "width");
	src->config.height = (uint32_t)obs_data_get_int(settings, "height");
	src->config.shrink_size = obs_data_get_bool(settings, "shrink_size");
	src->config.align = obs_data_get_int(settings, "align");
	src->config.auto_dir = obs_data_get_bool(settings, "auto_dir");
	src->config.wrapmode = obs_data_get_int(settings, "wrapmode");
	src->config.indent = obs_data_get_int(settings, "indent");
	src->config.ellipsize = obs_data_get_int(settings, "ellipsize");
	src->config.spacing = obs_data_get_int(settings, "spacing");

	src->config.outline = obs_data_get_bool(settings, "outline");
	src->config.outline_color = tp_data_get_color(settings, "outline_color");
	src->config.outline_width = obs_data_get_int(settings, "outline_width");
	src->config.outline_blur = obs_data_get_int(settings, "outline_blur");
	src->config.outline_blur_gaussian = obs_data_get_bool(settings, "outline_blur_gaussian");
	src->config.outline_shape = obs_data_get_int(settings, "outline_shape");

	src->config.shadow = obs_data_get_bool(settings, "shadow");
	src->config.shadow_color = tp_data_get_color(settings, "shadow_color");
	src->config.shadow_x = obs_data_get_int(settings, "shadow_x");
	src->config.shadow_y = obs_data_get_int(settings, "shadow_y");

	src->config.align_transition = obs_data_get_int(settings, "align_transition.v") |
				       obs_data_get_int(settings, "align_transition.h");

	src->config.fadein_ms = obs_data_get_int(settings, "fadein_ms");
	src->config.fadeout_ms = obs_data_get_int(settings, "fadeout_ms");
	src->config.crossfade_ms = obs_data_get_int(settings, "crossfade_ms");

	src->config.slide_pxps = obs_data_get_int(settings, "slide_pxps");

#ifdef PNG_FOUND
	src->config.save_file = obs_data_get_bool(settings, "save_file");
	BFREE_IF_NONNULL(src->config.save_file_dir);
	if (src->config.save_file) {
		const char *s = obs_data_get_string(settings, "save_file_dir");
		if (*s)
			src->config.save_file_dir = bstrdup(s);
		else {
			blog(LOG_ERROR, "save_file_dir is not specified");
			src->config.save_file = false;
		}
	}
#endif // PNG_FOUND

	src->config_updated = true;

	pthread_mutex_unlock(&src->config_mutex);
}

static void tp_get_defaults(obs_data_t *settings)
{
	obs_data_t *font_obj = obs_data_create();
	obs_data_set_default_int(font_obj, "size", 64);
	obs_data_set_default_obj(settings, "font", font_obj);
	obs_data_release(font_obj);

	obs_data_set_default_bool(settings, "markup", true);

	obs_data_set_default_int(settings, "color", 0xFFFFFFFF);
	obs_data_set_default_int(settings, "color.alpha", 0xFF);

	obs_data_set_default_int(settings, "width", 1920);
	obs_data_set_default_int(settings, "height", 1080);
	obs_data_set_default_bool(settings, "shrink_size", true);
	obs_data_set_default_bool(settings, "auto_dir", true);
	obs_data_set_default_int(settings, "wrapmode", PANGO_WRAP_WORD);
	obs_data_set_default_int(settings, "ellipsize", PANGO_ELLIPSIZE_NONE);
	obs_data_set_default_int(settings, "spacing", 0);

	obs_data_set_default_int(settings, "outline_color.alpha", 0xFF);

	obs_data_set_default_int(settings, "shadow_x", 2);
	obs_data_set_default_int(settings, "shadow_y", 3);
	obs_data_set_default_int(settings, "shadow_color.alpha", 0xFF);

	obs_data_set_default_int(settings, "align_transition.v", ALIGN_TOP);
	obs_data_set_default_int(settings, "align_transition.h", ALIGN_LEFT);
}

static void tp_get_defaults_v2(obs_data_t *settings)
{
	tp_get_defaults(settings);

	obs_data_set_default_bool(settings, "outline_blur_gaussian", true);
}

#define tp_set_visible(props, name, en)                                 \
	{                                                               \
		obs_property_t *prop = obs_properties_get(props, name); \
		if (prop)                                               \
			obs_property_set_visible(prop, en);             \
	}

static bool tp_prop_outline_changed(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);

	bool en = settings ? obs_data_get_bool(settings, "outline") : false;
	tp_set_visible(props, "outline_color", en);
	tp_set_visible(props, "outline_color.alpha", en);
	tp_set_visible(props, "outline_width", en);
	tp_set_visible(props, "outline_blur", en);
	tp_set_visible(props, "outline_blur_gaussian", en);
	tp_set_visible(props, "outline_shape", en);

	return true;
}

static bool tp_prop_shadow_changed(obs_properties_t *props, obs_property_t *property, obs_data_t *settings)
{
	UNUSED_PARAMETER(property);

	bool en = settings ? obs_data_get_bool(settings, "shadow") : false;
	tp_set_visible(props, "shadow_color", en);
	tp_set_visible(props, "shadow_color.alpha", en);
	tp_set_visible(props, "shadow_x", en);
	tp_set_visible(props, "shadow_y", en);

	return true;
}

static obs_properties_t *tp_get_properties(void *unused)
{
	UNUSED_PARAMETER(unused);
	obs_properties_t *props;
	obs_property_t *prop;
	props = obs_properties_create();

	obs_properties_add_font(props, "font", obs_module_text("Font"));

	obs_properties_add_text(props, "text", obs_module_text("Text"), OBS_TEXT_MULTILINE);
	obs_properties_add_bool(props, "from_file", obs_module_text("Read text from a file"));
	obs_properties_add_path(props, "text_file", obs_module_text("Text file"), OBS_PATH_FILE, NULL, NULL);
	obs_properties_add_bool(props, "markup", obs_module_text("Pango mark-up"));

	tp_data_add_color(props, "color", obs_module_text("Color"));

	obs_properties_add_int(props, "width", obs_module_text("Width"), 1, 16384, 1);
	obs_properties_add_int(props, "height", obs_module_text("Height"), 1, 16384, 1);
	obs_properties_add_bool(props, "shrink_size", obs_module_text("Automatically shrink size"));

	prop = obs_properties_add_list(props, "align", obs_module_text("Alignment"), OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Left"), ALIGN_LEFT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Center"), ALIGN_CENTER);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Right"), ALIGN_RIGHT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Left.Justify"), ALIGN_LEFT | ALIGN_JUSTIFY);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Center.Justify"), ALIGN_CENTER | ALIGN_JUSTIFY);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Right.Justify"), ALIGN_RIGHT | ALIGN_JUSTIFY);

	obs_properties_add_bool(props, "auto_dir", obs_module_text("Calculate the bidirectonal base direction"));

	prop = obs_properties_add_list(props, "wrapmode", obs_module_text("Wrap text"), OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Wrapmode.Word"), PANGO_WRAP_WORD);
	obs_property_list_add_int(prop, obs_module_text("Wrapmode.Char"), PANGO_WRAP_CHAR);
	obs_property_list_add_int(prop, obs_module_text("Wrapmode.WordChar"), PANGO_WRAP_WORD_CHAR);

	obs_properties_add_int(props, "indent", obs_module_text("Indent"), -32767, 32767, 1);

	prop = obs_properties_add_list(props, "ellipsize", obs_module_text("Ellipsize"), OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Ellipsize.None"), PANGO_ELLIPSIZE_NONE);
	obs_property_list_add_int(prop, obs_module_text("Ellipsize.Start"), PANGO_ELLIPSIZE_START);
	obs_property_list_add_int(prop, obs_module_text("Ellipsize.Middle"), PANGO_ELLIPSIZE_MIDDLE);
	obs_property_list_add_int(prop, obs_module_text("Ellipsize.End"), PANGO_ELLIPSIZE_END);

	obs_properties_add_int(props, "spacing", obs_module_text("Line spacing"), -65536, +65536, 1);

	// TODO: vertical

	prop = obs_properties_add_bool(props, "outline", obs_module_text("Outline"));
	obs_property_set_modified_callback(prop, tp_prop_outline_changed);
	tp_data_add_color(props, "outline_color", obs_module_text("Outline color"));
	obs_properties_add_int(props, "outline_width", obs_module_text("Outline width"), 0, 65536, 1);
	obs_properties_add_int(props, "outline_blur", obs_module_text("Outline blur"), 0, 65536, 1);
	obs_properties_add_bool(props, "outline_blur_gaussian", obs_module_text("Outline blur with gaussian function"));
	prop = obs_properties_add_list(props, "outline_shape", obs_module_text("Outline shape"), OBS_COMBO_TYPE_LIST,
				       OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Outline.Round"), OUTLINE_ROUND);
	obs_property_list_add_int(prop, obs_module_text("Outline.Bevel"), OUTLINE_BEVEL);
	obs_property_list_add_int(prop, obs_module_text("Outline.Rectangle"), OUTLINE_RECT);
	obs_property_list_add_int(prop, obs_module_text("Outline.Sharp"), OUTLINE_SHARP);

	prop = obs_properties_add_bool(props, "shadow", obs_module_text("Shadow"));
	obs_property_set_modified_callback(prop, tp_prop_shadow_changed);
	tp_data_add_color(props, "shadow_color", obs_module_text("Shadow color"));
	obs_properties_add_int(props, "shadow_x", obs_module_text("Shadow offset x"), -65536, 65536, 1);
	obs_properties_add_int(props, "shadow_y", obs_module_text("Shadow offset y"), -65536, 65536, 1);

	prop = obs_properties_add_list(props, "align_transition.h", obs_module_text("Transition alignment"),
				       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Left"), ALIGN_LEFT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Center"), ALIGN_CENTER);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Right"), ALIGN_RIGHT);

	prop = obs_properties_add_list(props, "align_transition.v", obs_module_text("Transition alignment"),
				       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Top"), ALIGN_TOP);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Center"), ALIGN_VCENTER);
	obs_property_list_add_int(prop, obs_module_text("Alignment.Bottom"), ALIGN_BOTTOM);

	obs_properties_add_int(props, "fadein_ms", obs_module_text("Fadein time [ms]"), 0, 4294, 100);
	obs_properties_add_int(props, "fadeout_ms", obs_module_text("Fadeout time [ms]"), 0, 4294, 100);
	obs_properties_add_int(props, "crossfade_ms", obs_module_text("Crossfade time [ms]"), 0, 4294, 100);

	obs_properties_add_int(props, "slide_pxps", obs_module_text("Slide [px/s] (0 to disable)"), 0, 65500, 50);

#ifdef PNG_FOUND
	obs_properties_add_bool(props, "save_file", obs_module_text("Save as PNG"));
	obs_properties_add_path(props, "save_file_dir", obs_module_text("Directory to save"), OBS_PATH_DIRECTORY, NULL,
				NULL);
#endif // PNG_FOUND

	return props;
}

static uint32_t tp_get_width(void *data)
{
	struct tp_source *src = data;

	uint32_t w = 0;
	struct tp_texture *t = src->textures;
	while (t) {
		if (w < t->width)
			w = t->width;
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
		if (src->config.slide_pxps)
			h += t->slide_h;
		else {
			if (h < t->height)
				h = t->height;
		}
		t = t->next;
	}

	return h;
}

static void tp_surface_to_texture(struct tp_texture *t)
{
	if (t->surface && !t->tex) {
		const uint8_t *surface = t->surface;
		t->tex = gs_texture_create(t->width, t->height, GS_BGRA, 1, &surface, 0);
	}
}

static void tp_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct tp_source *src = data;
	if (!textalpha_effect)
		return;

	obs_enter_graphics();
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	const int w = tp_get_width(data);
	const int h = tp_get_height(data);
	int xoff = 0, yoff = 0;

	for (struct tp_texture *t = src->textures; t; t = t->next) {
		if (!t->width || !t->height)
			continue;
		if (t->slide_u > 0 && t->slide_u > (int)t->height)
			continue;
		if (t->slide_u < 0 && (-t->slide_u) > (int)t->height)
			continue;
		tp_surface_to_texture(t);
		int y0 = t->slide_u > 0 ? t->slide_u : 0;
		int y1 = t->slide_u < 0 ? t->height + t->slide_u : t->height;
		xoff = 0;
		if ((src->config.align_transition & ALIGN_RIGHT) && (int)t->width < w)
			xoff += w - t->width;
		else if ((src->config.align_transition & ALIGN_CENTER) && (int)t->width < w)
			xoff += w / 2 - t->width / 2;
		if (!src->config.slide_pxps) {
			yoff = 0;
			if ((src->config.align_transition & ALIGN_BOTTOM) && (int)t->height != h)
				yoff += h - t->height;
			else if ((src->config.align_transition & ALIGN_VCENTER) && (int)t->height != h)
				yoff += h / 2 - t->height / 2;
		}
		if (xoff || yoff) {
			gs_matrix_push();
			gs_matrix_translate3f(xoff, yoff, 0);
		}
		gs_effect_set_texture(gs_effect_get_param_by_name(textalpha_effect, "image"), t->tex);
		gs_effect_set_float(gs_effect_get_param_by_name(textalpha_effect, "alpha"), t->fade_alpha / 255.f);
		while (gs_effect_loop(textalpha_effect, "Draw")) {
			gs_draw_sprite_subregion(t->tex, 0, 0, y0, t->width, y1);
		}
		if (xoff || yoff)
			gs_matrix_pop();

		if (src->config.slide_pxps) {
			yoff += t->slide_h;
		}
	}
	gs_blend_state_pop();
	obs_leave_graphics();
}

static inline void tp_load_new_texture(struct tp_source *src, uint64_t lastframe_ns)
{
	if (src->tex_new) {
		// new texture arrived

		struct tp_texture *tn = src->tex_new;
		src->tex_new = tn->next;
		tn->next = NULL;

		if (tn->config_updated) {
			// A texture with updated config is arrived.
			if (src->textures) {
				free_texture(src->textures);
				src->textures = NULL;
			}
		}

		if (tn->surface) {
			// if non-blank texture

			if (!tn->config_updated) {
				tn->fadein_start_ns = lastframe_ns;
				tn->fadein_end_ns =
					lastframe_ns +
					(tn->is_crossfade ? src->config.crossfade_ms : src->config.fadein_ms) * 1000000;
			}

			if (src->config.slide_pxps) {
				uint64_t slidein_ns = tn->height * 1000000000LL / src->config.slide_pxps;
				if (!src->textures) {
					tn->slidein_end_ns = lastframe_ns + slidein_ns;
				}
				else if (src->textures) {
					struct tp_texture *tl = src->textures;
					while (tl->next)
						tl = tl->next;
					tn->slidein_end_ns =
						(tl->slideout_start_ns ? tl->slideout_start_ns : lastframe_ns) +
						slidein_ns;
					debug("lastframe_ns=%f %p slidein_end_ns=%f\n", lastframe_ns * 1e-9, tn,
					      tn->slidein_end_ns * 1e-9);
				}
			}

			if (src->config.crossfade_ms == 0 && src->config.slide_pxps == 0) {
				if (src->textures) {
					free_texture(src->textures);
					src->textures = NULL;
				}
			}
		}
		else {
			// if blank texture

			// mark fadeout for old textures
			for (struct tp_texture *t = src->textures; t; t = t->next) {
				if (!t->fadeout_start_ns) {
					t->fadeout_start_ns = tn->time_ns;
					t->fadeout_end_ns = tn->time_ns + src->config.fadeout_ms * 1000000;
				}
			}
		}

		src->textures = pushback_texture(src->textures, tn);
	}
}

static struct tp_texture *tp_pop_old_textures(struct tp_texture *t, uint64_t now_ns, const struct tp_source *src)
{
	if (!t)
		return NULL;

	bool deprecated = false;
	if (t->fadeout_end_ns)
		deprecated = true;
	if (t->slideout_start_ns)
		deprecated = true;
	if (t->next && !src->config.slide_pxps)
		deprecated = true;

	bool transition_ongoing = false;
	if (t->fadeout_end_ns && now_ns < t->fadeout_end_ns)
		transition_ongoing = true;
	if (t->slidein_end_ns) // TODO: remove me
		transition_ongoing = true;
	if (t->slideout_start_ns && t->slide_u < (int)t->height)
		transition_ongoing = true;
	if (t->slideout_start_ns && t->next && t->slide_u < (int)t->next->height)
		transition_ongoing = true;

	if (deprecated && !transition_ongoing) {
		debug("tp_pop_old_textures: removing %p fadeout_end=%f slideout_start=%f slide_u=%d height=%d now=%f\n",
		      t, t->fadeout_end_ns * 1e-9, t->slideout_start_ns * 1e-9, t->slide_u, t->height, now_ns * 1e-9);
		t = popfront_texture(t);
		return tp_pop_old_textures(t, now_ns, src);
	}
	if (src->config.slide_pxps)
		return t;
	t->next = tp_pop_old_textures(t->next, now_ns, src);
	return t;
}

static void tp_tick(void *data, float seconds)
{
	struct tp_source *src = data;

	uint64_t now_ns = os_gettime_ns();
	uint64_t lastframe_ns = now_ns - (uint64_t)(seconds * 1e9);

	src->textures = tp_pop_old_textures(src->textures, now_ns, src);

	if (os_atomic_load_bool(&src->text_updating)) {
		// early notification for the new non-blank texture from the thread

		// mark fadeout for old textures
		if (src->config.crossfade_ms > 0)
			for (struct tp_texture *t = src->textures; t; t = t->next) {
				if (!t->fadeout_start_ns) {
					t->fadeout_start_ns = lastframe_ns;
					t->fadeout_end_ns = lastframe_ns + src->config.crossfade_ms * 1000000;
				}
			}

		if (src->config.slide_pxps) {
			uint64_t slideout_end_last = lastframe_ns;
			for (struct tp_texture *t = src->textures; t; t = t->next) {
				if (!t->slideout_start_ns) {
					t->slideout_start_ns = max_u64(t->slidein_end_ns, slideout_end_last);
					debug("now_ns=%f %p slideout_start_ns=%f slidein_end_ns=%f slideout_end_last=%f \n",
					      now_ns * 1e-9, t, t->slideout_start_ns * 1e-9, t->slidein_end_ns * 1e-9,
					      slideout_end_last * 1e-9);
				}
				slideout_end_last =
					t->slideout_start_ns + t->height * 1000000000LL / src->config.slide_pxps;
				debug("now_ns=%f %p slideout_end_last=%f slidetime=%f\n", now_ns * 1e-9, t,
				      slideout_end_last * 1e-9, t->height * 1e0 / src->config.slide_pxps);
			}
		}

		os_atomic_set_bool(&src->text_updating, false);
	}

	if (pthread_mutex_trylock(&src->tex_mutex) == 0) {
		tp_load_new_texture(src, lastframe_ns);
		pthread_mutex_unlock(&src->tex_mutex);
	}

	if (src->config.slide_pxps)
		for (struct tp_texture *t = src->textures; t; t = t->next) {
			if (t->slidein_end_ns) {
				if (now_ns >= t->slidein_end_ns) {
					t->slidein_end_ns = 0;
					t->slide_u = 0;
				}
				else
					t->slide_u = -(int64_t)((t->slidein_end_ns - now_ns) * src->config.slide_pxps /
								1000000000);
			}

			if (!t->slidein_end_ns && t->slideout_start_ns && now_ns > t->slideout_start_ns)
				t->slide_u =
					(int64_t)(now_ns - t->slideout_start_ns) * src->config.slide_pxps / 1000000000;

			t->slide_h = t->height - abs(t->slide_u);
			if (t->slide_h < 0)
				t->slide_h = 0;
		}

	src->textures = tp_pop_old_textures(src->textures, now_ns, src);

	for (struct tp_texture *t = src->textures; t; t = t->next) {

		// cleanup fadein
		if (t->fadein_start_ns && now_ns >= t->fadein_end_ns) {
			t->fadein_start_ns = t->fadein_end_ns = 0;
		}

		int fade_alpha = 255;

		if (t->fadein_start_ns) {
			fade_alpha = 255 * (now_ns - t->fadein_start_ns) / (t->fadein_end_ns - t->fadein_start_ns);
		}

		if (t->fadeout_end_ns && now_ns >= t->fadeout_end_ns) {
			fade_alpha = 0;
		}
		else if (t->fadeout_start_ns) {
			fade_alpha =
				fade_alpha * (t->fadeout_end_ns - now_ns) / (t->fadeout_end_ns - t->fadeout_start_ns);
		}

		if (fade_alpha < 0)
			fade_alpha = 0;
		else if (fade_alpha > 255)
			fade_alpha = 255;
		t->fade_alpha = fade_alpha;
	}
}

static struct obs_source_info tp_src_info = {
	.id = "obs_text_pthread_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_CAP_OBSOLETE,
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
	.icon_type = OBS_ICON_TYPE_TEXT,
};

bool obs_module_load(void)
{
	obs_register_source(&tp_src_info);

	struct obs_source_info tp_src_info_v2 = tp_src_info;
	tp_src_info_v2.get_defaults = tp_get_defaults_v2;
	tp_src_info_v2.version = 2;
	tp_src_info_v2.output_flags &= ~OBS_SOURCE_CAP_OBSOLETE;
	obs_register_source(&tp_src_info_v2);

	blog(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload()
{
	if (textalpha_effect) {
		gs_effect_destroy(textalpha_effect);
		textalpha_effect = NULL;
	}
	blog(LOG_INFO, "plugin unloaded");
}
