#ifndef ANIM_H
#define ANIM_H

typedef int AnimImage;
typedef struct Anim Anim;

typedef enum UVModeAxis
{
	ZERO_ONE,
	NEG_ONE_ONE,
	FIT_MIN,
	FIT_MAX,
} UVModeAxis;

typedef struct BoneNode
{
	float rot, scale;
	float img_pivot[2];
	float img_offset[2];
	AnimImage img;
	int z;
	int children_count;
	struct BoneNode **children;
} BoneNode;

typedef struct AnimAPI
{
	Anim *anim;
#define X(impl, mem, ret, args) ret (*mem)args;
#include "anim_api_def.h"
#undef X
} AnimAPI;

#endif
