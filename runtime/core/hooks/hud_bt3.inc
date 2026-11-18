#include <chrono>
// BT3-style life bars (item 15): partner and opponents get the full P1 bar, each in its own root slot
static const uint32_t HUD_T1P = 0x08A72288u, HUD_T2P = 0x08A723DCu, HUD_TCPU = 0x08A72530u, HUD_TSIZE = 16 * 20;
static bool hud_on = false;
static void bh_battle_start(bool want, int lay, bool wlan);
static bool bh_bar(const Ctx& c, uint32_t o, int idx);
static bool bh_timer(uint32_t tobj);
static bool bh_skip_node(uint32_t node);
static bool bh_senzu(const Ctx& c, uint32_t o, int idx);
static bool bh_on = false, bh_sz = false, bh_views = false;   // bh_views: split screen (bars shared over the whole screen, TTT per-player pieces stay)
static int bh_vpass = -1, bh_lay = 0;   // bh_lay: split layout of the battle (0, 2 or 4 views)   // HUD pass being walked: -1 full screen, >= 0 a player's view
   // bh_sz: the port-drawn HUD is the SZ one (mod sz_hud)
static int sz_senzu[2] = {-1, -1};
static bool sz_load();
static void sz_panel(const Ctx& c, uint32_t f, int side, int slot, float dy);
static const float SZ_BOT = 210;   // screen y offset of the SZ bottom copies: without energy and senzu the portrait mask ends at 61.2, about 1 px above the edge
static bool bh_msg_off();
static void bh_tw_end();
static void bh_names(const Ctx& c);
static void cl_ge(uint32_t cmd, uint32_t arg);
static bool bh_list_digit(float cx, float y, float h, int v, float k, float a);
static bool bh_list_row(float xl, float xr, float y, float h, float k, float a, float* wl);
static float bh_list_ki(float x, float y, float h, int n, float k, float a);
static uint32_t hud_mem = 0, hud_cpu[2] = {0, 0};
static int hud_cpu_n = 0, hud_tries[4];
static uint32_t hud_mgr = 0, hud_union = 0, hud_kids_extra[3];
static bool hud_union_cur = false;
static uint32_t hud_port_h[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};
static uint32_t hud_abil[4] = {0, 0, 0, 0};   // ability icon copies for bars 2P..4P (index = bar)
static int hud_abil_cur = -1;
static uint32_t hud_seen_pak[2], hud_seen_life[2];
static uint32_t hud_fighter_pak(uint32_t o);
static uint8_t hud_orig1p[HUD_TSIZE], hud_orig2p[HUD_TSIZE], hud_origcpu[HUD_TSIZE];
uint32_t psp_reserve(uint32_t n);
uint32_t psp_alloc(uint32_t n);
void psp_free(uint32_t a);
uint32_t psp_alloc_used();
uint32_t psp_reserve_left();
static bool cl_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&));
static void cl_init(const Ctx& c, uint32_t m);
static void cl_teardown(const Ctx& c, uint32_t m);
static void av_plate_loaded(uint32_t pak, uint32_t rhg_code);

static bool hud_wlan() {
    uint32_t no = psp_l32(0x08A78A28u);
    if (!no) return false;
    uint32_t ph = psp_l32(no + 0x2950u);
    return ph >= 11 && ph <= 16;
}
bool net_online_on();
int psp_net_live() { return psp_l32(0x08A78A28u) != 0 || net_online_on() ? 1 : 0; }   // a network session: the settings menu cannot freeze the game

// split screen layout from the local players: 0 none, 2 views (2 players), 4 views (3-4)
int psp_input_local(int k, uint32_t* b, uint8_t* x, uint8_t* y);
static int hud_split_layout() {
    if (!patch_split_screen.val || hud_wlan()) return 0;
    uint32_t md = psp_l32(0x08B48A28u), b; uint8_t x, y;
    if (md >= 11 && md <= 13) return 0;
    int n = psp_input_local(0, &b, &x, &y);
    return n < 2 ? 0 : n == 2 ? 2 : 4;
}
static bool hud_bt3_want() {
    bool on = patch_hud_bt3.val != 0;
    const char* e = getenv("TTT_HUD_BT3");
    if (e && *e) on = atoi(e) != 0;
    return on || hud_wlan() || hud_split_layout();
}
static bool hud_has(uint32_t pak, uint32_t code) {
    uint32_t n = pak ? psp_l32(pak) : 0;
    if (n > 0xFFFFu) n = __builtin_bswap32(n);
    return code <= n;
}
// battle_ui pieces with a layout copy per HUD mode (asset patch hud_bt3): BT3 bars, 2 views, 4 views
static void hud_redirect(Ctx& c) {
    static const uint32_t RA[6] = {0x089DB198u, 0x089D8484u, 0x089E9F9Cu, 0x089EAB30u, 0x089F1ED0u, 0x089D6844u};
    static const uint32_t CODE[6] = {14, 146, 40, 140, 129, 90};
    static const uint32_t BT3[6] = {281, 282, 295, 0, 0, 294};
    static const uint32_t V2[6] = {281, 285, 283, 284, 286, 287};
    static const uint32_t V4[6] = {281, 291, 289, 290, 292, 293};
    int i = 0;
    while (i < 6 && (c.r[31] != RA[i] || c.r[6] != CODE[i])) i++;
    if (i == 6 || !hud_bt3_want()) return;
    int lay = hud_split_layout();
    uint32_t to = lay == 4 ? V4[i] : lay == 2 ? V2[i] : BT3[i];
    if (!to) return;
    if (!hud_has(c.r[5], to)) { static int miss = 0; if (miss++ < 6) printf("hud: falta la entrada %u, el paquete tiene %u\n", to, c.r[5] ? psp_l32(c.r[5]) : 0); return; }
    static int n = 0;
    if (n++ < 12) printf("hud: ags %u -> %u (%s)\n", CODE[i], to, lay ? (lay == 4 ? "4 vistas" : "2 vistas") : "BT3");
    c.r[6] = to;
}

static void hud_name(uint32_t t, int k, uint32_t name) { psp_s32(t + 20 * k + 8, name); }

static void hud_prepare() {
    if (!hud_mem) {
        hud_mem = psp_reserve(1024);
        memcpy(hud_orig1p, psp_p(HUD_T1P), HUD_TSIZE);
        memcpy(hud_orig2p, psp_p(HUD_T2P), HUD_TSIZE);
        memcpy(hud_origcpu, psp_p(HUD_TCPU), HUD_TSIZE);
        strcpy((char*)psp_p(hud_mem + 0x300), "BTL_HP_MAIN_3P");
        strcpy((char*)psp_p(hud_mem + 0x310), "BTL_HP_MAIN_4P");
        for (int s = 0; s < 2; s++) {
            uint32_t t = hud_mem + s * 0x180, name = hud_mem + 0x300 + s * 0x10;
            memcpy(psp_p(t), psp_p(HUD_T1P), HUD_TSIZE);
            psp_s32(t, 0x115); psp_s32(t + 4, 0x46);
            hud_name(t, 0, name); hud_name(t, 15, name);
        }
    }
    if (hud_on) {
        memcpy(psp_p(HUD_T1P), hud_orig1p, HUD_TSIZE);
        memcpy(psp_p(HUD_T2P), psp_p(HUD_T1P), HUD_TSIZE);
        hud_name(HUD_T2P, 0, 0x08A723CCu); hud_name(HUD_T2P, 15, 0x08A723CCu);
    } else {
        memcpy(psp_p(HUD_T1P), hud_orig1p, HUD_TSIZE);
        memcpy(psp_p(HUD_T2P), hud_orig2p, HUD_TSIZE);
        memcpy(psp_p(HUD_TCPU), hud_origcpu, HUD_TSIZE);
    }
}

// the partner command box is only for a CPU partner of P1 (hidden with a human partner or none)
static bool hud_partner_cpu(const Ctx& c) {
    uint32_t t = psp_l32(0x08A7DBC8u);
    int n = t ? (int)psp_l32(t) : 0, me = rumble_local_slot(c), st = 0;
    if (n < 1 || n > 8) return true;
    uint32_t base = psp_l32(t + 4), mine = 0;
    for (int i = 0; i < n; i++) if ((int)psp_l32(base + 0x1A90u * i + 0x2C) == me) mine = base + 0x1A90u * i;
    if (mine) for (int i = 0; i < n; i++) {
        uint32_t e = base + 0x1A90u * i;
        if (e != mine && psp_l32(e + 4) == psp_l32(mine + 4)) st = psp_l8(e + 0x1690) ? 1 : 2;
    }
    static int last = -1;
    if (st != last) { last = st; printf("hud: orden del companero %s\n", st == 1 ? "visible (companero CPU)" : st == 2 ? "oculta (companero humano)" : "oculta (sin companero)"); }
    return st == 1;
}

static int hud_cpu_slot(uint32_t o) { return o == hud_cpu[0] ? 0 : o == hud_cpu[1] ? 1 : -1; }

static uint32_t hud_guest(const Ctx& c, uint32_t fn, uint32_t a0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
    Ctx c2 = c;
    c2.r[4] = a0; c2.r[5] = a1; c2.r[6] = a2; c2.r[7] = a3; c2.r[31] = 0;
    psp_call(c2, fn);
    return c2.r[2];
}

static void hud_virtual(const Ctx& c, uint32_t o, uint32_t off) {
    uint32_t v = psp_l32(o + 0x40) + off;
    hud_guest(c, psp_l32(v + 4), o + (uint32_t)(int32_t)(int16_t)psp_l16(v));
}

static uint32_t hud_view(const Ctx& c, uint32_t o) {
    uint32_t oc = psp_l32(o + 0xC), v = psp_l32(oc + 0x40) + 0x90;
    return hud_guest(c, psp_l32(v + 4), oc + (uint32_t)(int32_t)(int16_t)psp_l16(v));
}

static int hud_index(uint32_t o) {
    if (!hud_mgr || !o) return -1;
    for (int k = 0; k < 4; k++) if (psp_l32(hud_mgr + 0x74 + 4 * k) == o) return k;
    return -1;
}

// every piece of a bar: the slot rect already carries the size (190x48 native) and the mirror; the drawn scale does not
static float hud_sx[4] = {1, 1, 1, 1}, hud_sy[4] = {1, 1, 1, 1};

static void hud_apply(uint32_t hold, float sx, float sy, bool mirror) {
    uint32_t slot = hold ? psp_l32(hold + 8) : 0;
    if (!slot) return;
    int w = (int)psp_l32(slot + 0xEC);
    FReg m0, m1, ax, ay; m0.u = psp_l32(slot + 0x40); m1.u = psp_l32(slot + 0x54);
    ax.f = (mirror ? -sx : sx) * m0.f; ay.f = sy * m1.f;
    psp_s32(hold + 0x20, ax.u); psp_s32(hold + 0x24, ay.u);
    psp_s32(hold + 0x18, mirror && w > 0 ? (uint32_t)w : 0); psp_s32(hold + 0x1C, 0);
    psp_s32(hold + 0x50, 3);
}

// pieces attached to the root (senzu, partner command) and digit objects: the game draws them at scale 1, here at the bar scale
static void hud_scale(uint32_t hold, float sx, float sy) {
    if (!hold || !psp_l32(hold + 8)) return;
    FReg a, b; a.f = sx; b.f = sy;
    psp_s32(hold + 0x20, a.u); psp_s32(hold + 0x24, b.u);
    psp_s32(hold + 0x18, 0); psp_s32(hold + 0x1C, 0);
    psp_s32(hold + 0x50, psp_l32(hold + 0x50) & ~4u);
}

static void hud_digits(uint32_t d, float sx, float sy, int dx) {
    if (!d) return;
    FReg a, b; a.f = sx; b.f = sy;
    for (int k = 0; k < 10; k++) {
        uint32_t in = psp_l32(d + 4 * k);
        if (!in) continue;
        psp_s32(in + 0x20, a.u); psp_s32(in + 0x24, b.u);
        psp_s32(in + 8, (uint32_t)dx);
        psp_s32(in + 0x50, psp_l32(in + 0x50) & ~4u);
    }
}

// round timer: digits are right-aligned in their slots; the group is shifted so it stays centred on x 240 (the infinity's centre)
static void hud_timer(uint32_t d) {
    uint32_t sl = d ? psp_l32(d + 0x28) : 0;
    int cnt = d ? (int)psp_l32(d + 0x2C) : 0;
    if (!sl || cnt < 1 || cnt > 7) return;
    int nd = 1;
    if (psp_l8(d + 0x34)) nd = cnt;
    else {
        int v = (int)psp_l32(d + 0x30), p = 1;
        for (int i = 0; i < 7; i++, p *= 10) if ((v / p) % 10) nd = i + 1;
    }
    if (nd > cnt) nd = cnt;
    uint32_t s0 = psp_l32(sl), sn = psp_l32(sl + 4 * (nd - 1)), s1 = cnt > 1 ? psp_l32(sl + 4) : 0;
    if (!s0 || !sn) return;
    int x0 = (int)psp_l32(s0 + 0xE4), w0 = (int)psp_l32(s0 + 0xEC), xn = (int)psp_l32(sn + 0xE4);
    if (w0 <= 0 && s1) w0 = x0 - (int)psp_l32(s1 + 0xE4);
    int dx = (int)lroundf(240.0f - (xn + x0 + w0) * 0.5f);
    uint32_t md = psp_l32(0x08B48A28u);
    if (md == 3 || md == 12) dx = 0;   // survival: the same class is the defeated counter (BTL_DEFEAT, DEFEAT_1..3), digits stay in their slots
    static int logs = 0, last = -999;
    for (int k = 0; k < 10; k++) { uint32_t in = psp_l32(d + 4 * k); if (in) psp_s32(in + 8, (uint32_t)dx); }
}

// right bars: each piece goes where the mirror of its place in the P1 bar is (same body, same scale); placing them from
// their own truncated rects left the life and ki bars up to 1 px off the background and the lives digit off its circle
static int hud_dl[16], hud_dw[16];
static bool hud_dok[16];
static int hud_rx(uint32_t hold, int off) { uint32_t sl = hold ? psp_l32(hold + 8) : 0; return sl ? (int)psp_l32(sl + off) : 0; }

// union counters: the state 0x089F55E4 binds +0x80 to the local player's fighter every frame and derives the gauge
// (instance +0x2C) and the digit (+0x78 value) from its team leader; the opponent's copy runs it as its bar's fighter
static uint32_t hud_union_slot() {
    uint32_t bar = hud_mgr ? psp_l32(hud_mgr + 0x7C) : 0, fb = bar ? psp_l32(bar + 0x6C) : 0;
    return fb ? psp_l32(fb + 0x2C) : 0xFFFFFFFFu;
}
static void hud_bar_pieces(uint32_t o, int idx) {
    uint32_t h0 = psp_l32(o + 0x70), slot = h0 ? psp_l32(h0 + 8) : 0;
    if (slot) {
        int w = (int)psp_l32(slot + 0xEC), h = (int)psp_l32(slot + 0xF0);
        if (w > 0 && h > 0) { hud_sx[idx] = w / 190.0f; hud_sy[idx] = h / 48.0f; }
    }
    for (int k = 0; k < 16; k++) hud_apply(psp_l32(o + 0x70 + 4 * k), hud_sx[idx], hud_sy[idx], idx >= 2 && k != 13);
    if (!slot) return;
    int bx = (int)psp_l32(slot + 0xE4), bw = (int)psp_l32(slot + 0xEC);
    if (idx == 0) {
        for (int k = 0; k < 16; k++) {
            uint32_t hk = psp_l32(o + 0x70 + 4 * k);
            hud_dok[k] = hk && psp_l32(hk + 8);
            if (hud_dok[k]) { hud_dl[k] = hud_rx(hk, 0xE4) - bx; hud_dw[k] = hud_rx(hk, 0xEC); }
        }
        return;
    }
    if (idx < 2) return;
    static int logs = 0;
    for (int k = 1; k < 16; k++) {
        uint32_t hk = psp_l32(o + 0x70 + 4 * k);
        if (!hk || !psp_l32(hk + 8) || !hud_dok[k]) continue;
        int x = hud_rx(hk, 0xE4), off = bx + bw - hud_dl[k] - (k == 13 ? hud_dw[k] : 0) - x;
        psp_s32(hk + 0x18, (uint32_t)off);
    }
}

static void hud_attach_portrait(const Ctx& c, uint32_t o) { hud_virtual(c, o, 0xF0); }

// Original (Full): the targeted opponent always on the top right bar (bars 2/3 trade places by a y offset on every piece)
static int hud_dy[4] = {0, 0, 0, 0};
static int hud_swap_dy(const Ctx& c, int idx) {
    if (idx < 2 || !hud_on || bh_on || !hud_mgr || hud_split_layout()) return 0;
    uint32_t b2 = psp_l32(hud_mgr + 0x7C), b3 = psp_l32(hud_mgr + 0x80), f2 = b2 ? psp_l32(b2 + 0x6C) : 0, f3 = b3 ? psp_l32(b3 + 0x6C) : 0;
    if (!f2 || !f3 || (int)psp_l32(f2 + 0x1A38) <= 0 || (int)psp_l32(f3 + 0x1A38) <= 0) return 0;
    uint32_t b0 = psp_l32(hud_mgr + 0x74), v = b0 && psp_l32(b0 + 0xC) ? hud_view(c, b0) : 0;
    if (!guest_ptr(v) || psp_l32(f3 + 0x2C) != psp_l32(v + 0x14)) return 0;
    uint32_t s2 = hud_rx(psp_l32(b2 + 0x70), 0xE8), s3 = hud_rx(psp_l32(b3 + 0x70), 0xE8);
    return idx == 2 ? (int)(s3 - s2) : (int)(s2 - s3);
}
// y offset on the pieces attached to the bar's root slot (the body and its twin): child slots follow their parent instance
static void hud_shift(uint32_t o, int dy) {
    uint32_t h0 = psp_l32(o + 0x70), root = h0 ? psp_l32(h0 + 8) : 0;
    if (!root || !dy) return;
    for (int k = 0; k < 16; k++) { uint32_t hk = psp_l32(o + 0x70 + 4 * k); if (hk && psp_l32(hk + 8) == root) psp_s32(hk + 0x1C, (uint32_t)dy); }
}
static void hud_shift_log(int dy) {
    static int last = 0, logs = 0;
    if (dy != last && logs++ < 12) { last = dy; printf("hud: barras de los oponentes %s (dy %d)\n", dy ? "intercambiadas, el objetivo arriba" : "en su sitio", dy); }
}

// mean cost of a HUD part on the game thread (us), logged every 1200 calls; also a perf sub-zone
#ifdef _WIN32
extern "C" __declspec(dllimport) int __stdcall QueryThreadCycleTime(void* h, unsigned long long* c);
static inline uint64_t hud_tcyc() { unsigned long long c = 0; QueryThreadCycleTime((void*)(intptr_t)-2, &c); return c; }
#else
static inline uint64_t hud_tcyc() { return 0; }
#endif
static int hud_own = -1;   // TTT_HUD_OWN=1: own thread cycles of the slowest call (preempted or real work)
struct HudCost { const char* what; int id; double sum, mx, mxown; int n, logs; };
struct HudCostT {
    HudCost& h; std::chrono::steady_clock::time_point t; uint64_t c0, q0 = 0;
    HudCostT(HudCost& x) : h(x) {
        if (hud_own < 0) { const char* e = getenv("TTT_HUD_OWN"); hud_own = e && *e ? atoi(e) : 0; }
        if (hud_own) q0 = hud_tcyc();
        t = std::chrono::steady_clock::now(); c0 = perf_clock();
    }
    ~HudCostT() {
        uint64_t c1 = perf_clock();
        perf_sub_add(h.id, c1 - c0);
        double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t).count();
        if (hud_own && us > h.mx) { uint64_t q = hud_tcyc() - q0, tc = c1 - c0; h.mxown = tc ? us * std::min(1.0, (double)q / (double)tc) : 0; }
        h.sum += us; h.mx = std::max(h.mx, us);
        if (++h.n < 1200) return;
        if (h.logs++ < 8) {
            if (hud_own) printf("hud: costo medio de %s %.1f us, maximo %.1f us (trabajo propio %.1f us) (%d llamadas)\n", h.what, h.sum / h.n, h.mx, h.mxown, h.n);
            else printf("hud: costo medio de %s %.1f us, maximo %.1f us (%d llamadas)\n", h.what, h.sum / h.n, h.mx, h.n);
        }
        h.sum = h.mx = h.mxown = 0; h.n = 0;
    }
};
#define HUD_COST_C2(a, b) a##b
#define HUD_COST_C(a, b) HUD_COST_C2(a, b)
#define HUD_COST(name) static HudCost HUD_COST_C(_hc, __LINE__){name, perf_sub_id(name), 0, 0, 0, 0, 0}; HudCostT HUD_COST_C(_hct, __LINE__)(HUD_COST_C(_hc, __LINE__))

static void hud_colors(uint32_t in) {
    FReg a, b; a.f = 0.2f; b.f = 1.0f;
    psp_s32(in + 0x40, a.u); psp_s32(in + 0x44, a.u); psp_s32(in + 0x48, a.u); psp_s32(in + 0x4C, b.u);
}

// portrait from the fighter's own pak (entries 0x2F/0x30) into the BTL_CHARA element, as the P1 class does
// the fighter's pak is really in memory: sane entry count and an AGS at entry 0x2F (the record's pointer is set before the data arrives)
static bool hud_pak_ready(uint32_t pak) {
    if (!guest_ptr(pak)) return false;
    uint32_t n = __builtin_bswap32(psp_l32(pak));
    if (n < 0x30 || n > 0x1000) return false;
    uint32_t off = __builtin_bswap32(psp_l32(pak + 4 + 4 * 0x2E)), end = __builtin_bswap32(psp_l32(pak + 4 + 4 * 0x2F));
    return off > 4 + 4 * n && end > off && end < 0x2000000u && psp_l32(pak + off) == 0x00534741u;
}
// the fighter pak streams in after its record is set: the portrait waits for Ready/Fight (bars are hidden in the intros)
static bool hud_portrait_time() { uint32_t m = psp_l32(0x08A7D880u); int ph = guest_ptr(m) ? (int)psp_l32(m) : -1; return ph < 0 || ph >= 3; }
static void hud_portrait(const Ctx& c, uint32_t o) {
    if (psp_l32(o + 0x9C) || !hud_portrait_time()) return;
    uint32_t f = psp_l32(o + 0x6C), p = guest_ptr(f) ? psp_l32(0x08B61178u + 4 * (psp_l32(f + 0x2C) & 7)) : 0;
    if (!guest_ptr(p) || !guest_ptr(psp_l32(p + 0x10)) || !hud_pak_ready(psp_l32(psp_l32(p + 0x10)))) return;
    uint32_t h = hud_guest(c, 0x088404D8u, 0x08B48200u, psp_l32(psp_l32(p + 0x10)), 0x2F, 0x30);
    psp_s32(o + 0xDC, h);
    if (hud_cpu_slot(o) >= 0) { hud_port_h[hud_cpu_slot(o)] = h; hud_seen_pak[hud_cpu_slot(o)] = psp_l32(psp_l32(p + 0x10)); }
    uint32_t in = hud_guest(c, 0x089BFDF4u, 0x54);
    if (!in) return;
    hud_guest(c, 0x089D1EE8u, in, h, 1, 0xFFFFFFFFu);
    psp_s32(o + 0x9C, in);
    hud_colors(in);
}

static void hud_unload_portrait(const Ctx& c, int s2) {
    uint32_t o = hud_cpu[s2];
    if (!o) return;
    if (psp_l32(o + 0x9C)) { hud_guest(c, 0x089D248Cu, psp_l32(o + 0x9C), 3); psp_s32(o + 0x9C, 0); }
    if (hud_port_h[s2] != 0xFFFFFFFFu) { hud_guest(c, 0x08840640u, 0x08B48200u, hud_port_h[s2]); hud_port_h[s2] = 0xFFFFFFFFu; }
}


// item 46: survival brings a new character into the same fighter entry; the opponent's portrait and cached values follow it
static uint32_t hud_fighter_pak(uint32_t o) {
    uint32_t f = psp_l32(o + 0x6C), sl = f ? psp_l32(f + 0x2C) : 99, p = sl < 8 ? psp_l32(0x08B61178u + 4 * sl) : 0;
    return p && psp_l32(p + 0x10) ? psp_l32(psp_l32(p + 0x10)) : 0;
}
static void hud_refresh(const Ctx& c, uint32_t o, int idx) {
    int s = hud_cpu_slot(o);
    uint32_t f = psp_l32(o + 0x6C);
    if (s < 0 || !f) return;
    uint32_t pk = hud_fighter_pak(o), life = psp_l32(f + 0x1A34);
    bool revived = hud_seen_life[s] == 0 && life > 0 && psp_l32(f + 0x1A38) > 0;
    bool newpak = pk && hud_seen_pak[s] && pk != hud_seen_pak[s];
    hud_seen_life[s] = life ? life : 0;
    if (pk) hud_seen_pak[s] = pk;
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_HUD_REFRESH"); on = e && *e ? atoi(e) != 0 : 1; }
    if (!on || (!revived && !newpak)) return;
    static int logs = 0;
    if (logs++ < 40) printf("hud: barra %d recarga retrato (%s), pak %08X\n", idx, newpak ? "pak nuevo" : "revive", pk);
    hud_unload_portrait(c, s);
    hud_portrait(c, o);
    if (psp_l32(o + 0x9C)) hud_colors(psp_l32(o + 0x9C));
    hud_tries[idx] = 0;
    hud_virtual(c, o, 0xC8);
}

// item 48: with BT3 bars (no split screen) battle voice subtitles go centred like the intro ones (global 0x08A7B71C), not in the HUD tab
static const uint32_t SUB_G = 0x08A7B71Cu;
static uint32_t sub_id = 0xFFFFFFFFu, sub_name = 0, sub_mem = 0;
static std::chrono::steady_clock::time_point sub_t0;
static int32_t sub_snd = -1;
static std::chrono::steady_clock::time_point sub_end;
static bool sub_want() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_SUB_CENTER"); on = e && *e ? atoi(e) : 1; }
    return on && hud_on && !hud_split_layout() && psp_l32(0x08B48A28u) != 8;
}
static void sub_clear() {
    if (psp_l32(SUB_G) == sub_id) psp_s32(SUB_G, 0xFFFFFFFFu);
    sub_id = 0xFFFFFFFFu;
}
// item 50: a voice stream channel never reports its end in the port (the player stays in state 4 after the last frame), so the
// fighter intros and victory poses wait for their phase timeout (12 s) and the game never clears the centred subtitle. Watchdog:
// each voice start (sound callback 0x0885332C, kind 1, op 1, a1 = channel) gets the entry's real duration; a channel still playing
// past it is stopped with the game's own stop (0x0883A5A4), which raises the end event. TTT_VOICE_END=0 off, TTT_VOICE_END_MS margin
static struct { int32_t snd; std::chrono::steady_clock::time_point end; bool on; } vw_ch[8];
static double vw_seconds(const Ctx& c, int32_t snd) {
    uint32_t sz = hud_guest(c, 0x08831CACu, (uint32_t)snd);
    bool wav = sz % 152u == 1u;
    return (double)(sz / 0x98u) * (wav ? 2048.0 : 1024.0) / 44100.0;
}
static void vw_event(const Ctx& c, uint32_t op, uint32_t ch, int32_t snd) {
    static int on = -1, ms = 300, logs = 0;
    if (on < 0) { const char* e = getenv("TTT_VOICE_END"); on = e && *e ? atoi(e) != 0 : 1; e = getenv("TTT_VOICE_END_MS"); if (e && *e) ms = atoi(e); }
    uint32_t m = psp_l32(0x08A7D880u);
    if (logs++ < 60) printf("voz: evento %u sonido %d canal %u, fase %d\n", op, snd, ch, m >= 0x08800000u && m < 0x10000000u ? (int)psp_l32(m) : -1);
    if (ch >= 8) return;
    if (op == 3) { vw_ch[ch].on = false; return; }
    double sec = vw_seconds(c, snd);
    if (!on || sec <= 0.0 || sec > 30.0) { vw_ch[ch].on = false; return; }
    vw_ch[ch].snd = snd; vw_ch[ch].on = true;
    vw_ch[ch].end = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(sec * 1000.0) + ms);
}
static void vw_tick(const Ctx& c) {
    uint32_t mgr = psp_l32(0x08A78EE8u);
    if (mgr < 0x08800000u || mgr >= 0x10000000u) return;
    auto now = std::chrono::steady_clock::now();
    for (uint32_t ch = 0; ch < 8; ch++) {
        if (!vw_ch[ch].on || now < vw_ch[ch].end) continue;
        vw_ch[ch].on = false;
        if (!hud_guest(c, 0x0883A7F4u, mgr, ch)) continue;
        static int logs = 0;
        if (logs++ < 40) printf("voz: canal %u sigue sonando tras la duracion del sonido %d, se detiene\n", ch, vw_ch[ch].snd);
        hud_guest(c, 0x0883A5A4u, mgr, ch);
    }
}

static bool sub_wrap(uint32_t dir, Ctx& c) {
    if (dir == 0x08853290u) {
        if (sub_id != 0xFFFFFFFFu && std::chrono::steady_clock::now() >= sub_end) sub_clear();
        return false;
    }
    uint32_t op = c.r[7], snd = (uint32_t)(int32_t)(int16_t)c.r[6];
    if (c.r[4] == 1 && (op == 1 || op == 3)) vw_event(c, op, c.r[5], (int32_t)snd);
    if (c.r[4] != 1 || (op != 1 && op != 3) || !sub_want() || !hud_guest(c, 0x0885AC68u, 0)) return false;
    int32_t txt = (int32_t)hud_guest(c, 0x08853154u, snd);
    if (txt < 0 || hud_guest(c, 0x0885320Cu, snd)) return false;
    if (op == 3) {
        if (sub_id == (uint32_t)txt) sub_clear();
        return false;
    }
    // game rule: entry size / 0x98 frames of 1024 samples; a WAV-backed voice entry (size = frames * 152 + 1) holds 2048 per frame
    uint32_t sz = hud_guest(c, 0x08831CACu, snd);
    bool wav = sz % 152u == 1u;
    double sec = (double)(sz / 0x98u) * (wav ? 2048.0 : 1024.0) / 44100.0 + 5.0 / 30.0;
    if (sec < 1.0) sec = 1.0;
    if (sec > 20.0) sec = 20.0;
    psp_s32(SUB_G, (uint32_t)txt);
    sub_id = (uint32_t)txt;
    sub_name = 0;
    sub_snd = (int32_t)(int16_t)snd;
    sub_t0 = std::chrono::steady_clock::now();
    sub_end = std::chrono::steady_clock::now() + std::chrono::milliseconds((int)(sec * 1000));
    static int n = 0;
    if (n++ < 12) printf("hud: subtitulo centrado, sonido %d, texto %d, %.1f s%s\n", (int)(int16_t)snd, txt, sec, wav ? " (wav)" : "");
    c.r[2] = 0;
    return true;
}
// with bottom bars (3+ fighters) the redirected subtitle goes up to clear them (bars at y 231 in root 279);
// the speaker's name (character, or Steam name online) goes first in orange
static uint32_t sub_speaker(const Ctx& c, int ms);
static void cl_scaled_call(const Ctx& c, uint32_t fn, uint32_t font, int x, int y, uint32_t str, float s, float ax, float ay);
static void sub_text(Ctx& c, void (*orig)(Ctx&)) {
    if (sub_id == 0xFFFFFFFFu || !sub_want()) { orig(c); return; }
    uint32_t t = psp_l32(0x08A7DBC8u);
    int n = t ? (int)psp_l32(t) : 0;
    static int y = -1, on = -1;
    if (y < 0) { const char* e = getenv("TTT_SUB_Y"); y = e && *e ? atoi(e) : 216; }
    if (on < 0) { const char* e = getenv("TTT_SUB_NAME"); on = e && *e ? atoi(e) : 1; }
    if (n >= 3 && n <= 8) c.r[6] = (uint32_t)y;
    uint32_t font = c.r[4], str = c.r[7];
    int el = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - sub_t0).count();
    if (on && !sub_name && el < 1000) {
        sub_name = sub_speaker(c, el + 300);
        static int logs = 0;
        if (sub_name && logs++ < 12) { uint64_t h = 0; bool g = false; printf("hud: subtitulo de %s\n", text_show(sub_name, &h, &g).c_str()); }
    }
    if (!font) { orig(c); return; }
    if (!sub_mem) sub_mem = psp_reserve(0x400);
    uint32_t nb = sub_mem, lb = sub_mem + 0x80, keep = sub_mem + 0x300, org = sub_mem + 0x340;
    uint32_t L = 1, k = 0;
    for (; k < 600 && psp_l16(str + 2 * k); k++) if (psp_l16(str + 2 * k) == 10) L++;
    if (L > 4) { orig(c); return; }
    bool name = on && sub_name;
    k = 0;
    if (name) for (; k < 60 && psp_l16(sub_name + 2 * k); k++) psp_s16(nb + 2 * k, psp_l16(sub_name + 2 * k));
    psp_s16(nb + 2 * k, ':'); psp_s16(nb + 2 * k + 2, ' '); psp_s16(nb + 2 * k + 4, 0);
    int lh = (int)hud_guest(c, 0x088320CCu, font, nb), wn = name ? (int)hud_guest(c, 0x08831F68u, font, nb) : 0;
    if (lh <= 0 || lh > 40) lh = 13;
    static float sc = -1;   // TTT_SUB_SCALE: size of the centred subtitle (0.75 = 25% smaller than the game's)
    if (sc < 0) { const char* e = getenv("TTT_SUB_SCALE"); sc = e && *e ? (float)atof(e) : 0.75f; if (sc < 0.3f || sc > 2.0f) sc = 1.0f; }
    int slh = (int)lroundf(lh * sc);
    int x = (int)c.r[5], y0 = (int)c.r[6] + lh - slh - (int)(L - 1) * slh;
    uint32_t cur = psp_l32(psp_l32(font) + 0x10000u - 0x76E8u);
    if (!cur || cur >= 0x10000000u) { orig(c); return; }
    uint32_t shd = sub_mem + 0x380;
    for (int i = 0; i < 32; i += 4) { psp_s32(keep + i, psp_l32(cur + i)); psp_s32(org + i, psp_l32(cur + i)); psp_s32(shd + i, psp_l32(cur + i)); }
    psp_s32(org + 8, 0xFF0090FFu);
    static int shadow = -1;   // style +0xC shadow byte, +0x10/+0x14 offsets, +0x18 shadow colour (EBOOT 58.5); TTT_SUB_SHADOW=0 off
    if (shadow < 0) { const char* e = getenv("TTT_SUB_SHADOW"); shadow = e && *e ? atoi(e) : 1; }
    if (shadow) for (uint32_t st : {org, shd}) { psp_s32(st + 0xC, 1); psp_s32(st + 0x10, 1); psp_s32(st + 0x14, 1); psp_s32(st + 0x18, 0x78000000u); }
    hud_guest(c, 0x088321ECu, font, shd);
    uint32_t p = str;
    for (uint32_t li = 0; li < L; li++) {
        uint32_t m = 0, o = 0;
        while (m < 300 && psp_l16(p + 2 * m) && psp_l16(p + 2 * m) != 10) { uint32_t ch = psp_l16(p + 2 * m); if (ch != 13) psp_s16(lb + 2 * o++, ch); m++; }
        psp_s16(lb + 2 * o, 0);
        int yl = y0 + (int)li * slh, wt = (int)hud_guest(c, 0x08831F68u, font, lb);
        if (li == 0 && name && wn > 0) {
            int left = x - (wn + wt) / 2;
            hud_guest(c, 0x088321ECu, font, org);
            cl_scaled_call(c, 0x08834518u, font, left + wn / 2, yl, nb, sc, (float)x, (float)yl);
            hud_guest(c, 0x088321ECu, font, shd);
            cl_scaled_call(c, 0x08834518u, font, left + wn + wt / 2, yl, lb, sc, (float)x, (float)yl);
        } else cl_scaled_call(c, 0x08834518u, font, x, yl, lb, sc, (float)x, (float)yl);
        p += 2 * m + (psp_l16(p + 2 * m) ? 2 : 0);
    }
    hud_guest(c, 0x088321ECu, font, keep);
    c.r[2] = 0;
}

// centred subtitles (intro and battle) are 2D text drawn after the 3D: without this the map hides them
static void sub_depth(Ctx& c, void (*orig)(Ctx&)) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_SUB_DEPTH"); on = e && *e ? atoi(e) : 1; }
    orig(c);
    if (on) hud_guest(c, 0x08A64240u, 1);
}

// partner command box (BT3 bars): shown when the order changes (text instance state), held 2 s, faded in 0.5 s
static void cmd_fade(uint32_t box, uint32_t txt) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_CMD_FADE"); on = e && *e ? atoi(e) : 1; }
    if (!on || !txt || !psp_l32(txt)) return;
    static uint32_t last = 0;
    static std::chrono::steady_clock::time_point t0;
    uint32_t key = psp_l32(psp_l32(txt) + 0x30);
    auto now = std::chrono::steady_clock::now();
    if (key != last) {
        last = key; t0 = now;
        static int n = 0;
        if (n++ < 12) printf("hud: orden del companero cambia (estado %08X), se muestra\n", key);
    }
    double e = std::chrono::duration<double>(now - t0).count();
    float a = e < 2.0 ? 1.0f : e < 2.5 ? (float)(1.0 - (e - 2.0) / 0.5) : 0.0f;
    FReg one, al; one.f = 1.0f; al.f = a;
    for (uint32_t h : {box, txt}) {
        if (!h) continue;
        psp_s32(h + 0x40, one.u); psp_s32(h + 0x44, one.u); psp_s32(h + 0x48, one.u); psp_s32(h + 0x4C, al.u);
        psp_s32(h + 0x50, psp_l32(h + 0x50) & ~2u);
    }
}

static void sz_cmd_draw(uint32_t o);
static bool hud_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (cl_wrap(dir, c, orig)) return true;
    if (dir == 0x089F2194u && bh_on) {   // technique window: the BT3 name's scale ends with its draw; SZ shows no technique name
        const char* e = getenv("TTT_HUD_NAME");
        if (bh_sz && bh_lay != 4 && !(e && atoi(e) == 0)) return true;
        orig(c); bh_tw_end(); return true;
    }   // technique window: the BT3 name's scale ends with its draw
    if (dir == 0x089F6060u && bh_on) return true;   // BT3 bars: the blast stock replaces the union counter
    if (dir == 0x089D839Cu && bh_on && !bh_sz && bh_lay != 4 && !bh_msg_off()) return true;   // "¡Potenciación!" show: the BT3 "¡MEJORADA!" replaces it
    if ((dir == 0x0885332Cu || dir == 0x08853290u) && sub_wrap(dir, c)) return true;
    if (dir == 0x088404D8u) {
        if (c.r[31] == 0x08A30A28u) { uint32_t pak = c.r[5], rhg = c.r[7]; orig(c); av_plate_loaded(pak, rhg); return true; }   // name plate: own avatar
        if (c.r[31] == 0x089E82C0u && hud_on) { c.r[6] = 0x2F; c.r[7] = 0x30; return false; }
        if (c.r[31] == 0x089F5BA0u && hud_union_cur) { c.r[6] = 0x116; return false; }
        if (c.r[31] != 0x089E2F70u) { hud_redirect(c); return false; }
        hud_on = hud_bt3_want();
        int lay = hud_split_layout();
        bh_battle_start(hud_on, lay, hud_wlan());
        hud_cpu_n = 0; hud_cpu[0] = hud_cpu[1] = 0; hud_union = 0; memset(hud_tries, 0, sizeof(hud_tries)); memset(hud_abil, 0, sizeof(hud_abil));
        memset(hud_seen_pak, 0, sizeof(hud_seen_pak)); hud_seen_life[0] = hud_seen_life[1] = 30000;
        hud_prepare();
        if (hud_on) c.r[6] = lay == 4 && hud_has(c.r[5], 288) ? 288 : 0x117;
        static int n = 0;
        if (n++ < 20) printf("hud: barras %s%s, raiz %u\n", hud_on ? "BT3" : "originales", hud_wlan() ? " (multijugador)" : lay ? " (pantalla dividida)" : "", c.r[6]);
        orig(c);
        return true;
    }
    if (dir == 0x0885862Cu) {                 // local player index: the fighter of an ability icon copy while it runs
        orig(c);
        if (hud_abil_cur >= 0) c.r[2] = (uint32_t)hud_abil_cur;
        return true;
    }
    if (dir == 0x089D5C88u || dir == 0x089D5DDCu || dir == 0x089D5F44u || dir == 0x089D60E0u || dir == 0x089D64C8u || dir == 0x089D67CCu) {
        uint32_t o = c.r[4];
        int bar = -1;
        for (int k = 1; k < 4; k++) if (hud_abil[k] && hud_abil[k] == o) bar = k;
        if (dir == 0x089D5F44u && hud_on && hud_mgr && (bar > 0 || o == psp_l32(hud_mgr + 0x8C))) {
            int b = bar > 0 ? bar : 0;
            for (int k = 0; k < 4; k++) { uint32_t hk = psp_l32(o + 0x70 + 4 * k); hud_apply(hk, hud_sx[b], hud_sy[b], false); if (hk && b >= 2) psp_s32(hk + 0x1C, (uint32_t)hud_dy[b]); }
        }
        if (bar < 0) { orig(c); return true; }
        uint8_t keep[20];
        memcpy(keep, psp_p(0x08A71320u), 20);
        char name[20]; snprintf(name, sizeof(name), "BTL_ABILITY_%dP_%%d", bar + 1);
        memcpy(psp_p(0x08A71320u), name, strlen(name) + 1);
        int prev = hud_abil_cur;
        hud_abil_cur = (int)psp_l32(psp_l32(o + 0x80) + 0x2C);
        orig(c);
        hud_abil_cur = prev;
        memcpy(psp_p(0x08A71320u), keep, 20);
        return true;
    }
    if (dir == 0x089E1A3Cu) {                 // manager teardown: opponents' portraits and union counter go first
        cl_teardown(c, c.r[4]);
        for (int s2 = 0; s2 < 2 && c.r[4] == hud_mgr; s2++) {
            uint32_t o = hud_cpu[s2];
            if (!o) continue;
            hud_unload_portrait(c, s2);
        }
        for (int k = 1; k < 4 && c.r[4] == hud_mgr; k++) {
            uint32_t u = hud_abil[k];
            if (!u) continue;
            hud_abil[k] = 0;
            hud_guest(c, 0x089CFD08u, hud_mgr, u);
            uint32_t v = psp_l32(u + 0x40) + 8;
            hud_guest(c, psp_l32(v + 4), u + (uint32_t)(int32_t)(int16_t)psp_l16(v), 3);
        }
        if (hud_union && c.r[4] == hud_mgr) {
            uint32_t u = hud_union; hud_union = 0;
            hud_guest(c, 0x089CFD08u, hud_mgr, u);
            uint32_t v = psp_l32(u + 0x40) + 8;
            hud_guest(c, psp_l32(v + 4), u + (uint32_t)(int32_t)(int16_t)psp_l16(v), 3);
        }
        orig(c);
        return true;
    }
    if (dir == 0x089F62A0u || dir == 0x089F5B14u) {   // union counter: slot name and mirrored digit for the opponent's copy
        bool mine = hud_union && c.r[4] == hud_union;
        if (mine) { hud_union_cur = true; psp_s8(0x08A731D0u + 15, '3'); }
        orig(c);
        if (mine) { hud_union_cur = false; psp_s8(0x08A731D0u + 15, '1'); }
        return true;
    }
    if (dir == 0x089E38D4u || dir == 0x089E1F78u || dir == 0x089E22C0u) {   // manager setup, start, stop: also for the opponent's union counter
        uint32_t m = c.r[4];
        orig(c);
        if (m != hud_mgr) return true;
        uint32_t kids[7] = {hud_union, hud_abil[1], hud_abil[2], hud_abil[3], hud_kids_extra[0], hud_kids_extra[1], hud_kids_extra[2]};
        for (int k = 0; k < 7; k++) {
            uint32_t u = kids[k];
            if (!u) continue;
            if (dir == 0x089E1F78u) hud_guest(c, 0x089D15F0u, u);
            else if (dir == 0x089E22C0u) hud_guest(c, 0x089D161Cu, u);
            else {
                uint32_t v = psp_l32(u + 0x40) + 0x88;
                hud_guest(c, psp_l32(v + 4), u + (uint32_t)(int32_t)(int16_t)psp_l16(v), psp_l32(m + 0x70));
            }
        }
        return true;
    }
    if (dir == 0x089F6060u) {                 // union counter draw: same scale as its team's bar, mirrored for the opponent
        uint32_t o = c.r[4];
        int idx = hud_mgr && o == psp_l32(hud_mgr + 0xA4) ? 0 : (hud_union && o == hud_union ? 2 : -1);
        if (hud_on && idx >= 0) { hud_apply(psp_l32(o + 0x74), hud_sx[idx], hud_sy[idx], idx == 2); hud_digits(psp_l32(o + 0x78), hud_sx[idx], hud_sy[idx], 0); }
        orig(c);
        return true;
    }
    if (dir == 0x089E2EFCu) {                 // manager init: union counter for opponent 1 (the value is per team)
        uint32_t m = c.r[4];
        orig(c);
        hud_mgr = m;
        cl_init(c, m);
        if (!hud_on) return true;
        for (int k = 1; k < 4; k++) {         // ability icons for partner and opponents, bound to their fighter
            uint32_t bo = psp_l32(m + 0x74 + 4 * k), f = bo ? psp_l32(bo + 0x6C) : 0;
            if (!f) continue;
            uint32_t a = hud_guest(c, 0x089BFDF4u, 0x84);
            if (!a) continue;
            hud_guest(c, 0x089D5B20u, a);
            psp_s32(a + 0x80, f);
            hud_guest(c, 0x089D0310u, m, a, 0x80, 0);
            hud_abil[k] = a;
        }
        uint32_t opp = psp_l32(m + 0x7C);
        if (!opp || !psp_l32(opp + 0x6C)) return true;
        uint32_t u = hud_guest(c, 0x089BFDF4u, 0x84);
        if (!u) return true;
        hud_guest(c, 0x089F52BCu, u);
        psp_s32(u + 0x80, psp_l32(opp + 0x6C));
        hud_guest(c, 0x089D0310u, m, u, 0x80, 0);
        hud_union = u;
        return true;
    }
    if (dir == 0x089E8438u) {
        uint32_t o = c.r[4];
        orig(c);
        if (hud_cpu_n < 2) hud_cpu[hud_cpu_n++] = o;
        return true;
    }
    if (!hud_on) return false;
    if (dir == 0x089E84DCu) {
        uint32_t o = c.r[4];
        int s = hud_cpu_slot(o);
        if (s < 0) return false;
        uint32_t t = hud_mem + s * 0x180;
        memcpy(psp_p(HUD_TCPU), psp_p(t), HUD_TSIZE);
        orig(c);
        if (!psp_l32(o + 0x6C)) return true;
        psp_s32(o + 0x108, t);
        for (int k = 0; k < 16; k++) if (psp_l32(o + 0x70 + 4 * k)) hud_colors(psp_l32(o + 0x70 + 4 * k));
        hud_portrait(c, o);
        hud_attach_portrait(c, o);
        for (int k = 0; k < 16; k++) if (psp_l32(o + 0x70 + 4 * k)) psp_s32(psp_l32(o + 0x70 + 4 * k) + 0x50, 7);
        hud_virtual(c, o, 0xC8);
        return true;
    }
    if (dir == 0x089E5674u) {                 // draw: an opponent is drawn only when it is the viewer's target (+0x14)
        uint32_t o = c.r[4];
        int idx = hud_index(o);
        if (idx < 0) return false;
        if (idx >= 2) hud_refresh(c, o, idx);
        hud_bar_pieces(o, idx);
        hud_dy[idx] = hud_swap_dy(c, idx);
        if (idx == 2) hud_shift_log(hud_dy[idx]);
        hud_shift(o, hud_dy[idx]);
        uint32_t h11 = psp_l32(o + 0x9C), h0 = psp_l32(o + 0x70);
        if (idx >= 2 && !(h11 && psp_l32(h11)) && h0 && psp_l32(h0)) {   // opponent's portrait not there yet (its pak was not ready): keep trying, 8 frames in a row and then every 8
            int n = hud_portrait_time() ? hud_tries[idx]++ : 1;
            if (n < 8 || !(n & 7)) {
                if (!h11) hud_portrait(c, o);
                if (psp_l32(o + 0x9C)) hud_attach_portrait(c, o);
                h11 = psp_l32(o + 0x9C);
                static int logs = 0;
                bool ok = h11 && psp_l32(h11);
                if ((ok || n == 240) && logs++ < 20) {
                    uint32_t pk = hud_fighter_pak(o);
                    printf("hud: retrato de la barra %d %s tras %d intentos (pieza 0x%08X, recurso 0x%X, pak 0x%08X", idx, ok ? "listo" : "sin cargar", n + 1, h11, psp_l32(o + 0xDC), pk);
                    if (guest_ptr(pk)) { uint32_t off = __builtin_bswap32(psp_l32(pk + 4 + 4 * 0x2E)); printf(" entradas %u desplazamiento 0x%X marca 0x%08X", __builtin_bswap32(psp_l32(pk)), off, off < 0x2000000u ? psp_l32(pk + off) : 0); }
                    printf(")\n");
                }
            }
        }
        if (bh_bar(c, o, idx)) return true;
        uint32_t view = 0, keep = 0;
        if (hud_cpu_slot(o) >= 0 && psp_l32(o + 0x6C) && (view = hud_view(c, o))) {
            keep = psp_l32(view + 0x14);
            psp_s32(view + 0x14, psp_l32(psp_l32(o + 0x6C)));
        }
        orig(c);
        if (view) psp_s32(view + 0x14, keep);
        return true;
    }
    if (dir == 0x089E4354u) {                 // senzu of each team (icon, frame, count) at its bar scale
        uint32_t o = c.r[4];
        int idx = !hud_mgr ? -1 : o == psp_l32(hud_mgr + 0x9C) ? 0 : o == psp_l32(hud_mgr + 0xA0) ? 2 : -1;
        if (idx < 0) return false;
        hud_scale(psp_l32(o + 0x7C), hud_sx[idx], hud_sy[idx]);
        hud_scale(psp_l32(o + 0x80), hud_sx[idx], hud_sy[idx]);
        hud_digits(psp_l32(o + 0x84), hud_sx[idx], hud_sy[idx], 0);
        if (bh_senzu(c, o, idx)) { if (idx == 2) bh_names(c); return true; }
        orig(c);
        if (idx == 2) bh_names(c);
        return true;
    }
    if (dir == 0x089EBAB8u) {                 // partner command box at the scale of P1's team
        uint32_t o = c.r[4];
        if (!hud_partner_cpu(c)) return true;
        if (bh_on && bh_sz) { uint32_t ft = psp_l32(0x08A7DBC8u); int fn = ft ? (int)psp_l32(ft) : 0; if (fn >= 1 && fn <= 8) sz_cmd_draw(o); return true; }   // SZ bars: the port's name band with the order
        hud_scale(psp_l32(o + 0x74), hud_sx[0], hud_sy[0]);
        hud_scale(psp_l32(o + 0x78), hud_sx[0], hud_sy[0]);
        if (hud_on) cmd_fade(psp_l32(o + 0x74), psp_l32(o + 0x78));
        if (bh_on && !bh_sz) {   // BT3 bars: the command box under P1's name (x 22, y 58)
            uint32_t b = psp_l32(o + 0x74), sl = b ? psp_l32(b + 8) : 0;
            if (sl) { psp_s32(b + 0x18, (uint32_t)(22 - (int)psp_l32(sl + 0xE4))); psp_s32(b + 0x1C, (uint32_t)(58 - (int)psp_l32(sl + 0xE8))); }
        }
        if (hud_split_layout() == 4) {          // 4 views: the command text centred in its box (truncated rects left it high)
            uint32_t b = psp_l32(o + 0x74), t = psp_l32(o + 0x78);
            if (b && t && psp_l32(b + 8) && psp_l32(t + 8)) {
                int by = hud_rx(b, 0xE8), bh = hud_rx(b, 0xF0), ty = hud_rx(t, 0xE8), th = hud_rx(t, 0xF0);
                int dy = (int)lroundf(by + (bh - th) * 0.5f) - ty;
                static int logs = 0, last = -999;
                psp_s32(t + 0x1C, (uint32_t)dy);
            }
        }
        orig(c);
        return true;
    }
    if (dir == 0x089F4990u) { hud_timer(psp_l32(c.r[4] + 0x94)); if (bh_timer(c.r[4])) return true; orig(c); return true; }
    if (dir == 0x089E2D58u || dir == 0x089E2BB4u) {   // bar of one fighter unloaded / reloaded (transformation): opponents' portrait
        uint32_t fi = c.r[5];
        if (dir == 0x089E2D58u)
            for (int s2 = 0; s2 < 2; s2++) if (hud_cpu[s2] && psp_l32(hud_cpu[s2] + 0x6C) && psp_l32(psp_l32(hud_cpu[s2] + 0x6C) + 0x2C) == fi) hud_unload_portrait(c, s2);
        orig(c);
        if (dir == 0x089E2BB4u)
            for (int s2 = 0; s2 < 2; s2++) {
                uint32_t o = hud_cpu[s2];
                if (!o || !psp_l32(o + 0x6C) || psp_l32(psp_l32(o + 0x6C) + 0x2C) != fi) continue;
                hud_unload_portrait(c, s2);
                hud_portrait(c, o);
                if (psp_l32(o + 0x9C)) hud_colors(psp_l32(o + 0x9C));
                hud_tries[2 + s2] = 0;
            }
        return true;
    }
    if (dir == 0x089E642Cu) {                 // portrait attach (also on hits): state as for a teammate, never the blank one
        uint32_t o = c.r[4];
        if (hud_cpu_slot(o) < 0 || !psp_l32(o + 0x6C)) return false;
        uint32_t view = hud_view(c, o), keep = view ? psp_l32(view + 4) : 0;
        if (view) psp_s32(view + 4, psp_l32(psp_l32(o + 0x6C) + 4));
        orig(c);
        if (view) psp_s32(view + 4, keep);
        return true;
    }
    if (dir == 0x089E85ECu) {
        uint32_t o = c.r[4];
        if (hud_cpu_slot(o) < 0) return false;
        if (psp_l32(o + 0x6C)) psp_s8(o + 0x12C, 1);
        return true;
    }
    return false;
}
