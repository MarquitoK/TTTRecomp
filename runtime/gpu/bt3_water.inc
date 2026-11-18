// BT3 stage water splashes (EftWater_*), reimplemented. Units: BT3 world, Y down, 30 Hz.

constexpr float WTR_PI = 3.14159265f;
constexpr float WTR_TWO_PI = 6.2831853f;
constexpr float WTR_HALF_PI = 1.57079633f;
constexpr float WTR_FMAX = 3.40282347e+38f;
constexpr int WTR_WAIT = 0x001, WTR_LIVE = 0x002, WTR_FIN = 0x004, WTR_FOUT = 0x008;
constexpr int WTR_END = 0x040, WTR_DEAD = 0x080, WTR_RISE = 0x100;
constexpr int WTR_NSPLASH = 10, WTR_NTRAIL = 15, WTR_NDROP = 60, WTR_NRING = 30, WTR_NSPRAY = 30, WTR_NWAKE = 4;

struct WtrDrop {
    float pos[3] = {}, base[3] = {}, vel[3] = {};
    float size = 0, sizeVel = 0, sizeDamp = 0, rot = 0, rotVel = 0, alpha = 0, alphaStep = 0;
    float gravity = 0, t = 0, dt = 0, delay = 0;
    uint8_t r = 0, g = 0, b = 0, tex = 0, layer = 0;
    int flags = 0;
};

struct WtrRing {
    float pos[3] = {};
    float size = 0, sizeVel = 0, sizeDamp = 0, alpha = 0, alphaStep = 0, alphaMax = 0, rot = 0, fadeTime = 0, delay = 0;
    uint8_t r = 0, g = 0, b = 0, tex = 0, layer = 0;
    int flags = 0;
};

struct WtrSpray {
    float pos[3] = {}, base[3] = {}, vel[3] = {}, dir[3] = {};
    float tilt = 0, tiltVel = 0, size = 0, sizeVel = 0, sizeDamp = 0, progress = 0, progressStep = 0, progressMax = 0;
    float roll = 0, gravity = 0, nearScale = 0, widthScale = 0, t = 0, dt = 0, delay = 0;
    uint8_t r = 0, g = 0, b = 0, tex = 0, layer = 0;
    int flags = 0;
};

struct WtrGroup { std::vector<int> drops, sprays, rings; };
struct WtrSplash { float pos[3] = {}; int state = 0; WtrGroup g; };
struct WtrTrail { int flags = 0; WtrGroup g; };
struct WtrWake { int obj = 0; uint32_t frame = 0; int on = 0; };

// valid: fighter exists; pos: BtlCharApi_GetPos (pos + display offset); dir: BtlCharApi_GetDir (unit travel dir);
// speed: BtlCharApi_GetSpeed (speed along dir, world units per 30 Hz frame); U: GetHeight * 0.05
struct WtrObj { bool valid; float pos[3]; float dir[3]; float speed; float U; };

struct WtrSt {
    WtrSplash splash[WTR_NSPLASH];
    WtrTrail trail[WTR_NTRAIL];
    WtrDrop drop[WTR_NDROP];
    WtrRing ring[WTR_NRING];
    WtrSpray spray[WTR_NSPRAY];
    std::vector<int> splashList, trailList;
    WtrGroup free;
    WtrWake wake[WTR_NWAKE];
    int splashCount = 0, trailCount = 0, trailFading = 0, dropCount = 0, ringCount = 0, sprayCount = 0;
    uint8_t blastWait[2] = {};
    float level = 0; // water level used by the spawners (BtlStage_GetWaterLevel)
};

static inline float wtr_s(float x) { return x > WTR_FMAX ? WTR_FMAX : (x < -WTR_FMAX ? -WTR_FMAX : x); }
static inline float wtr_m(float a, float b) { return wtr_s(a * b); }
static inline float wtr_a(float a, float b) { return wtr_s(a + b); }
static inline float wtr_sqrt(float x) { return x < 0.0f ? WTR_FMAX : std::sqrt(x); }

static inline uint32_t wtr_col(int r, int g, int b, int a) {
    auto cl = [](int v) { return (uint32_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    int aa = a < 0 ? 0 : a * 255 / 128;
    return cl(r) | (cl(g) << 8) | (cl(b) << 16) | (cl(aa) << 24);
}

static inline float wtr_wrap(float a) {
    if (a >= WTR_PI) {
        int n = std::abs((int)((a - WTR_PI) / WTR_TWO_PI));
        a -= (float)n * WTR_TWO_PI + WTR_TWO_PI;
    } else if (a <= -WTR_PI) {
        int n = std::abs((int)((a + WTR_PI) / WTR_TWO_PI));
        a += (float)n * WTR_TWO_PI + WTR_TWO_PI;
    }
    return a;
}

constexpr float WTR_C9 = 2.60188699e-06f, WTR_C7 = -1.98074136e-04f, WTR_C5 = 8.33302550e-03f, WTR_C3 = -0.166666567f;

static inline float wtr_poly(float x) {
    float x2 = x * x;
    float t3 = WTR_C3 * x * x2, t5 = WTR_C5 * x * x2 * x2, t7 = WTR_C7 * x * x2 * x2 * x2;
    float t9 = WTR_C9 * x * x2 * x2 * x2 * x2;
    return (((x + t3) + t5) + t7) + t9;
}

// Mathf_SinFast / CosFast: wrap by repeated 2pi steps, then the polynomial
static inline float wtr_sinf(float a) {
    if (!std::isfinite(a)) return 0.0f;
    while (a < WTR_TWO_PI) a += WTR_TWO_PI;
    while (WTR_PI < a) a -= WTR_TWO_PI;
    return wtr_poly(a);
}
static inline float wtr_cosf(float a) { return wtr_sinf(a + WTR_HALF_PI); }

// Vu0_SinCos used by Mtx_RotateY / RotateZ
static inline void wtr_sincos(float a, float& s, float& c) {
    float u = a < 0.0f ? WTR_HALF_PI + a : WTR_HALF_PI - a;
    c = wtr_poly(u);
    float q = std::sqrt(std::fabs(1.0f - c * c));
    s = a < 0.0f ? -q : q;
}

static inline float wtr_dot(const float* a, const float* b) { return (a[0] * b[0] + a[1] * b[1]) + a[2] * b[2]; }
static inline void wtr_norm(float* o, const float* v) {
    float l = std::sqrt(wtr_dot(v, v));
    if (!(l > 0.0f)) { o[0] = o[1] = o[2] = 0.0f; return; }
    float q = 1.0f / l;
    o[0] = v[0] * q; o[1] = v[1] * q; o[2] = v[2] * q;
}
static inline void wtr_cross(float* o, const float* a, const float* b) {
    float x = a[1] * b[2] - b[1] * a[2], y = a[2] * b[0] - b[2] * a[0], z = a[0] * b[1] - b[0] * a[1];
    o[0] = x; o[1] = y; o[2] = z;
}

template <class T, int N>
static inline int wtr_alloc(T (&pool)[N]) {
    for (int i = 0; i < N; i++) {
        if (pool[i].flags == 0) { pool[i] = T{}; return i; }
    }
    return -1;
}

static void wtr_drop_spawn(WtrSt& st, std::vector<int>& list, const float* pos, float yaw, float pitch, float speed,
                           float rot, float rotVel, float size, float sizeVel, float sizeDamp, float gravity, float life,
                           float delay, int r, int g, int b, int tex, int layer, int flags) {
    int i = wtr_alloc(st.drop);
    if (i < 0) return;
    list.push_back(i);
    WtrDrop& d = st.drop[i];
    std::memcpy(d.base, pos, sizeof(d.base));
    d.rot = rot; d.rotVel = rotVel; d.size = size; d.sizeVel = sizeVel;
    d.sizeDamp = sizeDamp == 0.0f ? 0.0f : 1.0f / sizeDamp;
    d.gravity = wtr_m(gravity, 9.80665f);
    d.dt = 1.0f / 30.0f;
    d.delay = delay * 30.0f;
    d.r = (uint8_t)r; d.g = (uint8_t)g; d.b = (uint8_t)b; d.tex = (uint8_t)tex; d.layer = (uint8_t)layer;
    d.flags = flags;
    float a = wtr_wrap(yaw * WTR_PI / 180.0f);
    float v[3] = {wtr_sinf(a), 0.0f, wtr_cosf(a)};
    a = pitch * WTR_PI / 180.0f;
    wtr_norm(v, v);
    d.vel[0] = wtr_m(wtr_m(v[0], speed), wtr_cosf(a));
    d.vel[1] = wtr_m(-speed, wtr_sinf(a));
    d.vel[2] = wtr_m(wtr_m(v[2], speed), wtr_cosf(a));
    d.alpha = (flags & WTR_RISE) ? 0.0f : 128.0f;
    d.alphaStep = 128.0f / (life * 30.0f);
    st.dropCount++;
}

static void wtr_ring_spawn(WtrSt& st, std::vector<int>& list, const float* pos, float rot, float size, float sizeVel,
                           float sizeDamp, float alphaMax, float time, float delay, int r, int g, int b, int tex,
                           int layer, int flags) {
    int i = wtr_alloc(st.ring);
    if (i < 0) return;
    list.push_back(i);
    WtrRing& o = st.ring[i];
    std::memcpy(o.pos, pos, sizeof(o.pos));
    o.size = size; o.sizeVel = sizeVel;
    o.sizeDamp = sizeDamp == 0.0f ? 0.0f : 1.0f / sizeDamp;
    o.rot = rot;
    o.delay = delay * 30.0f;
    o.r = (uint8_t)r; o.g = (uint8_t)g; o.b = (uint8_t)b; o.tex = (uint8_t)tex; o.layer = (uint8_t)layer;
    o.flags = flags;
    if (flags & WTR_FIN) {
        o.alphaMax = alphaMax; o.alpha = 0.0f; o.fadeTime = time * 0.7f;
        o.alphaStep = alphaMax / (time * 0.3f * 30.0f);
    } else if (flags & WTR_FOUT) {
        o.fadeTime = time; o.alpha = alphaMax; o.alphaMax = 0.0f;
        o.alphaStep = alphaMax / (time * 30.0f);
    }
    st.ringCount++;
}

static void wtr_spray_spawn(WtrSt& st, std::vector<int>& list, const float* pos, float dist, float height, float yaw,
                            float pitch, float roll, float tilt, float tiltEnd, float speed, float size, float sizeVel,
                            float sizeDamp, float gravity, float nearScale, float widthScale, float life, float delay,
                            int r, int g, int b, int tex, int layer, int flags) {
    int i = wtr_alloc(st.spray);
    if (i < 0) return;
    list.push_back(i);
    WtrSpray& p = st.spray[i];
    std::memcpy(p.base, pos, sizeof(p.base));
    p.base[1] = wtr_a(p.base[1], height);
    p.size = size; p.sizeVel = sizeVel; p.tilt = tilt;
    p.tiltVel = (tiltEnd - tilt) / (life * 30.0f);
    p.sizeDamp = sizeDamp != 0.0f ? 1.0f / sizeDamp : 0.0f;
    p.gravity = wtr_m(gravity, 9.80665f);
    p.nearScale = nearScale; p.widthScale = widthScale;
    p.dt = 1.0f / 30.0f;
    p.delay = delay * 30.0f;
    p.roll = roll;
    p.r = (uint8_t)r; p.g = (uint8_t)g; p.b = (uint8_t)b; p.tex = (uint8_t)tex; p.layer = (uint8_t)layer;
    p.flags = flags;
    float a = wtr_wrap(yaw * WTR_PI / 180.0f);
    p.dir[0] = wtr_sinf(a); p.dir[1] = 0.0f; p.dir[2] = wtr_cosf(a);
    a = pitch * WTR_PI / 180.0f;
    wtr_norm(p.dir, p.dir);
    p.vel[0] = wtr_m(wtr_m(p.dir[0], speed), wtr_cosf(a));
    p.vel[1] = wtr_m(-speed, wtr_sinf(a));
    p.vel[2] = wtr_m(wtr_m(p.dir[2], speed), wtr_cosf(a));
    for (int k = 0; k < 3; k++) p.base[k] = wtr_a(p.base[k], wtr_m(p.dir[k], dist));
    if (flags & WTR_RISE) {
        p.progress = 0.0f; p.progressMax = 96.0f;
        p.progressStep = 96.0f / (life * 1.6f * 30.0f);
    }
    st.sprayCount++;
}

static void wtr_add_splash(WtrSt& st, PtRng& rng, const float* pos, int sprayA, int sprayB, int dropA, int dropB,
                           float scale) {
    int si = -1;
    for (int i = 0; i < WTR_NSPLASH; i++) {
        if (st.splash[i].state == 0) { si = i; break; }
    }
    if (si < 0) return;
    WtrSplash& sp = st.splash[si];
    sp = WtrSplash{};
    st.splashList.push_back(si);
    std::memcpy(sp.pos, pos, sizeof(sp.pos));
    sp.state = 2;
    st.splashCount++;
    const float org[3] = {0.0f, 0.0f, 0.0f};
    float height = wtr_m(7.0f, scale);
    float yaw = rng.f() * 360.0f;
    float step = 360.0f / (float)sprayA;
    float spread = rng.f() * 0.2f;
    for (int i = 0; i < sprayA; i++) {
        float sizeVel = rng.f() * 2.4f + 5.6f;
        float life = rng.f() * 0.4f + 0.5f;
        float pitch = rng.f() * 12.5f + 62.5f;
        wtr_spray_spawn(st, sp.g.sprays, org, 0.0f, height, yaw + step * (float)i, pitch, 0.0f, 0.8f, 0.65f,
                        wtr_m(40.0f, scale), 0.0f, wtr_m(sizeVel, scale), -5.0f, wtr_m(6.4f, scale), spread + 1.0f,
                        1.2f, life, 0.0f, 0x8C, 0x8C, 0x8C, 3, 0, WTR_RISE | WTR_LIVE);
    }
    yaw = rng.f() * 360.0f;
    step = 360.0f / (float)sprayB;
    for (int i = 0; i < sprayB; i++) {
        float sizeVel = rng.f() * 4.0f + 6.0f;
        float life = rng.f() * 0.4f + 0.8f;
        float pitch = rng.f() * 12.5f + 72.5f;
        wtr_spray_spawn(st, sp.g.sprays, org, 2.0f, height, yaw + step * (float)i, pitch, 0.0f, 0.9f, 0.75f,
                        wtr_m(40.0f, scale), 0.0f, wtr_m(sizeVel, scale), -5.0f, wtr_m(6.4f, scale), spread + 1.2f,
                        0.7f, life, 0.1f, 0x8C, 0x8C, 0x8C, 3, 0, WTR_RISE | WTR_WAIT);
    }
    yaw = rng.f() * 360.0f;
    step = 360.0f / (float)dropA;
    for (int i = 0; i < dropA; i++) {
        float speed = rng.f() * 40.0f + 110.0f;
        float sizeVel = rng.f() * 4.0f + 8.0f;
        float life = rng.f() * 0.5f + 0.9f;
        float pitch = rng.f() * 10.0f + 72.0f;
        float rot = rng.f() * 360.0f;
        wtr_drop_spawn(st, sp.g.drops, org, yaw + step * (float)i, pitch, wtr_m(speed, scale), rot, 0.0f,
                       wtr_m(2.0f, scale), wtr_m(sizeVel, scale), -5.0f, wtr_m(11.0f, scale), life, 0.0f, 0x80, 0x80,
                       0x80, 4, 0, WTR_RISE | WTR_FIN | WTR_LIVE);
    }
    yaw = rng.f() * 360.0f;
    step = 360.0f / (float)dropB;
    for (int i = 0; i < dropB; i++) {
        float sizeVel = rng.f() * 4.0f + 8.0f;
        float life = rng.f() * 0.6f + 1.4f;
        float pitch = rng.f() * 10.0f + 75.0f;
        float rot = rng.f() * 360.0f;
        wtr_drop_spawn(st, sp.g.drops, org, yaw + step * (float)i, pitch, wtr_m(55.0f, scale), rot, 0.0f,
                       wtr_m(2.0f, scale), wtr_m(sizeVel, scale), -5.0f, wtr_m(3.5f, scale), life, 0.0f, 0x80, 0x80,
                       0x80, 1, 0, WTR_FOUT | WTR_LIVE);
    }
    wtr_ring_spawn(st, sp.g.rings, pos, 0.0f, wtr_m(scale, 5.0f), wtr_m(scale, 12.0f), -4.0f, 96.0f, 1.4f, 0.0f, 0x80,
                   0x80, 0x80, 0, 0, WTR_FOUT | WTR_FIN | WTR_LIVE);
    wtr_ring_spawn(st, sp.g.rings, pos, 0.0f, wtr_m(scale, 8.0f), wtr_m(scale, 6.0f), -24.0f, 112.0f, 1.5f, 0.0f, 0xFF,
                   0xFF, 0xFF, 2, 0, WTR_FOUT | WTR_FIN | WTR_LIVE);
    float size = wtr_m(scale, 10.0f), sizeVel = wtr_m(scale, 4.0f);
    wtr_ring_spawn(st, sp.g.rings, pos, 0.0f, size, sizeVel, -24.0f, 96.0f, 1.2f, 0.9f, 0xFF, 0xFF, 0xFF, 2, 0,
                   WTR_FOUT | WTR_FIN | WTR_WAIT);
    wtr_ring_spawn(st, sp.g.rings, pos, 0.0f, size, sizeVel, -24.0f, 88.0f, 1.2f, 0.5f, 0xFF, 0xFF, 0xFF, 2, 0,
                   WTR_FOUT | WTR_FIN | WTR_WAIT);
}

// kind 0 fighter (arg = vel.y), 1 ki blast (arg = size), 2 technique (arg = dy); uses st.level
static void wtr_splash_for(WtrSt& st, PtRng& rng, int kind, const float pos[3], float arg, float U) {
    float at[3] = {pos[0], st.level + -1.0f, pos[2]};
    float scale = wtr_sqrt(arg) * 0.33f;
    if (kind == 0) wtr_add_splash(st, rng, at, 7, 4, 3, 2, wtr_m(U, scale));
    else if (kind == 1) wtr_add_splash(st, rng, at, 4, 3, 2, 2, wtr_m(scale, 0.8f));
    else if (kind == 2) wtr_add_splash(st, rng, at, 7, 4, 3, 2, wtr_m(wtr_m(U, scale), 1.2f));
}

static void wtr_splash_at(WtrSt& st, PtRng& rng, const float pos[3]) {
    float at[3] = {pos[0], st.level + -1.0f, pos[2]};
    wtr_add_splash(st, rng, at, 7, 4, 3, 2, 3.0f);
}

static void wtr_set_wake(WtrSt& st, int obj, bool on) {
    if (obj < 0 || obj >= WTR_NWAKE) return;
    WtrWake& w = st.wake[obj];
    if (on) {
        if (w.on != 0) return;
        w.obj = obj; w.on = 1; w.frame = 0;
    } else {
        if (w.on == 0) return;
        w.on = 0; w.frame = 0;
    }
}

// shared by the wake (even frames) and the technique trail; sc = U * scale
static void wtr_wake_sprays2(WtrSt& st, PtRng& rng, std::vector<int>& list, const float* pos, float height, float yaw,
                             float sc) {
    for (int i = 0; i < 2; i++) {
        float size = rng.f() * 3.0f + 1.0f;
        float sizeVel = rng.f() * 3.0f + 5.0f;
        float life = rng.f() * 0.2f + 0.3f;
        float pitch = rng.f() * 10.0f + 50.0f;
        float roll = rng.f() * 360.0f;
        wtr_spray_spawn(st, list, pos, 0.0f, height, yaw - 60.0f + (float)i * 120.0f, pitch, roll, 0.8f, 0.6f,
                        wtr_m(40.0f, sc), size, wtr_m(sizeVel, sc), -5.0f, wtr_m(6.4f, sc), 1.0f, 1.5f, life, 0.0f,
                        0x8C, 0x8C, 0x8C, 3, 0, WTR_RISE | WTR_LIVE);
    }
}

static void wtr_wake_drops2(WtrSt& st, PtRng& rng, std::vector<int>& list, const float* pos, float yaw, float sc) {
    float sizeVel = rng.f() * 3.5f + 6.5f;
    float life = rng.f() * 0.2f + 0.4f;
    float pitch = rng.f() * 10.0f + 60.0f;
    float puffYaw = yaw - 30.0f + (float)(rng.r() % 2) * 60.0f;
    float rot = rng.f() * 360.0f;
    wtr_drop_spawn(st, list, pos, puffYaw, pitch, wtr_m(90.0f, sc), rot, 0.0f, wtr_m(2.0f, sc), wtr_m(sizeVel, sc),
                   -5.0f, wtr_m(9.0f, sc), life, 0.0f, 0x80, 0x80, 0x80, 4, 0, WTR_RISE | WTR_FIN | WTR_LIVE);
    sizeVel = rng.f() * 3.5f + 6.5f;
    life = rng.f() * 0.3f + 0.6f;
    pitch = rng.f() * 10.0f + 70.0f;
    rot = rng.f() * 360.0f;
    wtr_drop_spawn(st, list, pos, yaw, pitch, wtr_m(55.0f, sc), rot, 0.0f, wtr_m(2.0f, sc), wtr_m(sizeVel, sc), -5.0f,
                   wtr_m(3.5f, sc), life, 0.0f, 0x80, 0x80, 0x80, 1, 0, WTR_FOUT | WTR_LIVE);
}

static float wtr_scale_of(const float* d) {
    float l = std::sqrt(wtr_dot(d, d));
    float s = wtr_sqrt(l) * 0.26f;
    if (s < 0.9f) s *= 0.65f;
    return s;
}

static void wtr_blast_trail(WtrSt& st, PtRng& rng, int obj, const float pos[3], const float prev[3], float U, float level) {
    (void)obj;
    st.level = level;
    int ti = wtr_alloc(st.trail);
    if (ti < 0) return;
    WtrTrail& tr = st.trail[ti];
    st.trailList.push_back(ti);
    tr.flags = WTR_LIVE;
    st.trailCount++;
    float at[3] = {pos[0], level + -1.0f, pos[2]};
    float delta[3] = {pos[0] - prev[0], pos[1] - prev[1], pos[2] - prev[2]};
    float dir[3];
    wtr_norm(dir, delta);
    delta[1] = 0.0f;
    float scale = wtr_scale_of(delta);
    float yaw = std::atan2(dir[0], dir[2]) * 180.0f / WTR_PI;
    float sc = wtr_m(U, scale);
    float height = wtr_m(7.0f, sc);
    wtr_wake_sprays2(st, rng, tr.g.sprays, at, height, yaw, sc);
    wtr_wake_drops2(st, rng, tr.g.drops, at, yaw, sc);
    wtr_ring_spawn(st, tr.g.rings, at, 0.0f, wtr_m(0.0f, sc), wtr_m(10.0f, sc), -24.0f, 112.0f, 0.6f, 0.1f, 0xFF, 0xFF,
                   0xFF, 2, 0, WTR_FOUT | WTR_FIN | WTR_WAIT);
}

static void wtr_update_wakes(WtrSt& st, PtRng& rng, float level, const WtrObj* objs, int nobj) {
    for (int n = 0; n < WTR_NWAKE; n++) {
        WtrWake& w = st.wake[n];
        if (w.on == 0) continue;
        WtrObj o = {false, {0, 0, 0}, {0, 0, 0}, 0.0f, 0.5f};
        if (w.obj >= 0 && w.obj < nobj && objs[w.obj].valid) o = objs[w.obj];
        float pos[3] = {o.pos[0], level + -1.0f, o.pos[2]};
        float delta[3] = {wtr_m(o.dir[0], o.speed), 0.0f, wtr_m(o.dir[2], o.speed)};
        float scale = wtr_scale_of(delta);
        float yaw = std::atan2(o.dir[0], o.dir[2]) * 180.0f / WTR_PI;
        float sc = wtr_m(o.U, scale);
        float height = wtr_m(7.0f, sc);
        if (!(w.frame & 1)) {
            wtr_wake_sprays2(st, rng, st.free.sprays, pos, height, yaw, sc);
        } else {
            float size = rng.f() * 3.0f + 1.0f;
            float sizeVel = rng.f() * 3.0f + 5.0f;
            float life = rng.f() * 0.2f + 0.3f;
            float pitch = rng.f() * 10.0f + 50.0f;
            float roll = rng.f() * 360.0f; // arguments are evaluated right to left: roll before the yaw pick
            float side = (float)(rng.r() % 2);
            wtr_spray_spawn(st, st.free.sprays, pos, 0.0f, height, yaw - 60.0f + side * 120.0f, pitch, roll, 0.8f, 0.5f,
                            wtr_m(40.0f, sc), size, wtr_m(sizeVel, sc), -5.0f, wtr_m(6.4f, sc), 1.0f, 1.5f, life,
                            0.0f, 0x8C, 0x8C, 0x8C, 3, 0, WTR_RISE | WTR_LIVE);
        }
        if (!(w.frame & 1)) wtr_wake_drops2(st, rng, st.free.drops, pos, yaw, sc);
        if (w.frame % 3 == 0) {
            wtr_ring_spawn(st, st.free.rings, pos, 0.0f, wtr_m(sc, 0.0f), wtr_m(sc, 10.0f), -24.0f, 112.0f, 0.6f, 0.1f,
                           0xFF, 0xFF, 0xFF, 2, 0, WTR_FOUT | WTR_FIN | WTR_WAIT);
        }
        w.frame++;
    }
}

static bool wtr_drop_step(WtrDrop& d) {
    if (d.flags & WTR_WAIT) {
        d.delay -= 1.0f;
        if (d.delay <= 0.0f) { d.delay = 0.0f; d.flags = (d.flags & ~WTR_WAIT) | WTR_LIVE; }
    }
    if (d.flags & WTR_LIVE) {
        d.pos[0] = wtr_a(d.base[0], wtr_m(d.vel[0], d.t));
        d.pos[1] = wtr_a(wtr_a(d.base[1], wtr_m(d.vel[1], d.t)), wtr_m(wtr_m(d.gravity, d.t), d.t));
        d.pos[2] = wtr_a(d.base[2], wtr_m(d.vel[2], d.t));
        d.size = wtr_a(d.size, d.sizeVel);
        d.sizeVel = wtr_a(d.sizeVel, wtr_m(d.sizeVel, d.sizeDamp));
        d.t += d.dt;
        if (!(d.flags & WTR_END)) {
            if (d.flags & WTR_FIN) {
                d.alpha += d.alphaStep;
                if (d.alpha >= 128.0f) { d.alpha = 128.0f; if (d.flags & WTR_RISE) d.flags |= WTR_END; }
            } else if (d.flags & WTR_FOUT) {
                d.alpha -= d.alphaStep;
                if (d.alpha <= 0.0f) { d.alpha = 0.0f; d.flags |= WTR_END; }
            }
        }
    }
    if (d.flags & WTR_END) {
        if (d.flags & WTR_RISE) {
            d.alpha += d.alphaStep;
            if (d.alpha >= 128.0f) { d.alpha = 128.0f; d.flags |= WTR_DEAD; }
        } else if (d.flags & WTR_FOUT) {
            d.alpha -= d.alphaStep;
            if (d.alpha <= 0.0f) { d.alpha = 0.0f; d.flags |= WTR_DEAD; }
        }
    }
    return (d.flags & WTR_DEAD) != 0;
}

static bool wtr_ring_step(WtrRing& o, float level) {
    if (o.flags & WTR_WAIT) {
        o.delay -= 1.0f;
        if (o.delay <= 0.0f) { o.delay = 0.0f; o.flags = (o.flags | WTR_LIVE) & ~WTR_WAIT; }
    }
    if (o.flags & WTR_LIVE) {
        o.pos[1] = level - 4.0f;
        o.size = wtr_a(o.size, o.sizeVel);
        o.sizeVel = wtr_a(o.sizeVel, wtr_m(o.sizeVel, o.sizeDamp));
        if (!(o.flags & WTR_END)) {
            if (o.flags & WTR_FIN) {
                o.alpha += o.alphaStep;
                if (o.alpha >= o.alphaMax) {
                    o.alpha = o.alphaMax;
                    o.flags &= ~WTR_FIN;
                    if (o.flags & WTR_FOUT) { o.alphaMax = 0.0f; o.alphaStep = o.alpha / (o.fadeTime * 30.0f); }
                }
            } else if (o.flags & WTR_FOUT) {
                o.alpha -= o.alphaStep;
                if (o.alpha <= o.alphaMax) { o.alpha = o.alphaMax; o.flags = (o.flags & ~WTR_FOUT) | WTR_END; }
            }
        }
    }
    if (o.flags & WTR_END) {
        o.alpha -= o.alphaStep;
        if (o.alpha <= 0.0f) { o.alpha = 0.0f; o.flags |= WTR_DEAD; }
    }
    return (o.flags & WTR_DEAD) != 0;
}

static bool wtr_spray_step(WtrSpray& p) {
    if (p.flags & WTR_WAIT) {
        p.delay -= 1.0f;
        if (p.delay <= 0.0f) { p.delay = 0.0f; p.flags = (p.flags | WTR_LIVE) & ~WTR_WAIT; }
    }
    if (p.flags & WTR_LIVE) {
        p.pos[0] = wtr_a(p.base[0], wtr_m(p.vel[0], p.t));
        p.pos[1] = wtr_a(wtr_a(p.base[1], wtr_m(p.vel[1], p.t)), wtr_m(wtr_m(p.gravity, p.t), p.t));
        p.pos[2] = wtr_a(p.base[2], wtr_m(p.vel[2], p.t));
        p.size = wtr_a(p.size, p.sizeVel);
        p.sizeVel = wtr_a(p.sizeVel, wtr_m(p.sizeVel, p.sizeDamp));
        p.tilt += p.tiltVel;
        p.t += p.dt;
        if (!(p.flags & WTR_END) && (p.flags & WTR_RISE)) {
            p.progress += p.progressStep;
            if (p.progress >= p.progressMax) { p.progress = p.progressMax; p.flags |= WTR_END; }
        }
    }
    if ((p.flags & WTR_END) && (p.flags & WTR_RISE)) {
        p.progress += p.progressStep;
        if (p.progress >= p.progressMax) { p.progress = p.progressMax; p.flags |= WTR_DEAD; }
    }
    return (p.flags & WTR_DEAD) != 0;
}

static void wtr_upd_drops(WtrSt& st, std::vector<int>& l) {
    for (size_t i = 0; i < l.size();) {
        if (wtr_drop_step(st.drop[l[i]])) { st.drop[l[i]].flags = 0; l.erase(l.begin() + i); st.dropCount--; }
        else i++;
    }
}
static void wtr_upd_rings(WtrSt& st, std::vector<int>& l, float level) {
    if (st.ringCount == 0) return;
    for (size_t i = 0; i < l.size();) {
        if (wtr_ring_step(st.ring[l[i]], level)) { st.ring[l[i]].flags = 0; l.erase(l.begin() + i); st.ringCount--; }
        else i++;
    }
}
static void wtr_upd_sprays(WtrSt& st, std::vector<int>& l) {
    if (st.sprayCount == 0) return;
    for (size_t i = 0; i < l.size();) {
        if (wtr_spray_step(st.spray[l[i]])) { st.spray[l[i]].flags = 0; l.erase(l.begin() + i); st.sprayCount--; }
        else i++;
    }
}
static bool wtr_upd_group(WtrSt& st, WtrGroup& g, float level) {
    wtr_upd_drops(st, g.drops);
    wtr_upd_sprays(st, g.sprays);
    wtr_upd_rings(st, g.rings, level);
    return g.drops.empty() && g.sprays.empty() && g.rings.empty();
}

static void wtr_fade_group(WtrSt& st, WtrGroup& g) {
    for (int i : g.drops) {
        WtrDrop& d = st.drop[i];
        if (d.flags & WTR_END) continue;
        d.alphaStep = (d.flags & WTR_RISE) ? (128.0f - d.alpha) / (0.1f * 30.0f) : d.alpha / (0.1f * 30.0f);
        d.flags |= WTR_END;
    }
    for (int i : g.rings) {
        WtrRing& o = st.ring[i];
        if (o.flags & WTR_END) continue;
        o.flags |= WTR_END;
        o.alphaStep = o.alpha / (0.1f * 30.0f);
    }
    for (int i : g.sprays) {
        WtrSpray& p = st.spray[i];
        if (p.flags & WTR_END) continue;
        p.flags |= WTR_END;
        p.progressStep = (p.progressMax - p.progress) / (0.1f * 30.0f);
    }
}

static void wtr_update(WtrSt& st, PtRng& rng, float level, const WtrObj* objs, int nobj) {
    st.level = level;
    wtr_update_wakes(st, rng, level, objs, nobj);
    if (st.splashCount != 0) {
        for (size_t i = 0; i < st.splashList.size();) {
            WtrSplash& sp = st.splash[st.splashList[i]];
            if (wtr_upd_group(st, sp.g, level)) { sp.state = 0; st.splashList.erase(st.splashList.begin() + i); st.splashCount--; }
            else i++;
        }
    }
    if (st.trailCount != 0) {
        st.trailFading = 0;
        for (size_t i = 0; i < st.trailList.size();) {
            int ti = st.trailList[i];
            WtrTrail& tr = st.trail[ti];
            bool done = wtr_upd_group(st, tr.g, level);
            if (done) { tr.flags = 0; st.trailList.erase(st.trailList.begin() + i); st.trailCount--; }
            else i++;
            // BT3 also fades (and so keeps busy) a trail it has just freed
            if ((uint32_t)(st.trailCount - st.trailFading) >= 6u) {
                if (!(tr.flags & WTR_END)) { wtr_fade_group(st, tr.g); tr.flags |= WTR_END; }
                st.trailFading++;
            }
        }
    }
    wtr_upd_drops(st, st.free.drops);
    wtr_upd_rings(st, st.free.rings, level);
    wtr_upd_sprays(st, st.free.sprays);
}

static double wtr_zscr(const float* p, const float cam[3], const float fw[3]);

struct WtrView { const float *cam, *vx, *vy, *fw; bool under; };

static inline float wtr_depth(const WtrView& v, const float* p) {
    float d[3] = {p[0] - v.cam[0], p[1] - v.cam[1], p[2] - v.cam[2]};
    return wtr_dot(d, v.fw);
}
static inline int wtr_zi(const WtrView& v, const float* p) { return (int)wtr_zscr(p, v.cam, v.fw); }
static inline double wtr_slot(int z, bool under) {
    z += under ? -500 : 500;
    return (double)(z < 0 ? 0 : (z >= 0x1000 ? 0xFFF : z));
}
static inline bool wtr_bad(const PtV* q, int n, const WtrView& v) {
    for (int i = 0; i < n; i++) {
        if (!std::isfinite(q[i].p[0]) || !std::isfinite(q[i].p[1]) || !std::isfinite(q[i].p[2])) return true;
        if (wtr_depth(v, q[i].p) <= 0.3f) return true;
    }
    return false;
}

template <class E>
static void wtr_billboard(const WtrView& v, const float* pos, float size, float rot, int r, int g, int b, int a,
                          int aref, int tex, int layer, E& emit) {
    if (!(std::fabs(size) < 100000.0f)) return; // stands in for BT3's screen box test
    float s = 0.0f, c = 1.0f;
    bool rt = rot != 0.0f;
    if (rt) wtr_sincos(wtr_wrap(rot * WTR_PI / 180.0f), s, c);
    float h = size * 0.5f;
    const float cx[4] = {-h, h, -h, h}, cy[4] = {-h, -h, h, h};
    const float uu[4] = {0, 1, 0, 1}, vv[4] = {0, 0, 1, 1};
    PtV q[4];
    uint32_t col = wtr_col(r, g, b, a);
    int zs = 0;
    for (int i = 0; i < 4; i++) {
        float x = cx[i], y = cy[i];
        if (rt) { float nx = x * c - y * s, ny = x * s + y * c; x = nx; y = ny; }
        for (int k = 0; k < 3; k++) q[i].p[k] = x * v.vx[k] + y * v.vy[k] + pos[k];
        q[i].u = uu[i]; q[i].v = vv[i]; q[i].c = col;
    }
    if (wtr_bad(q, 4, v)) return;
    for (int i = 0; i < 4; i++) zs += wtr_zi(v, q[i].p);
    emit((const PtV*)q, 4, tex, 4, layer >= 2 ? layer - 2 : layer, wtr_slot(zs >> 10, v.under), aref);
}

template <class E>
static void wtr_spray_quad(const WtrView& v, const float* pos, const float* rot3, float roll, float size, float nearS,
                           float widthS, int r, int g, int b, int a, int aref, int tex, int layer, E& emit) {
    if (!(std::fabs(size) < 100000.0f)) return; // stands in for BT3's screen box test
    float ax[3], ay[3], az[3], t[3];
    const float* cv = v.fw;
    float d = wtr_dot(cv, rot3), sh;
    float sg = 1.0f;
    if (d < 0.0f) { d = -d; sg = -1.0f; }
    sh = d * 0.5f;
    for (int k = 0; k < 3; k++) t[k] = rot3[k] - sg * cv[k];
    wtr_norm(t, t);
    for (int k = 0; k < 3; k++) az[k] = rot3[k] + t[k] * sh;
    wtr_norm(az, az);
    wtr_cross(ax, cv, az);
    wtr_norm(ax, ax);
    wtr_cross(ay, ax, az);
    wtr_norm(ay, ay);
    float h = size * 0.5f, zn = size * nearS, zf = size * 0.0f;
    h *= widthS;
    float cr[4][3] = {{-h, 0, zn}, {h, 0, zn}, {-h, 0, zf}, {h, 0, zf}};
    if (roll != 0.0f) {
        float mid = (cr[0][2] - cr[2][2]) * 0.5f, s, c;
        wtr_sincos(wtr_wrap(roll * WTR_PI / 180.0f), s, c);
        for (int i = 0; i < 4; i++) {
            float x = cr[i][0], z = cr[i][2] - mid;
            cr[i][0] = x * c + z * s;
            cr[i][2] = -x * s + z * c + mid;
        }
    }
    const float uu[4] = {0, 1, 0, 1}, vv[4] = {0, 0, 1, 1};
    PtV q[4];
    uint32_t col = wtr_col(r, g, b, a);
    for (int i = 0; i < 4; i++) {
        for (int k = 0; k < 3; k++) q[i].p[k] = cr[i][0] * ax[k] + cr[i][1] * ay[k] + cr[i][2] * az[k] + pos[k];
        q[i].u = uu[i]; q[i].v = vv[i]; q[i].c = col;
    }
    if (wtr_bad(q, 4, v)) return;
    int zs = 0;
    for (int i = 0; i < 4; i++) zs += wtr_zi(v, q[i].p);
    emit((const PtV*)q, 4, tex, 4, layer >= 2 ? layer - 2 : layer, wtr_slot(zs >> 10, v.under), aref);
}

// one triangle of the ground quad: near-plane clip (the 0.3 plane of EftGfx_UpdateClipPlanes), then BT3's fan
template <class E>
static void wtr_ground_tri(const WtrView& v, const PtV* in, int tex, int layer, E& emit) {
    PtV poly[4], out[4];
    float dist[3];
    int n = 0, nout = 0;
    for (int i = 0; i < 3; i++) {
        dist[i] = wtr_depth(v, in[i].p) - 0.3f;
        nout += dist[i] < 0.0f;
    }
    if (nout == 3) return;
    if (nout == 0) {
        for (int i = 0; i < 3; i++) poly[i] = in[i];
        n = 3;
    } else {
        for (int i = 0, j = 1; i < 3; i++, j++) {
            if (j >= 3) j = 0;
            const PtV &cu = in[i], &nx = in[j];
            bool oi = dist[i] < 0.0f, oj = dist[j] < 0.0f;
            if (!oi) out[n++] = cu;
            if (oi != oj) {
                float t = dist[i] / (dist[i] - dist[j]);
                PtV w = cu;
                for (int k = 0; k < 3; k++) w.p[k] = nx.p[k] * t + cu.p[k] * (1.0f - t);
                w.u = nx.u * t + cu.u * (1.0f - t);
                w.v = nx.v * t + cu.v * (1.0f - t);
                out[n++] = w;
            }
        }
        for (int i = 0; i < n; i++) poly[i] = out[i];
    }
    int z[4];
    for (int i = 0; i < n; i++) z[i] = wtr_zi(v, poly[i].p);
    for (int i = 2; i < n; i++) {
        PtV t[3] = {poly[0], poly[i - 1], poly[i]};
        int zz = ((z[0] + z[i - 1] + z[i]) / 3) >> 8;
        emit((const PtV*)t, 3, tex, 3, layer >= 2 ? layer - 2 : layer, wtr_slot(zz, v.under), -1);
    }
}

template <class E>
static void wtr_ground_quad(const WtrView& v, const float* pos, float size, float rot, int r, int g, int b, int a,
                            int tex, int layer, E& emit) {
    if (!(size < 100000.0f)) return; // PC port fix
    float s = 0.0f, c = 1.0f;
    if (rot != 0.0f) wtr_sincos(wtr_wrap(rot * WTR_PI / 180.0f), s, c);
    float h = size * 0.5f;
    const float cx[4] = {-h, h, -h, h}, cz[4] = {-h, -h, h, h};
    const float uu[4] = {0, 1, 0, 1}, vv[4] = {0, 0, 1, 1};
    PtV q[4];
    uint32_t col = wtr_col(r, g, b, a);
    for (int i = 0; i < 4; i++) {
        float x = cx[i] * c + cz[i] * s, z = -cx[i] * s + cz[i] * c;
        q[i].p[0] = x + pos[0]; q[i].p[1] = pos[1]; q[i].p[2] = z + pos[2];
        q[i].u = uu[i]; q[i].v = vv[i]; q[i].c = col;
    }
    for (int i = 0; i < 4; i++) {
        if (!std::isfinite(q[i].p[0]) || !std::isfinite(q[i].p[2])) return;
    }
    wtr_ground_tri(v, q, tex, layer, emit);
    wtr_ground_tri(v, q + 1, tex, layer, emit);
}

template <class E>
static void wtr_draw_drops(const WtrSt& st, const WtrView& v, const std::vector<int>& l, const float* org, E& emit) {
    for (int i : l) {
        const WtrDrop& d = st.drop[i];
        if (!(d.flags & WTR_LIVE)) continue;
        float p[3] = {d.pos[0] + org[0], d.pos[1] + org[1], d.pos[2] + org[2]};
        int al = (int)(uint8_t)(uint32_t)d.alpha;
        if (d.flags & WTR_RISE) wtr_billboard(v, p, d.size, d.rot, d.r, d.g, d.b, 0x60, al ? al : -1, d.tex, d.layer, emit);
        else wtr_billboard(v, p, d.size, d.rot, d.r, d.g, d.b, al, -1, d.tex, d.layer, emit);
    }
}

template <class E>
static void wtr_draw_sprays(const WtrSt& st, const WtrView& v, const std::vector<int>& l, const float* org, E& emit) {
    const float up[3] = {0.0f, -1.0f, 0.0f};
    for (int i : l) {
        const WtrSpray& p = st.spray[i];
        if (!(p.flags & WTR_LIVE)) continue;
        float rot3[3], omt = 1.0f - p.tilt;
        for (int k = 0; k < 3; k++) rot3[k] = up[k] * p.tilt + p.dir[k] * omt;
        float q[3] = {p.pos[0] + org[0], p.pos[1] + org[1], p.pos[2] + org[2]};
        wtr_spray_quad(v, q, rot3, p.roll, p.size, p.nearScale, p.widthScale, p.r, p.g, p.b, 0x40,
                       (int)(uint8_t)(uint32_t)p.progress, p.tex, p.layer, emit);
    }
}

template <class E>
static void wtr_draw_rings(const WtrSt& st, const WtrView& v, const std::vector<int>& l, E& emit) {
    for (int i : l) {
        const WtrRing& o = st.ring[i];
        if (!(o.flags & WTR_LIVE)) continue;
        wtr_ground_quad(v, o.pos, o.size, o.rot, o.r, o.g, o.b, (int)(uint8_t)(uint32_t)o.alpha, o.tex, o.layer, emit);
    }
}

template <class E>
static void wtr_draw(const WtrSt& st, const float cam[3], const float vx[3], const float vy[3], const float fw[3], float level,
              E emit) {
    WtrView v{cam, vx, vy, fw, cam[1] >= level};
    const float zero[3] = {0.0f, 0.0f, 0.0f};
    for (int i : st.splashList) {
        const WtrSplash& sp = st.splash[i];
        wtr_draw_drops(st, v, sp.g.drops, sp.pos, emit);
        wtr_draw_sprays(st, v, sp.g.sprays, sp.pos, emit);
        wtr_draw_rings(st, v, sp.g.rings, emit);
    }
    for (int i : st.trailList) {
        const WtrTrail& tr = st.trail[i];
        wtr_draw_drops(st, v, tr.g.drops, zero, emit);
        wtr_draw_sprays(st, v, tr.g.sprays, zero, emit);
        wtr_draw_rings(st, v, tr.g.rings, emit);
    }
    wtr_draw_rings(st, v, st.free.rings, emit);
    wtr_draw_sprays(st, v, st.free.sprays, zero, emit);
    wtr_draw_drops(st, v, st.free.drops, zero, emit);
}

// Enhanced splashes (Water effects = Enhanced): no BT3 sprites; the BT3 rings go to the water shader as ripples and foam
// (out: x, z, radius, strength); droplets and mist are the port's own particles (gpu_gl.h wse_*)
template <class E>
static void wtr_draw_enh(const WtrSt& st, const float cam[3], const float vx[3], const float vy[3], const float fw[3], float level,
                         E emit, float* rip, int& nrip, int maxrip) {
    WtrView v{cam, vx, vy, fw, cam[1] >= level};
    const float zero[3] = {0.0f, 0.0f, 0.0f};
    auto rings = [&](const std::vector<int>& l) {
        for (int i : l) {
            const WtrRing& o = st.ring[i];
            if (!(o.flags & WTR_LIVE) || !(o.size < 100000.0f) || !std::isfinite(o.pos[0]) || nrip >= maxrip) continue;
            float* r = rip + 4 * nrip++;
            r[0] = o.pos[0]; r[1] = o.pos[2]; r[2] = std::max(0.5f, o.size * 0.4f); r[3] = std::min(1.0f, (float)(uint8_t)(uint32_t)o.alpha / 96.0f);
        }
    };
    for (int i : st.splashList) {
        const WtrSplash& sp = st.splash[i];
        rings(sp.g.rings);
    }
    for (int i : st.trailList) {
        const WtrTrail& tr = st.trail[i];
        rings(tr.g.rings);
    }
    rings(st.free.rings);
}
