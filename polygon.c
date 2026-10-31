#include "polygon.h"
#include <math.h>
#include <stdbool.h>

int clip_polygon(float *p, int stride, int cnt, float px, float py, float d)
{
	if (cnt < 3) return cnt;
	float d1 = p[(cnt - 1) * stride] * px + p[(cnt - 1) * stride + 1] * py + d;
	if (d1 == 0.0f) d1 = -0.0f;
	float d2;
	int i = 0;
	for (; i < cnt * stride; i += stride)
	{
		d2 = p[i] * px + p[i + 1] * py + d;
		if (d2 == 0.0f) d2 = -0.0f;
		if (signbit(d1 * d2)) break;
		d1 = d2;
	}
	if (i == cnt * stride) return signbit(d2) ? 0 : cnt;
	float d3 = d2;
	float d4;
	int j = i + stride;
	for (;; j += stride)
	{
		d4 = p[j] * px + p[j + 1] * py + d;
		if (d4 == 0.0f) d4 = -0.0f;
		if (signbit(d3 * d4)) break;
		d3 = d4;
	}
	if (signbit(d1))
	{
		int base = i - stride;
		int dst1 = i - stride;
		int dst2 = j;
		if (dst1 == -stride)
		{
			base = 0;
			dst1 = j + stride;
		}
		else if (dst1 == stride)
		{
			base = 0;
			dst2 = 0;
		}
		else if (dst1 > stride)
		{
			base -= stride;
			dst2 = base;
		}
		int ip = i == 0 ? (cnt - 1) * stride : i - stride;
		int jp = j - stride;
		for (int k = 0; k < stride; k++)
		{
			float prop1 = (p[ip + k] * d2 - p[i + k] * d1) / (d2 - d1);
			float prop2 = (p[jp + k] * d4 - p[j + k] * d3) / (d4 - d3);
			p[dst1 + k] = prop1;
			p[dst2 + k] = prop2;
		}
		int length = j - i + stride * 2;
		for (int k = base < length ? base : length, dst = 0; k < base + length;)
			p[dst++] = p[k++];
		return length / stride;
	}
	if (j - i == stride)
	{
		for (int k = cnt * stride; k > j; )
		{
			k--;
			p[k + stride] = p[k];
		}
		int ip = i == 0 ? cnt * stride : i - stride;
		for (int k = 0; k < stride; k++)
		{
			float prop1 = (p[ip + k] * d2 - p[i + k] * d1) / (d2 - d1);
			float prop2 = (p[i + k] * d4 - p[j + k] * d3) / (d4 - d3);
			p[i + k] = prop1;
			p[i + stride + k] = prop2;
		}
		return cnt + 1;
	}
	int ip = i == 0 ? (cnt - 1) * stride : i - stride;
	int jp = j - stride;
	for (int k = 0; k < stride; k++)
	{
		float prop1 = (p[ip + k] * d2 - p[i + k] * d1) / (d2 - d1);
		float prop2 = (p[jp + k] * d4 - p[j + k] * d3) / (d4 - d3);
		p[i + k] = prop1;
		p[i + stride + k] = prop2;
	}
	for (int k = j, l = i + stride * 2; k < cnt * stride;) p[l++] = p[k++];
	return cnt + 2 - (j - i) / stride;
}

#include <stdio.h>

void rasterize_polygon(const float *p, int stride, int cnt, void (*scanline)(float *, float *, void *), void *user)
{
	if (cnt < 3) return;
	int top_i = 0, bottom_i = 0;
	float area = 0;
	for (int i = 0, pi = (cnt - 1) * stride; i < cnt * stride; pi = i, i += stride)
	{
		area += p[pi] * p[i + 1] - p[pi + 1] * p[i];
		if (p[top_i + 1] > p[i + 1]) top_i = i;
		if (p[bottom_i + 1] < p[i + 1]) bottom_i = i;
	}
	if (area == 0) return;
	float y = ceilf(p[top_i + 1]);
	float stop = ceilf(p[bottom_i + 1]);
	if (y == stop) return;
	int left = top_i, right = top_i;
	int pleft = -1, pright = -1;
	float dleft[stride], dright[stride];
	for (; y < stop; y++)
	{
		bool recalc_left = false, recalc_right = false;
		while (p[left + 1] <= y)
		{
			pleft = left;
			if (area > 0)
				left = left == 0 ? (cnt - 1) * stride : left - stride;
			else left = left == (cnt - 1) * stride ? 0 : left + stride;
			recalc_left = true;
		}
		while (p[right + 1] <= y)
		{
			pright = right;
			if (area > 0)
				right = right == (cnt - 1) * stride ? 0 : right + stride;
			else right = right == 0 ? (cnt - 1) * stride : right - stride;
			recalc_right = true;
		}
		if (recalc_left)
			for (int k = 0; k < stride; k++)
				dleft[k] = (p[left + k] - p[pleft + k]) / (p[left + 1] - p[pleft + 1]);
		if (recalc_right)
			for (int k = 0; k < stride; k++)
				dright[k] = (p[right + k] - p[pright + k]) / (p[right + 1] - p[pright + 1]);
		float prop_left[stride], prop_right[stride];
		for (int k = 0; k < stride; k++)
		{
			prop_left[k] = p[pleft + k] + dleft[k] * (y - p[pleft + 1]);
			prop_right[k] = p[pright + k] + dright[k] * (y - p[pright + 1]);
		}
		scanline(prop_left, prop_right, user);
	}
}
