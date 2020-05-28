#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <pango/pangocairo.h>
#include "obs-text-pthread.h"

#define debug(format, ...)

char *tp_load_text(struct tp_config *config)
{
	if (!config->text_file) return NULL;

	FILE *fp = fopen(config->text_file, "rb");
	if (!fp) return NULL;

	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	char *buf = bmalloc(len + 1);
	buf[len] = 0;

	fseek(fp, 0, SEEK_SET);
	fread(buf, 1, len, fp);

	fclose(fp);

	debug("tp_load_text returns \"%s\"\n", buf);
	return buf;
}

static double u32toFR(uint32_t u) { return (double)((u >>  0) & 0xFF) / 256.; }
static double u32toFG(uint32_t u) { return (double)((u >>  8) & 0xFF) / 256.; }
static double u32toFB(uint32_t u) { return (double)((u >> 16) & 0xFF) / 256.; }
static double u32toFA(uint32_t u) { return (double)((u >> 24) & 0xFF) / 256.; }

static void tp_stroke_path(cairo_t *cr, PangoLayout *layout, int offset_x, int offset_y, uint32_t color, int width, int blur)
{
	for (int b = blur; b>=-blur; b--) {
		double a = u32toFA(color) * (blur ? 0.5 - b * 0.5 / blur : 1.0);
		int w = (width+b)*2;
		if (w<0) break;
		cairo_move_to(cr, offset_x, offset_y);
		cairo_set_source_rgba(cr, u32toFR(color), u32toFG(color), u32toFB(color), a);
		if(w>0) {
			cairo_set_line_width(cr, w);
			cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
			pango_cairo_layout_path(cr, layout);
			cairo_stroke(cr);
		}
		else {
			pango_cairo_show_layout(cr, layout);
		}
	}
}

static void debug_fill_rgb(uint8_t *data, uint32_t color, int size)
{
	for(int i=0; i<size; i+=4) {
		data[i+2] = color;
		data[i+1] = color>>8;
		data[i+0] = color>>16;
	}
}

static struct tp_texture * tp_draw_texture(struct tp_config *config, char *text)
{
	struct tp_texture *n = bzalloc(sizeof(struct tp_texture));

	int outline_width = config->outline ? config->outline_width : 0;
	int outline_blur = config->outline ? config->outline_blur : 0;
	int outline_width_blur = outline_width + outline_blur;
	int shadow_abs_x = config->shadow ? abs(config->shadow_x) : 0;
	int shadow_abs_y = config->shadow ? abs(config->shadow_y) : 0;
	int offset_x = outline_width_blur + (config->shadow && config->shadow_x<0 ? -config->shadow_x : 0);
	int offset_y = outline_width_blur + (config->shadow && config->shadow_y<0 ? -config->shadow_y : 0);

	uint32_t body_width = config->width;
	uint32_t surface_width = body_width + outline_width_blur*2 + shadow_abs_x;
	uint32_t surface_height = config->height + outline_width_blur*2 + shadow_abs_y;

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, surface_width);
	n->surface = bzalloc(stride * surface_height);

	// FIXME: below is a workaround. CAIRO_OPERATOR_SOURCE and CAIRO_OPERATOR_OVER handles opacity wrongly.
	// For example, white background, white text, 0.5 opacity causes darker text.
	// Maybe, I should manually blend the colors.
	if (config->shadow)
		debug_fill_rgb(n->surface, config->shadow_color, stride * surface_height );
	else if (config->outline)
		debug_fill_rgb(n->surface, config->outline_color, stride * surface_height );
	else
		debug_fill_rgb(n->surface, config->color, stride * surface_height );


	cairo_surface_t *surface = cairo_image_surface_create_for_data(n->surface, CAIRO_FORMAT_ARGB32, surface_width, surface_height, stride);

	cairo_t *cr = cairo_create(surface);

	PangoLayout *layout = pango_cairo_create_layout(cr);

	debug("font name=<%s> style=<%s> size=%d flags=0x%X\n", config->font_name, config->font_style, config->font_size, config->font_flags);
	PangoFontDescription *desc = pango_font_description_new();
	pango_font_description_set_family(desc, config->font_name);
	pango_font_description_set_weight(desc, (config->font_flags & OBS_FONT_BOLD) ? PANGO_WEIGHT_BOLD : 0);
	pango_font_description_set_style(desc, (config->font_flags & OBS_FONT_ITALIC) ? PANGO_STYLE_ITALIC : 0);
	pango_font_description_set_size(desc, (config->font_size * PANGO_SCALE * 2)/3); // Scaling to approximate GDI text pts
	pango_layout_set_font_description(layout, desc);
	pango_font_description_free(desc);

	if (config->align & ALIGN_CENTER)
		pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	else if (config->align & ALIGN_RIGHT)
		pango_layout_set_alignment(layout, PANGO_ALIGN_RIGHT);
	else // ALIGN_LEFT
		pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
	pango_layout_set_justify(layout, !!(config->align & ALIGN_JUSTIFY));

	pango_layout_set_width(layout, body_width<<10);
	pango_layout_set_auto_dir(layout, config->auto_dir);
	pango_layout_set_wrap(layout, config->wrapmode);
	pango_layout_set_ellipsize(layout, config->ellipsize);
	pango_layout_set_spacing(layout, config->spacing * PANGO_SCALE);

	(config->markup ? pango_layout_set_markup : pango_layout_set_text)(layout, text, -1);

	PangoRectangle ink_rect, logical_rect;
	pango_layout_get_extents(layout, &ink_rect, &logical_rect);

	if (shadow_abs_x || shadow_abs_y) {
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		if(outline_width_blur)
			tp_stroke_path(cr, layout, offset_x + config->shadow_x, offset_y + config->shadow_y, config->shadow_color, outline_width, outline_blur);
		tp_stroke_path(cr, layout, offset_x + config->shadow_x, offset_y + config->shadow_y, config->shadow_color, 0, 0);
	}

	if (outline_width_blur > 0) {
		debug("stroking outline width=%d\n", outline_width);
		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
		tp_stroke_path(cr, layout, offset_x, offset_y, config->outline_color, outline_width, outline_blur);
	}

	if(config->shadow || config->outline)
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	else
		cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	tp_stroke_path(cr, layout, offset_x, offset_y, config->color, 0, 0);

	cairo_surface_flush(surface);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	if (config->shrink_size) {
		// TODO: if the alignment is center or right, we need to add offset.
		n->width = PANGO_PIXELS_FLOOR(ink_rect.width) + outline_width_blur*2 + shadow_abs_x;
		if (n->width > surface_width) n->width = surface_width;
		n->height = PANGO_PIXELS_FLOOR(ink_rect.height) + outline_width_blur*2 + shadow_abs_y;
		if (n->height > surface_height) n->height = surface_height;
		if (n->width != surface_width) {
			uint32_t new_stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, n->width);
			for (uint32_t y=1; y<n->height; y++) {
				memmove(n->surface+new_stride*y, n->surface+stride*y, new_stride);
			}
		}
	}
	else {
		n->width = surface_width;
		n->height = surface_height;
	}

	debug("tp_draw_texture end: width=%d height=%d \n",  n->width, n->height);

	return n;
}

bool tp_compare_stat(const struct stat *a, const struct stat *b)
{
	if (a->st_ino   != b->st_ino   ) return true;
	if (a->st_size  != b->st_size  ) return true;
#ifdef __USE_XOPEN2K8
	if (memcmp(&a->st_mtim, &b->st_mtim, sizeof(struct timespec))) return true;
#else // __USE_XOPEN2K8
	if (a->st_mtime != b->st_mtime ) return true;
#ifdef _STATBUF_ST_NSEC
	if (a->st_mtime_nsec != b->st_mtime_nsec) return true;
#endif // _STATBUF_ST_NSEC
#endif // __USE_XOPEN2K8
	return false;
}

static inline bool is_printable(const char *t)
{
	for (; *t; t++) {
		const char c = *t;
		if (!(c==' ' || c=='\n' || c=='\t' || c=='\r')) return true;
	}
	return false;
}

static void *tp_thread_main(void *data)
{
	struct tp_source *src = data;

	struct stat st_prev = {0};
	struct tp_config config_prev = {0};
	bool b_printable_prev = false;

	while (src->running) {
		os_sleep_ms(33);

		pthread_mutex_lock(&src->config_mutex);

		bool config_updated = src->config_updated;
		bool text_updated = false;

		// check config and copy
		if(config_updated) {
			BFREE_IF_NONNULL(config_prev.font_name);
			BFREE_IF_NONNULL(config_prev.font_style);
			BFREE_IF_NONNULL(config_prev.text_file);
			memcpy(&config_prev, &src->config, sizeof(struct tp_config));
			config_prev.font_name = bstrdup(src->config.font_name);
			config_prev.font_style = bstrdup(src->config.font_style);
			config_prev.text_file = bstrdup(src->config.text_file);
			src->config_updated = 0;
		}

		pthread_mutex_unlock(&src->config_mutex);

		// check file status
		struct stat st = {0};
		os_stat(config_prev.text_file, &st);
		if(tp_compare_stat(&st, &st_prev)) {
			text_updated = 1;
			memcpy(&st_prev, &st, sizeof(struct stat));
		}

		// TODO: how long will it take to draw a new texture?
		// If it takes much longer than frame rate, it should notify the main thread to start fade-out.

		// load file if changed and draw
		if (config_updated || text_updated) {
			uint64_t time_ns = os_gettime_ns();
			char *text = tp_load_text(&config_prev);
			bool b_printable = text ? is_printable(text) : 0;

			// make an early notification
			if(b_printable) {
				os_atomic_set_bool(&src->text_updating, true);
			}

			struct tp_texture *tex;
			if(b_printable) {
				tex = tp_draw_texture(&config_prev, text);
			}
			else {
				tex = bzalloc(sizeof(struct tp_texture));
			}
			tex->time_ns = time_ns;
			tex->config_updated = config_updated;
			tex->is_crossfade = b_printable && b_printable_prev && !config_updated;

			pthread_mutex_lock(&src->tex_mutex);
			src->tex_new = pushback_texture(src->tex_new, tex); tex = NULL;
			pthread_mutex_unlock(&src->tex_mutex);

			BFREE_IF_NONNULL(text);

			debug("tp_draw_texture & tp_draw_texture takes %f ms\n",  (os_gettime_ns() - time_ns) * 1e-6);

			if (text_updated)
				b_printable_prev = b_printable;
		}

	}

	tp_config_destroy_member(&config_prev);
}

void tp_thread_start(struct tp_source *src)
{
	nice(19);

	src->running = true;
	pthread_create(&src->thread, NULL, tp_thread_main, src);
}

void tp_thread_end(struct tp_source *src)
{
	src->running = false;
	pthread_join(src->thread, NULL);
}
