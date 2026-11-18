#pragma once
// framing of the generated renders (item 33), shared by the GPU capture (gpu/gpu.cpp) and the crops (disc/renders.h).
// Units: the head to waist distance on screen (the 3D view panel camera makes it alike for every model). rw = that distance
// in world units over Goku's (bones 0x2F to 0x03, 5.5 units): short (kids, Chaos, Saibaiman, Cell Jr) pulls back, since their
// heads are as big as an adult's; tall (Oozaru) raises the box and grows it a little
#include <cmath>
#include <algorithm>
namespace rf {
static const float WSPAN_REF = 5.5f, NECK_EYE = 0.98f / 5.5f, BIG_K = 1.272f, BIG_TOP = 0.36f, MARGIN = 1.06f, BIG_A = 272.0f / 176.0f;
static const float MARGIN_BOTTOM = 0.0f;   // no extra body under the big box (tried in s144: his big renders lost their framing)
// the big box inside a capture: captures made before MARGIN_BOTTOM have the box's own shape, the new ones are taller
static inline void capture_box(float cw, float ch, float* x0, float* y0, float* bw, float* bh) { *bw = cw / MARGIN; *bh = *bw / BIG_A; *x0 = (cw - *bw) * 0.5f; *y0 = *bh * (MARGIN - 1.0f) * 0.5f; (void)ch; }
struct Face { float k, up, dx; };
struct Tweak { float zoom = 1, dy = 0, dx = 0, bdy = 0, bzoom = 1, rup = 0, shead = 0; };   // rup: game room only, PSP pixels up; shead: select 3D layout only, head room above the head bone in head to waist spans (0 = default)   // per character (games/ttt/renders/tweaks.txt): zoom > 1 closer, dy > 0 face lower, dx > 0 face to the right, bdy > 0 lower in the big render, bzoom > 1 big render closer
enum { MID, SMALL, HUD1, HUD2, CLASH };
static const Face FACE[5] = {{0.46f, -0.10f, 0}, {0.46f, -0.109f, 0}, {0.46f, -0.19f, 0.062f}, {0.434f, -0.169f, 0.105f}, {0.46f, -0.10f, 0}};   // his Goku values, a little wider and 0.02 lower
// eye level: Goku's eyes (his head bone) are 0.98 units above his neck bone, 0.178 of his head to waist distance; other
// models put the head bone anywhere between the chin and the eyes (A17, Zarbon, Frieza: 0.2 above the neck), so the eyes
// are taken from the neck instead, scaled by the square root of the model's size (heads grow less than bodies)
static inline float eye_y(float head_y, float neck_y, float span, float rw) { return neck_y >= 0 ? neck_y - NECK_EYE * span / std::sqrt(std::max(0.25f, rw)) : head_y; }
static inline float kf(float rw) { return rw < 1.0f ? std::min(2.2f, std::pow(std::max(rw, 0.25f), -0.85f)) : std::min(1.15f, std::pow(rw, 0.3f)); }
static inline float raise(float rw) { return rw > 1.0f ? std::min(0.35f, (rw - 1.0f) * 0.4f) : 0.0f; }
// big render box from the head on screen and the head to waist distance there
static inline void big(float hx, float hy, float span, float rw, const Tweak& t, float* x0, float* y0, float* W, float* H) {   // hy = eye level
    *H = BIG_K * span * kf(rw) / std::max(0.3f, t.zoom * t.bzoom); *W = *H * BIG_A;
    *x0 = hx - *W * 0.5f - t.dx * 0.5f * *W; *y0 = hy - (BIG_TOP + raise(rw)) * *H - t.bdy * 0.5f * *H;
}
static inline void tweak_box(const Tweak& t, float* x0, float* y0, float* bw, float* bh, int w, int h) {
    float cx = *x0 + *bw * 0.5f, cy = *y0 + *bh * 0.5f;
    *bh /= std::max(0.3f, t.zoom); *bw = *bh * w / h;
    cx -= t.dx * *bw; cy -= t.dy * *bh;
    *x0 = cx - *bw * 0.5f; *y0 = cy - *bh * 0.5f;
}
// face box of output w x h, the Goku values applied to the head to waist distance (fallback)
static inline void face(int k, float hx, float hy, float span, float rw, int w, int h, float* x0, float* y0, float* bw, float* bh) {
    const Face& f = FACE[k];
    *bh = f.k * span * kf(rw); *bw = *bh * w / h;
    float cx = hx + f.dx * *bh, cy = hy - (f.up + raise(rw) * 0.6f) * *bh;
    *x0 = cx - *bw * 0.5f; *y0 = cy - *bh * 0.5f;
}
// the same box measured by the face itself: the head bone (eyes) to the mouth bone. Goku's box is expressed in units of his
// eyes to mouth distance about their midpoint, and every character gets that same box about its own eyes and mouth: kids,
// giants and long necked models come out alike without any rule. g = Goku's head x, y, waist x, y, mouth x, y; p = the
// character's. Needs the mouth bone on both
static inline bool face_by_mouth(int k, const float* g, const float* p, int w, int h, float* x0, float* y0, float* bw, float* bh) {
    if (g[4] < 0 || p[4] < 0) return false;
    float gd = std::hypot(g[4] - g[0], g[5] - g[1]), pd = std::hypot(p[4] - p[0], p[5] - p[1]);
    if (gd < 4.0f || pd < 4.0f) return false;
    float gx0, gy0, gbw, gbh;
    face(k, g[0], g[1], std::max(4.0f, g[3] - g[1]), 1.0f, w, h, &gx0, &gy0, &gbw, &gbh);
    float mgx = (g[0] + g[4]) * 0.5f, mgy = (g[1] + g[5]) * 0.5f;   // Goku's box about his eyes to mouth midpoint, in units of that distance
    float sk = gbh / gd, ox = (gx0 + gbw * 0.5f - mgx) / gd, oy = (gy0 + gbh * 0.5f - mgy) / gd;
    *bh = sk * pd; *bw = *bh * w / h;
    float cx = (p[0] + p[4]) * 0.5f + ox * pd, cy = (p[1] + p[5]) * 0.5f + oy * pd;
    *x0 = cx - *bw * 0.5f; *y0 = cy - *bh * 0.5f;
    return true;
}
}
