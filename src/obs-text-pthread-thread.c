#include <obs-module.h>
#include "plugin-macros.generated.h"
#include <util/platform.h>
#include <util/threading.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <string.h>
#include <pango/pangocairo.h>
#ifdef PNG_FOUND
#include <png.h>
#endif // PNG_FOUND
#include <inttypes.h>
#include "obs-text-pthread.h"

#define debug(format, ...)
// #define debug(format, ...) fprintf(stderr, format, ##__VA_ARGS__)

static char *tp_load_text_file(struct tp_config *config)
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

static inline int blur_step(int blur)
{
	// only odd number is allowed
	// roughly 16 steps to draw with pango-cairo, then blur by pixel.
	return (blur/8) | 1;
}

static void tp_stroke_path(cairo_t *cr, PangoLayout *layout, const struct tp_config *config, int offset_x, int offset_y, uint32_t color, int width, int blur)
{
	bool path_preserved = false;
	const int bs = blur_step(blur);
	int b_end = -blur;
	if (blur && b_end+width<=0) b_end = -width+1;
	int b_start = b_end + (2*blur) / bs * bs;
	for (int b=b_start; b>=b_end; b-=bs) {
		double a = u32toFA(color) * (blur ? 0.5 - b * 0.5 / blur : 1.0);
		if (a < 1e-2) continue;
		int w = (width+b)*2;
		if (w<0) break;
		cairo_move_to(cr, offset_x, offset_y);
		cairo_set_source_rgba(cr, u32toFR(color), u32toFG(color), u32toFB(color), a);
		if(w>0) {
			cairo_set_line_width(cr, w);
			if (config->outline_shape & OUTLINE_BEVEL) {
				cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);
			}
			else if(config->outline_shape & OUTLINE_RECT) {
				cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
				cairo_set_miter_limit(cr, 1.999);
			}
			else if(config->outline_shape & OUTLINE_SHARP) {
				cairo_set_line_join(cr, CAIRO_LINE_JOIN_MITER);
				cairo_set_miter_limit(cr, 3.999);
			}
			else {
				cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
			}
			if (!path_preserved)
				pango_cairo_layout_path(cr, layout);
			cairo_stroke_preserve(cr);
			path_preserved = true;
		}
		else {
			pango_cairo_show_layout(cr, layout);
		}
	}

	cairo_surface_flush(cairo_get_target(cr));

	if (bs>1) {
		cairo_surface_t *surface = cairo_get_target(cr);
		const int w = cairo_image_surface_get_width(surface);
		const int h = cairo_image_surface_get_height(surface);
		uint8_t *data = cairo_image_surface_get_data(surface);
		const int bs1 = bs+1;
		uint32_t *tmp = bzalloc(sizeof(uint32_t) * w * bs1);
		uint32_t **tt = bzalloc(sizeof(uint32_t*) * bs1);

		for (int k=0; k<bs1; k++) {
			tt[k] = tmp + w*k;
		}

		const int bs2 = bs/2;
		const int div = bs*bs;
		for (int k=0, kt=0; k<h; k++) {
			const int k2 = k+bs2 < h ? k+bs2 : h-1;
			const int k1 = k-bs2-1;

			for (; kt<=k2; kt++) {
				for (uint32_t i=0; i<w; i++) {
					uint32_t s = data[(i+kt*w)*4+3];
					if (i>0) s += tt[kt%bs1][i-1];
					if (kt>0) s += + tt[(kt-1)%bs1][i];
					if (kt>0 && i>0) s -= tt[(kt-1)%bs1][i-1];
					tt[kt%bs1][i] = s;
				}
			}

			for (int i=0; i<w; i++) {
				const int i2 = i+bs2 < w ? i+bs2 : w-1;
				const int i1 = i-bs2-1;
				uint32_t s = tt[k2%bs1][i2];
				if (k1>=0) s -= tt[k1%bs1][i2];
				if (i1>=0) s -= tt[k2%bs1][i1];
				if (k1>=0 && i1>=0) s += tt[k1%bs1][i1];
				s /= div;
				if (s>255) s = 255;
				data[(i+k*w)*4+3] = s;
			}
		}

		bfree(tmp);
		bfree(tt);
	}
}

static inline void copy_rgba2a(uint8_t *d, const uint8_t *s, const int stride, const uint32_t w, const uint32_t h)
{
	uint32_t size = h * stride;
	for(int i=0, k=0; i<size; i+=4) {
		d[k++] = s[i+3];
	}
}

static inline uint32_t blend_rgba_ch(uint32_t c1, uint32_t c2, uint32_t a1, uint32_t a2, uint32_t a_255)
{
	if (a_255==0) return 0; // completely transparent
	uint32_t c21 = ((255 - a2) * a1 * c1 + 255 * a2 * c2) / a_255;
	if (c21>255) c21 = 255;
	return c21;
}

// blend RGBA colors, c1 is lower layer and c2 is front layer.
static inline uint32_t blend_rgba(uint32_t c2, uint32_t c1)
{
	uint32_t a_255 = 255*255 - (255-(c1>>24)) * (255-(c2>>24));
	uint32_t a = a_255/255;
	return (a << 24) |
		(blend_rgba_ch((c1>>16)&0xFF, (c2>>16)&0xFF, c1>>24, c2>>24, a_255) << 16) |
		(blend_rgba_ch((c1>> 8)&0xFF, (c2>> 8)&0xFF, c1>>24, c2>>24, a_255) <<  8) |
		(blend_rgba_ch((c1    )&0xFF, (c2    )&0xFF, c1>>24, c2>>24, a_255)      );
}

static inline uint32_t blend_text_ch(uint32_t xt, uint32_t xb, uint32_t at, uint32_t ab, uint32_t u, uint32_t a_255)
{
	return a_255 ? ((255-u) * ab * xb + u * at * xt) / a_255 : 0;
}

static inline uint32_t blend_text(uint32_t ct, uint32_t cb, uint32_t u)
{
	uint32_t a_255 = u * (ct>>24) + (255-u) * (cb>>24);
	return ((a_255/255) << 24) |
		(blend_text_ch((ct>>16)&0xFF, (cb>>16)&0xFF, ct>>24, cb>>24, u, a_255) << 16) |
		(blend_text_ch((ct>> 8)&0xFF, (cb>> 8)&0xFF, ct>>24, cb>>24, u, a_255) <<  8) |
		(blend_text_ch((ct    )&0xFF, (cb    )&0xFF, ct>>24, cb>>24, u, a_255)      );
}

static inline void blend_outline_shadow(uint8_t *s, const int stride, const uint32_t w, const uint32_t h, const uint8_t *so, const uint8_t *ss, const struct tp_config *config)
{
	uint32_t size = h * stride;
	uint32_t ct_a = config->color & 0xFF000000;
	for(int i=0, k=0; i<size; i+=4, k+=1) {
		uint32_t cs = ss ? ss[k]<<24 | (config->shadow_color  & 0xFFFFFF) : 0;
		uint32_t co = so ? so[k]<<24 | (config->outline_color & 0xFFFFFF) : 0;
		uint32_t ct = s ? s[i]<<16 | s[i+1]<<8 | s[i+2] | s[i+3]<<24 : 0;
		uint32_t c = blend_text((ct & 0x00FFFFFF) | ct_a, ss ? so ? blend_rgba(co, cs) : cs : co, ct>>24);
		s[i  ] = c>>16;
		s[i+1] = c>>8;
		s[i+2] = c;
		s[i+3] = c>>24;
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
	if (config->outline_shape & OUTLINE_SHARP)
		outline_width_blur *= 2;
	int shadow_abs_x = config->shadow ? abs(config->shadow_x) : 0;
	int shadow_abs_y = config->shadow ? abs(config->shadow_y) : 0;
	int offset_x = outline_width_blur + (config->shadow && config->shadow_x<0 ? -config->shadow_x : 0);
	int offset_y = outline_width_blur + (config->shadow && config->shadow_y<0 ? -config->shadow_y : 0);

	uint32_t body_width = config->width;
	uint32_t surface_width = body_width + outline_width_blur*2 + shadow_abs_x;
	uint32_t surface_height = config->height + outline_width_blur*2 + shadow_abs_y;

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, surface_width);
	n->surface = bzalloc(stride * surface_height);

	uint8_t *surface_outline = NULL;
	uint8_t *surface_shadow = NULL;

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
	pango_layout_set_indent(layout, config->indent * PANGO_SCALE);

	pango_layout_set_width(layout, body_width<<10);
	pango_layout_set_auto_dir(layout, config->auto_dir);
	pango_layout_set_wrap(layout, config->wrapmode);
	pango_layout_set_ellipsize(layout, config->ellipsize);
	pango_layout_set_spacing(layout, config->spacing * PANGO_SCALE);

	(config->markup ? pango_layout_set_markup : pango_layout_set_text)(layout, text, -1);

	PangoRectangle ink_rect, logical_rect;
	pango_layout_get_extents(layout, &ink_rect, &logical_rect);
	uint32_t surface_ink_height = PANGO_PIXELS_FLOOR(ink_rect.height) + PANGO_PIXELS_FLOOR(ink_rect.y) + outline_width_blur*2 + shadow_abs_y;
	uint32_t surface_ink_height1 = surface_height > surface_ink_height ? surface_ink_height : surface_height;

	if (shadow_abs_x || shadow_abs_y) {
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		if(outline_width_blur)
			tp_stroke_path(cr, layout, config, offset_x + config->shadow_x, offset_y + config->shadow_y, config->shadow_color, outline_width, outline_blur);
		tp_stroke_path(cr, layout, config, offset_x + config->shadow_x, offset_y + config->shadow_y, config->shadow_color, 0, 0);

		surface_shadow = bzalloc(stride * surface_height);
		copy_rgba2a(surface_shadow, n->surface, stride, surface_width, surface_ink_height1);
		memset(n->surface, 0, stride * surface_height);
	}

	if (outline_width_blur > 0) {
		debug("stroking outline width=%d\n", outline_width);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		tp_stroke_path(cr, layout, config, offset_x, offset_y, config->outline_color, outline_width, outline_blur);

		surface_outline = bzalloc(stride * surface_height);
		copy_rgba2a(surface_outline, n->surface, stride, surface_width, surface_ink_height1);
		memset(n->surface, 0, stride * surface_height);
	}

	// workaround to draw light transparent color on light surface, which became darker.
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	debug_fill_rgb(n->surface, config->color, stride*surface_ink_height1);
	uint32_t color_draw = config->color;
	if (config->outline || config->shadow) color_draw |= 0xFF000000;
	tp_stroke_path(cr, layout, config, offset_x, offset_y, color_draw, 0, 0);

	if (config->outline || config->shadow)
		blend_outline_shadow(n->surface, stride, surface_width, surface_ink_height1, surface_outline, surface_shadow, config);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	if (config->shrink_size) {
		uint32_t xoff = PANGO_PIXELS_FLOOR(logical_rect.x);
		if (xoff < 0) {
			n->width = PANGO_PIXELS_CEIL(logical_rect.x + logical_rect.width) + outline_width_blur*2 + shadow_abs_x;
			xoff = 0;
		}
		else
			n->width = PANGO_PIXELS_CEIL(logical_rect.width) + outline_width_blur*2 + shadow_abs_x;
		if (n->width > surface_width) n->width = surface_width;
		n->height = PANGO_PIXELS_CEIL(logical_rect.y + logical_rect.height) + outline_width_blur*2 + shadow_abs_y;
		if (n->height > surface_height) n->height = surface_height;
		if (n->width != surface_width) {
			uint32_t new_stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, n->width);
			for (uint32_t y=1; y<n->height; y++) {
				memmove(n->surface+new_stride*y, n->surface+stride*y + xoff*4, new_stride);
			}
		}
	}
	else {
		n->width = surface_width;
		n->height = surface_height;
	}

	debug("tp_draw_texture end: width=%d height=%d \n",  n->width, n->height);

	if (surface_shadow)
		bfree(surface_shadow);
	if (surface_outline)
		bfree(surface_outline);

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
	if (a->st_mtimensec != b->st_mtimensec) return true;
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

#ifdef PNG_FOUND
static void png_list_write_config(FILE *fp, const struct tp_config *config, const struct tp_config *prev)
{
#define WRITE_IF_UPDATED(val, fmt) \
	if (!prev || config->val != prev->val) \
		fprintf(fp, "#\t"#val":\t%"fmt"\n", config->val)
	WRITE_IF_UPDATED(fadein_ms, PRIu32);
	WRITE_IF_UPDATED(fadeout_ms, PRIu32);
	WRITE_IF_UPDATED(crossfade_ms, PRIu32);
}

static FILE *fopen_png_list(uint64_t ns, const struct tp_config *config)
{
	uint64_t ms = ns / 1000000;
	char *fname = bmalloc(strlen(config->save_file_dir) + 24);
	sprintf(fname, "%s/list-%08ds%03d.dat", config->save_file_dir, (int)(ms/1000), (int)(ms%1000));
	FILE *fp = fopen(fname, "w");
	bfree(fname);
	png_list_write_config(fp, config, NULL);
	return fp;
}

static void save_to_png(const uint8_t *surface, int width, int height, uint64_t ns, FILE *fp_png_list, const struct tp_config *config)
{
	uint64_t ms = ns / 1000000;
	char *fname = bmalloc(strlen(config->save_file_dir) + 24);
	sprintf(fname, "%s/text-%08ds%03d.png", config->save_file_dir, (int)(ms/1000), (int)(ms%1000));
	FILE *fp = fopen(fname, "wb");
	if (!fp) {
		blog(LOG_ERROR, "text-pthread: save_to_png: failed to open %s", fname);
		bfree(fname);
		return;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr) {
		blog(LOG_ERROR, "text-pthread: save_to_png: png_create_write_struct failed");
		fclose(fp);
		bfree(fname);
		return;
	}
	// TODO: use png_create_write_struct_2 instead so that bzalloc and bfree can check memory leak

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		blog(LOG_ERROR, "text-pthread: save_to_png: png_create_info_struct failed");
		png_destroy_write_struct(&png_ptr, NULL);
		fclose(fp);
		bfree(fname);
		return;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		blog(LOG_ERROR, "text-pthread: save_to_png: png_jmpbuf failed");
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		bfree(fname);
		return;
	}

	png_init_io(png_ptr, fp);

	// you may call png_set_filter to tune the speed

	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);
	for (int y=0; y<height; y++) {
		png_write_row(png_ptr, surface + y*width*4);
	}

	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	if (fp_png_list) {
		fprintf(fp_png_list, "%"PRIu64"\ttext-%08ds%03d.png\t%d\t%d\n", ms, (int)(ms/1000), (int)(ms%1000), width, height);
	}
	bfree(fname);
}

static void png_list_empty(uint64_t ns, FILE *fp_png_list)
{
	uint64_t ms = ns / 1000000;
	fprintf(fp_png_list, "%"PRIu64"\t-\n", ms);
}
#endif // PNG_FOUND

static void *tp_thread_main(void *data)
{
	struct tp_source *src = data;

	struct stat st_prev = {0};
	struct tp_config config_prev = {0};
	bool b_printable_prev = false;
#ifdef PNG_FOUND
	FILE *fp_png_list = NULL;
#endif // PNG_FOUND

	setpriority(PRIO_PROCESS, 0, 19);
	os_set_thread_name("text-pthread");

	while (src->running) {
		os_sleep_ms(33);

		pthread_mutex_lock(&src->config_mutex);

		bool config_updated = src->config_updated;
		bool text_updated = false;
#ifdef PNG_FOUND
		bool save_file_updated = false;
#endif // PNG_FOUND

		// check config and copy
		if(config_updated) {
			if (!config_prev.from_file && !src->config.from_file && config_prev.text && src->config.text && strcmp(config_prev.text, src->config.text))
				text_updated = true;
#ifdef PNG_FOUND
			if (config_prev.save_file != src->config.save_file)
				save_file_updated = true;
			else if (!src->config.save_file)
				save_file_updated = false;
			else if (strcmp(config_prev.save_file_dir, src->config.save_file_dir))
				save_file_updated = true;
			if (fp_png_list && !save_file_updated)
				png_list_write_config(fp_png_list, &src->config, &config_prev);
#endif // PNG_FOUND

			BFREE_IF_NONNULL(config_prev.font_name);
			BFREE_IF_NONNULL(config_prev.font_style);
			BFREE_IF_NONNULL(config_prev.text);
			BFREE_IF_NONNULL(config_prev.text_file);
#ifdef PNG_FOUND
			BFREE_IF_NONNULL(config_prev.save_file_dir);
#endif // PNG_FOUND
			memcpy(&config_prev, &src->config, sizeof(struct tp_config));
			config_prev.font_name = bstrdup(src->config.font_name);
			config_prev.font_style = bstrdup(src->config.font_style);
			if (!config_prev.from_file) {
				config_prev.text = bstrdup(src->config.text);
				config_prev.text_file = NULL;
			}
			else {
				config_prev.text = NULL;
				config_prev.text_file = bstrdup(src->config.text_file);
			}
#ifdef PNG_FOUND
			config_prev.save_file_dir = src->config.save_file ? bstrdup(src->config.save_file_dir) : NULL;
#endif // PNG_FOUND
			src->config_updated = 0;
		}

		pthread_mutex_unlock(&src->config_mutex);

		// check file status
		if (config_prev.from_file) {
			struct stat st = {0};
			os_stat(config_prev.text_file, &st);
			if(tp_compare_stat(&st, &st_prev)) {
				text_updated = 1;
				memcpy(&st_prev, &st, sizeof(struct stat));
			}
		}

		// TODO: how long will it take to draw a new texture?
		// If it takes much longer than frame rate, it should notify the main thread to start fade-out.

		// load file if changed and draw
		if (config_updated || text_updated) {
			uint64_t time_ns = os_gettime_ns();
			char *text = config_prev.from_file ? tp_load_text_file(&config_prev) : config_prev.text;
			bool b_printable = text ? is_printable(text) : 0;
#ifdef PNG_FOUND
			uint8_t *png_surface = NULL;
			int png_width = 0;
			int png_height = 0;
#endif // PNG_FOUND

			// make an early notification
			if(b_printable) {
				os_atomic_set_bool(&src->text_updating, true);
			}

			struct tp_texture *tex;
			if(b_printable) {
				tex = tp_draw_texture(&config_prev, text);
#ifdef PNG_FOUND
				if (config_prev.save_file) {
					png_width = tex->width;
					png_height = tex->height;
					png_surface = bzalloc(4*png_width*png_height);
					memcpy(png_surface, tex->surface, 4*png_width*png_height);
				}
#endif // PNG_FOUND
			}
			else {
				tex = bzalloc(sizeof(struct tp_texture));
			}
			tex->time_ns = time_ns;
			tex->config_updated = config_updated && !text_updated;
			tex->is_crossfade = b_printable && b_printable_prev && text_updated;

			pthread_mutex_lock(&src->tex_mutex);
			src->tex_new = pushback_texture(src->tex_new, tex); tex = NULL;
			pthread_mutex_unlock(&src->tex_mutex);

			if (config_prev.from_file)
				BFREE_IF_NONNULL(text);

			debug("tp_draw_texture & tp_draw_texture takes %f ms\n",  (os_gettime_ns() - time_ns) * 1e-6);

			if (text_updated)
				b_printable_prev = b_printable;

#ifdef PNG_FOUND
			if (save_file_updated) {
				if (fp_png_list) {
					fclose(fp_png_list);
					fp_png_list = NULL;
				}
				if (config_prev.save_file) {
					fp_png_list = fopen_png_list(time_ns, &config_prev);
				}
			}
			if (png_surface && fp_png_list) {
				save_to_png(png_surface, png_width, png_height, time_ns, fp_png_list, &config_prev);
			}
			else if (fp_png_list) {
				png_list_empty(time_ns, fp_png_list);
			}
			BFREE_IF_NONNULL(png_surface);
#endif // PNG_FOUND
		}

	}

#ifdef PNG_FOUND
	if (fp_png_list) {
		png_list_empty(os_gettime_ns(), fp_png_list);
		fclose(fp_png_list);
	}
#endif // PNG_FOUND
	tp_config_destroy_member(&config_prev);
	return NULL;
}

void tp_thread_start(struct tp_source *src)
{
	src->running = true;
	pthread_create(&src->thread, NULL, tp_thread_main, src);
}

void tp_thread_end(struct tp_source *src)
{
	src->running = false;
	pthread_join(src->thread, NULL);
}
