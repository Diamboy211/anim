#ifndef ANIM_INTERNAL_H
#define ANIM_INTERNAL_H

#include "anim.h"
#include <SDL3/SDL.h>

#define MAX_MIPMAP_LEVEL 8

typedef struct Tex
{
	SDL_Surface *mipmaps[MAX_MIPMAP_LEVEL];
	Uint64 hash;
	Uint32 *pixels;
	int key_offset;
	int img_left, img_right, img_top, img_bottom;
	int img_w, img_h;
	float clamp_left, clamp_right, clamp_top, clamp_bottom;
	UVModeAxis mode_x, mode_y;
	bool do_clamp;
} Tex;

typedef struct TexCache
{
	Tex *tex_arena;
	char *key_arena;
	int *hash_table;
	int hash_table_size;
	int tex_arena_size;
	int key_arena_size;
	int tex_arena_cap;
	int key_arena_cap;
} TexCache;

struct Anim
{
	TexCache tc;
	SDL_Surface *target;
	UVModeAxis target_mode_x, target_mode_y;
};

void tex_cache_init(TexCache *tc);
void tex_cache_clear(TexCache *tc);
int tex_cache_get(TexCache *tc, const char *key, bool make_new, bool *is_new);

void anim_clear_cache(Anim *anim);

void anim_api_init(Anim *anim, AnimAPI *api);
void anim_api_clear(AnimAPI *api);

#endif
