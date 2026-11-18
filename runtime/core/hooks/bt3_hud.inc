// BT3 battle HUD drawn by the port from the bt3_hud mod (claude/BT3_HUD_spec.md): GE sprites written into the HUD's own list
#include "gpu/image.h"
#include <atomic>
#include <thread>
#include <map>
#include <string>
std::string psp_cr_file(const std::string& rel);
int psp_window_language();

static const float BH_SX = 0.7083f, BH_SY = 0.6071f, BH_EX = 480.0f / BH_SX / 2 - 256;   // BH_EX: widescreen stretch of the bar (decomp HudGauge_WideHpExtra)
struct BhTex { uint32_t a; int w, h, ow = 0, oh = 0; };   // w/h power of two (padded), ow/oh = PNG size
static std::map<std::string, BhTex, std::less<>> bh_tex;
static bool bh_loaded = false, bh_ok = false;
static uint32_t bh_top_f[2], bh_bot_f[2];   // panel fighters (left, right), top and bottom copies
static int bh_pn = 0;   // bar being drawn (0..3): its own trails, lamps, icons and portrait capture
static float bh_oy = 0;   // BT3 y offset of the panel being drawn (bottom copies)
static const float BH_BOT = 372;   // BT3 y offset of the bottom copies: panel at screen y 231..257, its name under it
static uint32_t bh_vbuf = 0, bh_vpos = 0, bh_cur_tex = 0;
static int bh_cur_add = -1, bh_lang = -1;
static int bh_hp_shown[4], bh_hp_drain[4], bh_ki_shown[4], bh_ki_drain[4];
static uint32_t bh_vb_last[4];
static int bh_aura[4];
static uint32_t bh_rng = 0x1234567u;
static int bh_rand(int n) { bh_rng = bh_rng * 1103515245u + 12345u; return (int)((bh_rng >> 16) % (uint32_t)n); }

static int bh_phase() { uint32_t m = psp_l32(0x08A7D880u); return guest_ptr(m) ? (int)psp_l32(m) : -1; }   // 0-2 intros, 3 Ready/Fight, 4 combat
static bool bh_mod() { return !psp_cr_file("BT3Hud/common/gauge_06.png").empty(); }
static bool sz_mod() { return !psp_cr_file("SZHud/layout.txt").empty(); }

static bool bh_sz_kit = false;
static void sz_release();
static uint32_t bh_gen = 1;   // bumped by bh_drop: cached texture pointers go stale
template <class P> static void bh_drop(P pick) {
    bh_gen++;
    for (auto it = bh_tex.begin(); it != bh_tex.end();) if (pick(it->first)) { psp_free(it->second.a); it = bh_tex.erase(it); } else ++it;
    if (!bh_tex.count("gauge_06")) bh_loaded = false;
}
static bool bh_late = false;   // set after the warm-up: anything built later costs a combat frame
static void bh_late_log(const char* what, const char* name, std::chrono::steady_clock::time_point t0) {
    static int logs = 0;
    if (!bh_late || logs >= 40) return;
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (ms < 0.1) return;
    logs++;
    printf("hud: trabajo en combate: %s %s %.2f ms\n", what, name, ms);
}
#define BH_LATE(what, name, ...) do { auto _lt = std::chrono::steady_clock::now(); __VA_ARGS__; bh_late_log(what, name, _lt); } while (0)
struct BhPng { std::vector<uint32_t> px; int w = 0, h = 0; bool ok = false; };
static std::string bh_path(const char* name) {
    bool sz = !strncmp(name, "T_UI_", 5);   // SZ textures (SZHud), loaded at half size: the 3840 canvas is drawn at 480 x 4
    std::string dir = sz ? "SZHud/" : "BT3Hud/";
    std::string lg = dir + (bh_lang == 1 ? "es/" : "en/") + name + ".png", f = sz ? psp_cr_file(lg) : std::string();   // SZ: localized copy first ("Golpes")
    if (f.empty()) f = psp_cr_file(dir + "common/" + name + ".png");
    if (f.empty() && !sz) f = psp_cr_file(lg);
    return f;
}
static void bh_decode(const std::string& f, bool sz, BhPng& r) {   // file and PNG only: safe off the game thread
    std::vector<uint8_t> d;
    std::string err;
    if (FILE* fp = f.empty() ? nullptr : fopen(f.c_str(), "rb")) {
        fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
        d.resize(n > 0 ? (size_t)n : 0);
        if (fread(d.data(), 1, d.size(), fp) != d.size()) d.clear();
        fclose(fp);
    }
    int w = 0, h = 0;
    std::vector<uint32_t>& px = r.px;
    if (!d.empty() && png_decode(d, &w, &h, px, &err) && sz && w >= 32 && h >= 32) {
        std::vector<uint32_t> q((size_t)(w / 2) * (h / 2));
        for (int y = 0; y < h / 2; y++)
            for (int x = 0; x < w / 2; x++) {
                uint32_t s4[4] = {px[(size_t)2 * y * w + 2 * x], px[(size_t)2 * y * w + 2 * x + 1], px[(size_t)(2 * y + 1) * w + 2 * x], px[(size_t)(2 * y + 1) * w + 2 * x + 1]}, o = 0;
                for (int k = 0; k < 4; k++) o |= ((((s4[0] >> (8 * k)) & 255) + ((s4[1] >> (8 * k)) & 255) + ((s4[2] >> (8 * k)) & 255) + ((s4[3] >> (8 * k)) & 255) + 2) / 4) << (8 * k);
                q[(size_t)y * (w / 2) + x] = o;
            }
        px.swap(q); w /= 2; h /= 2;
    }
    r.w = w; r.h = h;
    r.ok = !d.empty() && w > 0 && h > 0 && w <= (sz ? 1024 : 512) && h <= (sz ? 1024 : 512) && px.size() >= (size_t)w * h;
}
static const BhTex* bh_put(const char* name, const BhPng& r) {
    BhTex t{0, 0, 0};
    if (r.ok) {
        int W = 1, H = 1;
        while (W < r.w) W <<= 1;
        while (H < r.h) H <<= 1;
        t.a = psp_alloc((uint32_t)(W * H * 4)); t.w = W; t.h = H; t.ow = r.w; t.oh = r.h;
        memset(psp_p(t.a), 0, (size_t)W * H * 4);
        for (int y = 0; y < r.h; y++) memcpy(psp_p(t.a + (uint32_t)(y * W * 4)), r.px.data() + (size_t)y * r.w, (size_t)r.w * 4);
    } else printf("hud bt3: falta la textura %s\n", name);
    BhTex& e = bh_tex[name];
    e = t;
    return t.a ? &e : nullptr;
}
static const BhTex* bh_get(const char* name) {
    auto it = bh_tex.find(name);
    if (it != bh_tex.end()) return it->second.a ? &it->second : nullptr;
    auto lt0 = std::chrono::steady_clock::now();
    BhPng r;
    bh_decode(bh_path(name), !strncmp(name, "T_UI_", 5), r);
    const BhTex* t = bh_put(name, r);
    bh_late_log("textura", name, lt0);
    return t;
}
// loads every listed texture whose file exists, PNGs decoded on several threads; returns how many threads
static int bh_preload(const std::vector<std::string>& names) {
    std::vector<std::string> nm, fs;
    for (auto& n : names) {
        if (bh_tex.count(n)) continue;
        std::string f = bh_path(n.c_str());
        if (f.empty()) continue;
        nm.push_back(n); fs.push_back(f);
    }
    std::vector<BhPng> r(nm.size());
    std::atomic<size_t> next{0};
    auto work = [&]() { for (size_t i; (i = next++) < nm.size();) bh_decode(fs[i], !strncmp(nm[i].c_str(), "T_UI_", 5), r[i]); };
    int nt = (int)std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    nt = std::min(nt, (int)nm.size());
    std::vector<std::thread> th;
    for (int i = 1; i < nt; i++) th.emplace_back(work);
    work();
    for (auto& t : th) t.join();
    for (size_t i = 0; i < nm.size(); i++) bh_put(nm[i].c_str(), r[i]);
    return std::max(nt, 1);
}

// texture by interned name (SZ items) or by literal address (BT3 pieces): no map search per draw
static const BhTex* bh_gets(const SzS& n) {
    static std::unordered_map<const std::string*, const BhTex*> m; static uint32_t g = 0;
    if (g != bh_gen) { m.clear(); g = bh_gen; }
    auto f = m.find(n.p);
    if (f != m.end()) return f->second;
    const BhTex* t = bh_get(n.c_str());
    if (g == bh_gen) m[n.p] = t;
    return t;
}
static const BhTex* bh_getk(const char* lit) {
    static std::unordered_map<const char*, const BhTex*> m; static uint32_t g = 0;
    if (g != bh_gen) { m.clear(); g = bh_gen; }
    auto f = m.find(lit);
    if (f != m.end()) return f->second;
    const BhTex* t = bh_get(lit);
    if (g == bh_gen) m[lit] = t;
    return t;
}
static void bh_reset_combo();
static void bh_nt_reset();
static void sz_warm();
static bool bh_msg_off();
// decided at the HUD root load: Life bars = BT3 (2) or Sparking Zero (3) with its mod installed; split screen and network too
static void bh_battle_start(bool want, int lay, bool wlan) {
    const char* e = getenv("TTT_HUD_BT3");
    int mode = e ? atoi(e) : 0;
    bh_late = false;
    bh_sz = want && mode == 3 && sz_mod() && sz_load();
    bh_on = bh_sz || (want && mode == 2 && bh_mod());
    bh_views = bh_on && lay != 0;
    bh_lay = bh_on ? lay : 0;
    int lang = psp_window_language() == 3 ? 1 : 0;
    bool sz_was = bh_sz_kit;
    bh_sz_kit = bh_sz;
    if (bh_sz) bh_drop([](const std::string& n) { return n.compare(0, 5, "T_UI_") != 0; });   // the other HUD mod's pieces go back to the pool
    else if (sz_was || !bh_on) { bh_drop([](const std::string& n) { return n.compare(0, 5, "T_UI_") == 0; }); sz_release(); }
    if (!bh_on) bh_drop([](const std::string&) { return true; });
    if (bh_sz && lang != bh_lang) { bh_lang = lang; bh_drop([](const std::string& n) { return n == "T_UI_ComboTxt_L_00" || n == "T_UI_ComboTxt_R_00"; }); }
    if (bh_sz && !bh_vbuf) bh_vbuf = psp_reserve(4 * 0x8000);
    if (bh_sz) sz_warm();
    if (bh_on && !bh_sz && (!bh_loaded || lang != bh_lang)) {
        bh_lang = lang;
        bh_drop([](const std::string& n) { return !n.compare(0, 6, "notice") || !n.compare(0, 5, "combo"); });
        if (!bh_vbuf) bh_vbuf = psp_reserve(4 * 0x8000);
        static const char* N[] = {"gauge_06", "gauge_08_1", "gauge_08_2", "gauge_08_3", "gauge_08_4", "gauge_08_5", "gauge_14", "gauge_15",
                                  "gauge_00", "gauge_00_1", "gauge_00_2", "gauge_03", "gauge_16", "gauge_17", "gauge_07", "gauge_18", "gauge_19", "gauge_20", "gauge_21", "gauge_22", "combo_00", "combo_01", "combo_02", "combo_03", "combo_04", "combo_05", "combo_06", "combo_07", "notice_00", "notice_01", "notice_02", "notice_03", "notice_04", "notice_05", "notice_06", "notice_07"};
        int n = 0;
        for (auto* s : N) n += bh_get(s) != nullptr;
        bh_ok = n == (int)(sizeof N / sizeof *N);
        bh_loaded = true;
        printf("hud bt3: %d/%d texturas cargadas (%s)\n", n, (int)(sizeof N / sizeof *N), lang ? "es" : "en");
    }
    bh_on = bh_on && (bh_sz || bh_ok);
    bh_late = bh_on;
    bh_reset_combo();
    for (int s = 0; s < 4; s++) { bh_hp_shown[s] = bh_ki_shown[s] = -1; bh_hp_drain[s] = 100; bh_ki_drain[s] = 400; bh_vb_last[s] = 0; bh_aura[s] = 0; }
    static int logs = 0;
    if (logs++ < 20) printf("hud bt3: memoria del HUD %u KB en uso, reserva libre %u KB\n", psp_alloc_used() / 1024, psp_reserve_left() / 1024);
    if (logs < 20) printf("hud bt3: %s%s\n", bh_sz ? "barras SZ" : bh_on ? "barras BT3 reales" : mode >= 2 ? "sin el mod: barras estilo BT3" : "apagado", !bh_on ? "" : lay == 4 ? " (4 vistas)" : lay == 2 ? " (2 vistas)" : wlan ? " (red)" : "");
}

static void bh_w(uint32_t cmd, uint32_t arg) {
    uint32_t ctx = psp_l32(0x08B478D0u), p = ctx && ctx < 0x10000000u ? psp_l32(ctx + 8) : 0;
    if (!p || p >= 0x10000000u) return;
    psp_s32(p, (cmd << 24) | (arg & 0xFFFFFFu));
    psp_s32(ctx + 8, p + 4);
}
// both: in two-screen mode the sprites go to both screens (timer); otherwise each sprite goes to the side of its centre
static void bh_begin(bool both = false) {
    bh_w(0x00, both ? 0xF80003u : 0xF80001u);
    bh_cur_tex = 0; bh_cur_add = -1;
    bh_w(0x1E, 1); bh_w(0x21, 1); bh_w(0x22, 0); bh_w(0x23, 0); bh_w(0xC2, 0); bh_w(0xC3, 3); bh_w(0xC9, 0x100);
    bh_w(0x12, 3u | (7u << 2) | (3u << 7) | (1u << 23));
}
static void bh_end() { bh_w(0x00, 0xF80000u); }

static void bh_quad(const BhTex* t, float u0, float u1, float v0, float v1, float X0, float X1, float Y0, float Y1, uint32_t col = 0xFFFFFFFFu, bool add = false);
// one sprite: BT3 screen rect (512x448), side 1 mirrored at the right edge, mode 2 = centred (timer), 3 = anchored right
static void bh_spr(const BhTex* t, float u0, float u1, float v0, float v1, float x0, float x1, float y0, float y1, int side,
                   uint32_t col = 0xFFFFFFFFu, bool add = false, bool keep = false) {
    if (!t || x1 <= x0 || y1 <= y0) return;
    float X0, X1;
    if (side == 2) { X0 = 240 + (x0 - 256) * BH_SX; X1 = 240 + (x1 - 256) * BH_SX; }
    else if (side == 1) { X0 = 480 - x1 * BH_SX; X1 = 480 - x0 * BH_SX; if (!keep) std::swap(u0, u1); }
    else if (side == 3) { X0 = 480 - (512 - x0) * BH_SX; X1 = 480 - (512 - x1) * BH_SX; }   // right part, not mirrored
    else { X0 = x0 * BH_SX; X1 = x1 * BH_SX; }
    bh_quad(t, u0, u1, v0, v1, X0, X1, (y0 + bh_oy) * BH_SY, (y1 + bh_oy) * BH_SY, col, add);
}
// one sprite in screen units (480x272)
static void bh_quad(const BhTex* t, float u0, float u1, float v0, float v1, float X0, float X1, float Y0, float Y1, uint32_t col, bool add) {
    if (t->a != bh_cur_tex) {
        bh_w(0xA0, t->a); bh_w(0xA8, ((t->a >> 24) & 0xF) << 16 | (uint32_t)t->w);
        bh_w(0xB8, (uint32_t)(__builtin_ctz(t->h) << 8 | __builtin_ctz(t->w))); bh_w(0xCB, 0);
        bh_cur_tex = t->a;
    }
    if ((int)add != bh_cur_add) { bh_w(0xDF, add ? 2u | (10u << 4) : 2u | (3u << 4)); if (add) bh_w(0xE1, 0xFFFFFF); bh_cur_add = add; }
    uint32_t v = bh_vbuf + bh_vpos;
    bh_vpos = (bh_vpos + 48) % (4 * 0x8000);
    float f[2][6] = {{u0, v0, 0, X0, Y0, 0}, {u1, v1, 0, X1, Y1, 0}};
    for (int k = 0; k < 2; k++) {
        uint32_t b = v + 24 * k;
        memcpy(psp_p(b), &f[k][0], 8);
        psp_s32(b + 8, col);
        memcpy(psp_p(b + 12), &f[k][3], 12);
    }
    bh_w(0x10, ((v >> 24) & 0xF) << 16); bh_w(0x01, v); bh_w(0x04, (6u << 16) | 2);
}

// list of techniques: energy cost with the BT3 blast stock digit (tex 14), centred at cx, height h
static bool bh_list_digit(float cx, float y, float h, int v, float k, float a) {
    const BhTex* t = bh_on && !bh_sz && v >= 0 && v <= 7 ? bh_get("gauge_14") : nullptr;
    if (!t) return false;
    float w = h * BH_SX / BH_SY;
    int kk = (int)(std::min(1.0f, k) * 255), aa = (int)(std::min(1.0f, a) * 255);
    bh_begin();
    bh_quad(t, (float)(v % 4 * 32), (float)(v % 4 * 32 + 32), (float)(v / 4 * 32), (float)(v / 4 * 32 + 32), cx - w * 0.5f, cx + w * 0.5f, y, y + h, (uint32_t)aa << 24 | (uint32_t)kk << 16 | (uint32_t)kk << 8 | (uint32_t)kk);
    bh_end();
    return true;
}
// list of techniques skin: row pieces list_l / list_m / list_r (any size, drawn at the row height h); false without the three
static bool bh_list_row(float xl, float xr, float y, float h, float k, float a, float* wl) {
    static const char* N[] = {"list_l", "list_m", "list_r"};
    const BhTex* t[3];
    for (int i = 0; i < 3; i++) if (!(t[i] = bh_on && !bh_sz ? bh_get(N[i]) : nullptr)) return false;
    float w0 = h * t[0]->ow / t[0]->oh, w2 = h * t[2]->ow / t[2]->oh;
    if (wl) { *wl = w0; return true; }
    float x0 = floorf(xl), x1 = floorf(xl + w0), x2 = std::max(x1 + 1, floorf(xr));
    int kk = (int)(std::min(1.0f, k) * 255), aa = (int)(std::min(1.0f, a) * 255);
    uint32_t col = (uint32_t)aa << 24 | (uint32_t)kk << 16 | (uint32_t)kk << 8 | (uint32_t)kk;
    bh_begin();
    bh_quad(t[0], 0, t[0]->ow - 0.5f, 0, (float)t[0]->oh, x0, x1, y, y + h, col);   // translucent: edges meet, never overlap
    bh_quad(t[1], 0.5f, t[1]->ow - 0.5f, 0, (float)t[1]->oh, x1, x2, y, y + h, col);
    bh_quad(t[2], 0.5f, (float)t[2]->ow, 0, (float)t[2]->oh, x2, x2 + w2, y, y + h, col);
    bh_end();
    return true;
}
// list of techniques: ki cost as BT3 ki cells (tex 8 sub 1, 7 BT3 units apart), from x, height h; returns the end x or -1 (a < 0 measures)
static float bh_list_ki(float x, float y, float h, int n, float k, float a) {
    const BhTex* t = bh_on && !bh_sz && n > 0 ? bh_get("gauge_08_1") : nullptr;
    if (!t) return -1;
    float w = h * 16 * BH_SX / (17 * BH_SY), st = h * 7 * BH_SX / (17 * BH_SY);
    if (a < 0) return x + (n - 1) * st + w;   // measure only
    int kk = (int)(std::min(1.0f, k) * 255), aa = (int)(std::min(1.0f, a) * 255);
    uint32_t col = (uint32_t)aa << 24 | (uint32_t)kk << 16 | (uint32_t)kk << 8 | (uint32_t)kk;
    bh_begin();
    for (int i = 0; i < n; i++) bh_quad(t, 16, 32, 16, 33, x + i * st, x + i * st + w, y, y + h, col);
    bh_end();
    return x + (n - 1) * st + w;
}
static int bh_hp_sub(int b) { return b >= 7 ? 5 : b >= 2 ? 1 : b >= 1 ? 4 : 3; }
struct BhRamp {
    float v = 0, a = 0, b = 0, d = 0, t = 0;
    void start(float sec, float from, float to) { d = sec; a = from; b = to; t = 0; v = from; }
    bool step(float dt) { t += dt; float k = d > 0 ? std::min(1.0f, t / d) : 1.0f; v = a + (b - a) * k; return k >= 1.0f; }
    bool running() const { return v != b; }
};
static void bh_split(int v, int* b, int* r) { *b = v / 10000; *r = v % 10000; if (*r == 0 && *b) { *r = 10000; (*b)--; } }
static const BhTex* bh_strip(int sub) { static const char* S[] = {"gauge_08", "gauge_08_1", "gauge_08_2", "gauge_08_3", "gauge_08_4", "gauge_08_5"}; return bh_getk(S[sub]); }

// 30 Hz steps of the trails (BT3 ramps), frozen in pause
static void bh_step(int s, int hp, int ki, bool spk) {
    if (bh_hp_shown[s] < 0 || hp > bh_hp_shown[s]) bh_hp_shown[s] = hp;
    if (bh_ki_shown[s] < 0 || ki >= bh_ki_shown[s]) { bh_ki_shown[s] = ki; bh_ki_drain[s] = 400; }
    uint32_t vb = psp_l32(0x08A78E14u);
    int n = bh_vb_last[s] ? (int)(vb - bh_vb_last[s]) / 2 : 0;
    if (n <= 0 && bh_vb_last[s]) return;
    bh_vb_last[s] = bh_vb_last[s] ? bh_vb_last[s] + 2u * (uint32_t)n : vb;
    if (bp_open || n > 8) n = n > 8 ? 1 : 0;
    for (int i = 0; i < n; i++) {
        int gap = bh_hp_shown[s] - hp;
        if (gap > 20000) bh_hp_drain[s] = std::max(bh_hp_drain[s], 200); else if (gap > 10000) bh_hp_drain[s] = std::max(bh_hp_drain[s], 150);
        else if (gap > 5000) bh_hp_drain[s] = std::max(bh_hp_drain[s], 120);
        if (gap > 0) bh_hp_shown[s] = std::max(hp, bh_hp_shown[s] - bh_hp_drain[s]);
        if (bh_hp_shown[s] == hp) bh_hp_drain[s] = 100;
        if (ki < bh_ki_shown[s] - 40000) bh_ki_drain[s] = std::max(bh_ki_drain[s], 800); else if (ki < bh_ki_shown[s] - 20000) bh_ki_drain[s] = std::max(bh_ki_drain[s], 606);
        if (ki < bh_ki_shown[s]) bh_ki_shown[s] = std::max(ki, bh_ki_shown[s] - bh_ki_drain[s]);
        if (ki >= bh_ki_shown[s]) { bh_ki_shown[s] = ki; bh_ki_drain[s] = 400; }
        bh_aura[s] = std::max(0, std::min(128, bh_aura[s] + (spk ? 16 : -16)));
    }
}

// TTT portrait (holder +0x9C of the bar) in the BT3 face box (spec 3.4), fit by height, mirrored on the right
static uint32_t bh_cur_bar = 0;
// BT3 face pictures carry their own shape: round cut behind the blast digit, slanted cuts where the reserve markers and the ki bars start
static float bh_cov(float d) { return d <= -0.5f ? 0.0f : d >= 0.5f ? 1.0f : d + 0.5f; }
// right edge of the face window (BT3 units), traced on his BT3 reference: up the frame's slant, round, then down along the ki bars
static float bh_face_edge(float v) {
    static const float P[6][2] = {{10.3f, 63.4f}, {16.7f, 66.7f}, {22.5f, 67.2f}, {38.5f, 60.5f}, {49.7f, 54.2f}, {53.9f, 48.7f}};
    int i = 0;
    while (i < 4 && v > P[i + 1][0]) i++;
    return P[i][1] + (v - P[i][0]) * (P[i + 1][1] - P[i][1]) / (P[i + 1][0] - P[i][0]);
}
bool psp_bt3_face(int side, uint32_t have, uint32_t* gen, uint32_t* seq, int* dim, float* geo, uint32_t* col, std::vector<uint32_t>* px);
void psp_render_bt3_geo(const float* g);
struct BhFace { uint32_t gen = 0, seq = 0, buf[2] = {0, 0}, cap = 0; int cur = 0, idle = 0, w = 0, h = 0, side = -1; float uv[4] = {}, oy = -1; };
static BhFace bh_fc[4];
// the game's portrait is captured by the GPU (marker FA), its alpha multiplied by the panel silhouette (tex 7, as BT3) and drawn by the port
static void bh_face(const Ctx& c, uint32_t o, int side) {
    uint32_t h = psp_l32(o + 0x9C), in = h ? psp_l32(h) : 0, sl = h ? psp_l32(h + 8) : 0;
    const BhTex* m = bh_getk("gauge_07");
    if (!in || !m) return;
    FReg m1; m1.u = sl ? psp_l32(sl + 0x54) : 0;
    float hh = sl ? (float)(int)psp_l32(sl + 0xF0) : 0, nat = sl && m1.f > 0.01f && hh > 0 ? hh / m1.f : 64;
    float k = 1.66f * 64 * BH_SY / nat, x = -1.0f, y = 3.4f + bh_oy * BH_SY;   // larger than the window on every side: the picture's own edges never show
    bh_w(0x00, 0xF80001u);
    bh_w(0x00, 0xFA0001u | (uint32_t)bh_pn << 1);
    cp_inst(c, in, side ? 480 - x : x, y, side ? -k : k, k, 1, 1);
    bh_w(0x00, 0xFA0000u); bh_w(0x00, 0xF80000u);
    BhFace& f = bh_fc[bh_pn];
    if (f.side != side || f.oy != bh_oy) { f.side = side; f.oy = bh_oy; f.gen = 0; }   // the bar moved to another panel: compose again
    uint32_t gen = 0, seq = 0, col = 0; int dim[4]; float g[8]; std::vector<uint32_t> px;
    if (!psp_bt3_face(bh_pn, f.gen, &gen, &seq, dim, g, &col, &px)) return;
    f.idle = seq == f.seq ? f.idle + 1 : 0; f.seq = seq;
    if (f.idle > 3) return;
    if (gen != f.gen) {
        int pw = dim[0], ph = dim[1];
        float tw = (float)dim[2], th = (float)dim[3];
        if (g[4] <= 1.01f && g[5] <= 1.01f && g[6] <= 1.01f && g[7] <= 1.01f && tw > 2) { g[4] *= tw; g[5] *= tw; g[6] *= th; g[7] *= th; }
        if (!side && bh_oy == 0 && tw > 2 && th > 2) { float gn[8] = {g[0], g[1], g[2], g[3], g[4] / tw, g[5] / tw, g[6] / th, g[7] / th}; psp_render_bt3_geo(gn); }
        int OW = std::min(512, (int)std::ceil((g[1] - g[0]) * 4)), OH = std::min(256, (int)std::ceil((g[3] - g[2]) * 4)), W = 1, H = 1;   // 4 texels per screen pixel: smooth edges at any internal resolution
        while (W < OW) W <<= 1;
        while (H < OH) H <<= 1;
        if ((uint32_t)(W * H * 4) > f.cap) { psp_free(f.buf[0]); psp_free(f.buf[1]); f.buf[0] = psp_alloc((uint32_t)(W * H * 4)); f.buf[1] = psp_alloc((uint32_t)(W * H * 4)); f.cap = (uint32_t)(W * H * 4); }
        f.cur ^= 1;
        uint32_t* d = (uint32_t*)psp_p(f.buf[f.cur]);
        const uint32_t* mk = (const uint32_t*)psp_p(m->a);
        uint32_t mmax = 1;
        for (int i = 0; i < m->w * m->h; i++) mmax = std::max(mmax, mk[i] >> 24);
        auto mask = [&](float u, float v) -> float {   // bilinear alpha of tex 7
            u -= 0.5f; v -= 0.5f;
            int x0 = (int)std::floor(u), y0 = (int)std::floor(v); float fx = u - x0, fy = v - y0, r = 0;
            for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
                int x = x0 + i, y = y0 + j;
                float a = x >= 0 && y >= 0 && x < m->w && y < m->h ? (float)(mk[(size_t)y * m->w + x] >> 24) : 0.0f;
                r += a * (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
            }
            return r / mmax;
        };
        auto face = [&](float u, float v) -> uint32_t {   // bilinear, premultiplied, of the captured picture
            u = u * pw - 0.5f; v = v * ph - 0.5f;
            int x0 = (int)std::floor(u), y0 = (int)std::floor(v); float fx = u - x0, fy = v - y0, c[4] = {0, 0, 0, 0};
            for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
                int x = std::min(pw - 1, std::max(0, x0 + i)), y = std::min(ph - 1, std::max(0, y0 + j));
                uint32_t p = px[(size_t)y * pw + x]; float w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy), a = (p >> 24) * w;
                c[0] += (p & 255) * a; c[1] += ((p >> 8) & 255) * a; c[2] += ((p >> 16) & 255) * a; c[3] += a;
            }
            if (c[3] < 0.5f) return 0;
            return (uint32_t)(c[0] / c[3]) | (uint32_t)(c[1] / c[3]) << 8 | (uint32_t)(c[2] / c[3]) << 16 | (uint32_t)std::min(255.0f, c[3]) << 24;
        };
        memset(d, 0, (size_t)W * H * 4);
        for (int yy = 0; yy < OH; yy++) {
            float sy = g[2] + (yy + 0.5f) * (g[3] - g[2]) / OH, tv = g[6] + (sy - g[2]) / (g[3] - g[2]) * (g[7] - g[6]), bv = sy / BH_SY - bh_oy;
            for (int xx = 0; xx < OW; xx++) {
                float sx = g[0] + (xx + 0.5f) * (g[1] - g[0]) / OW, tu = g[4] + (sx - g[0]) / (g[1] - g[0]) * (g[5] - g[4]);
                float X = side ? 480 - sx : sx, bu = X / BH_SX;
                float cv = mask(bu + 2, bv - 8) * bh_cov((std::hypot(X - 21 * BH_SX, sy - (54 + bh_oy) * BH_SY) - 8.0f) / 0.25f) * bh_cov((bh_face_edge(bv) - bu) * 0.7f / 0.25f);
                if (cv <= 0) continue;
                uint32_t p = face(tu / tw, tv / th);
                d[(size_t)yy * W + xx] = (p & 0xFFFFFFu) | ((uint32_t)((p >> 24) * std::min(1.0f, cv)) << 24);
            }
        }
        d[0] = (gen & 0xFFFFFFu);
        f.w = W; f.h = H;
        f.uv[0] = 0; f.uv[1] = (float)OW; f.uv[2] = 0; f.uv[3] = (float)OH;
        f.gen = gen;
        static int logs = 0;
        if (logs++ < 4) printf("hud bt3: retrato lado %d recortado %dx%d (textura %dx%d) escala %.3f, silueta alfa maximo %u\n", side, OW, OH, W, H, k, mmax);
    }
    BhTex t{f.buf[f.cur], f.w, f.h};
    bh_begin();
    bh_quad(&t, f.uv[0], f.uv[1], f.uv[2], f.uv[3], g[0], g[1], g[2], g[3], col);
    bh_end();
}

// status icons (spec 3.5, HudGauge_SetStatIcons / UpdateStatIcons): the stat modifiers TTT shows as ability icons,
// 0x089128B4(fighter, stat, -1) as its update 0x089D60E0; BT3 icon i = stat {0, 3, 1, 2} (attack, speed, defence, ki)
struct BhStat { uint32_t mask = 0, pos = 0, prev = 0, vb = 0; int st = -1, from[4] = {}, to[4] = {}; BhRamp r; };
static BhStat bh_st[4];
static void bh_stat(const Ctx& c, uint32_t f, int side, float px, float py) {
    static const int S[4] = {0, 3, 1, 2};
    BhStat& z = bh_st[bh_pn];
    uint32_t mask = 0, up = 0;
    for (int i = 0; i < 4; i++) {
        int v = (int)hud_guest(c, 0x089128B4u, f, (uint32_t)S[i], 0xFFFFFFFFu);
        if (v) { mask |= 1u << i; if (v > 0) up |= 1u << i; }
    }
    uint32_t np = (mask & up) | (z.prev & z.pos & ~mask);
    if (mask != z.mask || z.pos != np) {
        z.prev = z.mask; z.mask = mask; z.pos = (mask & up) | (z.prev & z.pos & ~mask); z.st = 0;
        memcpy(z.from, z.to, sizeof z.to);
        for (int i = 0, n = 0; i < 4; i++) { z.to[i] = n * 24; if (mask & (1u << i)) n++; }
        static int logs = 0;
        if (logs++ < 30) printf("hud bt3: iconos lado %d mascara %X arriba %X\n", side, mask, up);
    }
    uint32_t vb = psp_l32(0x08A78E14u);
    float dt = z.vb && !bp_open ? std::min(0.1f, (vb - z.vb) / 60.0f) : 0;
    z.vb = vb;
    uint32_t rem = (z.mask ^ z.prev) & z.prev, add = (z.mask ^ z.prev) & z.mask, kept = z.mask & z.prev;
    if (z.st == 0) z.st = rem ? 0 : add ? 2 : -1;
    uint32_t show = z.mask, fade = 0;
    float k = 1, a = 1;
    bool moving = false;
    switch (z.st) {
    case 0: z.r.start(0.2f, 1, 0); z.st = 1;   // fall through
    case 1: show = z.prev; fade = rem; a = z.r.v; if (z.r.step(dt)) z.st = 2; break;
    case 2: z.r.start(0.3f, 0, 1); z.st = 3;   // fall through
    case 3: show = kept; moving = true; k = 1 - powf(1 - z.r.v, 3); if (z.r.step(dt)) z.st = add ? 4 : -1; break;
    case 4: z.r.start(0.2f, 0, 1); z.st = 5;   // fall through
    case 5: fade = add; a = z.r.v; if (z.r.step(dt)) z.st = -1; break;
    }
    const BhTex* fr = bh_get("gauge_15");
    static const char* T[4] = {"gauge_19", "gauge_20", "gauge_21", "gauge_22"};
    for (int i = 0; i < 4; i++) {
        if (!(show & (1u << i))) continue;
        float x = z.st == 1 ? (float)z.from[i] : moving ? z.from[i] + (z.to[i] - z.from[i]) * k : (float)z.to[i];
        float al = fade & (1u << i) ? a : 1;
        uint32_t A = (uint32_t)(std::max(0.0f, std::min(1.0f, al)) * 255) << 24;
        if (fr) bh_spr(fr, 0, 42, 22, 44, px + 102 + x, px + 144 + x, py + 24, py + 46, side, A | 0xFFFFFF);
        if (const BhTex* t = bh_get(T[i])) bh_spr(t, 0, (float)t->w, 0, (float)t->h, px + 107 + x, px + 107 + x + t->w, py + 19, py + 19 + t->h, side, A | (z.pos & (1u << i) ? 0xFFFFFF : 0x808080), false, true);
    }
}

// one fighter's panel (gauge part of the spec, sections 3.1 to 3.3): side 0 left, 1 right
static void bh_panel(const Ctx& c, uint32_t f, int side) {
    HUD_COST("hud bt3: panel");
    int hp = std::max(0, (int)psp_l32(f + 0x1A34)), ki = std::max(0, (int)psp_l32(f + 0x1A40)), sp = (int)psp_l32(f + 0x1A54);
    bool spk = psp_l8(f + 0xDDB) != 0;
    int bl = std::max(0, cl_energy(c, f));
    bh_step(bh_pn, hp, ki, spk);
    int trail = std::max(hp, bh_hp_shown[bh_pn]), kis = std::max(ki, bh_ki_shown[bh_pn]);
    const float px = -2, py = 8;
    const float E = BH_EX;
    if (auto* t = bh_getk("gauge_06")) {
        bh_spr(t, 0, 210, 0, 27, px, px + 210, py, py + 27, side);
        bh_spr(t, 210, 220, 0, 27, px + 210, px + 220 + E, py, py + 27, side);
        bh_spr(t, 220, (float)t->w, 0, 27, px + 220 + E, px + t->w + E, py, py + 27, side);
        bh_spr(t, 0, (float)t->w, 27, (float)t->h, px, px + t->w, py + 27, py + t->h, side);
    }
    bh_end();
    bh_face(c, bh_cur_bar, side);
    bh_begin();
    int b, r, b2, r2;
    bh_split(trail, &b, &r); bh_split(hp, &b2, &r2);
    const float hx = px + 69, hy = py + 9;
    auto lay = [&](int sub, int w, uint32_t col = 0xFFFFFFFFu) {
        static const float U[4] = {0, 64, 144, 160};
        const float X[4] = {0, 64, 144 + E, 160 + E};
        for (int k = 0; k < 3; k++) {
            if (w <= U[k]) break;
            float u1 = std::min((float)w, U[k + 1]), x1 = X[k] + (u1 - U[k]) * (X[k + 1] - X[k]) / (U[k + 1] - U[k]);
            bh_spr(bh_strip(sub), U[k], u1, 0, 16, hx + X[k], hx + x1, hy, hy + 16, side, col);
        }
    };
    if (b2 > 0) lay(bh_hp_sub(b2 - 1), 160, 0xFF9E9E9Eu);   // next bar down shown darker (x0.62, measured on BT3)
    if (trail != hp) lay(2, b > b2 ? 160 : r * 160 / 10000);
    if (hp > 0) lay(bh_hp_sub(b2), std::max(3, r2 * 160 / 10000));
    for (int i = 0; i < 6 && i < b2; i++) bh_spr(bh_strip(1), 0, 16, 16, 32, px + 73 + 9 * i, px + 89 + 9 * i, py + 2, py + 18, side);
    const float kx = px + 64, ky = py + 27;
    auto kibar = [&](int sub, int v, int unit) {
        for (int i = 0; i < 5; i++) {
            if (v <= i * unit) continue;
            int n = v >= (i + 1) * unit ? 17 : (v - i * unit) * 17 / unit;
            bh_spr(bh_strip(sub), 16, 30, (float)(33 - n), 33, kx + 8 * i, kx + 8 * i + 14, ky + 17 - n, ky + 17, side);
        }
    };
    kibar(2, kis, 20000);
    kibar(1, ki, 20000);
    if (sp > 0) kibar(5, sp, 6000);
    {   // ki lamps (HudGauge_UpdateKiLamps): a gained bar flashes its lamp, all five full pulse, max power lamps over them
        static BhRamp fl[4], pu[4]; static int bars0[4] = {-1, -1, -1, -1}; static uint32_t lvb[4] = {0, 0, 0, 0};
        const int side_ = side, side = bh_pn; (void)side_;
        uint32_t vb = psp_l32(0x08A78E14u);
        float dt = lvb[side] && !bp_open ? std::min(0.1f, (vb - lvb[side]) / 60.0f) : 0;
        lvb[side] = vb;
        int bars = ki / 20000;
        if (bars0[side] >= 0 && bars > bars0[side]) fl[side].start(1, 1, 0);
        bars0[side] = bars;
        float la[5] = {0, 0, 0, 0, 0};
        if (fl[side].step(dt)) {
            if (pu[side].step(dt)) pu[side].start(1, pu[side].v == 0 ? 0.0f : 1.0f, pu[side].v == 0 ? 1.0f : 0.0f);
            if (ki > 99999) for (float& a : la) a = pu[side].v;
        }
        if (ki <= 99999 && bars >= 1 && bars <= 5) la[bars - 1] = fl[side].v;
        if (auto* t = bh_getk("gauge_17")) for (int i = 0; i < 5; i++) {
            bool mp = sp >= (i + 1) * 6000;
            float a = mp ? 0.8f * pu[side].v : la[i];
            if (a > 0.004f) bh_spr(t, mp ? 32.f : 0.f, mp ? 51.f : 19.f, 0, 22, kx - 3 + 8 * i, kx + 16 + 8 * i, ky - 3, ky + 19, side_, (uint32_t)(std::min(1.0f, a) * 255) << 24 | 0xFFFFFF);
        }
    }
    if (bh_aura[bh_pn] > 0) if (auto* t = bh_getk("gauge_16")) for (int i = 0; i < 9; i++) {
        int cell = bp_open ? i : bh_rand(10);
        float x0 = -2 + (i % 5) * 46 * (256 + E) / 256 + (bp_open ? 0 : bh_rand(11) - 5), y0 = 8 + (i / 5) * 20 - 8 + (bp_open ? 0 : bh_rand(11) - 5);
        float u0 = (float)(cell % 4 * 64), v0 = (float)(cell / 4 * 64), cy0 = std::max(y0, py), cy1 = std::min(y0 + 64, py + 64);
        if (cy1 > cy0) bh_spr(t, u0, u0 + 64, v0 + (cy0 - y0), v0 + (cy1 - y0), x0, x0 + 64, cy0, cy1, side, (uint32_t)(bh_aura[bh_pn] * 2 - 1) << 24 | 0xFFFFFF, true);
    }
    const float bx = px + 29, by = py + 46;
    int n = std::min(7, bl / 100000);
    bool top = bh_oy == 0;   // the stock bar only on the top panels; the digit (team's) on every panel, the frame has its socket
    if (top && bl < 500000) {
        if (auto* t = bh_getk("gauge_15")) bh_spr(t, 0, 48, 0, 12, px + 27, px + 75, py + 46, py + 58, side);
        int fl = (bl % 100000) * 25 / 100000;
        if (fl > 0) bh_spr(bh_strip(1), 161, (float)(161 + fl), 0, 6, bx + 5, bx + 5 + fl, by + 3, by + 9, side);
    }
    bh_stat(c, f, side, px, py);
    if (auto* t = bh_getk("gauge_14")) bh_spr(t, (float)(n % 4 * 32), (float)(n % 4 * 32 + 32), (float)(n / 4 * 32), (float)(n / 4 * 32 + 32), bx - 22, bx + 10, by - 16, by + 16, side, 0xFFFFFFFFu, false, true);
    {
        static BhRamp gf[4]; static int n0[4] = {-1, -1, -1, -1}; static uint32_t gvb[4] = {0, 0, 0, 0};
        const int pn = bh_pn;
        uint32_t vb = psp_l32(0x08A78E14u);
        float dt = gvb[pn] && !bp_open ? std::min(0.1f, (vb - gvb[pn]) / 60.0f) : 0;
        gvb[pn] = vb;
        if (n0[pn] >= 0 && n > n0[pn]) gf[pn].start(1, 1, 0);
        n0[pn] = n;
        if (gf[pn].step(dt)) {
            if (bl >= 500000) gf[pn].start(1, gf[pn].v == 0 ? 0.0f : 1.0f, gf[pn].v == 0 ? 1.0f : 0.0f);
            else if (gf[pn].v == 1) gf[pn].start(1, 1, 0);
        }
        if (gf[pn].v > 0.004f) if (auto* t = bh_getk("gauge_18"))
            bh_spr(t, (float)(n % 4 * 32), (float)(n % 4 * 32 + 32), (float)(n / 4 * 32), (float)(n / 4 * 32 + 32), bx - 22, bx + 10, by - 16, by + 16, side, (uint32_t)(std::min(1.0f, gf[pn].v) * 255) << 24 | 0xFFFFFF, false, true);
    }
    static int logs = 0;
    if (logs++ < 6) printf("hud bt3: panel %d vida %d/%d ki %d naranja %d (%s) energia %d\n", side, hp, (int)psp_l32(f + 0x1A38), ki, sp, spk ? "sparking" : "normal", bl);
}

// senzu of each team under its blast digit: drawn by the port (cp_inst) with the game's holders, slots and digit object
static float bh_sz_edge[2] = {-1, -1};   // right end (left side) / left end (right side) of the senzu group, for the names
static int bh_sz_rect(uint32_t sl, int* r) {
    if (!guest_ptr(sl)) return 0;
    for (int k = 0; k < 4; k++) r[k] = (int)psp_l32(sl + 0xE4 + 4 * k);
    return r[2] > 0 && r[3] > 0;
}
// as the game's own draw: position = slot rect, int offset kept apart (children follow the parent the same way), forced draw flag
static void bh_sz_piece(const Ctx& c, uint32_t in, uint32_t sl, int ox, int oy, float sx, float sy, float* x0, float* x1) {
    int r[4];
    if (!in || !bh_sz_rect(sl, r)) return;
    float al = 1;   // the root slot of the BT3 layout fades to 0 after the intro: its colour is not used
    uint8_t keep[0x54];
    memcpy(keep, psp_p(in), sizeof(keep));
    FReg f;
    psp_s32(in, (uint32_t)r[0]); psp_s32(in + 4, (uint32_t)r[1]);
    psp_s32(in + 8, (uint32_t)ox); psp_s32(in + 0xC, (uint32_t)oy);
    f.f = 1; psp_s32(in + 0x10, f.u); psp_s32(in + 0x14, f.u); psp_s32(in + 0x18, f.u);
    f.f = al; psp_s32(in + 0x1C, f.u);
    f.f = sx; psp_s32(in + 0x20, f.u);
    f.f = sy; psp_s32(in + 0x24, f.u);
    psp_s32(in + 0x34, 0); psp_s8(in + 0x4C, 1); psp_s32(in + 0x50, 0);
    hud_guest(c, 0x089D1B94u, in);
    memcpy(psp_p(in), keep, sizeof(keep));
    float x = (float)(r[0] + ox);
    *x0 = std::min(*x0, x); *x1 = std::max(*x1, x + r[2] * sx);
}
static bool bh_senzu(const Ctx& c, uint32_t o, int idx) {
    if (!bh_on || !o) return false;
    if (bh_vpass >= 0) return true;
    int side = idx ? 1 : 0, dx = idx ? 57 : -51, dy = -6, st = (int)psp_l32(o + 0x48);
    if (bh_sz) { uint32_t d = psp_l32(o + 0x84); sz_senzu[side] = st == -1 || !d ? -1 : (int)psp_l32(d + 0x30); return true; }   // SZ: the count in the panel
    bh_sz_edge[side] = -1;
    if (st == -1 || (unsigned)(bh_phase() - 3) > 2) return true;
    uint32_t hi = psp_l32(o + 0x7C), hf = psp_l32(o + 0x80), d = psp_l32(o + 0x84);
    FReg sx, sy; sx.u = hi ? psp_l32(hi + 0x20) : 0; sy.u = hi ? psp_l32(hi + 0x24) : 0;
    if (!(sx.f > 0.1f && sx.f < 4)) sx.f = 1;
    if (!(sy.f > 0.1f && sy.f < 4)) sy.f = 1;
    float x0 = 1e9f, x1 = -1e9f;
    if (hi && psp_l8(hi + 0x15)) bh_sz_piece(c, psp_l32(hi), psp_l32(hi + 8), dx, dy, sx.f, sy.f, &x0, &x1);
    if (hf && psp_l8(hf + 0x15)) bh_sz_piece(c, psp_l32(hf), psp_l32(hf + 8), 0, 0, sx.f, sy.f, &x0, &x1);   // children: their rects already follow the icon
    int v = -1, nd = 0;
    uint32_t sa = d ? psp_l32(d + 0x28) : 0;
    int cnt = d ? (int)psp_l32(d + 0x2C) : 0;
    if (guest_ptr(sa) && cnt >= 1 && cnt <= 4) {
        v = (int)psp_l32(d + 0x30);
        nd = 1;
        if (psp_l8(d + 0x34)) nd = cnt;
        else for (int i = 0, p = 1; i < cnt; i++, p *= 10) if (v >= 0 && (v / p) % 10) nd = i + 1;
        nd = std::min(nd, cnt);
        for (int i = 0, p = 1; i < nd; i++, p *= 10) {
            int dg = v < 0 ? 0 : (v / p) % 10;
            bh_sz_piece(c, psp_l32(d + 4 * (uint32_t)dg), psp_l32(sa + 4 * (uint32_t)i), 0, 0, sx.f, sy.f, &x0, &x1);
        }
    }
    if (x1 > x0) bh_sz_edge[side] = side ? x0 : x1;
    return true;
}

// HUD walk: the enemy arrow (draw 0x089D9738) goes away with the BT3 bars (the timer sits there); census of drawn classes
// BT3 notices (spec 5, hud_d_b.c): node at the screen centre (256, 224); the band is the word's mask (clipped here as geometry)
struct BhPt { float x, y, u, v; };
static void bh_fan(const BhTex* t, const BhPt* p, int n, uint32_t col, bool add) {
    if (!t || n < 3) return;
    if (t->a != bh_cur_tex) {
        bh_w(0xA0, t->a); bh_w(0xA8, ((t->a >> 24) & 0xF) << 16 | (uint32_t)t->w);
        bh_w(0xB8, (uint32_t)(__builtin_ctz(t->h) << 8 | __builtin_ctz(t->w))); bh_w(0xCB, 0);
        bh_cur_tex = t->a;
    }
    if ((int)add != bh_cur_add) { bh_w(0xDF, add ? 2u | (10u << 4) : 2u | (3u << 4)); if (add) bh_w(0xE1, 0xFFFFFF); bh_cur_add = add; }
    n = std::min(n, 12);
    uint32_t v = bh_vbuf + bh_vpos;
    bh_vpos = (bh_vpos + 24 * 12) % (4 * 0x8000);
    for (int k = 0; k < n; k++) {
        uint32_t b = v + 24 * k;
        float f[6] = {p[k].u, p[k].v, 0, 240 + (p[k].x - 256) * BH_SX, p[k].y * BH_SY, 0};
        memcpy(psp_p(b), &f[0], 8); psp_s32(b + 8, col); memcpy(psp_p(b + 12), &f[3], 12);
    }
    bh_w(0x10, ((v >> 24) & 0xF) << 16); bh_w(0x01, v); bh_w(0x04, (5u << 16) | (uint32_t)n);
}
// the word (w x h centred at (cx, cy), scale k) clipped by the band (bw x bh centred at the node, turned by rot)
static void bh_word(const BhTex* t, float cx, float cy, float k, uint32_t col, bool add, float bw, float bh, float rot, float v0 = 0, float v1 = -1, float bx = 0) {
    if (!t) return;
    if (v1 < 0) v1 = (float)t->h;
    float hw = t->w * 0.5f * k, hh = (v1 - v0) * 0.5f * k;
    BhPt a[12] = {{cx - hw, cy - hh, 0, v0}, {cx + hw, cy - hh, (float)t->w, v0}, {cx + hw, cy + hh, (float)t->w, v1}, {cx - hw, cy + hh, 0, v1}}, o[12];
    int n = 4;
    if (bw > 0) {
        float cs = std::cos(rot), sn = std::sin(rot), ax[2] = {cs, sn}, ay[2] = {-sn, cs}, ex[2] = {bw * 0.5f, bh * 0.5f};
        for (int e = 0; e < 4 && n >= 3; e++) {
            const float* d = e < 2 ? ax : ay;
            float lim = ex[e / 2], sg = e % 2 ? -1.0f : 1.0f;
            auto in = [&](const BhPt& q) { return lim - sg * ((q.x - 256 - bx) * d[0] + (q.y - 224) * d[1]); };
            int m = 0;
            for (int i = 0; i < n && m < 11; i++) {
                const BhPt& p0 = a[i]; const BhPt& p1 = a[(i + 1) % n];
                float d0 = in(p0), d1 = in(p1);
                if (d0 >= 0) o[m++] = p0;
                if ((d0 >= 0) != (d1 >= 0) && m < 12) { float r = d0 / (d0 - d1); o[m++] = {p0.x + (p1.x - p0.x) * r, p0.y + (p1.y - p0.y) * r, p0.u + (p1.u - p0.u) * r, p0.v + (p1.v - p0.v) * r}; }
            }
            n = m; memcpy(a, o, sizeof(BhPt) * n);
        }
    }
    bh_fan(t, a, n, col, add);
}
static const short BH_PART[12][2] = {{40, -70}, {190, -40}, {190, 40}, {90, 90}, {-109, -61}, {-160, -20}, {-210, 10}, {-170, 60}, {30, -60}, {160, -30}, {-150, 30}, {-100, 60}};
struct BhNotice { int id = -1, st = 0, streak = 0, last = -2, lastph = -1, next = -1; bool chain = false, fought = false, done = false, ended = false; float wait = 0, tr[8] = {}; BhRamp ra, rb; uint32_t vb = 0; };
static BhNotice bh_nt;
static void bh_nt_reset();
static uint32_t bh_acol(float a) { return (uint32_t)(std::max(0.0f, std::min(1.0f, a)) * 255) << 24 | 0xFFFFFF; }
// seconds left of phase 3's Ready timer (0x089098D0 sub-state 1, 2.0 s, value at block +0x18); Fight and its sound come at 0
static float bh_ready_left() {
    uint32_t m = psp_l32(0x08A7D880u);
    if (!guest_ptr(m) || psp_l32(m) != 3) return -1;
    uint32_t b = m + 4;   // the phase block is the manager's +4 (sub-state +8, timer +0xC, its value +0x1C)
    if (psp_l32(b + 4) != 1) return -1;
    FReg v; v.u = psp_l32(b + 0x18);
    return v.f >= 0 && v.f <= 1 ? 2.0f * (1 - v.f) : -1;
}
// a word (rows v0..v1 of t) centred at (cx, cy), scaled by k and turned by rot
static void bh_quad_rot(const BhTex* t, float v0, float v1, float cx, float cy, float k, float rot, uint32_t col, bool add) {
    if (!t) return;
    float hw = t->w * 0.5f * k, hh = (v1 - v0) * 0.5f * k, cs = std::cos(rot), sn = std::sin(rot);
    const float X[4] = {-hw, hw, hw, -hw}, Y[4] = {-hh, -hh, hh, hh}, U[4] = {0, (float)t->w, (float)t->w, 0}, V[4] = {v0, v0, v1, v1};
    BhPt q[4];
    for (int i = 0; i < 4; i++) q[i] = {cx + X[i] * cs - Y[i] * sn, cy + X[i] * sn + Y[i] * cs, U[i], V[i]};
    bh_fan(t, q, 4, col, add);
}
// lightning streak (tex 7, 512x42 rows) across the node at cy, moved by dx, colour in PS2 units (0x80 = 1)
static void bh_streak(BhNotice& n, float cy, float dx, float r, float g, float b, float dt) {
    const BhTex* sk = bh_get("notice_07");
    if (!sk) return;
    auto c8 = [](float v) { return (uint32_t)std::min(255.0f, std::max(0.0f, v * 2)); };
    float v0 = (float)(n.streak * 42);
    BhPt q[4] = {{dx, cy - 21, 0, v0}, {512 + dx, cy - 21, 512, v0}, {512 + dx, cy + 21, 512, v0 + 42}, {dx, cy + 21, 0, v0 + 42}};
    bh_fan(sk, q, 4, 0xFF000000u | c8(b) << 16 | c8(g) << 8 | c8(r), true);
    if (dt > 0) n.streak = (n.streak + 1) % 3;
}
// battle end notices (spec 5, hud_d_b.c / hud_e.c): 2 K.O., 3 Perfect, 5 Time (BtlSeqFinish_Enter), 6 Winner, 7 Lose (BtlSeqEnd)
static void bh_end_draw(float dt) {
    BhNotice& n = bh_nt;
    const float PI = 3.14159265f;
    const BhTex* w = bh_get(n.id == 2 ? "notice_02" : n.id == 3 ? "notice_03" : n.id == 5 ? "notice_04" : n.id == 6 ? "notice_05" : "notice_06");
    if (!w) { n.id = -1; return; }
    const float cx = 256, cy = n.id >= 6 ? 352 : 224;
    bh_begin();
    if (n.id == 2) {   // HudNotice_UpdateKo
        float bh = 448;
        switch (n.st) {
        case 0: for (float& t : n.tr) t = 0; n.rb.start(0.15f, 0, 0.5f); n.st = 1;   // fall through
        case 1: case 2: {
            float sv = std::sin(n.rb.v * PI);
            if (dt > 0) { for (int i = 3; i > 0; i--) n.tr[i] = n.tr[i - 1]; n.tr[0] = sv; }
            for (int i = 1; i < 4; i++) if (n.tr[i] != n.tr[0]) bh_word(w, cx, cy, 1 + 0.4f * n.tr[i], bh_acol(std::min(0.375f, n.rb.v * 2)), false, 0, 0, 0);
            float k = 1 + 0.4f * sv, a = std::min(1.0f, n.rb.v * 2);
            bh_word(w, cx, cy, k, bh_acol(a), false, 0, 0, 0);
            bh_word(w, cx, cy, k, bh_acol(a), true, 0, 0, 0);
            if (n.rb.step(dt)) { if (n.st == 1) { n.rb.start(0.4f, 0.5f, 1); n.st = 2; } else { n.rb.start(0.2f, 0, 1); n.st = 3; } }
            break;
        }
        case 3: if (n.rb.step(dt)) { n.rb.start(0.55f, 0, 1); n.st = 4; } break;
        case 4: if (n.rb.step(dt)) { n.rb.start(0.4f, 1, 0); n.st = 5; } break;
        case 5: if (n.rb.step(dt)) { n.rb.start(0.2f, 1, 0); n.st = 6; } break;
        case 6: bh = n.rb.v * 64; if (n.rb.step(dt)) n.st = 7; break;
        default: n.id = -1; break;
        }
        if (n.id == 2 && n.st >= 3) {
            bh_word(w, cx, cy, 1, 0xFFFFFFFFu, false, 512, bh, 0);
            bh_word(w, cx, cy, 1, 0xFFFFFFFFu, true, 512, bh, 0);
            if (n.st == 3) bh_streak(n, cy, -(1 - n.rb.v) * 512, 0xFF, 0x60, 0x60, dt);
            if (n.st == 4) bh_streak(n, cy, (float)(bh_rand(33) - 16), 0xFF, 0x60, 0x60, dt);
        }
    } else if (n.id == 3) {   // HudNotice_UpdateBandA: the word (rows 64..128) masked by a band that slides in, opens, closes, slides out
        float bw = 512, bh = 4, bx = 0, ck = 0, ca = 0;
        switch (n.st) {
        case 0: n.ra.start(0.24f, 0, 1); n.st = 1;   // fall through
        case 1: bx = -(1 - n.ra.v) * 512; if (n.ra.step(dt)) { n.ra.start(0.24f, 0, 1); n.st = 2; } break;
        case 2: bh = n.ra.v * 60 + 4; if (n.ra.step(dt)) { n.ra.start(0.25f, 0, 1); n.st = 3; } break;
        case 3: bh = 64; if (n.ra.step(dt)) { n.ra.start(0.64f, 1, 0); n.st = 4; } break;
        case 4: { bh = 64; float c3 = std::pow(n.ra.v, 3.0f); ca = c3; ck = 2 - c3; if (n.ra.step(dt)) { n.ra.start(0.24f, 1, 0); n.st = 6; } break; }
        case 6: bh = n.ra.v * 60 + 4; if (n.ra.step(dt)) { n.ra.start(0.24f, 0, 1); n.st = 7; } break;
        case 7: bx = n.ra.v * 512; if (n.ra.step(dt)) n.st = 8; break;
        default: n.id = -1; break;
        }
        if (n.id == 3) {
            bh_word(w, cx, cy, 1, 0xFFFFFFFFu, false, bw, bh, 0, 64, 128, bx);
            if (ck > 0) bh_word(w, cx, cy, ck, bh_acol(ca), false, 0, 0, 0, 64, 128);
        }
    } else if (n.id == 5) {   // HudNotice_UpdateSpinB: rows 64..128 of tex 4, added
        float k = 1, a = 1, rot = 0;
        switch (n.st) {
        case 0: n.ra.start(0.83f, 1, 0); n.st = 1;   // fall through
        case 1: { float c3 = std::pow(n.ra.v, 3.0f); rot = c3 * 1.5f * 2 * PI; while (rot > PI) rot -= 2 * PI; a = 1 - c3; k = c3 + 1; if (n.ra.step(dt)) { n.ra.start(1, 0, 1); n.st = 2; } break; }
        case 2: if (n.ra.step(dt)) { n.ra.start(0.23f, 1, 0); n.st = 3; } break;
        case 3: { float c3 = std::pow(n.ra.v, 3.0f); a = c3; k = 2 - c3; if (n.ra.step(dt)) n.st = 4; break; }
        default: n.id = -1; break;
        }
        if (n.id == 5) { bh_quad_rot(w, 64, 128, cx, cy, k, rot, bh_acol(a), false); bh_quad_rot(w, 64, 128, cx, cy, k, rot, bh_acol(a * 0.5f), true); }
    } else if (n.id == 6) {   // HudNotice_UpdateTrail: eight copies zoom in one frame apart, then the streak, then a swell out
        bool word = false;
        float wk = 1, wa = 1;
        switch (n.st) {
        case 0: for (float& t : n.tr) t = 1; n.ra.start(0.93f, 1, 0); n.st = 1;   // fall through
        case 1: {
            bool done = n.ra.step(dt);
            if (dt > 0) { for (int i = 7; i > 0; i--) n.tr[i] = n.tr[i - 1]; n.tr[0] = n.ra.v; }
            for (int i = 0; i < 8; i++) {
                if (i > 0 && n.tr[i] == n.tr[0]) continue;
                float c = std::pow(1 - n.tr[i], 3.0f), k = 2 - c;
                bh_word(w, cx, cy, k, bh_acol(c), false, 0, 0, 0);
            }
            if (done && n.tr[0] == n.tr[7]) { n.ra.start(0.35f, 0, 1); n.st = 2; }
            break;
        }
        case 2: word = true; if (n.ra.step(dt)) { n.ra.start(0.8f, 1, 0); n.st = 3; } break;
        case 3: word = true; if (n.ra.step(dt)) { n.ra.start(0.35f, 1, 0); n.st = 4; } break;
        case 4: word = true; if (n.ra.step(dt)) { n.ra.start(0.3f, 1, 0); n.st = 5; } break;
        case 5: word = true; if (n.ra.step(dt)) { n.ra.start(0.3f, 1, 0); n.st = 6; } break;
        case 6: { word = true; float c3 = std::pow(n.ra.v, 3.0f); wa = c3; wk = (1 - c3) * 0.3f + 1; if (n.ra.step(dt)) n.st = 7; break; }
        default: n.id = -1; break;
        }
        if (n.id == 6 && word) {
            bh_word(w, cx, cy, wk, bh_acol(wa), false, 0, 0, 0);   // the decomp's second pass writes only the alpha plane (FRAME_1 mask)
            if (n.st == 2) bh_streak(n, cy, -(1 - n.ra.v) * 512, 0xC0, 0x70, 0x60, dt);
            if (n.st == 3) bh_streak(n, cy, (float)(bh_rand(33) - 16), 0xC0, 0x70, 0x60, dt);
        }
    } else {   // 7, HudNotice_UpdateBanner: drops from 100 px above, bounces 10 px, holds 1.73 s, shrinks and fades
        float dy = 0, a = 1, k = 1;
        switch (n.st) {
        case 0: n.ra.start(0.4f, 1, 0); n.st = 1;   // fall through
        case 1: { float t = std::sin(n.ra.v * PI * 0.5f); dy = -100 * t; a = 1 - t; if (n.ra.step(dt)) { n.ra.start(0.36f, 0, 1); n.st = 2; } break; }
        case 2: { float t = std::sin(n.ra.v * PI) * 0.1f; dy = -100 * t; a = 1 - t; if (n.ra.step(dt)) { n.ra.start(1.73f, 1, 0); n.st = 3; } break; }
        case 3: if (n.ra.step(dt)) { n.ra.start(0.4f, 1, 0); n.st = 4; } break;
        case 4: { float c3 = std::pow(n.ra.v, 3.0f); a = c3; k = c3 * 0.5f + 0.5f; if (n.ra.step(dt)) n.st = 5; break; }
        default: n.id = -1; break;
        }
        if (n.id == 7) bh_word(w, cx, cy + dy, k, bh_acol(a), false, 0, 0, 0);
    }
    bh_end();
}
static void bh_notice_draw(float dt) {
    BhNotice& n = bh_nt;
    if (n.id >= 2) { bh_end_draw(dt); return; }
    const BhTex* w = bh_get(n.id == 0 ? "notice_00" : "notice_01");   // Ready / Fight
    if (n.id < 0 || !w) return;
    float t3 = 0;
    bh_begin();
    if (n.id == 0) {
        float bw = 0, bhh = 4, rot = 0, k = 1, wa = 1;
        switch (n.st) {
        case 0: n.ra.start(0.2f, 0, 1); n.st = 1;   // fall through
        case 1: if (n.ra.step(dt)) { n.ra.start(0.36f, 0, 1); n.st = 2; } bw = 32 + 480 * n.ra.v; break;
        case 2: if (n.ra.step(dt)) {   // hold shortened so that Fight's 0.43 s zoom ends on the game's Fight (sound)
                float left = bh_ready_left(), hold = left > 0 ? std::max(0.0f, left - 0.86f) : 1.1f;
                n.chain = left > 0;
                n.rb.start(hold, 1, 0); n.st = 3;
                static int logs = 0;
                if (logs++ < 4) printf("hud bt3: preparado pausa %.2f s (faltan %.2f s para A luchar)\n", hold, left);
            } bw = 512; bhh = 4 + 124 * n.ra.v; rot = -n.ra.v * 3.14159265f; break;
        case 3: if (n.rb.step(dt)) { n.rb.start(0.43f, 1, 0); n.st = 4; } bw = 512; bhh = 128; rot = -3.14159265f; break;
        case 4: {
            bool done = n.rb.step(dt);
            t3 = std::pow(n.rb.v, 3.0f); k = (1 - t3) * 0.5f + 1; wa = t3;
            bw = 512 * k; bhh = 128 * k; rot = 0;
            for (int i = 0; i < 12; i++) {
                float a = n.rb.v - (i < 6 ? i : 11 - i) / 30.0f;
                a = std::pow(std::max(0.0f, std::min(1.0f, a)), 3.0f) * 0.5f;
                bh_word(w, 256 + BH_PART[i][0] * (1 - t3), 224 + BH_PART[i][1] * (1 - t3), 1, bh_acol(a), false, 0, 0, 0);
            }
            if (done) n.st = 5;
            break;
        }
        default: if (n.chain) { n.id = 1; n.st = 0; n.fought = n.done = true; } else n.id = -1; break;
        }
        if (n.id == 1) { bh_end(); bh_notice_draw(dt); return; }
        if (n.id == 0) bh_word(w, 256, 224, k, bh_acol(wa), false, bw, bhh, rot);
    } else {
        float k = 1, wa = 1, bhh = 448;
        switch (n.st) {
        case 0: n.rb.start(0.43f, 0, 1); n.st = 1;   // fall through
        case 1: {
            bool done = n.rb.step(dt);
            t3 = std::pow(n.rb.v, 3.0f); k = (1 - t3) * 0.5f + 1; wa = t3;
            for (int i = 0; !done && i < 12; i++) {
                float a = n.rb.v - (i < 6 ? i : 11 - i) / 30.0f;
                a = std::pow(std::max(0.0f, std::min(1.0f, a)), 3.0f) * 0.5f;
                bh_word(w, 256 + BH_PART[i][0] * (1 - t3), 224 + BH_PART[i][1] * (1 - t3), 1, bh_acol(a), false, 0, 0, 0);
            }
            if (done) { k = 1; wa = 1; n.rb.start(0.4f, 0, 1); n.st = 2; }
            break;
        }
        case 2: if (n.rb.step(dt)) { n.rb.start(0.45f, 0, 1); n.st = 3; } break;
        case 3: if (n.rb.step(dt)) { n.rb.start(0.2f, 1, 0); n.st = 4; } break;
        case 4: if (n.rb.step(dt)) { n.rb.start(0.2f, 1, 0); n.st = 5; } break;
        case 5: if (n.rb.step(dt)) n.st = 6; bhh = n.rb.v * 64; break;
        default: n.id = -1; break;
        }
        if (n.id == 1) {
            bh_word(w, 256, 224, k, bh_acol(wa), false, 512, bhh, 0);
            const BhTex* sk = bh_get("notice_07");
            if (sk && n.st >= 2 && n.st <= 4) {
                float dx = n.st == 2 ? -(1 - n.rb.v) * 512 : n.st == 3 ? (float)(bh_rand(33) - 16) : (1 - n.rb.v) * 512, c = n.st == 4 ? n.rb.v : 1.0f;
                float v0 = (float)(n.streak * 42);
                uint32_t col = 0xFF000000u | (uint32_t)(0x60 * 2 * c) << 16 | (uint32_t)(0x70 * 2 * c) << 8 | (uint32_t)(std::min(255.0f, 0xC0 * 2 * c));
                BhPt q[4] = {{dx, 203, 0, v0}, {512 + dx, 203, 512, v0}, {512 + dx, 245, 512, v0 + 42}, {dx, 245, 0, v0 + 42}};
                bh_word(w, 256, 224, 1, 0x80FFFFFFu, true, 512, bhh, 0);
                bh_fan(sk, q, 4, col, true);
                if (dt > 0) n.streak = (n.streak + 1) % 3;
            }
        }
    }
    bh_end();
}
// battle_text (AGS 14): the game sets its text index (+0x70) through container vt+0xA0 (0x089DB550) and opens it.
// 0 Ready (phase 3), finish phase 5 (0x08909E9C): 8 K.O., 5 perfect, 3 time up; result (0x08906D9C..): 1 win, 2 lose, 6 draw
static int bh_bt_map(int idx) { return idx == 0 ? 0 : idx == 8 ? 2 : idx == 5 ? 3 : idx == 3 ? 5 : idx == 1 ? 6 : idx == 2 ? 7 : -1; }
static int bh_bt_lidx = -1, bh_res = -1;
static float bh_res_a = -1;   // last result text of the battle: 1 win, 2 lose, 6 draw
static bool bh_notice_node(uint32_t node) {
    int st = (int)psp_l32(node + 0x48), idx = (int)psp_l32(node + 0x70), id = bh_bt_map(idx);
    if (bh_sz) return id >= 0;   // SZ: battles start and end without notices
    bool opened = st >= 0 && (bh_nt.last < 0 || idx != bh_bt_lidx);
    if (opened) {
        static int logs = 0;
        if (logs++ < 40) printf("hud bt3: texto de batalla indice %d -> aviso %d (fase %d)\n", idx, id, bh_phase());
        if (idx == 1 || idx == 2 || idx == 6) bh_res = idx;
        if (id == 0) { bh_nt.id = 0; bh_nt.st = 0; bh_nt.chain = bh_nt.fought = false; }
        else if (id > 0) { bh_nt.id = id; bh_nt.st = 0; }
    } else if (id == 0 && st == 1 && bh_nt.last != 1 && !bh_nt.fought && bh_nt.id < 2) { bh_nt.id = 1; bh_nt.st = 0; bh_nt.fought = true; }
    bh_nt.last = st; bh_bt_lidx = idx;
    return id >= 0;   // texts without a BT3 notice (draw, senzu...) keep the TTT draw
}
// once a frame from the right panel's draw (always walked), so a notice outlives the TTT text node
static void bh_nt_reset() { bh_nt = BhNotice(); bh_bt_lidx = -1; bh_res = -1; bh_res_a = -1; }
static void bh_notice_frame() {
    uint32_t vb = psp_l32(0x08A78E14u);
    if (vb == bh_nt.vb) return;
    float dt = bh_nt.vb && !bp_open ? std::min(0.1f, (vb - bh_nt.vb) / 60.0f) : 0;
    bh_nt.vb = vb;
    bh_notice_draw(dt);
}

// screen box and alpha of the through-mode primitives written into the GE list between p0 and p1
static const float BH_RES_X = 8;   // screen units from the left of the TTT title box
static bool bh_ge_box(uint32_t p0, uint32_t p1, float* bx, float* amax, int* np) {
    uint32_t base = 0, va = 0, vt = 0;
    bool have = false;
    bx[0] = bx[1] = 1e9f; bx[2] = bx[3] = -1e9f; *amax = 0; *np = 0;
    for (uint32_t p = p0; p + 4 <= p1 && p - p0 < 0x4000; p += 4) {
        uint32_t w = psp_l32(p), cmd = w >> 24, arg = w & 0xFFFFFF;
        if (cmd == 0x10) base = (arg & 0x0F0000) << 8;
        else if (cmd == 0x01) va = base | arg;
        else if (cmd == 0x12) { vt = arg; have = true; }
        else if (cmd == 0x04 && have) {
            if (!(vt & (1u << 23)) || (vt & (3u << 11))) return false;
            static const int TS[4] = {0, 1, 2, 4}, CS[8] = {0, 0, 0, 0, 2, 2, 2, 4};
            int tc = vt & 3, co = (vt >> 2) & 7, no = (vt >> 5) & 3, po = (vt >> 7) & 3, wt = (vt >> 9) & 3, wc = ((vt >> 14) & 7) + 1;
            int off = 0, al = 1, otc = 0, oco = 0, opo = 0;
            auto put = [&](int sz, int cnt) { if (!sz) return -1; off = (off + sz - 1) / sz * sz; int o = off; off += sz * cnt; al = std::max(al, sz); return o; };
            put(TS[wt], wc); otc = put(TS[tc], 2); oco = put(CS[co], 1); put(TS[no], 3); opo = put(TS[po], 3);
            int stride = (off + al - 1) / al * al, n = (int)(arg & 0xFFFF);
            if (po < 2 || n > 64 || !guest_ptr(va)) return false;
            for (int i = 0; i < n; i++) {
                uint32_t v = va + (uint32_t)(stride * i);
                float x, y;
                if (po == 2) { x = (float)(int16_t)psp_l16(v + opo); y = (float)(int16_t)psp_l16(v + opo + 2); }
                else { FReg a, b; a.u = psp_l32(v + opo); b.u = psp_l32(v + opo + 4); x = a.f; y = b.f; }
                bx[0] = std::min(bx[0], x); bx[1] = std::min(bx[1], y); bx[2] = std::max(bx[2], x); bx[3] = std::max(bx[3], y);
                float a = 1;
                if (co == 7 && oco >= 0) a = psp_l8(v + oco + 3) / 255.0f;
                else if (co == 6 && oco >= 0) a = (psp_l16(v + oco) >> 12) / 15.0f;
                else if (co == 5 && oco >= 0) a = (float)(psp_l16(v + oco) >> 15);
                *amax = std::max(*amax, a);
            }
            (void)otc;
            va += (uint32_t)(stride * n);
            (*np)++;
        }
    }
    return *np > 0 && bx[2] > bx[0] && bx[3] > bx[1];
}
// results screen (class vtable 0x08A74FF0, draw 0x08A1DCD4): the title (holder +0x94, AGS 223 results_text) is measured
// and replaced by the BT3 Winner / Lose word; draw results keep the TTT title
static bool bh_result_wrap(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t node = c.r[4];
    if (!bh_on || bh_sz || !node || (bh_res != 1 && bh_res != 2) || bh_phase() != 7) return false;
    uint32_t vt = psp_l32(node + 0x40);
    if (!guest_ptr(vt) || psp_l32(vt + 0x6C) != 0x08A1DCD4u) return false;
    uint32_t h = psp_l32(node + 0x94), in = h ? psp_l32(h) : 0;
    const BhTex* w = bh_get(bh_res == 1 ? "notice_05" : "notice_06");
    if (!in || !w) return false;
    uint8_t vis = psp_l8(h + 0x15);
    psp_s8(h + 0x15, 0);
    orig(c);
    psp_s8(h + 0x15, vis);
    if (!vis) return true;
    uint32_t ctx = psp_l32(0x08B478D0u);
    if (!guest_ptr(ctx)) return true;
    uint8_t keep[0x54];
    memcpy(keep, psp_p(in), sizeof(keep));
    psp_s32(in + 0x34, psp_l32(h + 8));
    psp_s32(in + 8, psp_l32(h + 0x18)); psp_s32(in + 0xC, psp_l32(h + 0x1C));
    psp_s32(in, psp_l32(h + 0x2C)); psp_s32(in + 4, psp_l32(h + 0x30));
    for (int k = 0; k < 3; k++) psp_s32(in + 0x40 + 4 * k, psp_l32(h + 0x34 + 4 * k));
    psp_s8(in + 0x4C, 1);
    psp_s32(in + 0x20, psp_l32(h + 0x20)); psp_s32(in + 0x24, psp_l32(h + 0x24)); psp_s32(in + 0x28, psp_l32(h + 0x28));
    for (int k = 0; k < 4; k++) psp_s32(in + 0x10 + 4 * k, psp_l32(h + 0x40 + 4 * k));
    psp_s32(in + 0x50, psp_l32(h + 0x50));
    cl_ge(0x00, 0xFB0001u);
    uint32_t p0 = psp_l32(ctx + 8);
    hud_guest(c, 0x089D1B94u, in);
    uint32_t p1 = psp_l32(ctx + 8);
    cl_ge(0x00, 0xFB0000u);
    float b[4], a = 0; int np = 0;
    static float lb[4] = {17, 12, 273, 44};   // TTT title box (measured: AGS 223 sprite, 256x32)
    bool ok = p1 > p0 && bh_ge_box(p0, p1, b, &a, &np);   // only with the vertex type written in this draw
    static int logs = 0;
    if (ok && logs++ < 3) printf("hud bt3: titulo de resultados %s, caja (%.1f, %.1f)-(%.1f, %.1f) alfa %.2f\n", bh_res == 1 ? "victoria" : "derrota", b[0], b[1], b[2], b[3], a);
    if (ok && b[2] - b[0] > 200 && b[2] - b[0] < 300 && b[3] - b[1] > 20 && b[3] - b[1] < 48) memcpy(lb, b, sizeof lb);
    memcpy(psp_p(in), keep, sizeof(keep));
    uint32_t sl = psp_l32(h + 8);
    FReg fa; fa.u = (psp_l32(h + 0x50) & 2) && guest_ptr(sl) ? psp_l32(sl + 0x2C) : psp_l32(h + 0x4C);   // the title's own alpha (slot colour or holder colour)
    static int miss = 0;   // the title closed: nothing drawn for several frames in a row
    miss = p1 > p0 ? 0 : miss + 1;
    if (miss > 8 || bh_phase() != 7) return true;
    if (ok) bh_res_a = a;   // the AGS fade of the title as last measured; kept on frames where it was not measured
    a = bh_res_a >= 0 ? bh_res_a : fa.f >= 0 && fa.f <= 1 ? fa.f : 1;
    if (a < 0.004f) return true;
    memcpy(b, lb, sizeof b);
    static std::map<uint32_t, std::pair<int, int>> wvis;   // visible columns of each BT3 word texture
    auto it = wvis.find(w->a);
    if (it == wvis.end()) {
        int u0 = w->w, u1 = 0;
        for (int y = 0; y < w->h; y++) for (int x = 0; x < w->w; x++) if (psp_l8(w->a + 4 * (uint32_t)(y * w->w + x) + 3) > 16) { u0 = std::min(u0, x); u1 = std::max(u1, x + 1); }
        if (u1 <= u0) { u0 = 0; u1 = w->w; }
        it = wvis.emplace(w->a, std::make_pair(u0, u1)).first;
    }
    float k = std::min((b[3] - b[1]) / (w->h * BH_SY), 1.2f * (b[2] - b[0]) / (w->w * BH_SX));
    float vc = (it->second.first + it->second.second) * 0.5f - w->w * 0.5f, vw = (float)(it->second.second - it->second.first);
    float cx = (b[0] + BH_RES_X + vw * k * BH_SX * 0.5f - 240) / BH_SX + 256 - vc * k, cy = (b[1] + b[3]) * 0.5f / BH_SY;   // visible word left aligned in the title box
    bh_begin();
    bh_word(w, cx, cy, k, bh_acol(a), false, 0, 0, 0);
    bh_end();
    return true;
}

static bool bh_skip_node(uint32_t node) {
    if (!bh_on || !node) return false;
    uint32_t vt = psp_l32(node + 0x40), fn = vt > 0x08000000u && vt < 0x0A000000u ? psp_l32(vt + 0x6C) : 0;
    uint32_t php = psp_l32(0x08A7D880u);
    bool ph5 = php && (int)psp_l32(php) >= 5 && fn != 0x089E5674u && fn != 0x089F4990u && fn != 0x089F2194u && fn != 0x089D5F44u;
    static uint32_t seen[48]; static int ns = 0;
    bool known = false;
    for (int i = 0; i < ns; i++) known |= seen[i] == fn;
    if (!known && ns < 48) { seen[ns++] = fn; printf("hud bt3: nodo %08X dibujo %08X\n", node, fn); }
    if (fn == 0x089D5F44u) for (int i = 0; i < 4; i++) if (uint32_t h = psp_l32(node + 0x70 + 4 * i)) psp_s8(h + 0x15, 0);   // TTT ability icons hidden (BT3 status icons)
    if (bh_lay == 4 && (fn == 0x089D6DE0u || fn == 0x089E9AF0u)) return false;   // 4 views: TTT's per-player counters and texts stay
    if (fn == 0x089E9AF0u) return true;   // TTT combo and damage counters: the BT3 / SZ combo replaces them
    if (fn == 0x089D6DE0u && !bh_sz && (unsigned)psp_l32(node + 0xA0) < 4 && !bh_msg_off()) return true;   // action texts with a BT3 message
    if (fn == 0x089D6DE0u && bh_sz) return true;   // SZ shows no action texts
    if (fn == 0x089DB318u) return bh_notice_node(node);
    if (fn == 0x089DB318u) {   // battle_text (AGS 14): states logged to map the BT3 notices
        static int lk[4] = {-9, -9, -9, -9}, logs = 0;
        int k[4] = {(int)psp_l32(node + 0x48), (int)psp_l32(node + 0x68), (int)psp_l32(node + 0x6C), (int)psp_l32(node + 0x70)};
        if (memcmp(k, lk, sizeof k) && logs++ < 60) {
            memcpy(lk, k, sizeof k);
            uint32_t ph = psp_l32(0x08A7D880u);
            printf("hud bt3: texto de batalla estado %d campos %d %d %d fase %d cuadro %u\n", k[0], k[1], k[2], k[3], ph ? (int)psp_l32(ph) : -1, psp_l32(0x08A78E14u));
        }
    }
    if (fn == 0x089D6DE0u || fn == 0x089EEF80u || ph5) {   // action texts, AGS 77 events and late battle pieces: states for the next notices
        static uint32_t ln[24], ls[24]; static int nn = 0, logs = 0;
        int st = (int)psp_l32(node + 0x48), k = 0;
        while (k < nn && ln[k] != node) k++;
        if (k == nn && nn < 24) { ln[nn] = node; ls[nn++] = 0xFFFFFFF0u; }
        if (k < 24 && ls[k] != (uint32_t)st && logs++ < 80) { ls[k] = (uint32_t)st; uint32_t ph = psp_l32(0x08A7D880u); printf("hud bt3: pieza %08X dibujo %08X estado %d texto %d fase %d cuadro %u\n", node, fn, st, (int)psp_l32(node + 0xA0), ph ? (int)psp_l32(ph) : -1, psp_l32(0x08A78E14u)); }
    }
    return fn == 0x089D9738u;
}

// technique name (BtlText_DrawEntryName): white text, blue outline (0x20, 0x20, 0xFF at half alpha), no band or burst
static bool bh_tw_on() { const char* e = getenv("TTT_HUD_NAME"); return bh_on && !bh_sz && bh_lay != 4 && !(e && atoi(e) == 0); }
static bool bh_tw_band(Ctx& c, void (*orig)(Ctx&)) {
    if (!bh_tw_on() || c.r[16] == cl_obj) return false;   // s0 = the window (the charge list copy keeps its own draw)
    cl_ge(0x00, 0xFB0001u);   // 0x089F2194 draws AGS 129 (band and burst) here: drawn into the list, discarded by the GPU
    orig(c);
    cl_ge(0x00, 0xFB0000u);
    return true;
}
static const float BH_TW_SCALE = 1.25f;
static bool bh_tw_scaled = false;
static struct { float sx, sy, ax, ay; } bh_tw_k;
static void bh_tw_end() {
    if (!bh_tw_scaled) return;
    cl_gu_on = false; cl_sx = bh_tw_k.sx; cl_sy = bh_tw_k.sy; cl_ax = bh_tw_k.ax; cl_ay = bh_tw_k.ay;
    bh_tw_scaled = false;
}
static void bh_tw_style(Ctx& c) {
    if (!bh_tw_on() || psp_l32(c.r[29] + 0x60) == cl_obj) return;   // the caller saved its object at sp + 0x60
    uint32_t st = c.r[29] + 0x40, a = psp_l32(st + 8) >> 24;
    psp_s32(st + 8, a << 24 | 0xFFFFFFu);
    psp_s8(st + 0xC, 1);
    psp_s32(st + 0x18, a << 24 | 0xFF2020u);
    psp_s32(st + 0x1C, 2);   // outline mode of the bar names (nm_draw)
    hud_guest(c, 0x088321ECu, c.r[4], st);
    bh_tw_k = {cl_sx, cl_sy, cl_ax, cl_ay};
    cl_sx = cl_sy = BH_TW_SCALE; cl_ax = (float)(int)c.r[5]; cl_ay = (float)(int)c.r[6]; cl_gu_on = true;   // the draw that follows is scaled
    bh_tw_scaled = true;
    static int logs = 0;
    if (logs++ < 3) printf("hud bt3: nombre de tecnica estilo BT3 (alfa %u)\n", a);
}

// BT3 combo (spec 6, hud_c_b.c): hits and damage of each side, fed by the TTT counters, drawn by the port
// BT3 event messages (spec 6, HudCombo_UpdateMessage): TTT action text index (EBOOT 127.2, state index + 3) -> BT3 msg
// 0 first attack -> 3, 1 counter -> 0, 2 lock-on -> 2, 3 burst -> 1 (max power); the other indexes stay TTT
static bool bh_msg_off() { const char* e = getenv("TTT_HUD_MSG"); return e && atoi(e) == 0; }
struct BhMsg { int st = 4, msg = -1; BhRamp r; };
static BhMsg bh_ms[2];
static int bh_ms_prev[2][5];
// 0x089997F4 = power-up ("¡Potenciación!", prompt class 0x089D72F4 -> 0x089D839C) -> 4
static const uint32_t BH_MS_FN[5] = {0x08998E20u, 0x08998FACu, 0x08999060u, 0x08999198u, 0x089997F4u};   // TTT order: burst, counter, lock-on, first attack
static const int BH_MS_TO[5] = {1, 0, 2, 3, 4};
static void bh_msg_feed(const Ctx& c, int s, uint32_t id) {
    int pick = -1;
    for (int i = 0; i < 4; i++) {
        int v = hud_guest(c, BH_MS_FN[i], id) != 0;
        if (v && !bh_ms_prev[s][i]) pick = BH_MS_TO[i];
        bh_ms_prev[s][i] = v;
    }
    if (pick >= 0 && hud_guest(c, 0x0899924Cu, id)) pick = -1;   // as TTT 0x089D68E8: this flag cancels them
    int v = hud_guest(c, BH_MS_FN[4], id) != 0;
    if (v && !bh_ms_prev[s][4]) pick = 4;
    bh_ms_prev[s][4] = v;
    if (pick < 0) return;
    bh_ms[s].msg = pick; bh_ms[s].st = 0;
    static int logs = 0;
    if (logs++ < 40) printf("hud bt3: mensaje lado %d luchador %d msg %d\n", s, (int)id, pick);
}
static bool bh_partner(int s) {
    uint32_t top = bh_top_f[s];
    if (!top || !hud_mgr) return false;
    for (int k = 0; k < 4; k++) {
        uint32_t bar = psp_l32(hud_mgr + 0x74 + 4 * k), fb = bar ? psp_l32(bar + 0x6C) : 0;
        if (fb && fb != top && psp_l32(fb + 4) == psp_l32(top + 4) && (int)psp_l32(fb + 0x1A38) > 0) return true;
    }
    return false;
}
static void bh_msg_draw(int s, float dt) {
    BhMsg& m = bh_ms[s];
    if (m.msg < 0 || m.st >= 4) return;
    static const char* T[] = {"combo_03", "combo_04", "combo_05", "combo_06", "combo_07"};
    const BhTex* t = bh_get(T[m.msg]);
    if (!t) return;
    float w = -(float)t->w, pos = 0;
    switch (m.st) {
    case 0: m.r.start(0.1f, 1, 0); m.st = 1;   // fall through
    case 1: if (m.r.step(dt)) { m.r.start(1.2f, 0, 1); m.st = 2; } else pos = w * m.r.v; break;
    case 2: if (m.r.step(dt)) { m.r.start(0.1f, 0, 1); m.st = 3; } break;
    case 3: if (m.r.step(dt)) { m.st = 4; return; } pos = w * m.r.v; break;
    }
    float x = pos + 5, y = 85 + 16 + (m.msg == 4 ? (bh_partner(s) ? 231 : 310) : 0);   // above the partner panel when there is one
    if (s) x = 372 - x;
    bh_spr(t, 0, (float)t->w, 0, (float)t->h, x, x + t->w, y, y + t->h, s ? 3 : 0);
}

struct BhCombo { int hst = 10, dst = 10, hits = 0, hraw = 0, hprev = 0, dmg = 0, dfrom = 0, last = -1, fh = 0, fd = 0; BhRamp hr, dr, cr; };
static BhCombo bh_cb[2];
static uint32_t bh_cb_vb = 0;
static int bh_cb_side = 0;
static void bh_set_hits(int s, int h) {
    BhCombo& b = bh_cb[s];
    b.hraw = h;
    h = std::min(h, 999);
    if (h < 2) { if ((unsigned)(b.hst - 7) < 4) return; b.hst = 7; return; }
    b.hst = h < b.hits ? 0 : (unsigned)b.hst < 6 ? 4 : 0;
    b.hprev = b.hits; b.hits = h;
}
static void bh_set_damage(int s, int d, bool is_new) {
    BhCombo& b = bh_cb[s];
    d = std::min(d, 99999);
    if (d < 0) { if ((unsigned)(b.dst - 7) < 4) return; b.dst = 7; return; }
    float sec = 0.6f;
    if (is_new && b.hraw <= b.hprev) b.dfrom = 0;
    else if (b.cr.running()) b.dfrom += (int)(b.cr.v * (b.dmg - b.dfrom));
    else b.dfrom = b.dmg;
    b.dmg = d;
    if (b.dmg - b.dfrom > 6000) sec = (b.dmg - b.dfrom) / 9500.0f;
    b.cr.start(std::min(sec, 1.5f), 0, 1);
    b.dst = (unsigned)b.dst < 6 ? 4 : 0;
}
static uint32_t bh_fid[2] = {0xFFFFFFFFu, 0xFFFFFFFFu};   // fighter id (entry +0x2C) of each top panel
// the getters the TTT counters use (0x089EA2F0, 0x089EAE54): hits 0x08998D8C (else 0x0899851C), combo on 0x08998928 == 1
// and 0x0899881C, damage on 0x08998A80, damage 0x08998680; asked for each panel's fighter, so both sides count
static void bh_combo_feed(const Ctx& c) {
    for (int s = 0; s < 2; s++) {
        uint32_t id = bh_lay == 2 ? (uint32_t)LMP_SLOT[s] : bh_fid[s];   // 2 views: left = player 1's combo, right = player 2's
        if (id == 0xFFFFFFFFu) continue;
        BhCombo& b = bh_cb[s];
        if (!bh_msg_off()) bh_msg_feed(c, s, id);
        // as the TTT counters: values are taken only on the frames 0x08998928 / 0x08998A80 report a hit; the counters
        // stay up until 0x0899881C says the combo is over
        bool on = hud_guest(c, 0x0899881Cu, id) != 0;
        int h = b.fh, d = b.fd;
        if (!on) { h = 0; d = 0; }
        else {
            if (hud_guest(c, 0x08998928u, id) == 1) { h = (int)hud_guest(c, 0x08998D8Cu, id); if (!h) h = (int)hud_guest(c, 0x0899851Cu, id); }
            if (hud_guest(c, 0x08998A80u, id)) d = std::min(99999, (int)hud_guest(c, 0x08998680u, id));
        }
        if (h >= 2 && h != b.fh) bh_set_hits(s, h);
        else if (h < 2 && b.fh >= 2) bh_set_hits(s, 0);
        if (d > 0 && d != b.fd) bh_set_damage(s, d, b.fd <= 0);
        else if (d <= 0 && b.fd > 0) bh_set_damage(s, -1, false);
        static int lk[2][2] = {{-1, -1}, {-1, -1}}, logs = 0;
        if ((h != lk[s][0] || d != lk[s][1]) && logs++ < 30) { lk[s][0] = h; lk[s][1] = d; printf("hud bt3: combo lado %d luchador %d golpes %d dano %d\n", s, (int)id, h, d); }
        b.fh = h; b.fd = d;
    }
}
// one node's slide: in (0.1 s), pop (0.1 s out and back), out (0.1 s); returns false when off. pos = x offset, pop = 0..1
static bool bh_cstate(int& st, BhRamp& r, float dt, float width, float* pos, float* pop) {
    *pos = 0; *pop = 0;
    switch (st) {
    case 0: r.start(0.1f, 1, 0); st = 1;   // fall through
    case 1: if (r.step(dt)) { st = 3; *pos = 0; } else *pos = width * r.v; break;
    case 4: r.start(0.1f, 0, 1); st = 5;   // fall through
    case 5: if (r.step(dt)) { if (r.v == 1.0f) r.start(0.1f, 1, 0); else st = 3; } *pop = r.v; break;
    case 7: r.start(0.1f, 0, 1); st = 8;   // fall through
    case 8: if (r.step(dt)) { st = 10; return false; } *pos = width * r.v; break;
    case 10: return false;
    default: break;
    }
    return true;
}
static void bh_combo_draw(int s, float dt) {
    BhCombo& b = bh_cb[s];
    const BhTex* lab = bh_get("combo_02");
    float pos, pop;
    const float gy = 85 + 80;
    auto at = [&](float x) { return s ? 372 - x : x; };
    int sd = s ? 3 : 0;
    if (bh_cstate(b.dst, b.dr, dt, -128, &pos, &pop)) {
        float nx = at(pos - pop * 10 + 5);
        if (dt > 0) b.cr.step(dt);
        int v = b.dmg < 2 ? b.dmg : (int)(b.dfrom + (b.dmg - b.dfrom) * b.cr.v);
        v = std::min(v, 99999);
        if (lab) bh_spr(lab, 0, 128, 32, 64, nx, nx + 128, gy, gy + 32, sd);
        if (const BhTex* t = bh_get("combo_01")) {
            int dg[5], top = 0;
            for (int i = 0, p = 1; i < 5; i++, p *= 10) { dg[i] = v / p % 10; if (dg[i]) top = i; }
            if (dt > 0 && (unsigned)(v - 100) <= 99898u && b.cr.running()) { int d; do d = bh_rand(10); while (d == b.last); b.last = dg[0] = d; }
            for (int i = 0; i <= top; i++) {
                float x = nx + 96 - 12 * i, u = (float)(dg[i] % 4 * 32), w = (float)(dg[i] / 4 * 32);
                bh_spr(t, u + 1, u + 31, w + 1, w + 31, x + 1, x + 31, gy + 1, gy + 31, sd);
            }
        }
    }
    if (bh_cstate(b.hst, b.hr, dt, -128, &pos, &pop)) {
        float nx = at(pos + 5), sc = 1 + pop * 0.5f;
        if (lab) bh_spr(lab, 0, 128, 2, 32, nx, nx + 128, gy - 30, gy + 2, sd);
        if (const BhTex* t = bh_get("combo_00")) {
            int dg[3], top = 0;
            for (int i = 0, p = 1; i < 3; i++, p *= 10) { dg[i] = b.hits / p % 10; if (dg[i]) top = i; }
            for (int i = 0; i <= top; i++) {
                float x = nx + 36 - 22 * i, u = (float)(dg[i] % 4 * 32), w = (float)(dg[i] / 4 * 32);
                bh_spr(t, u + 1, u + 31, w, w + 32, x + 1, x + 30, gy - 32 * sc, gy, sd);
            }
        }
    }
}
static void bh_reset_combo() { for (auto& b : bh_cb) b = BhCombo(); for (auto& z : bh_st) z = BhStat(); for (auto& m : bh_ms) m = BhMsg(); memset(bh_ms_prev, 0, sizeof bh_ms_prev); bh_cb_vb = 0; bh_cb_side = 0; bh_fid[0] = bh_fid[1] = 0xFFFFFFFFu; bh_bot_f[0] = bh_bot_f[1] = bh_top_f[0] = bh_top_f[1] = 0; bh_nt_reset(); }
static void bh_combo_frame(const Ctx& c) {
    HUD_COST("hud bt3: combo y avisos");
    uint32_t vb = psp_l32(0x08A78E14u);
    float dt = bh_cb_vb ? std::min(0.1f, (vb - bh_cb_vb) / 60.0f) : 0;
    bh_cb_vb = vb;
    if (bp_open) dt = 0;
    bh_combo_feed(c);
    bh_begin();
    bh_msg_draw(0, dt);
    bh_msg_draw(1, dt);
    bh_combo_draw(0, dt);
    bh_combo_draw(1, dt);
    bh_end();
}

// bar names (Graphics "Life bar names", always the Steam name online): under the panel, after the status icons' row
static void bh_name(const Ctx& c, uint32_t f, int side, bool bot = false) {
    bool player;
    uint32_t str = nm_text(c, f, &player), font = psp_l32(0x08A78EECu);
    if (!str || !font || (!player && !nm_want())) return;
    if (!nm_mem) nm_mem = psp_reserve(0x40);
    const float k = 1.0f;
    int tw = (int)hud_guest(c, 0x08831F68u, font, str);
    if (tw <= 0) return;
    int nc = 0;
    while (nc < 64 && psp_l16(str + 2 * (uint32_t)nc)) nc++;
    float vw = tw * k - 1.9f * nc;   // drawn width: the measure adds ~1.9 px of spacing per glyph
    float y = 41, e = bh_sz_edge[side];   // centred on the senzu icon
    if (bot) { y = (BH_BOT + 8) * BH_SY + 26; e = side ? 456.5f : 24.0f; }   // bottom copies: under the panel (text 259..269), clear of the blast digit (x 3..26)
    int x = side ? (int)lroundf(e >= 0 ? e - 4.5f - vw : 448 - vw) : (int)lroundf(e >= 0 ? e + 4 : 32);
    uint32_t st = nm_mem;
    psp_s32(st, 0); psp_s32(st + 4, 0); psp_s32(st + 8, 0xFFFFFFFFu); psp_s32(st + 0xC, 1);
    psp_s32(st + 0x10, 1); psp_s32(st + 0x14, 1); psp_s32(st + 0x18, 0xFF404040u); psp_s32(st + 0x1C, 2);
    uint32_t list = hud_guest(c, 0x08838054u, 0);
    hud_guest(c, 0x08832448u, font);
    hud_guest(c, 0x088321ECu, font, st);
    cl_call(c, 0x08832530u, font, (uint32_t)x, (uint32_t)lroundf(y), str, 0xFFFFFFFFu, 0xFFFFFFFFu);
    psp_s32(st + 8, 0xFFFFFFFFu); psp_s32(st + 0xC, 0); psp_s32(st + 0x18, 0xFF404040u); psp_s32(st + 0x1C, 0);
    hud_guest(c, 0x088321ECu, font, st);
    hud_guest(c, 0x08832500u, font);
    hud_guest(c, 0x08838120u, list);
    hud_guest(c, 0x08837F54u, 0x08B4E690u, list, 0);
    static int logs = 0;
    if (logs++ < 4) printf("hud bt3: nombre bajo el panel %d (%s) x %d ancho %d\n", side, player ? "jugador" : "personaje", x, tw);
}

// names after the senzu draw (the last TTT piece of the panels), never between the game's own pieces
static void bh_names(const Ctx& c) {
    if (!bh_on || bh_sz || bh_vpass >= 0 || (unsigned)(bh_phase() - 3) > 2) return;
    for (int s = 0; s < 2; s++) if (bh_top_f[s]) bh_name(c, bh_top_f[s], s);
    for (int s = 0; s < 2; s++) if (bh_bot_f[s]) bh_name(c, bh_bot_f[s], s, true);
}
// panel of a bar: 0 top left (P1), 1 top right (the opponent P1 targets), 2 bottom left (partner), 3 bottom right (the other opponent)
static int bh_place(const Ctx& c, int idx, uint32_t f) {
    if (idx < 2) return idx ? 2 : 0;
    if (bh_views) return idx == 2 ? 1 : 3;   // split screen: every fighter keeps his place
    uint32_t ob = psp_l32(hud_mgr + 0x74 + 4 * (uint32_t)(5 - idx)), of = ob ? psp_l32(ob + 0x6C) : 0;
    if (!of || (int)psp_l32(of + 0x1A38) <= 0) return 1;
    uint32_t b0 = psp_l32(hud_mgr + 0x74), v = b0 && psp_l32(b0 + 0xC) ? hud_view(c, b0) : 0;
    uint32_t tg = guest_ptr(v) ? psp_l32(v + 0x14) : 0xFFFFFFFFu;
    if (psp_l32(f + 0x2C) == tg) return 1;
    if (psp_l32(of + 0x2C) == tg) return 3;
    return idx == 2 ? 1 : 3;
}
static void sz_combo(const Ctx& c);
static void sz_boost();
// training "reset position" sends the battle back to Ready (phase 4/5 -> 3) without a new HUD: the bars are built again as at the start
static void bh_phase_watch() {
    static int last = -1;
    static uint32_t lvb = 0;
    uint32_t vb = psp_l32(0x08A78E14u);
    if (vb == lvb) return;
    lvb = vb;
    int ph = bh_phase();
    if (bp_open && ph >= 0 && ph <= 3) {   // the pause that asked for the reset is over: its Start check only runs again in combat
        bp_open = false;
        static int pl = 0;
        if (pl++ < 10) printf("hud bt3: pausa cerrada por el reinicio (fase %d)\n", ph);
    }
    if (last >= 4 && ph >= 0 && ph <= 3) {
        bh_reset_combo(); bh_nt_reset();
        for (int s = 0; s < 4; s++) { bh_hp_shown[s] = bh_ki_shown[s] = -1; bh_hp_drain[s] = 100; bh_ki_drain[s] = 400; bh_vb_last[s] = 0; bh_aura[s] = 0; }
        static int logs = 0;
        if (logs++ < 10) printf("hud bt3: combate reiniciado (fase %d -> %d), barras armadas de nuevo\n", last, ph);
    }
    if (ph >= 0) last = ph;
}
static bool bh_bar(const Ctx& c, uint32_t o, int idx) {
    bh_tw_end();
    bh_cur_bar = o;
    if (!bh_on || idx < 0 || idx > 3) return false;
    bh_phase_watch();
    if (bh_vpass >= 0) return true;   // a player's view pass: the shared bars were drawn in the full screen pass
    if (idx == 2 && !bh_sz) bh_notice_frame();
    if ((unsigned)(bh_phase() - 3) > 2) return true;   // bars only in Ready/Fight (3), combat (4) and the finish (5): not in intros or the result
    uint32_t f = psp_l32(o + 0x6C);
    if (!f || (int)psp_l32(f + 0x1A38) <= 0) return true;
    int pl = bh_place(c, idx, f), side = pl & 1;
    if (pl < 2) { bh_fid[side] = psp_l32(f + 0x2C); bh_top_f[side] = f; } else bh_bot_f[side] = f;
    static int lp[4] = {-1, -1, -1, -1}, logs = 0;
    if (lp[idx] != pl && logs++ < 16) printf("hud bt3: barra %d en el panel %d\n", idx, pl);
    lp[idx] = pl;
    bh_pn = idx; bh_oy = pl < 2 ? 0 : BH_BOT;
    if (bh_sz) { sz_panel(c, f, side, idx, pl < 2 ? 0 : SZ_BOT); bh_oy = 0; if (idx == 2 && bh_lay != 4) { sz_combo(c); sz_boost(); } return true; }
    bh_begin();
    bh_panel(c, f, side);
    bh_end();
    bh_oy = 0;
    if (idx == 2 && bh_lay != 4) bh_combo_frame(c);
    return true;
}

static void sz_timer(int t, bool inf);
// round timer (spec 4): digits of the game's own counter, infinity without a time limit
static bool bh_timer(uint32_t tobj) {
    uint32_t dig = tobj ? psp_l32(tobj + 0x94) : 0;
    if (!bh_on || !tobj) return false;
    uint32_t md = psp_l32(0x08B48A28u);
    if (md == 3 || md == 12) return false;
    if ((unsigned)(bh_phase() - 3) > 2) return true;
    int t = dig ? (int)psp_l32(dig + 0x30) : 0;
    bool inf = !dig || md == 7;
    for (uint32_t off : {0x74u, 0x90u}) if (uint32_t h = psp_l32(tobj + off)) psp_s8(h + 0x15, 0);
    if (bh_vpass >= 0) return true;
    if (bh_sz) { sz_timer(std::max(0, std::min(999, t)), inf); return true; }
    static int logs = 0;
    if (logs++ < 3) printf("hud bt3: reloj modo %u valor %d%s\n", md, t, inf ? " (infinito)" : "");
    bh_begin(true);
    const float cx = 256, cy = 37;
    if (auto* fr = bh_get("gauge_03")) bh_spr(fr, 0, 64, 0, 30, cx - 32, cx + 32, cy - 22, cy + 8, 2);
    if (inf) {
        if (auto* d = bh_get("gauge_00")) { bh_spr(d, 64, 96, 64, 96, cx - 33, cx - 1, cy - 28, cy + 4, 2); bh_spr(d, 96, 128, 64, 96, cx - 1, cx + 31, cy - 28, cy + 4, 2); }
    } else {
        t = std::max(0, std::min(999, t));
        const BhTex* d = bh_get(t >= 10 ? "gauge_00" : t >= 4 ? "gauge_00_1" : "gauge_00_2");
        char s[8]; snprintf(s, sizeof s, "%d", t);
        int nd = (int)strlen(s);
        float x = cx - (11 * (nd - 1) + 32) / 2.0f - 2;
        for (int i = 0; i < nd; i++, x += 11) {
            int v = s[i] - '0';
            bh_spr(d, (float)(v % 4 * 32), (float)(v % 4 * 32 + 32), (float)(v / 4 * 32), (float)(v / 4 * 32 + 32), x, x + 32, cy - 28, cy + 4, 2);
        }
    }
    bh_end();
    return true;
}

#include "core/hooks/sz_hud.inc"
