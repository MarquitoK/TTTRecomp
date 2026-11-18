#pragma once
// Item 16b: button textures composed for the device in use and the player's bindings (games/ttt/button_icons.txt)
#include <atomic>
#include <mutex>
#include <cmath>
#include <map>
#include <unordered_map>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include "gpu/image.h"
#include <memory>

enum { IM_ICON, IM_GLYPH, IM_COVER, IM_TEXT };
enum { SY_PSP, SY_STICK, SY_DPAD, SY_DPAD_UD, SY_DPAD_LR, SY_STICK_ANY, SY_ACT, SY_HELD, SY_NONE, SY_KEEP, SY_ARROW, SY_BG, SY_DEV };
enum { CO_HELD = 1, CO_FOLLOW };
// alt: while a condition holds (held special, prompt follow-up) the rectangle shows another button
struct IconAlt { int cond, sym; uint32_t arg; };
struct IconRect { int mode, x, y, w, h, sym, margin; uint32_t arg; int nalt; IconAlt alt[2]; };
struct IconPage { std::string pat; int ctx; bool dyn = false; int slot = -1; std::vector<IconRect> r; };
struct IconDone { uint32_t ep = 0; int w = 0, h = 0; bool any = false; std::vector<uint32_t> px; };
static std::vector<IconPage> icon_pages;
static bool icon_pages_read = false;
static std::recursive_mutex icon_mx;
static std::atomic<uint32_t> icon_ep{0}, icon_ep_dyn{0};
static std::atomic<int> icon_held{-1}, icon_follow{0};
static std::atomic<int> icon_ctx{0};
// per local player (item 20): epochs, held special and follow-up latch, and the device they were made for
static std::atomic<uint32_t> icon_ep_pl[4], icon_ep_dyn_pl[4];
static std::atomic<int> icon_held_pl[4], icon_follow_pl[4], icon_mode_pl[4], icon_fam_pl[4];
static int ic_mode = 0, ic_fam = 1, ic_held = -1, ic_follow = 0;    // player being composed (under icon_mx)
int psp_gpu_hud_player();
static int icons_player() { int k = psp_gpu_hud_player(); return k > 0 && k < local_players() && k < 4 ? k : 0; }
static std::map<uint32_t, std::vector<std::shared_ptr<IconDone>>> icon_done;   // per texture: its last states (held, prompt), newest first
static uint32_t icon_force_ep = 0; static int icon_force_held = -1, icon_force_follow = 0;   // icons_compose_as (under icon_mx)
static std::map<uint64_t, std::shared_ptr<IconDone>> icon_same;   // by page, state and source image: the same page in several paks is composed once
static std::map<std::string, std::vector<uint32_t>> icon_over;
static std::map<std::string, std::pair<int, int>> icon_over_wh;
static int icon_log_n = 0;

// command combos in texts: glyph characters 0xFF86..0xFF9F (slots 31..56) show the bound codes (table and parts added by the port)
enum { N_SLOTS = 26, SLOT_BASE = 31 };
static uint32_t slot_code[N_SLOTS], slot_gen = 0;
static uint64_t slot_use[N_SLOTS], slot_tick = 0;
static int slot_page0 = -1;

static bool icon_glob(const char* p, const char* s) {
    if (!*p) return !*s;
    if (*p == '*') { for (;; s++) { if (icon_glob(p + 1, s)) return true; if (!*s) return false; } }
    return *s && tolower((unsigned char)*p) == tolower((unsigned char)*s) && icon_glob(p + 1, s + 1);
}

static bool icon_sym1(const char* s, int& sym, uint32_t& arg) {
    static const struct { const char* n; int sym; uint32_t arg; } T[] = {
        {"cro", SY_PSP, PB_CRO}, {"cir", SY_PSP, PB_CIR}, {"squ", SY_PSP, PB_SQU}, {"tri", SY_PSP, PB_TRI},
        {"l", SY_PSP, PB_L}, {"r", SY_PSP, PB_R}, {"select", SY_PSP, PB_SELECT}, {"start", SY_PSP, PB_START},
        {"up", SY_PSP, PB_UP}, {"down", SY_PSP, PB_DOWN}, {"left", SY_PSP, PB_LEFT}, {"right", SY_PSP, PB_RIGHT},
        {"stick_up", SY_STICK, D_UP}, {"stick_down", SY_STICK, D_DOWN}, {"stick_left", SY_STICK, D_LEFT}, {"stick_right", SY_STICK, D_RIGHT},
        {"dpad", SY_DPAD, 0}, {"dpad_ud", SY_DPAD_UD, 0}, {"dpad_lr", SY_DPAD_LR, 0}, {"stick", SY_STICK_ANY, 0},
        {"cr_ud", SY_DEV, 0}, {"cr_lr", SY_DEV, 1}, {"cr_stick", SY_DEV, 2}, {"cr_zout", SY_DEV, 3}, {"cr_zin", SY_DEV, 4}, {"cr_form", SY_DEV, 5}};
    if (s[0] == '@') {
        for (int a = 0; a < N_ACTIONS; a++) if (!strcmp(ACT[a].id, s + 1)) { sym = SY_ACT; arg = (uint32_t)a; return true; }
        return false;
    }
    if (!strcmp(s, "held")) { sym = SY_HELD; arg = 0; return true; }
    if (!strcmp(s, "none")) { sym = SY_NONE; arg = 0; return true; }
    if (!strcmp(s, "keep")) { sym = SY_KEEP; arg = 0; return true; }
    if (!strcmp(s, "bg")) { sym = SY_BG; arg = 0; return true; }
    static const char* ar[4] = {"arrow_up", "arrow_down", "arrow_left", "arrow_right"};
    for (int i = 0; i < 4; i++) if (!strcmp(s, ar[i])) { sym = SY_ARROW; arg = (uint32_t)(D_UP + i); return true; }
    for (auto& t : T) if (!strcmp(t.n, s)) { sym = t.sym; arg = t.arg; return true; }
    return false;
}

// button[?held=alt][?follow=alt]
static bool icon_sym(const char* s, IconRect& r) {
    char b[64];
    snprintf(b, sizeof(b), "%s", s);
    char* q = strchr(b, '?');
    if (q) *q++ = 0;
    if (!icon_sym1(b, r.sym, r.arg)) return false;
    r.nalt = 0;
    while (q && *q) {
        char* nx = strchr(q, '?');
        if (nx) *nx++ = 0;
        char* eq = strchr(q, '=');
        if (!eq || r.nalt >= 2) return false;
        *eq = 0;
        IconAlt& a = r.alt[r.nalt];
        a.cond = !strcmp(q, "held") ? CO_HELD : !strcmp(q, "follow") ? CO_FOLLOW : 0;
        if (!a.cond || !icon_sym1(eq + 1, a.sym, a.arg)) return false;
        r.nalt++;
        q = nx;
    }
    return true;
}

static void icon_pages_load() {
    icon_pages_read = true;
    FILE* f = fopen("../../games/ttt/button_icons.txt", "r");
    if (!f) { printf("icons: falta games/ttt/button_icons.txt\n"); return; }
    char line[256];
    int n = 0;
    while (fgets(line, sizeof(line), f)) {
        char* s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (!*s || *s == '#' || *s == '\n' || *s == '\r') continue;
        if (*s == '[') {
            char pat[200], cx[16];
            if (sscanf(s + 1, "%199s %15[^]]", pat, cx) != 2) continue;
            IconPage p; p.pat = pat;
            p.ctx = !strcmp(cx, "menu") ? CTX_MENU : !strcmp(cx, "battle") ? CTX_BATTLE : !strcmp(cx, "story") ? CTX_STORY : !strcmp(cx, "play") ? -2 : -1;
            icon_pages.push_back(p);
            continue;
        }
        char md[16], sy[64];
        IconRect r{};
        r.margin = 1;
        if (icon_pages.empty() || sscanf(s, "%15s %d %d %d %d %63s %d", md, &r.x, &r.y, &r.w, &r.h, sy, &r.margin) < 6) continue;
        r.mode = !strcmp(md, "glyph") ? IM_GLYPH : !strcmp(md, "cover") ? IM_COVER : !strcmp(md, "text") ? IM_TEXT : IM_ICON;
        if (!icon_sym(sy, r)) { printf("icons: boton desconocido %s\n", sy); continue; }
        icon_pages.back().r.push_back(r);
        if (r.nalt) icon_pages.back().dyn = true;
        n++;
    }
    fclose(f);
    printf("icons: %d texturas, %d botones\n", (int)icon_pages.size(), n);
    slot_page0 = (int)icon_pages.size();
    for (int k = 0; k < N_SLOTS; k++) { IconPage p; p.ctx = -1; p.slot = k; icon_pages.push_back(p); }
}

int icons_slot_page(int k) {
    if (!patch_button_icons.val) return -1;
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    if (!icon_pages_read) icon_pages_load();
    return slot_page0 >= 0 && k >= 0 && k < N_SLOTS ? slot_page0 + k : -1;
}

int icons_page_of(const char* virt, const char* real) {
    if (!patch_button_icons.val) return -1;
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    if (!icon_pages_read) icon_pages_load();
    for (size_t i = 0; i < icon_pages.size(); i++)
        if (icon_glob(icon_pages[i].pat.c_str(), virt) || icon_glob(icon_pages[i].pat.c_str(), real)) return (int)i;
    return -1;
}

// pages worth composing ahead (text_names.h worker): fixed menu pages; the longest plain part of the pattern to find their textures
int icons_pages_n() {
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    if (!icon_pages_read) icon_pages_load();
    return (int)icon_pages.size();
}
std::string icons_page_core(int i, bool battle) {
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    if (i < 0 || i >= (int)icon_pages.size()) return "";
    const IconPage& p = icon_pages[i];
    if (p.slot >= 0) return "";
    if (battle ? p.ctx != CTX_BATTLE : (p.dyn || p.ctx == CTX_BATTLE || p.ctx == CTX_STORY || p.ctx == -2)) return "";
    std::string best, cur;
    for (char c : p.pat + "*") { if (c == '*' || c == '?') { if (cur.size() > best.size()) best = cur; cur.clear(); } else cur += c; }
    return best.size() >= 6 ? best : "";
}

bool icons_compose(int page, uint32_t id, int w0, int h0, int w, int h, const uint32_t* px, bool psp, int* ow, int* oh, const uint32_t** opx);
// battle pages composed during the loading screen: the epoch icons_update gives them in battle (player 1, a held special, the prompt)
bool icons_compose_as(int page, uint32_t id, int w0, int h0, int w, int h, const uint32_t* px, bool psp, int held, int follow) {
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    if (page < 0 || page >= (int)icon_pages.size()) return false;
    uint32_t hh = 2166136261u;
    auto mix = [&](const void* p, size_t m) { const uint8_t* b = (const uint8_t*)p; for (size_t i = 0; i < m; i++) hh = (hh ^ b[i]) * 16777619u; };
    int v[3] = {pl_mode[0], pl_family[0], CTX_BATTLE};
    mix(v, sizeof(v)); mix(cfg.pad, sizeof(cfg.pad)); mix(cfg.kb, sizeof(cfg.kb));
    uint32_t ep = hh | 1;
    if (icon_pages[page].dyn) { int d[2] = {held, follow}; mix(d, sizeof(d)); ep = hh | 1; }
    icon_force_ep = ep; icon_force_held = held; icon_force_follow = follow;
    int ow, oh; const uint32_t* opx;
    bool r = icons_compose(page, id, w0, h0, w, h, px, psp, &ow, &oh, &opx);
    icon_force_ep = 0;
    return r;
}

uint32_t icons_epoch(int page) {
    if (page >= 0 && page < (int)icon_pages.size() && icon_pages[page].slot >= 0) {
        uint32_t v[2] = {slot_code[icon_pages[page].slot], (uint32_t)pl_family[0]}, h = 2166136261u;
        for (int i = 0; i < 8; i++) h = (h ^ ((const uint8_t*)v)[i]) * 16777619u;
        return h | 1;
    }
    bool dyn = page >= 0 && page < (int)icon_pages.size() && icon_pages[page].dyn;
    int k = icons_player();
    if (k) return (dyn ? icon_ep_dyn_pl[k] : icon_ep_pl[k]).load(std::memory_order_relaxed);
    return (dyn ? icon_ep_dyn : icon_ep).load(std::memory_order_relaxed);
}

// title screen: its "press start" page was drawn in the last moments
static std::atomic<uint64_t> icon_title_ms{0};
static std::atomic<uint64_t> icon_dyn_ms{0};
void icons_bound(int page) {
    if (page >= 0 && page < (int)icon_pages.size() && icon_pages[page].dyn) icon_dyn_ms.store(GetTickCount64(), std::memory_order_relaxed);
    if (page < (int)icon_pages.size() && icon_pages[page].pat.find("press_start") != std::string::npos) icon_title_ms.store(GetTickCount64(), std::memory_order_relaxed);
}
static bool icons_title() { return GetTickCount64() - icon_title_ms.load(std::memory_order_relaxed) < 300; }

static std::atomic<uint32_t> glyph_tab{0};
uint32_t psp_reserve(uint32_t n);

// the glyph metric table moved to guest memory with 57 entries (the game's 31 then the slots), read at its 8 sites
uint32_t icons_glyph_table() {
    if (!patch_button_icons.val || !psp_mem) return 0;
    uint32_t t = glyph_tab.load();
    if (t) return t;
    t = psp_reserve(57 * 22);
    memcpy(psp_p(t), psp_p(0x08A78B08u), 31 * 22);
    static const uint16_t e[11] = {14, 1, 2, 0, 0, 16, 16, 0, 0, 0, 0};
    for (int k = SLOT_BASE; k < SLOT_BASE + N_SLOTS; k++) memcpy(psp_p(t + k * 22), e, 22);
    glyph_tab = t;
    return t;
}

static void icons_update(int ctx) {
    if (!patch_button_icons.val) return;
    icon_ctx = ctx;
    int n = local_players();
    bool drawn = GetTickCount64() - icon_dyn_ms.load(std::memory_order_relaxed) < 300;
    for (int k = 0; k < n && k < 4; k++) {
        uint32_t h = 2166136261u;
        auto mix = [&](const void* p, size_t m) { const uint8_t* b = (const uint8_t*)p; for (size_t i = 0; i < m; i++) h = (h ^ b[i]) * 16777619u; };
        int v[3] = {pl_mode[k], pl_family[k], ctx};
        mix(v, sizeof(v)); mix(cfg.pad, sizeof(cfg.pad)); mix(cfg.kb, sizeof(cfg.kb));
        icon_mode_pl[k] = pl_mode[k]; icon_fam_pl[k] = pl_family[k];
        icon_ep_pl[k].store(h | 1, std::memory_order_relaxed);
        if (!k) icon_ep.store(h | 1, std::memory_order_relaxed);
        // held keyboard special and command prompt follow-up only re-key the pages that picture them
        int held = ctx == CTX_BATTLE ? input_held_action(k) : -1, follow = 0;
        if (ctx == CTX_BATTLE) {
            int codes[12]; uint32_t serial = 0;
            int m = psp_input_prompt_of(k, codes, 12, &serial);
            for (int i = 0; i < m; i++) if (codes[i] >= 0x10 && codes[i] <= 0x13) follow = 1;
        }
        // both states outlive their input while the prompt keeps being drawn (release and answer animations)
        static int held_l[4] = {-1, -1, -1, -1}, follow_l[4] = {0, 0, 0, 0};
        if (follow) { follow_l[k] = 1; held_l[k] = -1; }
        else if (held >= 0) { held_l[k] = held; follow_l[k] = 0; }
        else if (!drawn) { held_l[k] = -1; follow_l[k] = 0; }
        held = held_l[k]; follow = follow_l[k];
        icon_held_pl[k] = held; icon_follow_pl[k] = follow;
        if (!k) { icon_held = held; icon_follow = follow; }
        int d[2] = {held, follow};
        mix(d, sizeof(d));
        icon_ep_dyn_pl[k].store(h | 1, std::memory_order_relaxed);
        if (!k) icon_ep_dyn.store(h | 1, std::memory_order_relaxed);
    }
    // text glyph metrics (0x08A78B08, 22 bytes each): L and R get the face buttons' 16x16 box on the text line
    if (psp_mem) {
        static const uint16_t lr[11] = {14, 1, 2, 0, 0, 16, 16, 0, 0, 0, 0};
        for (uint32_t t : {0x08A78B08u, glyph_tab.load()}) {
            if (!t) continue;
            memcpy(psp_p(t + 7 * 22), lr, 22);
            memcpy(psp_p(t + 8 * 22), lr, 22);
        }
    }
}

// which action a pictured PSP button stands for in a context: exact output, output containing it, then movement
static int icon_action(int ctx, int sym, uint32_t arg, bool pad) {
    if (sym == SY_ACT) return (int)arg;
    int dir = sym == SY_STICK ? (int)arg : arg == PB_UP ? D_UP : arg == PB_DOWN ? D_DOWN : arg == PB_LEFT ? D_LEFT : arg == PB_RIGHT ? D_RIGHT : 0;
    for (int pass = 0; pass < 3; pass++)
        for (int a = 0; a < N_ACTIONS; a++) {
            const Action& A = ACT[a];
            if (TAB_CTX[A.tab] != ctx || A.base || A.mk == MK_TWICE) continue;
            if ((pad ? A.from_pad : A.from_kb) >= 0 || (pad ? A.mod_pad : A.mod_kb) >= 0 || !binds_of(a, pad)[0]) continue;
            bool ok = false;
            if (sym == SY_PSP && pass == 0) ok = A.out == arg && !A.dir;
            else if (sym == SY_PSP && pass == 1) ok = (A.out & arg) && !A.dir;
            else if (pass == 2 || sym == SY_STICK) ok = dir && A.dir == dir && !A.out;
            if (ok) return a;
        }
    return -1;
}

static uint32_t icon_native(int sym, uint32_t arg) {
    if (sym == SY_STICK) return C_PAD(arg == D_UP ? P_LSU : arg == D_DOWN ? P_LSD : arg == D_LEFT ? P_LSL : P_LSR);
    if (sym != SY_PSP) return 0;
    switch (arg) {
        case PB_CRO: return C_PAD(P_SOUTH); case PB_CIR: return C_PAD(P_EAST); case PB_SQU: return C_PAD(P_WEST); case PB_TRI: return C_PAD(P_NORTH);
        case PB_L: return C_PAD(P_L1); case PB_R: return C_PAD(P_R1); case PB_SELECT: return C_PAD(P_SELECT); case PB_START: return C_PAD(P_START);
        case PB_UP: return C_PAD(P_DUP); case PB_DOWN: return C_PAD(P_DDOWN); case PB_LEFT: return C_PAD(P_DLEFT); case PB_RIGHT: return C_PAD(P_DRIGHT);
    }
    return 0;
}

// drawing on the composed page: straight alpha, analytic antialiasing
struct Cv { int w, h; uint32_t* p; };
static const uint32_t IC_BODY = 0x2E323B, IC_RIM = 0xC9CED8, IC_LIP = 0x0E1014, IC_TEXT = 0xF4F6FA, IC_ACCENT = 0xF2A33A, IC_YELLOW = 0xF4D83A;

static void cv_put(Cv& c, int x, int y, uint32_t rgb, float a) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h || a <= 0.0f) return;
    if (a > 1.0f) a = 1.0f;
    uint32_t& d = c.p[(size_t)y * c.w + x];
    float da = (float)(d >> 24) / 255.0f, oa = a + da * (1.0f - a);
    if (oa <= 0.0f) return;
    uint32_t o = 0;
    for (int s = 0; s < 24; s += 8) {
        float sc = (float)((rgb >> (16 - s)) & 0xFF), dc = (float)((d >> s) & 0xFF);
        o |= (uint32_t)((sc * a + dc * da * (1.0f - a)) / oa + 0.5f) << s;
    }
    d = o | ((uint32_t)(oa * 255.0f + 0.5f) << 24);
}

template <class F> static void cv_fill(Cv& c, float x0, float y0, float x1, float y1, uint32_t rgb, float alpha, F sdf) {
    for (int y = (int)std::floor(y0) - 1; y <= (int)std::ceil(y1) + 1; y++)
        for (int x = (int)std::floor(x0) - 1; x <= (int)std::ceil(x1) + 1; x++) {
            float d = sdf(x + 0.5f, y + 0.5f);
            float k = 0.5f - d;
            if (k > 0.0f) cv_put(c, x, y, rgb, alpha * (k > 1.0f ? 1.0f : k));
        }
}

static float sd_round(float px, float py, float x0, float y0, float x1, float y1, float r) {
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hx = (x1 - x0) * 0.5f - r, hy = (y1 - y0) * 0.5f - r;
    float qx = std::fabs(px - cx) - hx, qy = std::fabs(py - cy) - hy;
    float ox = qx > 0 ? qx : 0, oy = qy > 0 ? qy : 0;
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}
static float sd_seg(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax, vy = by - ay, t = ((px - ax) * vx + (py - ay) * vy) / (vx * vx + vy * vy + 1e-6f);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    float dx = px - ax - vx * t, dy = py - ay - vy * t;
    return std::sqrt(dx * dx + dy * dy);
}
static float sd_tri(float px, float py, const float* v) {
    float d = 1e9f; int in = 0;
    for (int i = 0; i < 3; i++) {
        float ax = v[i * 2], ay = v[i * 2 + 1], bx = v[(i + 1) % 3 * 2], by = v[(i + 1) % 3 * 2 + 1];
        d = std::min(d, sd_seg(px, py, ax, ay, bx, by));
        if (((ay > py) != (by > py)) && px < (bx - ax) * (py - ay) / (by - ay) + ax) in ^= 1;
    }
    return in ? -d : d;
}

static void cv_round(Cv& c, float x0, float y0, float x1, float y1, float r, uint32_t rgb, float a = 1.0f) {
    cv_fill(c, x0, y0, x1, y1, rgb, a, [&](float x, float y) { return sd_round(x, y, x0, y0, x1, y1, r); });
}
static void cv_circle(Cv& c, float cx, float cy, float r, uint32_t rgb, float a = 1.0f) {
    cv_fill(c, cx - r, cy - r, cx + r, cy + r, rgb, a, [&](float x, float y) { return std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) - r; });
}
static void cv_line(Cv& c, float ax, float ay, float bx, float by, float t, uint32_t rgb) {
    cv_fill(c, std::min(ax, bx) - t, std::min(ay, by) - t, std::max(ax, bx) + t, std::max(ay, by) + t, rgb, 1.0f,
            [&](float x, float y) { return sd_seg(x, y, ax, ay, bx, by) - t * 0.5f; });
}
static void cv_tri(Cv& c, const float* v, uint32_t rgb, float ring = 0.0f) {
    float x0 = std::min({v[0], v[2], v[4]}) - ring, x1 = std::max({v[0], v[2], v[4]}) + ring;
    float y0 = std::min({v[1], v[3], v[5]}) - ring, y1 = std::max({v[1], v[3], v[5]}) + ring;
    cv_fill(c, x0, y0, x1, y1, rgb, 1.0f, [&](float x, float y) { float d = sd_tri(x, y, v); return ring > 0 ? std::fabs(d) - ring * 0.5f : d; });
}
static void cv_arrow(Cv& c, float cx, float cy, float s, int dir, uint32_t rgb) {
    float dx = dir == D_LEFT ? -1.0f : dir == D_RIGHT ? 1.0f : 0.0f, dy = dir == D_UP ? -1.0f : dir == D_DOWN ? 1.0f : 0.0f;
    float v[6] = {cx + dx * s, cy + dy * s, cx - dx * s * 0.6f - dy * s, cy - dy * s * 0.6f + dx * s, cx - dx * s * 0.6f + dy * s, cy - dy * s * 0.6f - dx * s};
    cv_tri(c, v, rgb);
}

// text through GDI: grey antialiased coverage becomes alpha
static HDC icon_dc = nullptr;
static HFONT icon_font(int px) {   // kept: one per size
    static std::map<int, HFONT> fs;
    HFONT& f = fs[px];
    if (!f) f = CreateFontW(-px, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    return f;
}
static int icon_text_w(const wchar_t* s, int px) {
    if (!icon_dc) icon_dc = CreateCompatibleDC(nullptr);
    HFONT f = icon_font(px), o = (HFONT)SelectObject(icon_dc, f);
    SIZE z{0, 0};
    GetTextExtentPoint32W(icon_dc, s, (int)wcslen(s), &z);
    SelectObject(icon_dc, o);
    return z.cx;
}
static void cv_text(Cv& c, float x0, float y0, float x1, float y1, const wchar_t* s, uint32_t rgb, float hk = 0.72f) {
    int w = (int)(x1 - x0), h = (int)(y1 - y0);
    if (w < 2 || h < 2 || !*s) return;
    int px = std::max(4, (int)(h * hk));
    int tw0 = icon_text_w(s, px);
    if (tw0 > w * 0.9f) px = std::max(4, (int)(px * (w * 0.9f) / tw0));
    while (px > 4 && icon_text_w(s, px) > w * 0.9f) px--;
    if (!icon_dc) icon_dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bi{}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bm = CreateDIBSection(icon_dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bm) return;
    HGDIOBJ ob = SelectObject(icon_dc, bm);
    HFONT f = icon_font(px), of = (HFONT)SelectObject(icon_dc, f);
    memset(bits, 0, (size_t)w * h * 4);
    SetBkMode(icon_dc, TRANSPARENT); SetTextColor(icon_dc, RGB(255, 255, 255));
    RECT rc{0, 0, w, h};
    DrawTextW(icon_dc, s, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();
    const uint32_t* b = (const uint32_t*)bits;
    auto ink = [&](int x, int y) { uint32_t v = b[(size_t)y * w + x]; return std::max({(int)(v & 0xFF), (int)((v >> 8) & 0xFF), (int)((v >> 16) & 0xFF)}); };
    int mx0 = w, mx1 = -1, my0 = h, my1 = -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (ink(x, y) > 40) { mx0 = std::min(mx0, x); mx1 = std::max(mx1, x); my0 = std::min(my0, y); my1 = std::max(my1, y); }
    int dx = mx1 >= 0 ? (w - 1 - mx1 - mx0) / 2 : 0, dy = my1 >= 0 ? (h - 1 - my1 - my0) / 2 : 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int g = ink(x, y);
            if (g) cv_put(c, (int)x0 + x + dx, (int)y0 + y + dy, rgb, g / 255.0f);
        }
    SelectObject(icon_dc, of);
    SelectObject(icon_dc, ob); DeleteObject(bm);
}

static void icon_label(uint32_t code, wchar_t* s, int n) {
    uint32_t t = code >> 24, v = code & 0xFFFFFF;
    if (t == 1) {
        switch (v) {
            case VK_UP: wcsncpy(s, L"\u2191", n); return;
            case VK_DOWN: wcsncpy(s, L"\u2193", n); return;
            case VK_LEFT: wcsncpy(s, L"\u2190", n); return;
            case VK_RIGHT: wcsncpy(s, L"\u2192", n); return;
            case VK_BACK: wcsncpy(s, L"Bksp", n); return;
            case VK_RSHIFT: case VK_LSHIFT: case VK_SHIFT: wcsncpy(s, L"Shift", n); return;
            case VK_RCONTROL: case VK_LCONTROL: case VK_CONTROL: wcsncpy(s, L"Ctrl", n); return;
            case VK_RMENU: case VK_LMENU: case VK_MENU: wcsncpy(s, L"Alt", n); return;
        }
    }
    if (t == 5 && v < N_PADCODES) {
        static const wchar_t* xb[12] = {L"A", L"B", L"X", L"Y", L"LB", L"RB", L"LT", L"RT", L"LS", L"RS", L"View", L"Menu"};
        static const wchar_t* ps[12] = {L"", L"", L"", L"", L"L1", L"R1", L"L2", L"R2", L"L3", L"R3", L"SHARE", L"START"};
        if (v < 12) { wcsncpy(s, ic_fam == 1 ? xb[v] : ps[v], n); return; }
    }
    code_name(code, s, n, true);
}

// file name for an override in assets/Icons
static std::string icon_file(uint32_t code) {
    uint32_t t = code >> 24, v = code & 0xFFFFFF;
    char b[64];
    if (t == 2) {
        static const char* m[8] = {"", "left", "right", "wheel", "side1", "side2", "wheel_up", "wheel_down"};
        snprintf(b, sizeof(b), "mouse_%s", v < 8 ? m[v] : "x");
        return b;
    }
    if (t == 5 && v < N_PADCODES) {
        static const char* xb[N_PADCODES] = {"A", "B", "X", "Y", "LB", "RB", "LT", "RT", "LS", "RS", "View", "Menu", "dpad_up", "dpad_down", "dpad_left", "dpad_right",
                                             "ls_up", "ls_down", "ls_left", "ls_right", "rs_up", "rs_down", "rs_left", "rs_right"};
        static const char* ps[12] = {"cross", "circle", "square", "triangle", "L1", "R1", "L2", "R2", "L3", "R3", "Share", "Options"};
        snprintf(b, sizeof(b), "%s_%s", ic_fam == 1 ? "xbox" : "ps", ic_fam != 1 && v < 12 ? ps[v] : xb[v]);
        return b;
    }
    wchar_t w[32]; icon_label(code, w, 32);
    std::string r = "key_";
    for (wchar_t* p = w; *p; p++) r += (*p >= L'0' && *p <= L'9') || (*p >= L'A' && *p <= L'Z') || (*p >= L'a' && *p <= L'z') ? (char)*p : '_';
    return r;
}

static bool icon_override(Cv& c, uint32_t code, float x0, float y0, float x1, float y1) {
    std::string n = icon_file(code);
    auto it = icon_over.find(n);
    if (it == icon_over.end()) {
        std::vector<uint8_t> f; std::vector<uint32_t> px; int w = 0, h = 0; std::string err;
        FILE* fp = fopen(("../../assets/Icons/" + n + ".png").c_str(), "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END); long sz = ftell(fp); fseek(fp, 0, SEEK_SET);
            f.resize(sz > 0 ? (size_t)sz : 0);
            if (sz <= 0 || fread(f.data(), 1, f.size(), fp) != f.size() || !png_decode(f, &w, &h, px, &err)) px.clear();
            fclose(fp);
        }
        it = icon_over.emplace(n, px).first;
        icon_over_wh[n] = {w, h};
    }
    if (it->second.empty()) return false;
    int w = icon_over_wh[n].first, h = icon_over_wh[n].second;
    float k = std::min((x1 - x0) / w, (y1 - y0) / h), ox = (x0 + x1 - w * k) * 0.5f, oy = (y0 + y1 - h * k) * 0.5f;
    for (int y = (int)oy; y < (int)(oy + h * k); y++)
        for (int x = (int)ox; x < (int)(ox + w * k); x++) {
            int sx = std::min(w - 1, (int)((x + 0.5f - ox) / k)), sy = std::min(h - 1, (int)((y + 0.5f - oy) / k));
            uint32_t p = it->second[(size_t)sy * w + sx];
            cv_put(c, x, y, ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF), (p >> 24) / 255.0f);
        }
    return true;
}

static void icon_keycap(Cv& c, float x0, float y0, float x1, float y1, const wchar_t* s) {
    float h = y1 - y0, r = h * 0.22f, t = std::max(1.0f, h * 0.07f);
    cv_round(c, x0, y0 + h * 0.06f, x1, y1, r, IC_LIP);
    cv_round(c, x0, y0, x1, y1 - h * 0.06f, r, IC_RIM);
    cv_round(c, x0 + t, y0 + t, x1 - t, y1 - h * 0.06f - t, r - t, IC_BODY);
    cv_text(c, x0 + t, y0 + t, x1 - t, y1 - h * 0.06f - t, s, IC_TEXT, 0.62f);
}

static void icon_mouse(Cv& c, float x0, float y0, float x1, float y1, int b, bool glyph) {
    float s = std::min(x1 - x0, y1 - y0), cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    float bw = s * 0.7f, bh = s * 1.0f, l = cx - bw * 0.5f, r = cx + bw * 0.5f, t = cy - bh * 0.5f, btm = cy + bh * 0.5f, th = std::max(1.0f, s * 0.07f);
    float split = t + bh * 0.44f;
    auto body = [&](float x, float y) { return sd_round(x, y, l, t, r, btm, bw * 0.5f); };
    if (!glyph) cv_fill(c, l, t, r, btm, IC_BODY, 1.0f, body);
    cv_fill(c, l, t, r, btm, IC_RIM, 1.0f, [&](float x, float y) { float d = body(x, y); return std::max(d, -(d + th)); });
    auto part = [&](float px0, float px1, float py0, float py1) {
        cv_fill(c, px0, py0, px1, py1, IC_ACCENT, 1.0f, [&](float x, float y) {
            float d = body(x, y) + th;
            float e = std::max(std::max(px0 - x, x - px1), std::max(py0 - y, y - py1));
            return std::max(d, e);
        });
    };
    if (b == 1) part(l, cx - th * 0.5f, t, split - th * 0.5f);
    if (b == 2) part(cx + th * 0.5f, r, t, split - th * 0.5f);
    cv_line(c, l, split, r, split, th, IC_RIM);
    cv_line(c, cx, t, cx, split, th, IC_RIM);
    if (b == 3 || b == 6 || b == 7) cv_round(c, cx - s * 0.07f, t + bh * 0.12f, cx + s * 0.07f, t + bh * 0.32f, s * 0.07f, IC_ACCENT);
    if (b == 6) cv_arrow(c, cx, split + bh * 0.24f, s * 0.12f, D_UP, IC_TEXT);
    if (b == 7) cv_arrow(c, cx, split + bh * 0.24f, s * 0.12f, D_DOWN, IC_TEXT);
    if (b == 4 || b == 5) cv_round(c, l - th * 0.6f, split + bh * (b == 4 ? 0.04f : 0.22f), l + th * 1.4f, split + bh * (b == 4 ? 0.18f : 0.36f), th, IC_ACCENT);
}

static void icon_ps_shape(Cv& c, float cx, float cy, float s, int v, uint32_t rgb) {
    float t = std::max(1.0f, s * 0.09f), k = s * 0.26f;
    if (v == P_SOUTH) { cv_line(c, cx - k, cy - k, cx + k, cy + k, t, rgb); cv_line(c, cx - k, cy + k, cx + k, cy - k, t, rgb); }
    if (v == P_EAST) cv_fill(c, cx - k - t, cy - k - t, cx + k + t, cy + k + t, rgb, 1.0f, [&](float x, float y) { return std::fabs(std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) - k) - t * 0.5f; });
    if (v == P_WEST) cv_fill(c, cx - k - t, cy - k - t, cx + k + t, cy + k + t, rgb, 1.0f, [&](float x, float y) { return std::fabs(sd_round(x, y, cx - k * 0.9f, cy - k * 0.9f, cx + k * 0.9f, cy + k * 0.9f, 0.0f)) - t * 0.5f; });
    if (v == P_NORTH) { float tv[6] = {cx, cy - k * 1.05f, cx + k * 1.05f, cy + k * 0.75f, cx - k * 1.05f, cy + k * 0.75f}; cv_tri(c, tv, rgb, t); }
}

static void icon_dpad(Cv& c, float x0, float y0, float x1, float y1, int hi) {
    float s = std::min(x1 - x0, y1 - y0), cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, a = s * 0.46f, w = s * 0.17f, th = std::max(1.0f, s * 0.06f);
    auto cross = [&](float x, float y, float g) { return std::min(sd_round(x, y, cx - a + g, cy - w + g, cx + a - g, cy + w - g, s * 0.05f), sd_round(x, y, cx - w + g, cy - a + g, cx + w - g, cy + a - g, s * 0.05f)); };
    cv_fill(c, cx - a, cy - a, cx + a, cy + a, IC_RIM, 1.0f, [&](float x, float y) { return cross(x, y, 0); });
    cv_fill(c, cx - a, cy - a, cx + a, cy + a, IC_BODY, 1.0f, [&](float x, float y) { return cross(x, y, th); });
    for (int d = D_UP; d <= D_RIGHT; d++) {
        if (!(hi & (1 << d))) continue;
        float ax0 = d == D_LEFT ? cx - a : d == D_RIGHT ? cx + w : cx - w, ax1 = d == D_LEFT ? cx - w : d == D_RIGHT ? cx + a : cx + w;
        float ay0 = d == D_UP ? cy - a : d == D_DOWN ? cy + w : cy - w, ay1 = d == D_UP ? cy - w : d == D_DOWN ? cy + a : cy + w;
        cv_round(c, ax0 + th, ay0 + th, ax1 - th, ay1 - th, s * 0.03f, IC_YELLOW);
    }
}

// one bound code drawn in a rectangle of the composed page
static void icon_draw(Cv& c, float x0, float y0, float x1, float y1, uint32_t code, int mode) {
    if (icon_override(c, code, x0, y0, x1, y1)) return;
    uint32_t t = code >> 24, v = code & 0xFFFFFF;
    bool noarrow = (mode & 0x100) != 0;
    mode &= 0xFF;
    bool glyph = mode == IM_GLYPH;
    float w = x1 - x0, h = y1 - y0, s = std::min(w, h), cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    wchar_t lab[64]; icon_label(code, lab, 64);
    // wheel in a wide, short box: the mouse at full height with its arrow beside it
    if (t == 2 && !glyph && (v == 6 || v == 7) && w > h * 1.4f) {
        float bx = cx - h * 0.765f;
        icon_mouse(c, bx, y0, bx + h, y1, 3, false);
        cv_arrow(c, bx + h * 1.12f, cy, h * 0.28f, v == 6 ? D_UP : D_DOWN, IC_ACCENT);
        return;
    }
    if (t == 2 && mode == IM_TEXT) { float k = h * 0.09f; icon_mouse(c, x0, y0 + k, x1, y1 - k, (int)v, glyph); return; }
    if (t == 2) { icon_mouse(c, x0, y0, x1, y1, (int)v, glyph); return; }
    if (t == 1) {
        if (glyph) { cv_text(c, x0, y0, x1, y1, lab, IC_TEXT, 0.8f); return; }
        float kh = mode == IM_TEXT ? h * 0.9f : std::min(h, std::max(s, w > h * 1.3f ? h : s));
        float kw = std::min(w, std::max(kh, (float)icon_text_w(lab, (int)(kh * 0.62f)) + kh * 0.45f));
        icon_keycap(c, cx - kw * 0.5f, cy - kh * 0.5f, cx + kw * 0.5f, cy + kh * 0.5f, lab);
        return;
    }
    if (t != 5 || v >= N_PADCODES) return;
    if (v >= P_DUP && v <= P_DRIGHT) {
        int d = D_UP + (int)(v - P_DUP);
        if (glyph) cv_arrow(c, cx, cy, s * 0.4f, d, IC_TEXT); else icon_dpad(c, x0, y0, x1, y1, 1 << d);
        return;
    }
    if (v >= P_LSU) {
        int d = D_UP + (int)((v - P_LSU) % 4);
        const wchar_t* n = v < P_RSU ? (ic_fam == 1 ? L"LS" : L"L") : (ic_fam == 1 ? L"RS" : L"R");
        if (!glyph) { cv_circle(c, cx, cy, s * 0.47f, IC_RIM); cv_circle(c, cx, cy, s * 0.47f - std::max(1.0f, s * 0.06f), IC_BODY); }
        cv_text(c, cx - s * 0.3f, cy - s * 0.3f, cx + s * 0.3f, cy + s * 0.3f, n, IC_TEXT, 0.6f);
        float dx = d == D_LEFT ? -1.0f : d == D_RIGHT ? 1.0f : 0.0f, dy = d == D_UP ? -1.0f : d == D_DOWN ? 1.0f : 0.0f;
        if (!noarrow) cv_arrow(c, cx + dx * s * 0.36f, cy + dy * s * 0.36f, s * 0.1f, d, IC_ACCENT);
        return;
    }
    if (v <= P_NORTH) {
        static const uint32_t col[4] = {0x6CC04A, 0xE3483C, 0x3C8DE0, 0xF0C23A};
        if (!glyph) { cv_circle(c, cx, cy, s * 0.48f, 0x5A5E66); cv_circle(c, cx, cy, s * 0.48f - std::max(1.0f, s * 0.07f), 0x1C1D21); }
        if (ic_fam == 1) cv_text(c, cx - s * 0.4f, cy - s * 0.4f, cx + s * 0.4f, cy + s * 0.4f, lab, col[v], 0.85f);
        else icon_ps_shape(c, cx, cy, glyph ? s * 1.15f : s, (int)v, IC_TEXT);
        return;
    }
    if (glyph) { cv_text(c, x0, y0, x1, y1, lab, IC_TEXT, 0.8f); return; }
    if (v == P_L3 || v == P_R3) {
        cv_circle(c, cx, cy, s * 0.47f, IC_RIM); cv_circle(c, cx, cy, s * 0.47f - std::max(1.0f, s * 0.06f), IC_BODY);
        cv_text(c, cx - s * 0.36f, cy - s * 0.36f, cx + s * 0.36f, cy + s * 0.36f, lab, IC_TEXT, 0.6f);
        return;
    }
    float ph = mode == IM_TEXT ? h * 0.85f : w > h * 1.3f ? h : h * 0.8f;
    float pw = mode == IM_TEXT ? std::min(w, std::max(ph * 1.4f, (float)icon_text_w(lab, (int)(ph * 0.6f)) + ph * 0.7f)) : w;
    float th = std::max(1.0f, ph * 0.08f);
    cv_round(c, cx - pw * 0.5f, cy - ph * 0.5f, cx + pw * 0.5f, cy + ph * 0.5f, ph * 0.45f, IC_RIM);
    cv_round(c, cx - pw * 0.5f + th, cy - ph * 0.5f + th, cx + pw * 0.5f - th, cy + ph * 0.5f - th, ph * 0.45f - th, IC_BODY);
    cv_text(c, cx - pw * 0.5f + th, cy - ph * 0.5f + th, cx + pw * 0.5f - th, cy + ph * 0.5f - th, lab, IC_TEXT, 0.72f);
}

static void icon_joined(uint32_t a, uint32_t b, wchar_t* s, int n) {
    wchar_t x[32], y[32];
    icon_label(a, x, 32);
    if (!b) { wcsncpy(s, x, n); return; }
    icon_label(b, y, 32);
    swprintf(s, n, L"%ls/%ls", x, y);
}

// the rectangle's content for the device in use: false keeps the original pixels
static bool icon_rect(Cv& c, const IconPage& pg, const IconRect& r, int ctx, float U, bool* cleared) {
    bool pad = ic_mode == 1;
    int sym = r.sym; uint32_t arg = r.arg;
    for (int i = 0; i < r.nalt; i++) {
        const IconAlt& a = r.alt[i];
        int held = ic_held;
        if (a.cond == CO_HELD ? held < 0 : !ic_follow) continue;
        sym = a.sym; arg = a.arg;
        if (sym == SY_HELD) { sym = SY_ACT; arg = (uint32_t)held; }
        break;
    }
    if (sym == SY_KEEP) return false;
    float x0 = r.x * U, y0 = r.y * U, x1 = (r.x + r.w) * U, y1 = (r.y + r.h) * U;
    // drawing stays half a game pixel inside the box: the game's quads sample a little past their edges
    float in = r.mode == IM_COVER || r.mode == IM_TEXT ? 0.0f : U * 0.5f;
    auto clear = [&]() {
        if (*cleared) return;
        *cleared = true;
        int m = r.mode == IM_COVER ? 0 : r.margin * (int)U;
        for (int y = std::max(0, (int)y0 - m); y < (int)y1 + m && y < c.h; y++)
            for (int x = std::max(0, (int)x0 - m); x < (int)x1 + m && x < c.w; x++) {
                uint32_t& d = c.p[(size_t)y * c.w + x];
                if (r.mode != IM_COVER) {
                    bool other = false;
                    for (auto& o : pg.r)
                        if (&o != &r && x >= o.x * U && x < (o.x + o.w) * U && y >= o.y * U && y < (o.y + o.h) * U) { other = true; break; }
                    if (!other) d &= 0x00FFFFFFu;
                    continue;
                }
                int xl = std::max(0, (int)x0 - 1), xr = std::min(c.w - 1, (int)x1);
                uint32_t a = c.p[(size_t)y * c.w + xl], b = c.p[(size_t)y * c.w + xr];
                float k = (x - xl) / (float)std::max(1, xr - xl);
                uint32_t o = 0;
                for (int s = 0; s < 32; s += 8) o |= (uint32_t)(((a >> s) & 0xFF) * (1 - k) + ((b >> s) & 0xFF) * k + 0.5f) << s;
                d = o;
            }
    };
    if (sym == SY_NONE) { clear(); return true; }
    if (sym == SY_ARROW) {
        clear();
        float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, s = std::min(x1 - x0, y1 - y0) - 2 * in, t = std::max(1.0f, s * 0.08f);
        float dx = arg == D_LEFT ? -1.0f : arg == D_RIGHT ? 1.0f : 0.0f, dy = arg == D_UP ? -1.0f : arg == D_DOWN ? 1.0f : 0.0f;
        float hx = cx + dx * s * 0.44f, hy = cy + dy * s * 0.44f, bx = cx - dx * s * 0.02f, by = cy - dy * s * 0.02f, k = s * 0.42f;
        float v[6] = {hx, hy, bx - dy * k, by + dx * k, bx + dy * k, by - dx * k};
        float ex = cx - dx * s * 0.42f, ey = cy - dy * s * 0.42f;
        cv_line(c, bx, by, ex, ey, s * 0.32f + 2 * t, IC_LIP);
        cv_tri(c, v, IC_LIP, 2 * t);
        cv_line(c, bx, by, ex, ey, s * 0.32f, 0xE6E9EF);
        cv_tri(c, v, 0xE6E9EF);
        return true;
    }
    // background behind a glyph: keycap on keyboard, trigger/shoulder pill or face circle on a controller
    if (sym == SY_BG) {
        int held = ic_held;
        uint32_t code = ic_follow ? binds_of(A_B_CHARGE, pad)[0] : held >= 0 && !pad ? binds_of(held, false)[0] : 0, v = code & 0xFFFFFF;
        clear();
        float bx0 = x0 + in, by0 = y0 + in, bx1 = x1 - in, by1 = y1 - in, w = bx1 - bx0, h = by1 - by0, s = std::min(w, h), cx = (bx0 + bx1) * 0.5f, cy = (by0 + by1) * 0.5f;
        if (!pad) icon_keycap(c, cx - s * 0.5f, cy - s * 0.5f, cx + s * 0.5f, cy + s * 0.5f, L"");
        else if ((code >> 24) == 5 && (v == P_L1 || v == P_R1 || v == P_L2 || v == P_R2 || v == P_SELECT || v == P_START)) {
            float ph = s * 0.66f, th = std::max(1.0f, ph * 0.08f);
            cv_round(c, bx0, cy - ph * 0.5f, bx1, cy + ph * 0.5f, ph * 0.45f, IC_RIM);
            cv_round(c, bx0 + th, cy - ph * 0.5f + th, bx1 - th, cy + ph * 0.5f - th, ph * 0.45f - th, IC_BODY);
        } else { cv_circle(c, cx, cy, s * 0.48f, 0x5A5E66); cv_circle(c, cx, cy, s * 0.48f - std::max(1.0f, s * 0.07f), 0x1C1D21); }
        return true;
    }
    // fixed controls of the Character Reference viewer: d-pad / arrow keys, right stick / WASD, L1 R1 / mouse wheel
    if (sym == SY_DEV) {
        clear();
        if (arg <= 1 && pad) { icon_dpad(c, x0 + in, y0 + in, x1 - in, y1 - in, arg ? (1 << D_LEFT) | (1 << D_RIGHT) : (1 << D_UP) | (1 << D_DOWN)); return true; }
        if (arg == 2 && pad) { icon_draw(c, x0 + in, y0 + in, x1 - in, y1 - in, C_PAD(P_RSU), r.mode | 0x100); return true; }
        if (arg == 5) {   // form: L2/R2 on a pad, Q/E on the keyboard
            wchar_t lab[64];
            if (pad) icon_joined(C_PAD(P_L2), C_PAD(P_R2), lab, 64); else icon_joined(C_KEY('Q'), C_KEY('E'), lab, 64);
            float h = y1 - y0, w = x1 - x0, kw = std::min(w, std::max(h, (float)icon_text_w(lab, (int)(h * 0.5f)) + h * 0.4f));
            icon_keycap(c, (x0 + x1 - kw) * 0.5f + in, y0 + in, (x0 + x1 + kw) * 0.5f - in, y1 - in, lab);
            return true;
        }
        if (arg >= 3) { icon_draw(c, x0 + in, y0 + in, x1 - in, y1 - in, pad ? C_PAD(arg == 3 ? P_L1 : P_R1) : C_MOUSE(arg == 3 ? 7 : 6), r.mode); return true; }
        wchar_t lab[64];
        if (arg == 2) wcscpy(lab, L"WASD");
        else icon_joined(arg ? C_KEY(VK_LEFT) : C_KEY(VK_UP), arg ? C_KEY(VK_RIGHT) : C_KEY(VK_DOWN), lab, 64);
        float h = y1 - y0, w = x1 - x0, kw = std::min(w, std::max(h, (float)icon_text_w(lab, (int)(h * 0.5f)) + h * 0.4f));
        icon_keycap(c, (x0 + x1 - kw) * 0.5f + in, y0 + in, (x0 + x1 + kw) * 0.5f - in, y1 - in, lab);
        return true;
    }
    if (sym == SY_DPAD || sym == SY_DPAD_UD || sym == SY_DPAD_LR || sym == SY_STICK_ANY) {
        if (pad) {
            clear();
            if (sym == SY_STICK_ANY) icon_draw(c, x0 + in, y0 + in, x1 - in, y1 - in, C_PAD(P_LSU), r.mode | 0x100);
            else {
                int hi = sym == SY_DPAD_UD ? (1 << D_UP) | (1 << D_DOWN) : sym == SY_DPAD_LR ? (1 << D_LEFT) | (1 << D_RIGHT) : 0;
                icon_dpad(c, x0 + in, y0 + in, x1 - in, y1 - in, hi);
            }
            return true;
        }
        uint32_t k[4] = {0, 0, 0, 0};
        static const int ord[4] = {D_UP, D_DOWN, D_LEFT, D_RIGHT};
        for (int i = 0; i < 4; i++) { int a = icon_action(ctx, SY_STICK, (uint32_t)ord[i], false); k[i] = a >= 0 ? binds_of(a, false)[0] : 0; }
        wchar_t lab[64];
        if (sym == SY_DPAD_UD) icon_joined(k[0], k[1], lab, 64);
        else if (sym == SY_DPAD_LR) icon_joined(k[2], k[3], lab, 64);
        else { lab[0] = 0; static const int wasd[4] = {0, 2, 1, 3}; for (int i : wasd) if (k[i]) { wchar_t x[32]; icon_label(k[i], x, 32); if (wcslen(x) == 1) wcscat(lab, x); } }
        if (!lab[0]) return false;
        clear();
        float h = y1 - y0, w = x1 - x0, kw = std::min(w, std::max(h, (float)icon_text_w(lab, (int)(h * 0.5f)) + h * 0.4f));
        icon_keycap(c, (x0 + x1 - kw) * 0.5f + in, y0 + in, (x0 + x1 + kw) * 0.5f - in, y1 - in, lab);
        return true;
    }
    // every pictured PSP button is redrawn: the context's action, then the other contexts, then the native button
    uint32_t native = icon_native(sym, arg), code = 0;
    static const int alt[3] = {CTX_BATTLE, CTX_MENU, CTX_STORY};
    int a = icon_action(ctx, sym, arg, pad);
    for (int i = 0; a < 0 && sym != SY_ACT && i < 3; i++) if (alt[i] != ctx) a = icon_action(alt[i], sym, arg, pad);
    if (a >= 0) {
        const uint32_t* b = binds_of(a, pad);
        code = b[0];
        bool dirs = sym == SY_STICK || (sym == SY_PSP && (arg & (PB_UP | PB_DOWN | PB_LEFT | PB_RIGHT)));
        if (pad && dirs) for (int i = 0; i < N_BINDS; i++) if (native && b[i] == native) code = native;
    }
    if (!code && pad) code = native;
    if (!code) return false;
    clear();
    icon_draw(c, x0 + in, y0 + in, x1 - in, y1 - in, code, r.mode);
    return true;
}

bool icons_compose(int page, uint32_t id, int w0, int h0, int w, int h, const uint32_t* px, bool psp, int* ow, int* oh, const uint32_t** opx) {
    if (page < 0 || w0 <= 0 || h0 <= 0 || w <= 0 || h <= 0) return false;
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    if (page >= (int)icon_pages.size()) return false;
    uint32_t ep = icon_force_ep ? icon_force_ep : icons_epoch(page);
    const IconPage& pg0 = icon_pages[page];
    int pk = pg0.slot >= 0 || icon_force_ep ? 0 : icons_player();
    if (pk) { ic_mode = icon_mode_pl[pk]; ic_fam = icon_fam_pl[pk]; ic_held = icon_held_pl[pk]; ic_follow = icon_follow_pl[pk]; }
    else { ic_mode = pl_mode[0]; ic_fam = pl_family[0]; ic_held = icon_held.load(); ic_follow = icon_follow.load(); }
    if (icon_force_ep) { ic_held = icon_force_held; ic_follow = icon_force_follow; }
    auto& vs = icon_done[id ^ ((uint32_t)pk << 28)];
    auto give = [&](const IconDone& d) { if (!d.any) return false; *ow = d.w; *oh = d.h; *opx = d.px.data(); return true; };
    for (size_t i = 0; ep && i < vs.size(); i++) if (vs[i]->ep == ep) { auto d = vs[i]; vs.erase(vs.begin() + (long)i); vs.insert(vs.begin(), d); return give(*d); }
    auto keep = [&](const std::shared_ptr<IconDone>& d) { vs.insert(vs.begin(), d); if (vs.size() > 6) vs.pop_back(); };
    uint64_t ck = 1469598103934665603ull;
    { auto mix = [&](uint64_t v) { ck = (ck ^ v) * 1099511628211ull; };
      mix((uint64_t)page); mix((uint64_t)pk); mix(ep); mix((uint64_t)w0 << 32 | (uint32_t)h0); mix((uint64_t)w << 32 | (uint32_t)h); mix(psp);
      size_t n = (size_t)w * h; uint64_t hh = 0x9E3779B97F4A7C15ull;
      for (size_t i = 0; i < n; i++) hh = (hh ^ px[i]) * 0x100000001B3ull + (hh >> 29);   // every pixel: sparse samples can match on transparent edges
      mix(hh); }
    auto is = icon_same.find(ck);
    if (is != icon_same.end() && is->second->ep == ep && ep) { keep(is->second); return give(*is->second); }
    auto slot = std::make_shared<IconDone>();
    keep(slot);
    if (icon_same.size() > 64) icon_same.clear();
    icon_same[ck] = slot;
    IconDone& dn = *slot;
    PERF_SUB("iconos de botones");
    const IconPage& pg = icon_pages[page];
    int U = std::max(1, std::min(8, 2048 / std::max(w0, h0)));
    int W = w0 * U, H = h0 * U;
    dn.px.assign((size_t)W * H, 0);
    auto rs0 = std::chrono::steady_clock::now();
    std::vector<int> xa_(W), xb_(W); std::vector<float> fx_(W);
    for (int x = 0; x < W; x++) {
        float sx = (x + 0.5f) * w / W - 0.5f;
        int xa = std::max(0, std::min(w - 1, (int)std::floor(sx)));
        xa_[x] = xa; xb_[x] = std::min(w - 1, xa + 1); fx_[x] = std::max(0.0f, std::min(1.0f, sx - xa));
    }
    uint32_t* out = dn.px.data();
    auto rows = [&](int ry0, int ry1) {
        for (int y = ry0; y < ry1; y++) {
            float sy = (y + 0.5f) * h / H - 0.5f;
            int ya = std::max(0, std::min(h - 1, (int)std::floor(sy))), yb = std::min(h - 1, ya + 1);
            float fy = std::max(0.0f, std::min(1.0f, sy - ya));
            const uint32_t* ra = px + (size_t)ya * w; const uint32_t* rb = px + (size_t)yb * w;
            for (int x = 0; x < W; x++) {
                int xa = xa_[x], xb = xb_[x]; float fx = fx_[x];
                uint32_t q[4] = {ra[xa], ra[xb], rb[xa], rb[xb]};
                float wt[4] = {(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
                float A = 0, C[3] = {0, 0, 0};
                for (int i = 0; i < 4; i++) {
                    float a = (float)(q[i] >> 24) * (psp ? 2.0f : 1.0f) * wt[i];
                    A += a;
                    for (int s = 0; s < 3; s++) C[s] += (float)((q[i] >> (s * 8)) & 0xFF) * a;
                }
                uint32_t o = std::min(255u, (uint32_t)(A + 0.5f)) << 24;
                if (A > 0) for (int s = 0; s < 3; s++) o |= std::min(255u, (uint32_t)(C[s] / A + 0.5f)) << (s * 8);
                out[(size_t)y * W + x] = o;
            }
        }
    };
    int nt = (int)std::thread::hardware_concurrency();
    nt = std::max(1, std::min(8, nt - 1));
    if ((size_t)W * H < 65536) nt = 1;
    if (nt > 1) {
        std::vector<std::thread> th;
        for (int k = 1; k < nt; k++) th.emplace_back(rows, H * k / nt, H * (k + 1) / nt);
        rows(0, H / nt);
        for (auto& t : th) t.join();
    } else rows(0, H);
    auto rs1 = std::chrono::steady_clock::now();
    Cv c{W, H, dn.px.data()};
    int ctx = pg.ctx >= 0 ? pg.ctx : icon_ctx.load();
    // text glyphs follow the screen: battle or story bindings in play, menu bindings in menus (combos keep battle, combo_codes)
    if (pg.ctx == -2) ctx = ctx == CTX_STORY ? CTX_STORY : ctx == CTX_MENU ? CTX_MENU : CTX_BATTLE;
    int n = 0;
    for (auto& r : pg.r) { bool cl = false; if (icon_rect(c, pg, r, ctx, (float)U, &cl)) n++; }
    if (pg.slot >= 0 && slot_code[pg.slot]) {
        for (auto& p : dn.px) p = 0;
        icon_draw(c, U * 0.5f, U * 0.5f, U * 13.5f, U * 13.5f, slot_code[pg.slot], IM_ICON);
        n = 1;
    }
    if (psp) for (auto& p : dn.px) p = (p & 0x00FFFFFFu) | ((((p >> 24) + 1) >> 1) << 24);
    dn.ep = ep; dn.w = W; dn.h = H; dn.any = n > 0;
    if (icon_log_n < 60) { icon_log_n++; printf("icons: %s %s jugador %d, %d de %d botones | %dx%d, estado %08X ctx %d mantenido %d aviso %d%s, remuestreo %.1f ms en %d hilos, botones %.1f ms\n", pg.slot >= 0 ? "slot" : pg.pat.c_str(), ic_mode == 1 ? (ic_fam == 1 ? "xbox" : "playstation") : "teclado", pk + 1, n, (int)pg.r.size(),
                                              W, H, ep, icon_ctx.load(), ic_held, ic_follow, icon_force_ep ? " (en la carga)" : "", std::chrono::duration<double, std::milli>(rs1 - rs0).count(), nt, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rs1).count()); }
    if (!dn.any) { dn.px.clear(); dn.px.shrink_to_fit(); return false; }
    *ow = W; *oh = H; *opx = dn.px.data();
    return true;
}

// texts: each run of glyphs joined by '+' that the game draws for a command becomes the bound combination
static uint32_t first_bind(int a, bool pad) {
    const uint32_t* l = binds_of(a, pad);
    for (int k = 0; k < N_BINDS; k++) if (l[k] && ((l[k] >> 24) == 5) == pad) return l[k];
    return 0;
}

static bool combo_codes(const uint16_t* t, int n, std::vector<uint32_t>& out) {
    uint32_t mask = 0; int dir = 0; bool dbl = false;
    for (int i = 0; i < n; i++) {
        uint32_t m = 0; int d = 0;
        switch (t[i]) {
            case 0xFF68: m = PB_START; break; case 0xFF69: m = PB_SELECT; break;
            case 0xFF6A: m = PB_CIR; break; case 0xFF6B: m = PB_SQU; break; case 0xFF6C: m = PB_TRI; break; case 0xFF6D: m = PB_CRO; break;
            case 0xFF6E: m = PB_L; break; case 0xFF6F: m = PB_R; break;
            case 0xFF71: d = D_UP; break; case 0xFF72: d = D_DOWN; break; case 0xFF73: d = D_RIGHT; break; case 0xFF74: d = D_LEFT; break;
            case 0xFF75: m = PB_CIR; dbl = true; break; case 0xFF76: m = PB_SQU; dbl = true; break;
            case 0xFF77: m = PB_TRI; dbl = true; break; case 0xFF78: m = PB_CRO; dbl = true; break;
            case 0xFF7B: m = PB_UP; break; case 0xFF7C: m = PB_DOWN; break; case 0xFF7D: m = PB_RIGHT; break; case 0xFF7E: m = PB_LEFT; break;
            default: return false;
        }
        if (m & mask) dbl = true;
        mask |= m;
        if (d) { if (dir) return false; dir = d; }
    }
    int best = -1;
    for (int a = 0; a < N_ACTIONS; a++) {
        const Action& A = ACT[a];
        if ((A.tab != TAB_BATTLE && A.tab != TAB_SPECIAL) || !(A.base | A.out) || (A.base | A.out) != mask || A.dir != dir || (A.mk == MK_TWICE) != dbl) continue;
        if (best < 0 || A.tab == TAB_SPECIAL) best = a;
    }
    if (best < 0) return false;
    bool pad = pl_mode[0] == 1;
    const Action& A = ACT[best];
    auto push = [&](uint32_t c) { if (c) out.push_back(c); return c != 0; };
    if (pad && A.tab == TAB_SPECIAL) {
        int m = best <= A_S_DEX ? A_B_GUARD : best <= A_S_AT3 ? A_B_BLAST : best <= A_S_FU3 ? A_B_FUSION : -1;
        if (m >= 0) return push(first_bind(A_B_CHARGE, true)) && (!A.dir || push(C_PAD(P_LSU + A.dir - 1))) && push(first_bind(m, true));
        return push(first_bind(A_B_TRANSF, true)) && (!A.dir || push(C_PAD(P_LSU + A.dir - 1)));
    }
    int kind = pad ? A.k_pad : A.k_kb, from = pad ? A.from_pad : A.from_kb, mod = pad ? A.mod_pad : A.mod_kb;
    if (from >= 0) { uint32_t c = first_bind(from, pad); return push(c) && (kind != K_DOUBLE || push(c)); }
    if (kind == K_NONE) return false;
    return (mod < 0 || push(first_bind(mod, pad))) && push(first_bind(best, pad));
}

static int slot_for(uint32_t code) {
    int best = 0;
    for (int k = 0; k < N_SLOTS; k++) {
        if (slot_code[k] == code) { slot_use[k] = ++slot_tick; return k; }
        if (slot_use[k] < slot_use[best]) best = k;
    }
    slot_code[best] = code; slot_use[best] = ++slot_tick; slot_gen++;
    return best;
}

// glyph character that draws the icon of one control (keyboard key, mouse button, pad input), 0 when icons are off
int icons_glyph_of(uint32_t code) {
    if (!patch_button_icons.val || !psp_mem || !icons_glyph_table()) return 0;
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    return 0xFF67 + SLOT_BASE + slot_for(code);
}

// returns a guest copy of the string with its combos rewritten, or 0 to keep it
uint32_t icons_text(uint32_t s) {
    if (!patch_button_icons.val || !psp_mem || s < 0x08800000u || s >= 0x0FF00000u) return 0;
    auto glyph = [](uint16_t ch) { return ch >= 0xFF68 && ch <= 0xFF7E; };
    uint16_t in[512];
    int n = 0;
    bool cand = false;
    for (; n < 511; n++) {
        uint16_t ch = psp_l16(s + 2 * n);
        if (!ch) break;
        in[n] = ch;
        if (n >= 2 && in[n - 1] == '+' && glyph(in[n - 2]) && glyph(ch)) cand = true;
    }
    if (!cand || n >= 511 || !icons_glyph_table()) return 0;
    std::lock_guard<std::recursive_mutex> lock_(icon_mx);
    static std::unordered_map<uint64_t, uint32_t> cache;
    static uint32_t ring = 0, ring_pos = 0;
    const uint32_t RING = 65536;
    uint64_t key = 1469598103934665603ull ^ icon_ep.load() ^ ((uint64_t)slot_gen << 32);
    for (int i = 0; i < n; i++) key = (key ^ in[i]) * 1099511628211ull;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    std::vector<uint16_t> o;
    bool changed = false;
    for (int i = 0; i < n;) {
        if (!glyph(in[i])) { o.push_back(in[i++]); continue; }
        int j = i;
        while (j + 2 < n && in[j + 1] == '+' && glyph(in[j + 2])) j += 2;
        uint16_t t[256]; int nt = 0;
        for (int k = i; k <= j; k += 2) t[nt++] = in[k];
        std::vector<uint32_t> codes;
        if (nt >= 2 && combo_codes(t, nt, codes)) {
            for (size_t k = 0; k < codes.size(); k++) { if (k) o.push_back('+'); o.push_back((uint16_t)(0xFF67 + SLOT_BASE + slot_for(codes[k]))); }
            changed = true;
        } else for (int k = i; k <= j; k++) o.push_back(in[k]);
        i = j + 1;
    }
    uint32_t r = 0;
    if (changed) {
        uint32_t need = (uint32_t)(o.size() + 1) * 2;
        if (!ring) ring = psp_reserve(RING);
        if (ring_pos + need > RING) { ring_pos = 0; cache.clear(); }
        r = ring + ring_pos;
        o.push_back(0);
        memcpy(psp_p(r), o.data(), need);
        ring_pos = (ring_pos + need + 3) & ~3u;
    }
    if (cache.size() > 4096) cache.clear();
    cache[key] = r;
    return r;
}
