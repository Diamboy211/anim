#ifndef LOADER_H
#define LOADER_H

#include <SDL3/SDL.h>
#include <time.h>
#include "anim.h"

typedef struct Loader
{
	void (*anim_draw)(AnimAPI *, float);
	SDL_SharedObject *so;
	const char *path, *copy_path;
	time_t last_modified;
} Loader;

void loader_init(Loader *loader);
bool load_lib(Loader *loader);
void unload_lib(Loader *loader);

#endif
