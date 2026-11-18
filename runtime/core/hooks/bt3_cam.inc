// item 62: BT3 lock-on fighter camera (BT3 ChrCam_CalcLockOn/UpdateLockOnYaw/Update, rates per 1/30 s scaled by real time)
// wrapper on 0x0898F130(slot, eye*, angles*): the game copies combatant +0x460/+0x470; with Graphics > BT3 camera the copy is replaced
// in free fighting; the combatant keeps TTT's values (movement reads +0x474). TTT_BT3_CAM 0 off, 1 on; TTT_CAM_H model height
// sceGuSetMatrix 0x08A642A8: the battle projection takes BT3's vertical field of view (433/224) while the shown camera is a BT3 one
#include <cmath>
int psp_input_cam(int k);

struct Bt3Cam { bool live; float yaw, side, rate, eye[3], rot[3]; std::chrono::steady_clock::time_point t; uint32_t cam; int keys, cut; bool ign; uint32_t pw[4]; float po[4], off[3]; bool kept; };
static Bt3Cam bc_st[8];
static int bc_on = 0, bc_preset[4] = {0, 0, 0, 0};
static float bc_h = 20.0f, bc_m5 = 0.0f;
static const float BC_M5 = 433.0f / 224.0f;

static void bc_read() {
    static int n = 0;
    if (n++ % 30) return;
    const char* e = getenv("TTT_BT3_CAM");
    int on = e && *e ? atoi(e) : 0;
    if (on != bc_on) { bc_on = on; printf("cam: camara BT3 %s\n", on ? "activa" : "apagada"); }
    e = getenv("TTT_CAM_H");
    if (e && *e) bc_h = std::max(5.0f, std::min(80.0f, (float)atof(e)));
}

static float bc_ease(float x) { float v = 2.0f * x - 1.0f; return 0.5f + 0.5f * (2.0f * v - v * fabsf(v)); }
static float bc_g(float v) { return 2.0f * v - v * fabsf(v); }
static float bc_rk(float r30, float dt) { return 1.0f - powf(1.0f - std::min(r30, 1.0f), dt * 30.0f); }

static uint32_t bc_entry(uint32_t slot) {
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!t) return 0;
    uint32_t base = psp_l32(t + 4);
    int cnt = (int)psp_l32(t);
    for (int i = 0; i < cnt && i < 8; i++) { uint32_t e = base + 0x1A90u * i; if (psp_l32(e + 0xC) && psp_l32(e + 0x2C) == slot) return e; }
    return 0;
}

static void bc_proj(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t m = c.r[5];
    if (c.r[4] != 0 || !m) { orig(c); return; }
    float m0 = lf(m), m5 = lf(m + 20), m11 = lf(m + 44);
    if (!(m11 < -0.5f && m5 > 0.5f && m5 < 8.0f)) { orig(c); return; }
    uint32_t cur = psp_l32(0x08A7E25Cu);
    bool ours = false;
    for (auto& s : bc_st) if (s.live && s.cam == cur) ours = true;
    if (fabsf(m5 - BC_M5) < 0.02f) { orig(c); return; }
    bc_m5 = m5;
    uint32_t mg = psp_l32(0x08A7E260u);
    int kind = ours ? 0 : mg && cur >= mg + 0x720u && cur < mg + 0x720u + 4 * 0x290u && (cur - mg - 0x720u) % 0x290u == 0 ? 1 : 2;
    static int lkind = -1, vlogs = 0;
    if (kind != lkind && vlogs < 40 && bc_on) {
        vlogs++; lkind = kind;
        uint32_t e0 = bc_entry(0);
        static const char* KN[3] = {"BT3", "del juego (luchador)", "del juego (cman)"};
        printf("cam: vista %s (accion 0x%X)\n", KN[kind], e0 ? psp_l32(e0 + 0x984) : 0);
    }
    if (!ours || !bc_on) { orig(c); return; }
    float k = BC_M5 / m5;
    ef(m, m0 * k); ef(m + 20, m5 * k);
    orig(c);
    ef(m, m0); ef(m + 20, m5);
    static int logs = 0;
    if (logs < 1) { logs++; printf("cam: campo de vision BT3 (m5 %.3f -> %.3f)\n", m5, m5 * k); }
}

static void bc_mats(Bt3Cam& s, float f) {
    static const uint32_t OFF[4] = {0x80, 0x94, 0xC0, 0xD4};
    static int logs = 0;
    if (logs < 1 && f != 1.0f) {
        logs++;
        printf("cam: matrices +80 (%.3f %.3f %.3f %.3f) +C0 (%.3f %.3f %.3f %.3f)\n", lf(s.cam + 0x80), lf(s.cam + 0x94), lf(s.cam + 0xA8), lf(s.cam + 0xAC),
               lf(s.cam + 0xC0), lf(s.cam + 0xD4), lf(s.cam + 0xE8), lf(s.cam + 0xEC));
    }
    for (int i = 0; i < 4; i++) {
        uint32_t a = s.cam + OFF[i], u = psp_l32(a);
        if (u != s.pw[i]) { FReg r; r.u = u; s.po[i] = r.f; }
    }
    bool c0 = s.po[1] != 0.0f && s.po[3] != 0.0f && fabsf((s.po[2] / s.po[3]) / (s.po[0] / s.po[1]) - 1.0f) < 0.1f;
    for (int i = 0; i < 4; i++) {
        uint32_t a = s.cam + OFF[i];
        ef(a, s.po[i] * (i < 2 || c0 ? f : 1.0f));
        s.pw[i] = psp_l32(a);
    }
}

// 0x0898F22C(slot): special camera priority (1 with marks +0xECE/+0x1070); 0 while BT3 ignores the cut, so the loop keeps the shown view
static void bc_prio(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t slot = c.r[4];
    orig(c);
    if (bc_on && slot < 8 && bc_st[slot].ign) c.r[2] = 0;
}

// fighter body position (node [entity +0xDA0 +0x18] +0x40, the point the target ring follows): includes the motion of the attack
// animations that the logical position +0x950 leaves out; its height goes back to the feet with the idle offset
static float bc_yoff[8];
static bool bc_yset[8];
static void bc_body(uint32_t cb, uint32_t e, float* out) {
    uint32_t sl = psp_l32(cb + 0x2Cu) & 7, n = psp_l32(e + 0xDB8u);
    float p[3] = {lf(e + 0x950), lf(e + 0x954), lf(e + 0x958)};
    if (n < 0x08800000u || n >= 0x10000000u) { for (int i = 0; i < 3; i++) out[i] = p[i]; return; }
    float q[3] = {lf(n + 0x40), lf(n + 0x44), lf(n + 0x48)};
    if (!bc_yset[sl] || psp_l32(cb + 0x984) == 0xC) { bc_yoff[sl] = p[1] - q[1]; bc_yset[sl] = true; }
    out[0] = q[0]; out[1] = q[1] + bc_yoff[sl]; out[2] = q[2];
    float gx = q[0] - p[0], gz = q[2] - p[2];
    static int logs = 0, gap = 0;
    if (gx * gx + gz * gz > 25.0f && logs < 20 && gap-- <= 0) {
        logs++; gap = 10;
        printf("cam: cuerpo separado %.1f de la posicion (luchador %u, accion 0x%X)\n", sqrtf(gx * gx + gz * gz), sl, psp_l32(cb + 0x984));
    }
}

static void bc_wrap(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t slot = c.r[4], pe = c.r[5], pa = c.r[6];
    orig(c);
    bc_read();
    if (slot >= 8 || !pe || !pa) return;
    Bt3Cam& s = bc_st[slot];
    auto now = std::chrono::steady_clock::now();
    float dt = s.live ? std::chrono::duration<float>(now - s.t).count() : 0.0f;
    dt = std::max(0.0f, std::min(dt, 0.1f));
    s.t = now;
    s.cam = pe - 0x260u;
    uint32_t me = bc_entry(slot);
    uint32_t t = psp_l32(0x08A7DBC8u), base = t ? psp_l32(t + 4) : 0;
    int cnt = t ? (int)psp_l32(t) : 0;
    int ti = me ? (int)hud_guest(c, 0x0894075Cu, me) : -1;
    uint32_t op = ti >= 0 && ti < cnt && ti < 8 ? base + 0x1A90u * ti : 0;
    uint32_t ph = psp_l32(0x08A7D880u);
    ph = ph ? psp_l32(ph) : 0;
    uint32_t a = me ? entity_of(me) : 0, b = op ? entity_of(op) : 0;
    uint32_t act = me ? psp_l32(me + 0x984) : 0;
    int cut = me ? (psp_l8(me + 0xECE) ? 1 : 0) | (psp_l8(me + 0x1070) ? 2 : 0) : 0;
    // the game's camera for skills, supers and ultimates (0x10C..0x14A, with or without a cut) and cuts after the KO; combo cuts stay BT3
    bool keep = (act >= 0x10Cu && act <= 0x14Au) || (cut && ph >= 5);
    static int clog = 0;
    if (bc_on && (cut != s.cut || keep != s.kept) && clog < 40 && ph >= 3) {
        clog++;
        printf("cam: luchador %u %s (marcas %d, accion 0x%X)\n", slot, keep ? "camara del juego" : cut ? "corte ignorado, sigue BT3" : "BT3", cut, act);
    }
    s.kept = keep;
    s.cut = cut;
    bool use = bc_on && a && b && ph >= 3 && ph <= 7 && !psp_input_clash() && !keep;
    s.ign = use && cut;
    if (!use) {
        if (s.cam) bc_mats(s, 1.0f);
        s.live = false;
        return;
    }
    const float PI = 3.14159265f, h = bc_h, lim = std::max(atanf(5.0f), PI / 4);
    int pk = lmp_player(slot >> 1, slot & 1), keys = pk >= 0 ? psp_input_cam(pk) : 0;
    int pr = pk >= 0 ? bc_preset[pk] : 0;
    if (pk >= 0 && (keys & 4) && !(s.keys & 4)) { pr = bc_preset[pk] = (pr + 1) % 3; printf("cam: distancia %d\n", pr + 1); }
    float pos[3], ob[3], to[3];
    bc_body(me, a, pos); bc_body(op, b, ob);
    for (int i = 0; i < 3; i++) to[i] = ob[i] - pos[i];
    float len = sqrtf(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]), hz = sqrtf(to[0] * to[0] + to[2] * to[2]);
    float dy = len > 0.01f ? to[1] / len : 0.0f;
    float oy = hz > 0.01f ? atan2f(to[0], to[2]) : s.yaw;
    bool snap = !s.live;
    if (snap) { s.yaw = oy; s.side = s.side > 0.0f ? lim : -lim; s.live = true; }
    float step = wrap_angle(oy - s.yaw) * bc_rk(0.25f, dt);
    s.yaw = wrap_angle(s.yaw + step);
    float hs = step; while (hs > PI / 2) hs -= PI; while (hs < -PI / 2) hs += PI;
    if (keys & 1) s.side = -lim;
    else if (keys & 2) s.side = lim;
    else s.side = std::max(-lim, std::min(lim, s.side + hs));
    s.keys = keys;
    // CalcLockOn
    static const float PRE[3][3] = {{1.2f, 2.4f, -0.05f}, {1.5f, 2.8f, -0.06f}, {2.0f, 3.3f, -0.10f}};
    float T[3] = {0, -0.7f * h, 0}, d = std::max(0.5f * h + 1.0f, 10.0f);
    float O[3] = {0, -PRE[pr][0] * d - T[1], -PRE[pr][1] * d}, bp = PRE[pr][2] - 0.001f * h;
    float u = 0.8f + 0.2f * bc_ease(std::max(0.0f, std::min(1.0f, len / 100.0f)));
    for (float& v : O) v *= u;
    T[1] *= 1.0f - (1.0f - u) * 0.5f;
    const float k = 1.0f;
    const float mp = 0.35f * PI;
    float ea = std::max(-1.0f, std::min(1.0f, -asinf(std::max(-1.0f, std::min(1.0f, dy))) / mp));
    float eb = bc_ease(std::max(ea, 0.0f));
    float pitch = mp * bc_g(ea) * 0.4f, cp = cosf(pitch), sp = sinf(pitch);
    float y2 = O[1] * cp - O[2] * sp, z2 = O[1] * sp + O[2] * cp;
    O[1] = y2 * (1.0f - 0.3f * eb); O[2] = z2 * (1.0f - 0.3f * eb); O[0] *= 1.0f - 0.3f * eb;
    float rx = bp + pitch - bp * eb;
    float fov = 2.0f * atanf(256.0f / 433.0f), half = fov * 0.7f, tn = tanf(half);
    float oxz = sqrtf(O[0] * O[0] + O[2] * O[2]);
    float spread = hz > 0.01f ? atanf(tn * oxz / hz) : PI / 2;
    float sn = s.side / lim, turn = half + spread * (lim - fov) / lim;
    float ct = cosf(turn * sn), st = sinf(turn * sn);
    float x3 = O[0] * ct + O[2] * st, z3 = O[2] * ct - O[0] * st;
    O[0] = x3; O[2] = z3;
    float yofs = 0.5f * (turn + spread * lim / (PI / 2)) * sn;
    float ry = wrap_angle(s.yaw - yofs);
    float cy = cosf(s.yaw), sy = sinf(s.yaw);
    for (int i = 0; i < 3; i++) T[i] += pos[i];
    float eye[3] = {T[0] + (-cy * O[0] + sy * O[2]), T[1] + O[1], T[2] + (sy * O[0] + cy * O[2])};
    // ChrCam_Update: rate toward CalcRate by 1/75 per 1/30 s, then smoothing
    float ol = sqrtf((T[0] - eye[0]) * (T[0] - eye[0]) + (T[1] - eye[1]) * (T[1] - eye[1]) + (T[2] - eye[2]) * (T[2] - eye[2]));
    float want = std::min(1.0f, 0.2f + 0.2f * ol / h), mv = dt * 30.0f / 75.0f;
    s.rate = snap ? want : s.rate + std::max(-mv, std::min(mv, want - s.rate));
    float r = snap ? 1.0f : bc_rk(s.rate, dt);
    float rot[3] = {rx, ry, 0};
    // smoothing relative to the target: the fighter's own motion never leaves the camera behind
    for (int i = 0; i < 3; i++) {
        s.off[i] += (eye[i] - T[i] - s.off[i]) * r;
        s.eye[i] = T[i] + s.off[i];
        s.rot[i] = wrap_angle(s.rot[i] + wrap_angle(rot[i] - s.rot[i]) * r);
    }
    for (int i = 0; i < 3; i++) { ef(pe + 4 * i, s.eye[i]); ef(pa + 4 * i, s.rot[i]); }
    bc_mats(s, bc_m5 > 0.5f ? BC_M5 / bc_m5 : 1.0f);
    static int logs = 0;
    if (logs < 4 && slot == 0) {
        logs++;
        printf("cam: BT3 h %.1f escala %.2f distancia %.1f ojo (%.1f %.1f %.1f) ang (%.3f %.3f) lado %.2f tasa %.2f\n", h, k, len, s.eye[0], s.eye[1], s.eye[2], s.rot[0], s.rot[1], s.side, s.rate);
    }
}
