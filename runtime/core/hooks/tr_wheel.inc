#include "disc/roster_def.h"
// transformation wheel (SZ): params [[0x08B61178 + 4*slot] + 0x91C]; tw_act: -1 empty, 0..3 transformation i, 10..12 fusion i
// pages: 0 transformations, 1 Metamoru (fusion class 1), 2 Potara (class 2); pieces AGS 298 + RHG 299 (0 ring, 1 empty, 2 locked,
// 3 centre, 4..15 portraits page*4 + position, filled by disc/renders.h)
int input_wheel_on();
int input_wheel_open();
int input_wheel_page();
int input_wheel_dev();
uint32_t input_wheel_code(int pos);
int icons_glyph_of(uint32_t code);
uint32_t psp_reserve(uint32_t n);
void tw_prepare();
volatile int tw_ready = 0, tw_pages = 1, tw_act[3][4], tw_shown = 0, tw_cell[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
volatile uint32_t tw_cell_epoch = 1;
static int tw_tgt[3][4], tw_cost[3][4], tw_ok[3][4];
static const int TW_AGS = 298, TW_RHG = 299;
static const float TW_D = 32.5f, TW_R = 23.75f, TW_BX = 26.25f, TW_BY = 25.0f, TW_KEY = 0.625f;   // position size, distance to the centre, back wheels offset, key scale (rect units)
static uint32_t tw_hand = 0xFFFFFFFFu, tw_hold = 0, tw_name = 0;
static int tw_ns = 0, tw_w[16], tw_h[16];

static uint32_t tw_call(const Ctx& c, uint32_t fn, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, bool out, uint32_t* res) {
    Ctx c2 = c;
    uint32_t sp = (c.r[29] - 0x80u) & ~0xFu, o = sp + 0x40u;
    psp_s32(o, 0xFFFFFFFFu);
    c2.r[29] = sp; c2.r[4] = a0; c2.r[5] = a1; c2.r[6] = a2; c2.r[7] = a3; c2.r[8] = out ? o : 0; c2.r[31] = 0;
    psp_call(c2, fn);
    if (res) *res = psp_l32(o);
    return c2.r[2];
}

static bool tw_want(uint32_t f) {
    if (!input_wheel_open() || !tw_ready || psp_input_context() != 1 || psp_input_clash() || !cl_phase_ok() || !cl_free(f)) return false;
    int codes[12]; uint32_t serial = 0;
    int n = psp_input_prompt(codes, 12, &serial);
    for (int i = 0; i < n && i < 12; i++) if (codes[i] >= 0x10 && codes[i] <= 0x13) return false;
    return true;
}

static void tw_tick(const Ctx& c, uint32_t f) {
    if (!input_wheel_on() || !f) { tw_ready = 0; tw_shown = 0; return; }
    int slot = (int)psp_l32(f + 0x2C);
    uint32_t pr = slot >= 0 && slot < 8 ? psp_l32(0x08B61178u + 4u * (uint32_t)slot) : 0, p = guest_ptr(pr) ? psp_l32(pr + 0x91C) : 0;
    if (!guest_ptr(p)) { tw_ready = 0; tw_shown = 0; return; }
    static uint32_t last_p = 0;
    static int logs = 0;
    int act[3][4], tgt[3][4], cost[3][4];
    for (int g = 0; g < 3; g++) for (int s = 0; s < 4; s++) { act[g][s] = -1; tgt[g][s] = -1; cost[g][s] = 0; }
    for (int i = 0; i < 4; i++) {
        int t = psp_l8(p + 0xA2 + i);
        if (t == 0xFF) continue;
        act[0][i] = i; tgt[0][i] = t; cost[0][i] = psp_l8(p + 0xA6 + i);
    }
    int pages = 1, used[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++) {
        int t = psp_l8(p + 0xBE + i), k = psp_l8(p + 0xBB + i);
        if (t == 0xFF || t == 0) continue;
        int pg = k == 2 ? 2 : 1, s = used[pg]++;
        if (s > 2) continue;
        act[pg][s] = 10 + i; tgt[pg][s] = t; cost[pg][s] = psp_l8(p + 0xB8 + i);
        pages |= 1 << pg;
    }
    bool cells = false;
    for (int g = 0; g < 3; g++) for (int s = 0; s < 4; s++) {
        tw_act[g][s] = act[g][s]; tw_tgt[g][s] = tgt[g][s]; tw_cost[g][s] = cost[g][s];
        int v = tgt[g][s] >= 0 && tgt[g][s] < roster::ALL ? tgt[g][s] : -1;
        if (tw_cell[g * 4 + s] != v) { tw_cell[g * 4 + s] = v; cells = true; }
    }
    if (cells) tw_cell_epoch = tw_cell_epoch + 1;
    tw_prepare();
    bool any = false;
    for (int g = 0; g < 3; g++) for (int s = 0; s < 4; s++) any = any || act[g][s] >= 0;
    tw_pages = pages; tw_ready = any ? 1 : 0;
    if (p != last_p && logs < 40) {
        last_p = p; logs++;
        printf("rueda: luchador en la ranura %d, transformaciones", slot);
        for (int i = 0; i < 4; i++) printf(" [%d: destino %d costo %d]", i, psp_l8(p + 0xA2 + i), psp_l8(p + 0xA6 + i));
        for (int i = 0; i < 3; i++) if (psp_l8(p + 0xBE + i) != 0xFF && psp_l8(p + 0xBE + i)) printf(", fusion %d destino %d costo %d clase %d", i, psp_l8(p + 0xBE + i), psp_l8(p + 0xB8 + i), psp_l8(p + 0xBB + i));
        printf("\n");
        fflush(stdout);
    }
    tw_shown = tw_want(f) ? 1 : 0;
    if (!tw_shown) return;
    static int vlogs = 0, prev_key = -1;
    int key = input_wheel_page();
    for (int g = 0; g < 3; g++) for (int s = 0; s < 4; s++) {
        int a = tw_act[g][s], ok = 0;
        if (a >= 0 && a <= 3) ok = tw_call(c, 0x089834F4u, f, (uint32_t)a, 1, 0, false, nullptr) != 0;
        else if (a >= 10) { uint32_t o = 0; ok = tw_call(c, 0x08983E34u, f, (uint32_t)(a - 10), 1, 0, true, &o) != 0; }
        tw_ok[g][s] = ok;
        key = key * 3 + (a >= 0 ? 1 + ok : 0);
    }
    if (key != prev_key && vlogs < 60) {
        prev_key = key; vlogs++;
        int g = input_wheel_page();
        printf("rueda: pagina %d:", g);
        for (int s = 0; s < 4; s++) printf(" %s", tw_act[g][s] < 0 ? "vacio" : tw_ok[g][s] ? "disponible" : "bloqueado");
        printf("\n");
    }
}

static void tw_load(const Ctx& c, uint32_t pak) {
    tw_hold = 0; tw_hand = 0xFFFFFFFFu; tw_ns = 0;
    if (!input_wheel_on()) return;
    uint32_t a = cp_ags(pak, TW_AGS);
    static int logs = 0;
    if (!a) { if (logs++ < 4) printf("rueda: falta la AGS %d en battle_ui, sin dibujo\n", TW_AGS); return; }
    tw_ns = (int)psp_l32(a + 0xC);
    if (tw_ns > 16) tw_ns = 16;
    for (int i = 0; i < tw_ns; i++) { tw_w[i] = (int)psp_l32(a + 0x20 + 128 * i + 18 * 4); tw_h[i] = (int)psp_l32(a + 0x20 + 128 * i + 19 * 4); }
    tw_hand = hud_guest(c, 0x088404D8u, 0x08B48200u, pak, TW_AGS, TW_RHG);
    tw_hold = hud_guest(c, 0x089BFDF4u, 0x54);
    if (tw_hold) hud_guest(c, 0x089D1EE8u, tw_hold, tw_hand, 0, (uint32_t)(tw_ns - 1));
    if (!tw_name) { tw_name = psp_reserve(16); if (tw_name) { const char* s = "BTL_TR_WHEEL"; for (int i = 0; i < 13; i++) psp_s8(tw_name + i, (uint8_t)s[i]); } }
    if (logs++ < 4) { printf("rueda: piezas AGS %d con %d estados, anillo %dx%d, retrato %dx%d\n", TW_AGS, tw_ns, tw_w[0], tw_h[0], tw_ns > 4 ? tw_w[4] : 0, tw_ns > 4 ? tw_h[4] : 0); fflush(stdout); }
}

static void tw_free(const Ctx& c) {
    if (tw_hold) hud_guest(c, 0x089D248Cu, tw_hold, 3);
    if (tw_hand != 0xFFFFFFFFu) hud_guest(c, 0x08840640u, 0x08B48200u, tw_hand);
    tw_hold = 0; tw_hand = 0xFFFFFFFFu; tw_ns = 0; tw_shown = 0;
}

// one wheel: four positions in a cross around (cx, cy), u = rect units per 100, z = scale, k = colour, a = alpha
static void tw_wheel(const Ctx& c, int g, float cx, float cy, float u, float z, float k, float a) {
    if (tw_ns < 16 || tw_w[0] <= 0) return;
    float D = TW_D * u * z, R = TW_R * u * z, rs = D / tw_w[0];
    static const float OX[4] = {-1, 0, 1, 0}, OY[4] = {0, -1, 0, 1};
    for (int s = 0; s < 4; s++) {
        float x = cx + OX[s] * R, y = cy + OY[s] * R;
        int act = tw_act[g][s];
        bool ok = act >= 0 && tw_ok[g][s];
        if (act < 0) { cp_inst(c, cp_state(tw_hold, 1), x - D * 0.5f, y - D * 0.5f, rs, rs, k, a); continue; }
        float pw = tw_w[4 + g * 4 + s] * rs, ph = tw_h[4 + g * 4 + s] * rs;
        cp_inst(c, cp_state(tw_hold, 4 + g * 4 + s), x - pw * 0.5f, y - ph * 0.5f, rs, rs, ok ? k : k * 0.75f, a);
        cp_inst(c, cp_state(tw_hold, 0), x - D * 0.5f, y - D * 0.5f, rs, rs, k, a);
        if (!ok) cp_inst(c, cp_state(tw_hold, 2), x - D * 0.5f, y - D * 0.5f, rs, rs, k, a * 0.6f);
        int v = tw_cost[g][s];
        if (v > 0 && v <= 9) {
            float ds = D * 0.36f / cp_dh;
            float dx0 = x - D * 0.5f + D * 0.06f, dy0 = y + D * 0.5f - cp_dh * ds - D * 0.02f;
            if (!bh_list_digit(dx0 + cp_dw * ds * 0.5f, dy0 - cp_dh * ds * 0.15f, cp_dh * ds * 1.3f, v, ok ? k : k * 0.6f, a))   // BT3 bars: the BT3 blast digit
                cp_inst(c, cp_digit(v), dx0, dy0, ds, ds, ok ? k : k * 0.6f, a);
        }
    }
    float hs = rs;
    cp_inst(c, cp_state(tw_hold, 3), cx - tw_w[3] * hs * 0.5f, cy - tw_h[3] * hs * 0.5f, hs, hs, k, a);
}

// controls around the centre of the front wheel: left, up, right, down (keyboard click 2 3 1, pad left stick directions)
static void tw_keys(const Ctx& c, float cx, float cy, float u, float a) {
    static uint32_t str = 0;
    if (!str) str = psp_reserve(16);
    int dev = input_wheel_dev();
    if (!str || dev < 0 || a <= 0.05f) return;
    float hub = 20.0f * TW_D * u / 64.0f, s = TW_KEY * u, half = 8.0f * s;
    static const float OX[4] = {-1, 0, 1, 0}, OY[4] = {0, -1, 0, 1};
    uint32_t font = psp_l32(0x08A78EECu);
    uint32_t list = hud_guest(c, 0x08838054u, 0);
    hud_guest(c, 0x08832448u, font);
    int al = (int)(a * 255.0f);
    cl_style((uint32_t)al << 24 | 0xFFFFFFu, (uint32_t)al << 24 | 0x404040u, 0);
    hud_guest(c, 0x088321ECu, font, cl_str(12));
    float ksx = cl_sx, ksy = cl_sy, kax = cl_ax, kay = cl_ay;
    for (int i = 0; i < 4; i++) {
        int gl = icons_glyph_of(input_wheel_code(i));
        if (!gl) break;
        psp_s16(str, (uint16_t)gl); psp_s16(str + 2, 0);
        int x = (int)lroundf(cx + OX[i] * hub * 0.95f - half), y = (int)lroundf(cy + OY[i] * hub * 0.95f - half);
        cl_sx = cl_sy = s; cl_ax = (float)x; cl_ay = (float)y; cl_gu_on = true;
        cl_call(c, 0x08832530u, font, (uint32_t)x, (uint32_t)y, str, 0xFFFFFFFFu, 0xFFFFFFFFu);
        cl_gu_on = false;
    }
    cl_sx = ksx; cl_sy = ksy; cl_ax = kax; cl_ay = kay;
    cl_style(0xFFFFFFFFu, 0xFF404040u, 0);
    hud_guest(c, 0x088321ECu, font, cl_str(12));
    hud_guest(c, 0x08832500u, font);
    hud_guest(c, 0x08838120u, list);
    hud_guest(c, 0x08837F54u, 0x08B4E690u, list, 0);
}

// main wheel in the slot BTL_TR_WHEEL, the other pages behind: upper left = next, lower right = previous; turning between them
static void tw_draw(const Ctx& c, uint32_t ra) {
    if (!tw_hold || tw_ns < 16) return;
    static auto t_last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float dt = std::min(0.1f, std::chrono::duration<float>(now - t_last).count());
    t_last = now;
    static float vis = 0, px[3] = {0, 0, 0}, py[3] = {0, 0, 0}, pz[3] = {1, 1, 1}, pk[3] = {1, 1, 1};
    static bool placed = false;
    vis += (tw_shown ? 1.0f : -1.0f) * dt / 0.1f;
    vis = vis < 0 ? 0 : vis > 1 ? 1 : vis;
    if (vis <= 0) { placed = false; return; }
    float x = 10, y = 70, w = 100;
    uint32_t sl = ra && tw_name ? hud_guest(c, 0x08840DC4u, psp_l32(ra + 0x30), tw_name) : 0;
    if (sl) { x = (float)(int)psp_l32(sl + 0xE4); y = (float)(int)psp_l32(sl + 0xE8); w = (float)(int)psp_l32(sl + 0xEC); if (w <= 0) w = 100; }
    static int sll = 0;
    if (sll < 4) { sll++; printf("rueda: ranura %s %.0f %.0f %.0f\n", sl ? "BTL_TR_WHEEL" : "por defecto", x, y, w); }
    float u = w / 100.0f, cx = x + w * 0.5f, cy = y + w * 0.5f;
    int P[3], n = 0, cur = input_wheel_page(), ci = 0;
    for (int g = 0; g < 3; g++) if (tw_pages & (1 << g)) { if (g == cur) ci = n; P[n++] = g; }
    float tx[3], ty[3], tz[3], tk[3];
    for (int i = 0; i < n; i++) {
        int d = (i - ci + n) % n, g = P[i];
        if (d == 0) { tx[g] = cx; ty[g] = cy; tz[g] = 1.0f; tk[g] = 1.0f; }
        else if (d == 1) { tx[g] = cx - TW_BX * u; ty[g] = cy - TW_BY * u; tz[g] = 0.8f; tk[g] = 0.6f; }
        else { tx[g] = cx + TW_BX * u; ty[g] = cy + TW_BY * u; tz[g] = 0.8f; tk[g] = 0.6f; }
    }
    float m = placed ? 1.0f - expf(-dt * 16.0f) : 1.0f;
    placed = true;
    for (int i = 0; i < n; i++) {
        int g = P[i];
        px[g] += (tx[g] - px[g]) * m; py[g] += (ty[g] - py[g]) * m; pz[g] += (tz[g] - pz[g]) * m; pk[g] += (tk[g] - pk[g]) * m;
    }
    int order[3], no = 0;
    for (int i = 0; i < n; i++) order[no++] = P[i];
    std::sort(order, order + no, [&](int a, int b) { return pz[a] < pz[b]; });
    for (int i = 0; i < no; i++) tw_wheel(c, order[i], px[order[i]], py[order[i]], u, pz[order[i]], pk[order[i]], vis * (pz[order[i]] < 0.99f ? 0.9f : 1.0f));
    if (no) { int g = order[no - 1]; tw_keys(c, px[g], py[g], u * pz[g], vis); }
}

// transformation type 2 (BT3 TransformC, action 0xFC): availability 0x089834F4 refuses it at 0x08983638, the setup 0x08983954 returns 0 at 0x08983A14;
// TransformC and Metamoru both open with event camera 036 (+0x908, played at 0x089AAC48): with Metamoru the transformation always takes 039 (+0x914), none when absent
static uint32_t tf_ent = 0;
static std::chrono::steady_clock::time_point tf_at;
static bool tf_after(uint32_t pc, Ctx& c) {
    if (pc != 0x08983634u && pc != 0x08983A00u && pc != 0x089AAC48u) return false;
    if (!patch_transform_cam2.val) return true;
    if (pc == 0x08983634u) { if (c.r[4] == 2) c.r[4] = 0x7F; return true; }
    if (pc == 0x08983A00u) {
        c.r[6] = 0x7F;
        if (c.r[20] != 2) return true;
        uint32_t sl = psp_l32(c.r[16] + 0x2C);
        tf_ent = sl < 64 ? psp_l32(0x08B61178u + 4u * sl) : 0; tf_at = std::chrono::steady_clock::now();
        static int lg = 0;
        if (lg < 4) { lg++; printf("transformacion: tipo 2 (camara 2, cman) habilitado, accion 0xFC\n"); }
        return true;
    }
    uint32_t e = c.r[10];
    if (!tf_ent || e != tf_ent) return true;
    tf_ent = 0;
    if (std::chrono::steady_clock::now() - tf_at > std::chrono::seconds(15)) return true;
    uint32_t p = psp_l32(e + 0x91C), c4 = psp_l32(e + 0x914);
    bool meta = false;
    if (guest_ptr(p)) for (int i = 0; i < 3; i++) if (psp_l8(p + 0xBB + i) == 1) meta = true;
    static int lg = 0;
    if (meta) { c.r[4] = c4; if (lg < 4) { lg++; printf("transformacion: personaje con Metamoru, camara de entrada 039_camera_event_4%s\n", c4 ? "" : " (no existe, sin camara)"); } }
    return true;
}
