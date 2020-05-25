#include <obs-module.h>
#include <util/platform.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <pango/pangocairo.h>
#include "obs-text-pthread.h"

// TODO: remove below before release.
#define debug(format, ...) fprintf(stderr, format, ##__VA_ARGS__)

char *tp_load_text(struct tp_source *src)
{
	if (!src->text_file) return NULL;

	FILE *fp = fopen(src->text_file, "rb");
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

static struct tp_texture * tp_draw_texture(struct tp_source *src, char *text)
{
	// TODO: if using config in src, it should lock or make a copy before unlock.
	struct tp_texture *n = bzalloc(sizeof(struct tp_texture));
	n->time_ns = os_gettime_ns();

	// TODO: size
	uint32_t width = 1920, height = 1080;
	n->width = width;
	n->height = height;

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	n->surface = bzalloc(stride * height);
	cairo_surface_t *surface = cairo_image_surface_create_for_data(n->surface, CAIRO_FORMAT_ARGB32, width, height, stride);

	cairo_t *cr = cairo_create(surface);
	// TODO: should I set rectangle?
	cairo_set_source_rgba(cr, 0, 0, 0, 0);
	cairo_fill(cr);

	PangoLayout *layout = pango_cairo_create_layout(cr);

	pango_layout_set_markup(layout, text, -1);

	cairo_set_source_rgba(cr, 0, 0, 0, 1.0);
	pango_cairo_show_layout(cr, layout);

	cairo_surface_flush(surface);

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);

	return n;
}

static void *tp_thread_main(void *data)
{
	struct tp_source *src = data;

	char *text_file_prev = NULL;
	struct stat st_prev = {0};

	while (src->running) {
		struct timespec tv = {0, 33 * 1000 * 1000};
		nanosleep(&tv, NULL);

		pthread_mutex_lock(&src->config_mutex);

		bool need_draw = false;

		// check config
		if(src->text_file != text_file_prev) {
			text_file_prev = src->text_file;
			need_draw = true;
		}

		// check file status
		struct stat st = {0};
		os_stat(src->text_file, &st);
		memset(&st.st_atim, 0, sizeof(st.st_atim)); // ignore atime
		if(memcmp(&st, &st_prev, sizeof(struct stat))) {
			need_draw = 1;
			memcpy(&st_prev, &st, sizeof(struct stat));
		}

		// TODO: how long will it take to draw a new texture?
		// If it takes much longer than frame rate, it should notify the main thread to start fade-out.

		// load file if changed and draw
		char *text = NULL;
		if (need_draw)
			text = tp_load_text(src);

		pthread_mutex_unlock(&src->config_mutex);

		if (need_draw) {
			struct tp_texture *tex = tp_draw_texture(src, text);

			pthread_mutex_lock(&src->tex_mutex);
			if (src->tex_new) {
				free_texture(src->tex_new);
			}
			src->tex_new = tex;
			pthread_mutex_unlock(&src->tex_mutex);
		}

		if (text) {
			bfree(text);
			text = NULL;
		}
	}
}

void tp_thread_start(struct tp_source *src)
{
	// TODO: reduce priority of this thread

	src->running = true;
	pthread_create(&src->thread, NULL, tp_thread_main, src);
}

void tp_thread_end(struct tp_source *src)
{
	src->running = false;
	pthread_join(src->thread, NULL);
}
