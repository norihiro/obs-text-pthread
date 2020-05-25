#ifndef OBS_TEXT_PTHREAD_H
#define OBS_TEXT_PTHREAD_H

#include <pthread.h>

struct tp_texture
{
	uint32_t width, height;
	gs_texture_t *tex;
	uint8_t *surface;
	uint64_t time_ns;

	struct tp_texture *next;
};

struct tp_source
{
	// config
	// write from main
	// read from thread
	pthread_mutex_t config_mutex;
	char *text_file;
	volatile bool running;

	// new texture
	// write from thread
	// read from main and set to NULL
	pthread_mutex_t tex_mutex;
	struct tp_texture *tex_new;

	// internal use for main
	struct tp_texture *textures;

	// threads
	pthread_t thread;
};

void tp_thread_start(struct tp_source *src);
void tp_thread_end(struct tp_source *src);

static inline void free_texture(struct tp_texture *t)
{
	if (t->tex) {
		obs_enter_graphics();
		for (struct tp_texture *i = t; i; i = i->next) {
			if (i->tex) gs_texture_destroy(i->tex);
			i->tex = NULL;
		}
		obs_leave_graphics();
	}
	if (t->surface) bfree(t->surface);
	if (t->next) free_texture(t->next);
	bfree(t);
}

static inline struct tp_texture *adv_texture(struct tp_texture *t)
{
	struct tp_texture *n = t->next;
	t->next = NULL;
	free_texture(t);
	return n;
}

#endif // OBS_TEXT_PTHREAD_H
