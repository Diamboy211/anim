#ifndef POLYGON_H
#define POLYGON_H

int clip_polygon(float *p, int stride, int cnt, float px, float py, float d);
void rasterize_polygon(const float *p, int stride, int cnt, void (*scanline)(float *, float *, void *), void *user);

#endif
