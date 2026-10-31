#include "anim_internal.h"
#include <SDL3_image/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include "polygon.h"

void tex_cache_init(TexCache *tc)
{
	SDL_memset(tc, 0, sizeof *tc);
}

void tex_cache_clear(TexCache *tc)
{
	free(tc->tex_arena);
	free(tc->key_arena);
	free(tc->hash_table);
	tex_cache_init(tc);
}

static Uint64 hash(const char *s)
{
	Uint64 r = 0xCBF29CE484222325ULL;
	for (; *s; s++)
	{
	        r ^= (uint8_t)*s;
        	r *= 0x100000001B3ULL;
	}
	return r;
}

#define LAST_SIZE 2147483647
static int voodoo(int p)
{
	static const int table[] = {11, 23, 47, 97, 193, 389, 773, 1549, 3089, 6173, 12347, 24697, 49393, 98779, 197551, 395107, 790219, 1580437, 3160877, 6321751, 12643511, 25287023, 50574053, 101148101, 202296197, 404592389, 809184763, 1618369531, LAST_SIZE};
	for (int i = 1; i < 32; i += i) p = p | p >> i;
	p = (p + 1) >> 3;
	int nidx = 16 * !!(p & 0xFFFF0000)
	         +  8 * !!(p & 0xFF00FF00)
	         +  4 * !!(p & 0xF0F0F0F0)
	         +  2 * !!(p & 0xCCCCCCCC)
	         +      !!(p & 0xAAAAAAAA);
	return table[nidx];
}

static int tex_cache_read(TexCache *tc, const char *key, Uint64 h, int *hm)
{
	if (tc->hash_table_size == 0) return -1;
	*hm = h % tc->hash_table_size;
	int arena_i = -1;
	for (;;)
	{
		arena_i = tc->hash_table[*hm];
		if (arena_i < 0) return arena_i;
		if (tc->tex_arena[arena_i].hash == h)
			if (strcmp(tc->key_arena + tc->tex_arena[arena_i].key_offset, key) == 0)
				return arena_i;
		(*hm)++;
		if (*hm == tc->hash_table_size) *hm = 0;
	}
}

int tex_cache_get(TexCache *tc, const char *key, bool make_new, bool *is_new)
{
	Uint64 h = hash(key);
	int hm = 0;
	int arena_i = tex_cache_read(tc, key, h, &hm);
	if (!make_new) return arena_i;
	else if (arena_i >= 0)
	{
		*is_new = false;
		return arena_i;
	}
	if (tc->hash_table_size < LAST_SIZE && tc->tex_arena_size >= tc->hash_table_size / 4 * 3)
	{
		int new_size = voodoo(tc->hash_table_size);
		int *hash_table2 = realloc(tc->hash_table, sizeof(int) * new_size);
		if (hash_table2 == NULL) return -1;
		tc->hash_table = hash_table2;
		tc->hash_table_size = new_size;
		for (int i = 0; i < tc->hash_table_size; i++)
			tc->hash_table[i] = -1;
		for (int i = 0; i < tc->tex_arena_size; i++)
		{
			int hm = tc->tex_arena[i].hash % tc->hash_table_size;
			for (;;)
			{
				if (tc->hash_table[hm] < 0) break;
				hm++;
				if (hm == tc->hash_table_size) hm = 0;
			}
			tc->hash_table[hm] = i;
		}
	}
	int kl = strlen(key) + 1;
	if (tc->key_arena_size + kl > tc->key_arena_cap)
	{
		int new_size = tc->key_arena_cap * 2;
		if (new_size < 8) new_size = 8;
		if (new_size < tc->key_arena_size + kl)
			new_size = tc->key_arena_size + kl;
		char *key_arena2 = realloc(tc->key_arena, sizeof(char) * new_size);
		if (key_arena2 == NULL) return -1;
		tc->key_arena = key_arena2;
		tc->key_arena_cap = new_size;
	}
	if (tc->tex_arena_size == tc->tex_arena_cap)
	{
		int new_size = tc->tex_arena_cap * 2;
		if (new_size < 8) new_size = 8;
		Tex *tex_arena2 = realloc(tc->tex_arena, sizeof(Tex) * new_size);
		if (tex_arena2 == NULL) return -1;
		tc->tex_arena = tex_arena2;
		tc->tex_arena_cap = new_size;
	}
	arena_i = tc->tex_arena_size++;
	tc->hash_table[hm] = arena_i;
	tc->tex_arena[arena_i].hash = h;
	tc->tex_arena[arena_i].key_offset = tc->key_arena_size;
	memcpy(tc->key_arena + tc->key_arena_size, key, kl);
	tc->key_arena_size += kl;
	*is_new = true;
	return arena_i;
}

void tc_tester()
{
	TexCache tc;
	tex_cache_init(&tc);
	int aid[128];
	bool b;
	for (int c = 0; c < 32; c++) aid[c] = -1;
	for (int cc = 32; cc < 128; cc++)
	{
		int c = cc ^ 21;
		char s[5] = {'a', 'A', 'a', c, 0};
		int id = tex_cache_get(&tc, s, true, &b);
		aid[c] = id;
	}
	for (int c = 127; c + 1 > 0; c--)
	{
		char s[5] = {'a', 'A', 'a', c, 0};
		int id = tex_cache_get(&tc, s, false, &b);
		if (aid[c] != id) printf("wtf%d\n", (int)c);
	}
	tex_cache_clear(&tc);
}

void next_mipmap_dims(int *w, int *h)
{
	*w = (*w + 1) / 2;
	*h = (*h + 1) / 2;
}

#define BLENDER(x) left += (x) & 0x03030303, res += ((x) & 0xFCFCFCFC) >> 2
#define COMBINE (res + ((left & 0xFCFCFCFC) >> 2))
static void tex_regen_mipmaps(Tex *tex)
{
	for (int m = 0; m + 1 < MAX_MIPMAP_LEVEL; m++)
	{
		SDL_Surface *src = tex->mipmaps[m];
		SDL_Surface *dst = tex->mipmaps[m + 1];
		Uint32 *psrc = src->pixels;
		Uint32 *psrcn = psrc + src->w;
		Uint32 *pdst = dst->pixels;
		int y = 0;
		for (; y + 1 < src->h; y += 2)
		{
			int x = 0;
			for (; x + 1 < src->w; x += 2)
			{
				Uint32 res = 0, left = 0x02020202;
				for (int i = 0; i < 2; i++)
				{
					BLENDER(*psrc);
					psrc++;
					BLENDER(*psrcn);
					psrcn++;
				}
				*pdst++ = COMBINE;
			}
			if (x < src->w)
			{
				Uint32 res = 0, left = 0x02020202;
				BLENDER(*psrc);
				psrc++;
				BLENDER(*psrcn);
				psrcn++;
				*pdst++ = COMBINE;
			}
			psrc += src->w;
			psrcn += src->w;
		}
		if (y < src->h)
		{
			int x = 0;
			for (; x + 1 < src->w; x += 2)
			{
				Uint32 res = 0, left = 0x02020202;
				for (int i = 0; i < 2; i++)
				{
					BLENDER(*psrc);
					psrc++;
				}
				*pdst++ = COMBINE;
			}
			if (x < src->w)
			{
				Uint32 res = 0, left = 0x02020202;
				BLENDER(*psrc);
				psrc++;
				*pdst++ = COMBINE;
			}
		}
	}
}

static AnimImage anim_image(AnimAPI *api, const char *path)
{

	TexCache *tc = &api->anim->tc;
	bool is_new;
	int arena_idx = tex_cache_get(tc, path, true, &is_new);
	if (arena_idx < 0) return -1;
	Tex *tex = tc->tex_arena + arena_idx;

	tex->do_clamp = false;
	tex->mode_x = tex->mode_y = ZERO_ONE;

	if (!is_new) return arena_idx;

	SDL_Surface *img = IMG_Load(path);
	if (img == NULL) return -1;

	int left, right, top, bottom;
	if (!SDL_ISPIXELFORMAT_ALPHA(img->format))
	{
		left = 0;
		right = img->w;
		top = 0;
		bottom = img->h;
	}
	else
	{
		left = img->w;
		right = 0;
		top = img->h;
		bottom = 0;
		const SDL_PixelFormatDetails *format = SDL_GetPixelFormatDetails(img->format);
		const SDL_Palette *pal = SDL_GetSurfacePalette(img);
		for (int i = 0; i < img->h; i++)
			for (int j = 0; j < img->w; j++)
			{
				void *base = (Uint8 *)img->pixels + i * img->pitch + j * format->bytes_per_pixel;
				Uint32 pix = *(Uint32 *)base;
				Uint8 alpha = 0;
				SDL_GetRGBA(pix, format, pal, NULL, NULL, NULL, &alpha);
				if (alpha)
				{
					left = SDL_min(left, j);
					right = SDL_max(right, j + 1);
					top = SDL_min(top, i);
					bottom = SDL_max(bottom, i + 1);
				}
			}
		left = SDL_min(left, right);
		top = SDL_min(top, bottom);
	}

	if (bottom == 0)
	{
		tex->pixels = NULL;
		tex->img_left = tex->img_right = tex->img_top = tex->img_bottom = 0;
		tex->img_w = img->w;
		tex->img_h = img->h;
		goto done_bound;
	}
	size_t total_size = 0;
	for (int w = right - left, h = bottom - top, i = 0; i < MAX_MIPMAP_LEVEL; next_mipmap_dims(&w, &h), i++)
		total_size += sizeof(Uint32) * w * h;
	tex->pixels = SDL_malloc(total_size);
	if (tex->pixels == NULL) goto err_after_img;

	Uint32 *p = tex->pixels;
	for (int w = right - left, h = bottom - top, i = 0; i < MAX_MIPMAP_LEVEL; next_mipmap_dims(&w, &h), i++)
	{
		tex->mipmaps[i] = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, p, sizeof(Uint32) * w);
		p += w * h;
	}
	SDL_Rect srcrect;
	srcrect.x = left; srcrect.y = top;
	srcrect.w = right - left; srcrect.h = bottom - top;
	SDL_SetSurfaceBlendMode(img, SDL_BLENDMODE_NONE);
	SDL_BlitSurface(img, &srcrect, tex->mipmaps[0], NULL);
	SDL_PremultiplySurfaceAlpha(tex->mipmaps[0], false);
	for (int i = 0; i < MAX_MIPMAP_LEVEL; i++)
		SDL_SetSurfaceBlendMode(tex->mipmaps[i], SDL_BLENDMODE_BLEND_PREMULTIPLIED);
	tex_regen_mipmaps(tex);
	tex->img_left = left;
	tex->img_right = right;
	tex->img_top = top;
	tex->img_bottom = bottom;
	tex->img_w = img->w;
	tex->img_h = img->h;

done_bound:
	SDL_DestroySurface(img);
	return arena_idx;
/*
err_after_pixels:
	free(tex->pixels);
	tex->pixels = NULL;
*/
err_after_img:
	SDL_DestroySurface(img);
	return -1;
}

static void mat_mul_mat(float *dst, const float *a, const float *b)
{
	float r[6];
	r[0] = a[0] * b[0] + a[1] * b[3];
	r[1] = a[0] * b[1] + a[1] * b[4];
	r[2] = a[0] * b[2] + a[1] * b[5] + a[2];
	r[3] = a[3] * b[0] + a[4] * b[3];
	r[4] = a[3] * b[1] + a[4] * b[4];
	r[5] = a[3] * b[2] + a[4] * b[5] + a[5];
	SDL_memcpy(dst, r, sizeof r);
}

static void mat_mul_vec(float *dst, const float *a, const float *b)
{
	float r[2];
	r[0] = a[0] * b[0] + a[1] * b[1] + a[2];
	r[1] = a[3] * b[0] + a[4] * b[1] + a[5];
	SDL_memcpy(dst, r, sizeof r);
}

static void mat_bone(float *restrict dst, const BoneNode *bone)
{
	float s = SDL_sinf(bone->rot);
	float c = SDL_cosf(bone->rot);
	
	dst[0] = c * bone->scale;
	dst[1] = -s * bone->scale;
	dst[3] = s * bone->scale;
	dst[4] = c * bone->scale;

	dst[2] = (bone->img_offset[0] * bone->scale - bone->img_pivot[0]) * c - (bone->img_offset[1] * bone->scale - bone->img_pivot[1]) * s + bone->img_pivot[0];
	dst[5] = (bone->img_offset[0] * bone->scale - bone->img_pivot[0]) * s + (bone->img_offset[1] * bone->scale - bone->img_pivot[1]) * c + bone->img_pivot[1];
}

#define DEF_CONV(type, param, axis, size) \
static float type##_zero_one_##param(Anim *anim, AnimImage img, float x) \
{ \
	Tex *tex = &anim->tc.tex_arena[img]; \
	int dmax = SDL_max(tex->img_w, tex->img_h); \
	int dmin = SDL_min(tex->img_w, tex->img_h); \
	switch (tex->type##_##axis) \
	{ \
	default: \
	case ZERO_ONE:    return x; \
	case NEG_ONE_ONE: return x + x - 1.0f; \
	case FIT_MIN:     return (x + x - 1.0f) * tex->img_##size / dmin; \
	case FIT_MAX:     return (x + x - 1.0f) * tex->img_##size / dmax; \
	}; \
} \
static float scr_zero_one_##param(Anim *anim, float x) \
{ \
	int dmax = SDL_max(anim->target->w, anim->target->h); \
	int dmin = SDL_min(anim->target->w, anim->target->h); \
	switch (anim->target_mode_##axis) \
	{ \
	default: \
	case ZERO_ONE:    return x; \
	case NEG_ONE_ONE: return x + x - 1.0f; \
	case FIT_MIN:     return (x + x - 1.0f) * anim->target->size / dmin; \
	case FIT_MAX:     return (x + x - 1.0f) * anim->target->size / dmax; \
	}; \
}

DEF_CONV(mode, u, x, w)
DEF_CONV(mode, v, y, h)

#define DEF_CONV_SCR(param, axis, size) \
static float scr_##param##_zero_one(Anim *anim, float x) \
{ \
	int dmax = SDL_max(anim->target->w, anim->target->h); \
	int dmin = SDL_min(anim->target->w, anim->target->h); \
	switch (anim->target_mode_##axis) \
	{ \
	default: \
	case ZERO_ONE:    return x; \
	case NEG_ONE_ONE: return x * 0.5f + 0.5f; \
	case FIT_MIN:     return x * dmin / anim->target->size * 0.5f + 0.5f; \
	case FIT_MAX:     return x * dmax / anim->target->size * 0.5f + 0.5f; \
	}; \
}

DEF_CONV_SCR(x, x, w)
DEF_CONV_SCR(y, y, h)

struct sl_draw_args
{
	Anim *anim;
	Tex *tex;
};

static void sl_draw(float *l, float *r, void *user)
{
	struct sl_draw_args *args = user;
	Anim *anim = args->anim;
	Tex *tex = args->tex;

	int y = l[1];
	float du = (r[2] - l[2]) / (r[0] - l[0]);
	float dv = (r[3] - l[3]) / (r[0] - l[0]);
	float dm = du * du * tex->img_w * tex->img_w + dv * dv * tex->img_h * tex->img_h;
	int mipmap_level = 0;
	float mip_div = 1.0f;
	for (;;)
	{
		if (dm < 2.0f || mipmap_level + 1 >= MAX_MIPMAP_LEVEL) break;
		dm *= 0.25f;
		mip_div *= 0.5f;
		mipmap_level++;
	}

	SDL_Surface *tex_s = tex->mipmaps[mipmap_level];
	Uint32 *tex_pixels = tex_s->pixels;
	SDL_Surface *target = anim->target;
	Uint32 *row = (Uint32 *)((Uint8 *)target->pixels + target->pitch * y);

	int x0 = ceilf(l[0]);
	int x1 = ceilf(r[0]);
	if (x0 < 0) x0 = 0;
	if (x1 > target->w) x1 = target->w;
	if (y < 0 || y >= target->h) return;
	for (int x = x0; x < x1; x++)
	{
		float u = l[2] + du * (x - l[0]);
		float v = l[3] + dv * (x - l[0]);
		if (tex->do_clamp)
		{
			u = SDL_max(tex->clamp_left, SDL_min(tex->clamp_right, u));
			v = SDL_max(tex->clamp_top, SDL_min(tex->clamp_bottom, v));
		}
		u *= tex->img_w;
		v *= tex->img_h;
		if (u <= tex->img_left || u >= tex->img_right
		|| v <= tex->img_top || v >= tex->img_bottom)
			continue;
		u = (u - tex->img_left) * mip_div - 0.5f;
		v = (v - tex->img_top) * mip_div - 0.5f;
		int iu = floorf(u);
		int iv = floorf(v);
		float fu = u - iu;
		float fv = v - iv;
		Uint32 ifu = 256.0f * fu, nfu = 256 - ifu;
		Uint32 ifv = 256.0f * fv, nfv = 256 - ifv;
		Uint32 p00 = iu >= 0 && iv >= 0 ? tex_pixels[iv * tex_s->w + iu] : 0;
		Uint32 p10 = iu + 1 < tex_s->w && iv >= 0 ? tex_pixels[iv * tex_s->w + iu + 1] : 0;
		Uint32 p01 = iu >= 0 && iv + 1 < tex_s->h ? tex_pixels[(iv + 1) * tex_s->w + iu] : 0;
		Uint32 p11 = iu + 1 < tex_s->w && iv + 1 < tex_s->h ? tex_pixels[(iv + 1) * tex_s->w + iu + 1] : 0;
		Uint32 pb = 0;
		for (int i = 0; i < 32; i += 8)
		{
			Uint32 b = nfu * nfv * ((p00 >> i) & 0xFF)
			         + ifu * nfv * ((p10 >> i) & 0xFF)
			         + nfu * ifv * ((p01 >> i) & 0xFF)
			         + ifu * ifv * ((p11 >> i) & 0xFF);
			b >>= 16;
			pb += b << i;
		}
		Uint32 p = row[x];
		Uint8 ab = pb >> 24;
		Uint32 pn = 0xFF000000;
		for (int i = 0; i < 24; i += 8)
		{
			Uint8 cn = (((p >> i) & 0xFF) * (256 - ab) + ((pb >> i) & 0xFF) * 256) >> 8;
			pn += cn << i;
		}
		row[x] = pn;
	}
}

static void anim_render_one_impl(Anim *anim, BoneNode *bone, float *mat)
{
	Tex *tex = &anim->tc.tex_arena[bone->img];
	if (tex->do_clamp)
	{
		SDL_Log("stub anim_render_one_impl do_clamp");
		return;
	}

	float cu0 = (float)(tex->img_left - 1) / tex->img_w;
	float cu1 = (float)(tex->img_right + 1) / tex->img_w;
	float cv0 = (float)(tex->img_top - 1) / tex->img_h;
	float cv1 = (float)(tex->img_bottom + 1) / tex->img_h;

	float x0 = mode_zero_one_u(anim, bone->img, cu0);
	float x1 = mode_zero_one_u(anim, bone->img, cu1);
	float y0 = mode_zero_one_v(anim, bone->img, cv0);
	float y1 = mode_zero_one_v(anim, bone->img, cv1);

	float props[32] = {
		x0, y0, cu0, cv0,
		x1, y0, cu1, cv0,
		x1, y1, cu1, cv1,
		x0, y1, cu0, cv1
	};
	for (int i = 0; i < 16; i += 4)
		mat_mul_vec(props + i, mat, props + i);
	for (int i = 0; i < 16; i += 4)
	{
		props[i] = scr_x_zero_one(anim, props[i]) * anim->target->w;
		props[i + 1] = scr_y_zero_one(anim, props[i + 1]) * anim->target->h;
	}
	const float planes[12] = {
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		-1.0f, 0.0f, anim->target->w,
		0.0f, -1.0f, anim->target->h
	};
	int vert_cnt = 4;
	for (int i = 0; i < 12; i += 3)
		vert_cnt = clip_polygon(props, 4, vert_cnt, planes[i], planes[i + 1], planes[i + 2]);
	rasterize_polygon(props, 4, vert_cnt, sl_draw, &(struct sl_draw_args){ anim, tex });
}

typedef struct RenderCmd
{
	float mat[6];
	BoneNode *bone;
} RenderCmd;

static int count_nodes(BoneNode *root)
{
	int r = 1;
	for (int i = 0; i < root->children_count; i++)
		r += count_nodes(root->children[i]);
	return r;
}

static RenderCmd *populate_cmd(RenderCmd *start, BoneNode *root, float *base_mat)
{
	mat_bone(start->mat, root);
	mat_mul_mat(start->mat, base_mat, start->mat);
	start->bone = root;
	base_mat = start->mat;
	start++;
	for (int i = 0; i < root->children_count; i++)
		start = populate_cmd(start, root->children[i], base_mat);
	return start;
}

static void z_sort_cmd(RenderCmd *cmd, int size)
{
	if (size < 2) return;
	for (int i = 1; i < size; i++)
	{
		int j = i;
		RenderCmd tmp = cmd[i];
		while (j >= 1 && cmd[j - 1].bone->z > tmp.bone->z)
		{
			cmd[j] = cmd[j - 1];
			j--;
		}
		cmd[j] = tmp;
	}
}

static void anim_render_all(AnimAPI *api, BoneNode *root)
{
	int nodes = count_nodes(root);
	RenderCmd cmds[nodes];
	float mat[6] = { 1, 0, 0, 0, 1, 0 };
	populate_cmd(cmds, root, mat);
	z_sort_cmd(cmds, nodes);
	for (int i = 0; i < nodes; i++)
		anim_render_one_impl(api->anim, cmds[i].bone, cmds[i].mat);
}

static void anim_render_one(AnimAPI *api, BoneNode *bone)
{
	float mat[6];
	mat_bone(mat, bone);
	anim_render_one_impl(api->anim, bone, mat);
}

static void anim_set_clamp_bounds(AnimAPI *api, AnimImage img, float left, float right, float top, float bottom)
{
	Tex *tex = &api->anim->tc.tex_arena[img];
	tex->do_clamp = true;
	tex->clamp_left = left;
	tex->clamp_right = right;
	tex->clamp_top = top;
	tex->clamp_bottom = bottom;
}

static void anim_set_no_clamp(AnimAPI *api, AnimImage img)
{
	api->anim->tc.tex_arena[img].do_clamp = false;
}

static void anim_image_set_uv_mode(AnimAPI *api, AnimImage img, UVModeAxis x, UVModeAxis y)
{
	Tex *tex = &api->anim->tc.tex_arena[img];
	tex->mode_x = x;
	tex->mode_y = y;
}

static void anim_target_set_uv_mode(AnimAPI *api, UVModeAxis x, UVModeAxis y)
{
	api->anim->target_mode_x = x;
	api->anim->target_mode_y = y;
}

static void anim_shader_fill(AnimAPI *api, void (*shader)(float *, const float *, float, void *), void *user)
{
	SDL_Surface *target = api->anim->target;
	float x0 = scr_zero_one_u(api->anim, 0);
	float x1 = scr_zero_one_u(api->anim, 1);
	float y0 = scr_zero_one_v(api->anim, 0);
	float y1 = scr_zero_one_v(api->anim, 1);
	float dx = (x1 - x0) / target->w;
	float dy = (y1 - y0) / target->h;
	float dm = hypotf(dx, dy);
	for (int i = 0; i < target->h; i++)
		for (int j = 0; j < target->w; j++)
		{
			Uint32 *p = (Uint32 *)((Uint8 *)target->pixels + i * target->pitch) + j;
			float uv[2] = { x0 + dx * j, y0 + dy * i };
			float rgba[4] = { 0, 0, 0, 1 };
			shader(rgba, uv, dm, user);
			Uint32 pn = 0xFF000000;
			for (int k = 0; k < 3; k++)
			{
				Uint32 c = SDL_max(0, SDL_min(255, rgba[k] * 255.0f + ((*p >> (k * 8)) & 0xFF) * (1.0f - rgba[3])));
				pn += c << (k * 8);
			}
			*p = pn;
		}
}

static void anim_rgba_fill(AnimAPI *api, const float *rgba)
{
	SDL_Surface *target = api->anim->target;
	Uint8 rgba2[4];
	for (int i = 0; i < 4; i++)
		rgba2[i] = SDL_max(0.0f, SDL_min(1.0f, rgba[i])) * 255.0f;
	for (int i = 0; i < target->h; i++)
		for (int j = 0; j < target->w; j++)
		{
			Uint32 *p = (Uint32 *)((Uint8 *)target->pixels + i * target->pitch) + j;
			Uint32 pn = 0xFF000000;
			for (int k = 0; k < 3; k++)
			{
				Uint32 c = rgba2[k] + ((((*p >> (k * 8)) & 0xFF) * (255 - rgba2[3])) >> 8);
				pn += c << (k * 8);
			}
			*p = pn;
		}

}

void anim_clear_cache(Anim *anim)
{
	TexCache *tc = &anim->tc;
	for (int i = 0; i < tc->tex_arena_size; i++)
	{
		for (int j = 0; j < MAX_MIPMAP_LEVEL; j++)
			SDL_DestroySurface(tc->tex_arena[i].mipmaps[j]);
		SDL_free(tc->tex_arena[i].pixels);
	}
	tex_cache_clear(tc);
}

void anim_api_init(Anim *anim, AnimAPI *api)
{
	tex_cache_init(&anim->tc);
	api->anim = anim;
	anim->target_mode_x = ZERO_ONE;
	anim->target_mode_y = ZERO_ONE;
#define X(impl, name, ret, args) api->name = impl;
#include "anim_api_def.h"
#undef X
}

void anim_api_clear(AnimAPI *api)
{
	anim_clear_cache(api->anim);
	SDL_memset(api, 0, sizeof *api);
}
