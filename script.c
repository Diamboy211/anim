#include "anim.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define IMG(name, path) AnimImage name = api->image(api, path); api->image_set_uv_mode(api, name, FIT_MIN, FIT_MIN); api->set_no_clamp(api, name)

static void bone_set_(BoneNode *bone, float rot, float scale, float px, float py, float ox, float oy, AnimImage img, int z, int children_count, BoneNode **children)
{
	bone->rot = rot;
	bone->scale = scale;
	bone->img_pivot[0] = px;
	bone->img_pivot[1] = py;
	bone->img_offset[0] = ox;
	bone->img_offset[1] = oy;
	bone->img = img;
	bone->z = z;
	bone->children_count = children_count;
	bone->children = children;
}
#define bone_set(bone, rot, scale, px, py, ox, oy, img, z, children) \
	bone_set_(bone, rot, scale, px, py, ox, oy, img, z, sizeof children / sizeof *children, children)
#define leaf_set(bone, rot, scale, px, py, ox, oy, img, z) \
	bone_set_(bone, rot, scale, px, py, ox, oy, img, z, 0, NULL)

static float smoothstep(float a, float b, float x)
{
	if (a > b)
	{
		a *= -1;
		b *= -1;
		x *= -1;
	}
	if (x <= a) return 0;
	if (x >= b) return 1;
	x = (x - a) / (b - a);
	return x * x * (3.0f - x * 2.0f);
}

static float sp(const float *uv, float dx, float dy, float ph)
{
	float v = sinf(uv[0] * dx + uv[1] * dy + ph) * 0.5f + 0.5f;
	return v * v * v;
}

static void heart_shader(float *rgb, const float *uv, float dm, void *user)
{
	float t = *(float *)user;
	rgb[0] = 1.0f;
	rgb[1] = 0.81f;
	rgb[2] = 0.934f;

	float ripple = (sp(uv, 16.0f, 4.8f, t * -0.8f)
		+ sp(uv, 16.0f, -4.0f, -t)
		+ sp(uv, 12.0f, -8.0f, t * -1.3f)
		+ sp(uv, 8.0f, -17.6f, t * -1.7f)
		+ sp(uv, -4.8f, -16.0f, t * -2.2f)) * 0.2f
		+ (sp(uv, 24.0f, 7.2f, t * -0.9f)
		+ sp(uv, 24.0f, -6.0f, t * 1.1f)
		+ sp(uv, 18.0f, -12.0f, t * -1.5f)
		+ sp(uv, 6.0f, -19.8f, t * -2.0f)
		+ sp(uv, -7.2f, -18.0f, t * -2.5f)) * 0.13f + 0.15f;
	rgb[1] += ripple * 0.1f;
	rgb[2] += ripple * 0.05f;
	
	float l2 = uv[0]*uv[0] + uv[1]*uv[1];
	if (l2 == 0.0f)
	{
		rgb[1] = 1.0f;
		rgb[2] = 1.0f;
		return;
	}
	float an = fabsf(atan2f(uv[0], uv[1]));
	float r = ((-0.1903f * an + 0.8208f) * an - 0.7959f) * an + 1.3f;
	float d = r / sqrtf(l2) + t;
	float df = d - roundf(d);
	float diff = r / l2;
	float l = smoothstep(diff * dm * 3, -diff * dm * 3, fabsf(df)) * 2.0f;
	rgb[1] += (1.0f - rgb[1]) * l;
	rgb[2] += (1.0f - rgb[2]) * l;
}

static void intro(AnimAPI *api, float t)
{
	IMG(bg, "assets/bg.png");
	IMG(original, "assets/intro_text/original.png");
	IMG(mery1, "assets/intro_text/mery1.png");
	IMG(mery2, "assets/intro_text/mery2.png");
	IMG(lonely, "assets/intro_text/lonely.png");
	IMG(dash, "assets/intro_text/dash.png");
	IMG(momone, "assets/intro_text/momone.png");
	IMG(chinoi, "assets/intro_text/chinoi.png");
	IMG(character, "assets/intro_text/char.png");
	IMG(hoshino, "assets/intro_text/hoshino.png");
	IMG(una, "assets/intro_text/una.png");
	IMG(handle, "assets/intro_text/handle.png");
	IMG(anim, "assets/intro_text/anim.png");
	IMG(me, "assets/intro_text/me.png");

	IMG(window_una_none, "assets/window/una_none.png");
	IMG(window_high, "assets/window/high.png");
	IMG(icons, "assets/window/icons.png");
	float beat = t * (119.0f / 60.0f);
	int beati = (beat * 0.25f - floorf(beat * 0.25f)) * 4.0f;
	BoneNode b_bg,
		b_original, b_mery1, b_mery2, b_lonely,
		b_dash, b_momone, b_chinoi,
		b_character, b_hoshino, b_una, b_handle,
		b_anim, b_me,
		b_window_none, b_window_high, b_icons;
	BoneNode *c_bg[] = {
		&b_original, &b_mery1, &b_mery2, &b_lonely,
		&b_dash, &b_momone, &b_chinoi,
		&b_character, &b_hoshino, &b_una, &b_handle,
		&b_anim, &b_me,
		&b_window_none, &b_window_high, &b_icons
	};
	float rand_rot[52] = { -0.005f, 0.04f, -0.034f, -0.044f, 0.018f, -0.032f, -0.005f, 0.015f, -0.05f, 0.016f, -0.032f, 0.008f, -0.048f, -0.021f, 0.015f, -0.001f, 0.046f, -0.013f, 0.047f, 0.003f, -0.04f, -0.047f, 0.05f, -0.002f, 0.05f, 0.021f, -0.025f, -0.016f, -0.007f, 0.046f, -0.05f, -0.016f, 0.036f, 0.012f, 0.008f, 0.028f, -0.005f, -0.015f, 0.003f, 0.019f, -0.042f, -0.02f, -0.017f, -0.032f, 0.023f, -0.036f, 0.011f, -0.043f, -0.02f, -0.038f, 0.033f, -0.015f };
	float *rt = rand_rot + beati * 13;
	bone_set(&b_bg, 0, 1, 0, 0, 0, 0, bg, -200, c_bg);
	leaf_set(&b_original, rt[0], 1, -0.70703f, -0.076172f, 0, 0, original, 0);
	leaf_set(&b_mery1, rt[1], 1, -0.385742f, -0.083984f, 0, 0, mery1, 0);
	leaf_set(&b_mery2, rt[2], 1, -0.192383f, -0.087891f, 0, 0, mery2, 0);
	leaf_set(&b_lonely, rt[3], 1, 0.035156f, -0.087891f, 0, 0, lonely, 0);
	leaf_set(&b_dash, rt[4], 1, 0.300781f, -0.085938f, 0, 0, dash, 0);
	leaf_set(&b_momone, rt[5], 1, 0.535156f, -0.080078f, 0, 0, momone, 0);
	leaf_set(&b_chinoi, rt[6], 1, 0.758789f, -0.079102f, 0, 0, chinoi, 0);
	leaf_set(&b_character, rt[7], 1, -0.461914f, 0.043945f, 0, 0, character, 0);
	leaf_set(&b_hoshino, rt[8], 1, -0.103516f, 0.042969f, 0, 0, hoshino, 0);
	leaf_set(&b_una, rt[9], 1, 0.107422f, 0.041016f, 0, 0, una, 0);
	leaf_set(&b_handle, rt[10], 1, 0.439453f, 0.042969f, 0, 0, handle, 0);
	leaf_set(&b_anim, rt[11], 1, -0.196289f, 0.166992f, 0, 0, anim, 0);
	leaf_set(&b_me, rt[12], 1, 0.179688f, 0.166992f, 0, 0, me, 0);
	leaf_set(&b_window_none, 0, 1, 0, 0, 0, 0, window_una_none, -195);
	leaf_set(&b_window_high, 0, 1, 0, 0, 0, 0, window_high, -195);
	leaf_set(&b_icons, 0, 1, 0, 0, 0, 0, icons, -195);
	api->render_all(api, &b_bg);
}

static void bridge(AnimAPI *api, float t)
{
	IMG(bg_frame, "assets/bg_frame.png");
	IMG(icons, "assets/window/icons.png");
	api->shader_fill(api, heart_shader, &t);
	BoneNode b_bg, b_icons;
	BoneNode *c_bg[] = { &b_icons };
	bone_set(&b_bg, 0, 1, 0, 0, 0, 0, bg_frame, -200, c_bg);
	leaf_set(&b_icons, 0, 1, 0, 0, 0, 0, icons, -195);
	api->render_all(api, &b_bg);
}

static void chorus(AnimAPI *api, float t)
{
	IMG(bg, "assets/bg.png");
	IMG(torso, "assets/body/torso.png");
	IMG(wing_left, "assets/body/wing_left.png");
	IMG(wing_right, "assets/body/wing_right.png");
	IMG(arm_left, "assets/body/arm_left.png");
	IMG(arm_right, "assets/body/arm_right.png");
	IMG(ribbon_left, "assets/body/ribbon_left.png");
	IMG(ribbon_right, "assets/body/ribbon_right.png");
	IMG(skirt, "assets/body/skirt.png");
	IMG(belt_heart, "assets/body/belt_heart.png");
	IMG(neck, "assets/neck.png");
	IMG(jaw, "assets/head/jaw.png");
	IMG(brows, "assets/head/eyes/brows.png");

	IMG(eyes_n, "assets/head/eyes/closed_n.png");
	IMG(eyes_u, "assets/head/eyes/closed_u.png");
	IMG(eyes_c, "assets/head/eyes/closing.png");
	IMG(eyes_o, "assets/head/eyes/open.png");

	IMG(mouth_a, "assets/head/mouth/a.png");
	IMG(mouth_i, "assets/head/mouth/i.png");
	IMG(mouth_u, "assets/head/mouth/u.png");
	IMG(mouth_e, "assets/head/mouth/e.png");
	IMG(mouth_o, "assets/head/mouth/o.png");
	IMG(mouth_c, "assets/head/mouth/closed.png");

	IMG(hair_back, "assets/hair_back/backest.png");
	IMG(hair_back_left, "assets/hair_back/left.png");
	IMG(hair_back_right, "assets/hair_back/right.png");

	IMG(ahoge_left, "assets/hair/ahoge_left.png");
	IMG(ahoge_right, "assets/hair/ahoge_right.png");
	IMG(lock_left, "assets/hair/lock_left.png");
	IMG(lock_right, "assets/hair/lock_right.png");
	IMG(swirl_left, "assets/hair/swirl_left.png");
	IMG(swirl_right, "assets/hair/swirl_right.png");
	IMG(tail_left, "assets/hair/tail_left.png");
	IMG(tail_right, "assets/hair/tail_right.png");
	IMG(hair_still, "assets/hair/still.png");
	IMG(halo, "assets/halo.png");

	IMG(window_una_open, "assets/window/una_open.png");
	IMG(window_una_closing, "assets/window/una_closing.png");
	IMG(window_una_closed, "assets/window/una_closed.png");
	IMG(window_high, "assets/window/high.png");
	IMG(icons, "assets/window/icons.png");

	float beat = t * (119.0f / 60.0f);
	int trip = beat * 6.0f;
	float fbeat = beat - (int)beat;
	if (fbeat < 0) fbeat += 1;
	static const char *lipsync =
		"uuueeuuucuucoooooocccuuuoouoouuuuoouoooooocccccc"
		"eeeuuiiiioouaaaoouuuceeeuuuaaaiiciiciiiiiicccccc"
		"aaaoouuucaaaoooooocccoocoocooaaaaeeeaaaaaacccccc"
		"oocooiiiioouoocooiiiiaacaaaiicaaaiicooooociiiiii";
	const int lipsync_len = strlen(lipsync);
	char syl = trip < 0 ? 'c' : trip >= lipsync_len ? 'i' : lipsync[trip];
	AnimImage mouth;
	switch (syl)
	{
		case 'a' : mouth = mouth_a; break;
		case 'i' : mouth = mouth_i; break;
		case 'u' : mouth = mouth_u; break;
		case 'e' : mouth = mouth_e; break;
		case 'o' : mouth = mouth_o; break;
		default:
		case 'c' : mouth = mouth_c; break;
	}
	AnimImage eyes;
	if (beat < 3.0f) eyes = eyes_o;
	else if (beat < 3.1f) eyes = eyes_c;
	else if (beat < 3.25f) eyes = eyes_u;
	else if (beat < 3.35f) eyes = eyes_c;
	else if (beat < 8.0f) eyes = eyes_o;
	else if (beat < 8.1f) eyes = eyes_c;
	else if (beat < 16.0f) eyes = eyes_u;
	else if (beat < 16.1f) eyes = eyes_c;
	else if (beat < 22.0f) eyes = eyes_o;
	else if (beat < 22.1f) eyes = eyes_c;
	else if (beat < 22.25f) eyes = eyes_u;
	else if (beat < 22.35f) eyes = eyes_c;
	else if (beat < 23.0f) eyes = eyes_o;
	else if (beat < 23.1f) eyes = eyes_c;
	else if (beat < 23.25f) eyes = eyes_u;
	else if (beat < 23.35f) eyes = eyes_c;
	else if (beat < 31.0f) eyes = eyes_o;
	else if (beat < 31.1f) eyes = eyes_c;
	else eyes = eyes_n;

	AnimImage window_una;
	if (fbeat < 0.1f) window_una = window_una_closing;
	else if (fbeat < 0.25f) window_una = window_una_closed;
	else if (fbeat < 0.35f) window_una = window_una_closing;
	else window_una = window_una_open;

	float bounce = beat - (int)beat;
	if (bounce < 0) bounce += 1.0f;
	bounce = smoothstep(-0.13, 0.13, bounce) * 2.0f - smoothstep(0.13, 0.5, bounce) - 1.0f;
	BoneNode b_bg, b_torso,
		b_wing_left, b_wing_right,
		b_arm_left, b_arm_right,
		b_ribbon_left, b_ribbon_right,
		b_skirt, b_belt_heart, b_neck, b_jaw,
		b_brows, b_eyes, b_mouth,
		b_hair_back, b_hair_back_left, b_hair_back_right,
		b_ahoge_left, b_ahoge_right,
		b_lock_left, b_lock_right,
		b_swirl_left, b_swirl_right,
		b_tail_left, b_tail_right,
		b_hair_still, b_halo,
		b_window_una, b_window_high, b_icons;
	BoneNode *c_torso[] = {
		&b_wing_left, &b_wing_right,
		&b_arm_left, &b_arm_right,
		&b_skirt, &b_neck,
		&b_hair_back, &b_hair_back_left, &b_hair_back_right,
	};
	BoneNode *c_bg[] = { &b_torso, &b_window_una, &b_window_high, &b_icons };
	BoneNode *c_arm_left[] = { &b_ribbon_left };
	BoneNode *c_arm_right[] = { &b_ribbon_right };
	BoneNode *c_skirt[] = { &b_belt_heart };
	BoneNode *c_neck[] = { &b_jaw };
	BoneNode *c_jaw[] = {
		&b_brows, &b_eyes, &b_mouth,
		&b_ahoge_left, &b_ahoge_right,
		&b_lock_left, &b_lock_right,
		&b_swirl_left, &b_swirl_right,
		&b_tail_left, &b_tail_right,
		&b_hair_still, &b_halo,
	};
	bone_set(&b_bg, 0, 1, 0, 0, 0, 0, bg, -200, c_bg);
	bone_set(&b_torso, 0, 1.2, 0, 0, 0, bounce * 0.02f + 0.15f, torso, -65, c_torso);
	leaf_set(&b_wing_left, bounce * -0.3f, 1, -0.2439236f, 0.0182292f, 0, 0, wing_left, -85);
	leaf_set(&b_wing_right, bounce * 0.3f, 1, 0.2439236f, 0.0182292f, 0, 0, wing_right, -85);
	bone_set(&b_arm_left, bounce * -0.08f, 1, -0.146701f, -0.019097f, 0, 0, arm_left, -80, c_arm_left);
	bone_set(&b_arm_right, bounce * 0.08f, 1, 0.146701f, -0.019097f, 0, 0, arm_right, -80, c_arm_right);
	leaf_set(&b_ribbon_left, bounce * 0.08f, 1, -0.3237847f, 0.4756944f, 0, 0, ribbon_left, -75);
	leaf_set(&b_ribbon_right, bounce * -0.08f, 1, 0.3237847f, 0.4756944f, 0, 0, ribbon_right, -75);
	bone_set(&b_skirt, 0, 1, 0, 0, 0, 0, skirt, -70, c_skirt);
	leaf_set(&b_belt_heart, 0, 1, 0, 0, 0, 0, belt_heart, -60);
	bone_set(&b_neck, 0, 1, 0, 0, 0, 0, neck, -55, c_neck);
	bone_set(&b_jaw, 0, 1, 0, 0, 0, bounce * 0.006f, jaw, -50, c_jaw);
	leaf_set(&b_brows, 0, 1, 0, 0, 0, 0, brows, -15);
	leaf_set(&b_eyes, 0, 1, 0, 0, 0, 0, eyes, -15);
	leaf_set(&b_mouth, 0, 1, 0, 0, 0, 0, mouth, -45);
	leaf_set(&b_hair_back, 0, 1, 0, 0, 0, 0, hair_back, -100);
	leaf_set(&b_hair_back_left, 0, 1, 0, 0, 0, 0, hair_back_left, -90);
	leaf_set(&b_hair_back_right, 0, 1, 0, 0, 0, 0, hair_back_right, -90);
	leaf_set(&b_ahoge_left, -bounce * 0.5f, 1, 0, -0.874132f, 0, 0, ahoge_left, -20);
	leaf_set(&b_ahoge_right, bounce * 0.5f, 1, 0, -0.874132f, 0, 0, ahoge_right, -25);
	leaf_set(&b_lock_left, -bounce * 0.08f, 1, -0.263889f, -0.614583f, 0, 0, lock_left, -40);
	leaf_set(&b_lock_right, bounce * 0.08f, 1, 0.263889f, -0.614583f, 0, 0, lock_right, -40);
	leaf_set(&b_swirl_left, -bounce * 0.2f, 1, -0.201389f, -0.606771f, 0, 0, swirl_left, -30);
	leaf_set(&b_swirl_right, bounce * 0.2f, 1, 0.201389f, -0.606771f, 0, 0, swirl_right, -30);
	leaf_set(&b_tail_left, -bounce * 0.04f, 1, -0.210069f, -0.78125f, 0, 0, tail_left, -95);
	leaf_set(&b_tail_right, bounce * 0.04f, 1, 0.210069f, -0.78125f, 0, 0, tail_right, -95);
	leaf_set(&b_hair_still, 0, 1, 0, 0, 0, 0, hair_still, -35);
	leaf_set(&b_halo, -bounce * 0.1f, 1, 0, -0.25f, 0.07f, 0.07f, halo, -10);
	leaf_set(&b_window_una, 0, 1, 0, 0, 0, 0, window_una, -195);
	leaf_set(&b_window_high, 0, 1, 0, 0, 0, 0, window_high, -195);
	leaf_set(&b_icons, 0, 1, 0, 0, 0, 0, icons, -195);
	api->render_all(api, &b_bg);
}

static void outro(AnimAPI *api, float t)
{
	IMG(bg, "assets/bg.png");
	IMG(thanks, "assets/outro_text/thanks.png");
	IMG(forr, "assets/outro_text/for.png");
	IMG(watching, "assets/outro_text/watching.png");
	float beat = t * (119.0f / 60.0f);
	int beati = (beat * 0.25f - floorf(beat * 0.25f)) * 4.0f;
	BoneNode b_bg, b_thanks, b_for, b_watching;
	BoneNode *c_bg[] = { &b_thanks, &b_for, &b_watching };
	float rand_rot[12] = { -0.005f, 0.04f, -0.034f, -0.044f, 0.018f, -0.032f, -0.005f, 0.015f, -0.05f, 0.016f, -0.032f, 0.008f };
	float *rt = rand_rot + beati * 3;
	bone_set(&b_bg, 0, 1, 0, 0, 0, 0, bg, 0, c_bg);
	leaf_set(&b_thanks, rt[0], 1, -0.563477f, 0.036133f, 0, 0, thanks, 0);
	leaf_set(&b_for, rt[1], 1, -0.075195f, 0.058594f, 0, 0, forr, 0);
	leaf_set(&b_watching, rt[2], 1, 0.522461f, 0.046875f, 0, 0, watching, 0);
	api->render_all(api, &b_bg);
}

void anim_draw(AnimAPI *api, float t)
{
	api->target_set_uv_mode(api, FIT_MIN, FIT_MIN);
	float bg[4] = {1,1,1,1};
	api->rgba_fill(api, bg);

	float bar = t * (119.0f / 240.0f);
	if (bar < 2) intro(api, t);
	else if (bar < 9.97f) bridge(api, t - 2.0f * 240.0f / 119.0f);
	else if (bar < 17.97f) chorus(api, t - 9.97f * 240.0f / 119.0f);
	else outro(api, t - 17.97f * 240.0f / 119.0f);
	if (bar > 1.75f && bar < 2.25f)
	{
		float l = (0.25f - fabsf(bar - 2.0f)) * 4.0f;
		float fg[4] = {l,l,l,l};
		api->rgba_fill(api, fg);
	}
}
