// BT3 stage underwater bubbles (EftBubble_*), reimplemented. Units: BT3 world, Y down, 30 Hz.

constexpr int BUB_MAX = 100;

struct Bub {
    float pos[4] = {}, vel[3] = {};
    float drag = 0, size = 0, rot = 0, alpha = 0, alphaStep = 0, rise = 0, riseGrow = 0, phase = 0, amp = 0, phaseStep = 0;
    int objId = 0, view = 0;
    uint8_t r = 0, g = 0, b = 0, tex = 0, layer = 0;
    bool active = false;
    int prev = -1, next = -1;
};

struct BubChr { int objId = 0; float burst = 0; bool trail = false, bursting = false; };

// pos = BtlCharApi_GetPos, dir = unit GetDir, height = GetHeight, node[id] = world position of model node id (GetNodePos)
struct BubObj { bool valid; float pos[3]; float dir[3]; float height; bool inWater; bool nodesOk; float node[0x40][3]; };

struct BubSt {
    Bub pool[BUB_MAX];
    BubChr chr[2];
    int head = -1, tail = -1, count = 0;
    float camTimer[2] = {};
    bool burstFix = true;   // false: BT3 as compiled (EftBubble_RandDir result lost, stack vector taken as zero)
};

static void bub_reset(BubSt& st) {
    bool fix = st.burstFix;
    st = BubSt{};
    st.burstFix = fix;
}

static inline float bub_height(const BubObj& o) { return o.valid ? o.height : 10.0f; }
static inline void bub_pos(const BubObj& o, float* out) {
    if (o.valid) { out[0] = o.pos[0]; out[1] = o.pos[1]; out[2] = o.pos[2]; }
    else out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
}
static inline void bub_dir(const BubObj& o, float* out) {
    for (int k = 0; k < 3; k++) out[k] = o.valid ? o.dir[k] : 0.0f;
}
static inline void bub_node(const BubObj& o, int id, float* out) {
    if (o.valid && o.nodesOk && id >= 0 && id < 0x40) { out[0] = o.node[id][0]; out[1] = o.node[id][1]; out[2] = o.node[id][2]; out[3] = 1.0f; }
    else bub_pos(o, out);
}

static void bub_add(BubSt& st, const float* pos, const float* vel, float drag, float size, float alpha, float rot, float rise,
                    float riseGrow, float phase, float amp, int r, int g, int b, int objId, int view, int tex, float phaseStep,
                    float life, int layer) {
    int i = -1;
    for (int k = 0; k < BUB_MAX; k++) {
        if (!st.pool[k].active) { st.pool[k] = Bub{}; i = k; break; }
    }
    if (i < 0) return;
    Bub& o = st.pool[i];
    o.prev = st.head < 0 ? -1 : st.tail;
    o.next = -1;
    if (st.head < 0) st.head = i;
    if (st.tail >= 0) st.pool[st.tail].next = i;
    st.tail = i;
    std::memcpy(o.pos, pos, sizeof(o.pos));
    std::memcpy(o.vel, vel, sizeof(o.vel));
    o.drag = drag == 0.0f ? 0.0f : 1.0f / drag;
    o.rot = rot; o.size = size; o.alpha = alpha;
    o.alphaStep = life <= 0.0f ? 0.0f : -alpha / (life * 30.0f);
    o.rise = rise;
    o.riseGrow = riseGrow == 0.0f ? 0.0f : 1.0f / riseGrow;
    o.phase = phase; o.amp = amp; o.phaseStep = phaseStep;
    o.r = (uint8_t)r; o.g = (uint8_t)g; o.b = (uint8_t)b;
    o.objId = objId; o.view = view; o.tex = (uint8_t)tex; o.layer = (uint8_t)layer;
    o.active = true;
    st.count++;
}

static const int BB_IDS_G[20] = {0x03, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x20, 0x21, 0x22, 0x23, 0x30};   // same order as BB_IDS (wrappers60.inc)
static const int BUB_BONES[20][2] = {
    {0x0B, 0x0A}, {0x0A, 0x09}, {0x09, 0x08}, {0x08, -1}, {0x0F, 0x0E}, {0x0E, 0x0D}, {0x0D, 0x0C}, {0x0C, -1},
    {0x12, 0x13}, {0x13, 0x14}, {0x14, 0x15}, {0x15, -1}, {0x20, 0x21}, {0x21, 0x22}, {0x22, 0x23}, {0x23, -1},
    {0x03, 0x10}, {0x10, 0x11}, {0x11, 0x30}, {0x30, -1},
};

static void bub_emit_body(BubSt& st, PtRng& R, const BubObj& o, int objId, int chance, int view) {
    float dir[3], a[4], base[4], b[4], step[3], vel[3];
    bub_dir(o, dir);
    float H = bub_height(o);
    float spacing = H * 0.25f;
    if (spacing < 6.0f) spacing = 6.0f;
    for (int i = 0; i < 20; i++) {
        bub_node(o, BUB_BONES[i][0], a);
        std::memcpy(base, a, sizeof(base));
        int n = 1;
        step[0] = step[1] = step[2] = 0.0f;
        if (BUB_BONES[i][1] >= 0) {
            bub_node(o, BUB_BONES[i][1], b);
            for (int k = 0; k < 3; k++) b[k] -= a[k];
            float len = std::sqrt(wtr_dot(b, b));
            if (spacing < len) {
                n = (int)(len / spacing + 1.0f);
                float q = 1.0f / (float)n;
                for (int k = 0; k < 3; k++) step[k] = b[k] * q;
            }
        }
        for (int j = 0; j < n; j++) {
            if (R.r() % chance != 0) continue;
            float size = R.f() * 0.4f + 0.4f;
            float amp = R.f() * 0.2f + 0.1f;
            float rise = R.f() * -0.05f - 0.05f;
            float life = R.f() * 0.4f + 0.6f;
            size *= H * 0.05f;
            amp *= H * 0.05f;
            rise *= H * 0.05f;
            float vs = R.f() * 0.3f + 0.7f;
            for (int k = 0; k < 3; k++) vel[k] = dir[k] * vs;
            for (int k = 0; k < 3; k++) a[k] = base[k] + (float)j * step[k];
            float phase = R.f() * WTR_PI; // arguments are evaluated right to left: phase before rot
            float rot = R.f() * 360.0f;
            bub_add(st, a, vel, -8.0f, size, 128.0f, rot, rise, 6.0f, phase, amp, 0x80, 0x80, 0x80, objId, view, 5, 0.25f, life, 0);
        }
    }
}

static void bub_emit_burst(BubSt& st, PtRng& R, const BubObj& o, int objId, int count) {
    float pos[4], dir[3], p[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    bub_pos(o, pos);
    bub_dir(o, dir);
    float H = bub_height(o);
    for (int i = 0; i < count; i++) {
        float size = R.f() * 4.5f + 3.0f;
        float rise = R.f() * -0.2f - 0.2f;
        float amp = R.f() * 0.5f + 0.3f;
        float dist = R.f() * 10.0f;
        size *= H * 0.05f;
        rise *= H * 0.05f;
        amp *= H * 0.05f;
        dist *= H * 0.05f;
        float a1 = R.f() * WTR_TWO_PI;
        float vx = wtr_sinf(a1), vy = wtr_cosf(a1);
        float a2 = R.f() * WTR_TWO_PI;
        if (st.burstFix) {
            float s, c;
            wtr_sincos(wtr_wrap(a2), s, c);
            p[0] = vx * c; p[1] = vy; p[2] = -vx * s; p[3] = 0.0f;
        }
        for (int k = 0; k < 4; k++) p[k] = wtr_a(wtr_m(p[k], dist), pos[k]);
        float ds = R.f() * 0.3f + 0.7f;
        for (int k = 0; k < 3; k++) dir[k] *= ds;
        float rot = R.f() * 360.0f;
        float phase = R.f() * WTR_PI;
        int tex = (R.r() & 3) ? 7 : 8;
        bub_add(st, p, dir, -8.0f, size, 112.0f, rot, rise, 6.0f, phase, amp, 0x80, 0x80, 0x80, objId, -1, tex, 0.5f, -1.0f, 0);
    }
}

static void bub_emit_camera(BubSt& st, PtRng& R, int view, const float* cp, const float* rt, const float* dn, const float* fw) {
    float pos[4], zero[3] = {0.0f, 0.0f, 0.0f};
    for (int k = 0; k < 3; k++) pos[k] = fw[k] * 10.0f + cp[k];
    pos[3] = 1.0f;
    pos[1] += 20.0f;
    float x = (float)(R.r() % 2001 - 1000) * 0.015f;
    float y = (float)(R.r() % 2001 - 1000) * 0.015f;
    for (int k = 0; k < 3; k++) pos[k] += rt[k] * x;
    for (int k = 0; k < 3; k++) pos[k] += dn[k] * y;
    int tex;
    float size;
    if (R.r() % 6 != 0) { tex = 5; size = R.f() * 0.15f + 0.15f; }
    else { tex = 6; size = R.f() * 1.5f + 1.5f; }
    float phase = R.f() * WTR_PI;
    bub_add(st, pos, zero, 0.0f, size, 128.0f, 0.0f, -0.3f, 0.0f, phase, 0.1f, 0x80, 0x80, 0x80, -1, view, tex, 0.25f, -1.0f, 0);
}

// caller: BtlFx_UpdateWaterFx, when in-water flag 0x11 turns on (flag 0x24 clear, stage has water); arg = BtlCharApi_GetSpeed(obj)
static void bub_start_burst(BubSt& st, int obj, float arg) {
    if (obj < 0 || obj > 1) return;
    BubChr& c = st.chr[obj];
    if (c.bursting) return;
    float n = arg * 5.0f;
    c.objId = obj;
    c.burst = n < 40.0f ? 40.0f : (n > 60.0f ? 60.0f : n);
    c.bursting = true;
}

static bool bub_step(Bub& b, float water, const float camPos[][3], int views) {
    for (int k = 0; k < 3; k++) b.pos[k] += b.vel[k];
    b.vel[0] += b.vel[0] * b.drag;
    b.pos[1] += b.rise;
    b.vel[1] += b.vel[1] * b.drag;
    b.rise += b.rise * b.riseGrow;
    b.vel[2] += b.vel[2] * b.drag;
    b.pos[0] += wtr_cosf(b.phase) * b.amp;
    b.pos[2] += wtr_sinf(b.phase) * b.amp;
    b.phase += b.phaseStep;
    if (b.phase > WTR_PI) b.phase -= WTR_TWO_PI;
    b.alpha += b.alphaStep;
    if (b.alpha <= 0.0f) { b.alpha = 0.0f; return true; }
    if (b.pos[1] - b.size * 0.5f < water) return true;
    if (b.view != -1 && b.view < views && camPos[b.view][1] < water) return true;
    return false;
}

static void bub_update(BubSt& st, PtRng& R, float level, const BubObj objs[2], const float camPos[][3], const float camRight[][3],
                       const float camDown[][3], const float camFwd[][3], int views, bool paused, bool split) {
    if (paused) return;
    float water = std::isfinite(level) ? level : 5.0f;
    for (int i = 0; i < 2; i++) {
        BubChr& c = st.chr[i];
        int ob = c.objId & 1;
        if (c.trail) bub_emit_body(st, R, objs[ob], c.objId, 20, c.objId);
        if (c.bursting) {
            if (c.burst > 0.0f) { bub_emit_burst(st, R, objs[ob], c.objId, (int)c.burst); c.burst -= 8.0f; }
            else { c.burst = 0.0f; c.bursting = false; }
        }
    }
    if (!split) {
        for (int i = 0; i < 1 && i < views; i++) {
            if (camPos[i][1] < water) continue;
            st.camTimer[i] += (R.r() & 3) ? 0.05f : 0.1f;
            if (st.camTimer[i] > 1.0f) {
                st.camTimer[i] -= 1.0f;
                bub_emit_camera(st, R, i, camPos[i], camRight[i], camDown[i], camFwd[i]);
            }
            const BubChr& c = st.chr[i];
            const BubObj& o = objs[c.objId & 1];
            if (!c.trail && o.valid && o.inWater) bub_emit_body(st, R, o, c.objId, 120, i);
        }
    }
    if (st.count == 0) return;
    int i = st.head;
    while (i >= 0) {
        Bub& b = st.pool[i];
        bool dead = bub_step(b, water, camPos, views);
        int nx = b.next;
        if (dead) {
            b.active = false;
            if (b.prev < 0) st.head = b.next; else st.pool[b.prev].next = b.next;
            if (b.next < 0) st.tail = b.prev; else st.pool[b.next].prev = b.prev;
            b.prev = b.next = -1;
            st.count--;
        }
        i = nx;
    }
}

// tx, ty: tangent of the half field of view (x, y) of the 640x448 frame; 0 skips BT3's on-screen test of the corners
template <class E>
static void bub_draw_one(const WtrView& v, const Bub& b, float tx, float ty, E& emit) {
    float s = 0.0f, c = 1.0f;
    bool rt = b.rot != 0.0f;
    if (rt) wtr_sincos(wtr_wrap(b.rot * WTR_PI / 180.0f), s, c);
    float h = b.size * 0.5f, w = b.pos[3];
    if (w == 0.0f) return;
    const float cx[4] = {-h, h, -h, h}, cy[4] = {-h, -h, h, h};
    const float uu[4] = {0, 1, 0, 1}, vv[4] = {0, 0, 1, 1};
    PtV q[4];
    uint32_t col = wtr_col(b.r, b.g, b.b, (int)(uint8_t)(uint32_t)b.alpha);
    for (int i = 0; i < 4; i++) {
        float x = cx[i], y = cy[i];
        if (rt) { float nx = x * c - y * s, ny = x * s + y * c; x = nx; y = ny; }
        for (int k = 0; k < 3; k++) {
            float p = x * v.vx[k] + y * v.vy[k] + b.pos[k];
            q[i].p[k] = w == 1.0f ? p : p / w; // homogeneous w of the bubble (only burst bubbles leave 1)
        }
        q[i].u = uu[i]; q[i].v = vv[i]; q[i].c = col;
    }
    if (wtr_bad(q, 4, v)) return;
    if (tx > 0.0f && ty > 0.0f) {
        for (int i = 0; i < 4; i++) {
            float d[3] = {q[i].p[0] - v.cam[0], q[i].p[1] - v.cam[1], q[i].p[2] - v.cam[2]};
            float z = wtr_dot(d, v.fw);
            float gx = wtr_dot(d, v.vx) / (z * tx) * 5120.0f, gy = wtr_dot(d, v.vy) / (z * ty) * 3584.0f;
            if (!(gx <= 6143.0f && gx > -6144.0f && gy <= 5375.0f && gy > -5376.0f)) return;
        }
    }
    int zs = 0;
    for (int i = 0; i < 4; i++) zs += wtr_zi(v, q[i].p);
    int z = zs >> 10;
    z = z < 0 ? 0 : (z >= 0x1000 ? 0xFFF : z);
    emit((const PtV*)q, 4, b.tex, 4, b.layer >= 2 ? b.layer - 2 : b.layer, (double)z, -1);
}

template <class E>
static void bub_draw(const BubSt& st, const float cam[3], const float vx[3], const float vy[3], const float fw[3], float level,
                     int view, E emit, float tx = 0.0f, float ty = 0.0f) {
    (void)level;
    WtrView v{cam, vx, vy, fw, false};
    for (int i = st.head; i >= 0; i = st.pool[i].next) {
        const Bub& b = st.pool[i];
        if (b.active && (b.view == view || b.view == -1)) bub_draw_one(v, b, tx, ty, emit);
    }
}
