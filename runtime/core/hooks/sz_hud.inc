// SZ battle HUD (mod sz_hud): SZHud/layout.txt evaluated by ui/sz_layout.h, drawn with the bt3_hud GE writer
#include "ui/sz_layout.h"
#include <tuple>
#include <set>
#include <climits>
#include <mutex>
#include <thread>
#include "disc/fonttext.h"

static SzLayout sz_lay;
static int sz_state = 0;   // 0 not read, 1 ok, -1 failed
static std::vector<std::string> sz_names;   // every T_UI_ name in layout.txt and the numbered families: loaded by the warm-up

static bool sz_load() {
    if (sz_state) return sz_state > 0;
    std::string f = psp_cr_file("SZHud/layout.txt"), t;
    if (FILE* fp = f.empty() ? nullptr : fopen(f.c_str(), "rb")) {
        char b[65536];
        size_t n;
        while ((n = fread(b, 1, sizeof b, fp)) > 0) t.append(b, n);
        fclose(fp);
    }
    sz_state = !t.empty() && sz_lay.parse(t) ? 1 : -1;
    {
        std::set<std::string> u;
        for (size_t i = t.find("T_UI_"); i != std::string::npos; i = t.find("T_UI_", i + 5)) {
            size_t e = i + 5;
            while (e < t.size() && (isalnum((unsigned char)t[e]) || t[e] == '_')) e++;
            u.insert(t.substr(i, e - i));
        }
        static const struct { const char* f; int a, b; } F[] = {{"T_UI_TimeNum_%02d", 0, 9}, {"T_UI_ComboNum_%02d", 0, 9}, {"T_UI_DamageoNum_%02d", 0, 9},
            {"T_UI_HUD_Buff_%02d", 1, 4}, {"T_UI_HUD_Debuff_%02d", 1, 4}, {"T_UI_BuNum_%02d", 0, 10}, {"T_UI_HpStock_%02d", 0, 8},
            {"T_UI_HpGauge_Bar_%02d", 1, 9}, {"T_UI_SkillGuide_Num_%02d", 0, 9}};
        char b[48];
        for (auto& x : F) for (int v = x.a; v <= x.b; v++) { snprintf(b, sizeof b, x.f, v); u.insert(b); }
        sz_names.assign(u.begin(), u.end());
    }
    printf("hud sz: %s (%d clases, %d materiales)\n", sz_state > 0 ? "diseno leido" : "layout.txt invalido", (int)sz_lay.cls.size(), (int)sz_lay.mat.size());
    return sz_state > 0;
}

// M_UI_Gauge on the CPU: base x mask alpha, cut where the filter shifted by the value (white = shown) says; two buffers per gauge
struct SzGTab;
struct SzGauge { uint32_t buf[2] = {0, 0}; int cur = 0; float val = -2; SzS base; BhTex t{0, 0, 0}; bool full[2] = {false, false}; const SzGTab* tab = nullptr; const BhTex *tb = nullptr, *tm = nullptr, *tf = nullptr; bool tv = false; uint32_t tgen = 0; std::vector<int> ids[2]; };
static std::map<std::string, SzGauge, std::less<>> sz_g[4];   // per bar: the bottom copies compose their own gauges
static inline uint32_t sz_px(const BhTex* t, float u, float v) {
    int x = std::max(0, std::min(t->ow - 1, (int)(u * t->ow))), y = std::max(0, std::min(t->oh - 1, (int)(v * t->oh)));
    return psp_l32(t->a + 4u * (uint32_t)(y * t->w + x));
}
static inline float sz_pos(bool vert, float u, float v) { return vert ? 1 - v : u; }
static inline int sz_cut(const BhTex* f, float s, float v) { return s < 0 ? 255 : s >= 1 ? 0 : f ? (int)(sz_px(f, s, v) & 255) : (s < 0.5f ? 255 : 0); }
// values where the visible shape starts and ends being cut: the gauge value 0..1 is spread over that span, not over the texture width
struct SzSpan { float lo = 0, hi = 1; };
// per gauge (base, mask, filter, direction): base x mask alpha and colour per texel, filter row per line, span; built once
struct SzGTab { int ow = 0, oh = 0, w = 0, fw = 0, fh = 0, fow = 0; uint32_t fa = 0; std::vector<uint8_t> a0; std::vector<uint32_t> rgb; std::vector<int> frow, funi; SzSpan sp; };
static int sz_gtab_old = -1;
static inline int sz_tcut(const SzGTab& T, const uint32_t* F, int y, float s) {
    if (s < 0) return 255;
    if (s >= 1) return 0;
    if (!F) return s < 0.5f ? 255 : 0;
    int x = std::max(0, std::min(T.fow - 1, (int)(s * T.fow)));
    return (int)(F[T.frow[y] + x] & 255);
}
static const SzGTab& sz_gtab(const BhTex* b, const BhTex* m, const BhTex* f, bool vert, const std::string& name) {
    static std::map<std::tuple<uint32_t, uint32_t, uint32_t, bool>, SzGTab> M;
    static uint32_t gen = 0;
    if (gen != bh_gen) { M.clear(); gen = bh_gen; }
    auto key = std::make_tuple(b->a, m ? m->a : 0u, f ? f->a : 0u, vert);
    auto it = M.find(key);
    if (it != M.end()) return it->second;
    auto lt0 = std::chrono::steady_clock::now();
    SzGTab& T = M[key];
    T.ow = b->ow; T.oh = b->oh; T.w = b->w;
    T.a0.resize((size_t)T.ow * T.oh); T.rgb.resize((size_t)T.ow * T.oh); T.frow.resize(T.oh);
    if (f) { T.fa = f->a; T.fw = f->w; T.fh = f->oh; T.fow = f->ow; }
    for (int y = 0; y < T.oh; y++) {
        float v = (y + 0.5f) / T.oh;
        if (f) T.frow[y] = std::max(0, std::min(f->oh - 1, (int)(v * f->oh))) * f->w;
        for (int x = 0; x < T.ow; x++) {
            float u = (x + 0.5f) / T.ow;
            uint32_t p = psp_l32(b->a + 4u * (uint32_t)(y * b->w + x));
            int a = (int)(p >> 24);
            if (m) a = a * (int)(sz_px(m, u, v) >> 24) / 255;
            T.a0[(size_t)y * T.ow + x] = (uint8_t)a; T.rgb[(size_t)y * T.ow + x] = p & 0xFFFFFFu;
        }
    }
    const uint32_t* F = f ? (const uint32_t*)psp_p(f->a) : nullptr;
    if (F) {   // filter columns with one value on every line the gauge reads: a shift that keeps it changes nothing there
        T.funi.assign(T.fow, -1);
        for (int k = 0; k < T.fow; k++) {
            int v0 = (int)(F[T.frow[0] + k] & 255);
            bool same = true;
            for (int y = 1; y < T.oh && same; y++) same = (int)(F[T.frow[y] + k] & 255) == v0;
            if (same) T.funi[k] = v0;
        }
    }
    auto shown = [&](float q, bool all) {
        for (int y = 0; y < T.oh; y += 2) {
            float v = (y + 0.5f) / T.oh;
            for (int x = 0; x < T.ow; x += 2) {
                if (T.a0[(size_t)y * T.ow + x] < 16) continue;
                bool on = sz_tcut(T, F, y, sz_pos(vert, (x + 0.5f) / T.ow, v) - q + 0.5f) > 127;
                if (all ? !on : on) return !all;
            }
        }
        return all;
    };
    SzSpan r;
    float lo = 0, hi = 1.5f;
    for (int i = 0; i < 14; i++) { float c = (lo + hi) * 0.5f; if (shown(c, true)) hi = c; else lo = c; }
    r.hi = hi;
    lo = -0.5f; hi = r.hi;
    for (int i = 0; i < 14; i++) { float c = (lo + hi) * 0.5f; if (shown(c, false)) hi = c; else lo = c; }
    r.lo = lo;
    if (r.hi - r.lo < 0.05f) r = SzSpan();
    T.sp = r;
    printf("hud sz: medidor %s de %.3f a %.3f\n", name.c_str(), r.lo, r.hi);
    bh_late_log("tabla de medidor", name.c_str(), lt0);
    return T;
}
static const BhTex* sz_gauge(const SzItem& it, int slot) {
    const BhTex* b = bh_gets(it.tex);
    if (!b) return nullptr;
    SzGauge& g = sz_g[slot][it.key];
    const BhTex* m = it.mask.empty() ? nullptr : bh_gets(it.mask);
    const BhTex* f = it.filter.empty() ? nullptr : bh_gets(it.filter);
    if (sz_gtab_old < 0) { const char* e = getenv("TTT_SZ_GAUGE_OLD"); sz_gtab_old = e && *e ? atoi(e) : 0; }
    if (!g.tab || g.tb != b || g.tm != m || g.tf != f || g.tv != it.vert || g.tgen != bh_gen) {
        g.tab = &sz_gtab(b, m, f, it.vert, it.tex.str() + "|" + it.mask.str() + "|" + it.filter.str() + (it.vert ? "|v" : ""));
        g.tb = b; g.tm = m; g.tf = f; g.tv = it.vert; g.tgen = bh_gen;
        g.full[0] = g.full[1] = false;
    }
    const SzGTab& T = *g.tab;
    SzSpan sp = T.sp;
    float q = roundf((it.value >= 1 ? sp.hi : sp.lo + it.value * (sp.hi - sp.lo)) * 512) / 512;
    if (g.t.a && g.val == q && g.base == it.tex) return &g.t;
    if (!g.buf[0] || g.t.w != b->w || g.t.h != b->h) BH_LATE("memoria de medidor", it.key.c_str(), psp_free(g.buf[0]); psp_free(g.buf[1]); g.buf[0] = psp_alloc((uint32_t)(b->w * b->h * 4)); g.buf[1] = psp_alloc((uint32_t)(b->w * b->h * 4)); g.full[0] = g.full[1] = false);
    g.cur ^= 1;
    uint32_t o = g.buf[g.cur];
    if (sz_gtab_old) {   // A/B: the per-texel sampling of the first version
        for (int y = 0; y < b->oh; y++) {
            float v = (y + 0.5f) / b->oh;
            for (int x = 0; x < b->ow; x++) {
                float u = (x + 0.5f) / b->ow, s = sz_pos(it.vert, u, v) - q + 0.5f;
                uint32_t p = psp_l32(b->a + 4u * (uint32_t)(y * b->w + x));
                int a = (int)(p >> 24);
                if (m) a = a * (int)(sz_px(m, u, v) >> 24) / 255;
                a = a * sz_cut(f, s, v) / 255;
                psp_s32(o + 4u * (uint32_t)(y * b->w + x), (p & 0xFFFFFFu) | (uint32_t)a << 24);
            }
        }
        g.full[g.cur] = false;
    } else {
        uint32_t* O = (uint32_t*)psp_p(o);
        const uint32_t* F = f ? (const uint32_t*)psp_p(f->a) : nullptr;
        bool all = !g.full[g.cur];   // the first fill of a buffer writes every texel (filtering reads the colour of the clear ones too)
        static uint8_t div[65536];   // a * cut / 255
        static bool dv = false;
        if (!dv) { dv = true; for (int i = 0; i < 65536; i++) div[i] = (uint8_t)((i >> 8) * (i & 255) / 255); }
        // per column (per line when vertical): -1 shown, -2 cut, else the filter index; id = what decides its texels
        int n = it.vert ? T.oh : T.ow;
        std::vector<int>& ids = g.ids[g.cur];
        if ((int)ids.size() != n) { ids.assign(n, INT32_MIN); all = true; }
        static int written = 0, lines = 0, calls = 0;
        for (int i = 0; i < n; i++) {
            float s = it.vert ? 1 - (i + 0.5f) / T.oh - q + 0.5f : (i + 0.5f) / T.ow - q + 0.5f;
            int k = s < 0 ? -1 : s >= 1 ? -2 : !F ? (s < 0.5f ? -1 : -2) : std::max(0, std::min(T.fow - 1, (int)(s * T.fow)));
            int id = k == -1 ? 255 : k == -2 ? 0 : it.vert ? (int)(F[T.frow[i] + k] & 255) : T.funi[k] >= 0 ? T.funi[k] : 1000 + k;
            if (!all && ids[i] == id) continue;
            ids[i] = id;
            lines++;
            if (it.vert) {
                int cut = id;
                const uint8_t* A = &T.a0[(size_t)i * T.ow];
                const uint32_t* C = &T.rgb[(size_t)i * T.ow];
                uint32_t* D = O + (size_t)i * T.w;
                for (int x = 0; x < T.ow; x++) if (all || A[x]) { D[x] = C[x] | (uint32_t)div[A[x] << 8 | cut] << 24; written++; }
            } else {
                for (int y = 0; y < T.oh; y++) {
                    size_t j = (size_t)y * T.ow + i;
                    uint8_t a = T.a0[j];
                    if (!all && !a) continue;
                    int cut = k == -1 ? 255 : k == -2 ? 0 : (int)(F[T.frow[y] + k] & 255);
                    O[(size_t)y * T.w + i] = T.rgb[j] | (uint32_t)div[a << 8 | cut] << 24;
                    written++;
                }
            }
        }
        if (++calls == 600) { static int logs = 0; if (logs++ < 6) printf("hud sz: medidores, media de %d lineas y %d texeles por composicion\n", lines / calls, written / calls); calls = lines = written = 0; }
        g.full[g.cur] = true;
    }
    static uint32_t ver = 0;   // the GE texture cache keys on 24 sampled words: a new stamp in a transparent texel makes every composite a new texture
    ver = (ver + 1) & 0xFFFFFF;
    uint32_t sp_at = (psp_l32(o) >> 24) == 0 ? o : o + 4u * (uint32_t)((b->oh - 1) * b->w + b->ow - 1);
    if ((psp_l32(sp_at) >> 24) == 0) psp_s32(sp_at, ver);
    g.t = *b; g.t.a = o; g.val = q; g.base = it.tex;
    return &g.t;
}

// at the HUD load (loading screen): every texture and gauge table the panels, timer, combo and technique list can use, so none
// is made mid battle; each state is swept over the first 2 s of its animations (pieces shown only for a few frames count too)
static const BhTex* sz_cpart();
static void sz_list_bake();
static void sz_warm() {
    static uint32_t done = 0;
    if (done == bh_gen) return;
    auto t0 = std::chrono::steady_clock::now();
    int nth = bh_preload(sz_names);
    double tload = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::vector<SzItem> v;
    for (int st = 0; st < 6; st++) for (int k = 0; k <= 60; k += st == 0 ? 60 : 1) {
        float t = k / 30.0f;
        SzData d;
        d.hp = st ? 21000 : 30000; d.trail = st ? 24000 : 30000; d.ki = st == 5 ? 100000 : st ? 55000 : 40000; d.kitrail = d.ki; d.energy = st == 5 ? 500000 : st ? (k % 11) * 100000 : 0; d.senzu = st ? 1 : 2;
        d.orange = st == 2 ? 15000 : 0; d.charging = st == 2 && (k & 1); d.spark = st == 3; d.now = 100.0f + t; d.nw = 40; d.nh = 11;
        d.t_show = st == 1 ? t : 5; d.t_hide = st == 4 ? t : -1;
        d.t_hit = d.t_break = d.t_unit = d.t_max = d.t_egain = d.t_estart = st ? t : -1;
        d.t_spark = st == 3 ? t : -1; d.t_unspark = st == 4 ? t : -1; d.t_charge = st == 2 ? t : -1; d.t_uncharge = st == 4 ? t : -1;
        for (int i = 0; i < 5; i++) d.cell[i] = st ? t : -1;
        d.tm = st == 5 ? -1 : st == 3 ? 9 - k % 10 : 155 - k; d.tchg[0] = t;
        d.hits = st ? 7 + k * 3 : 0; d.dmg = st ? 1234 + k * 987 : 0; d.t_cshow = d.t_dshow = st ? t : -1; d.t_chide = d.t_dhide = st == 4 ? t : -1;
        d.nbuf = st ? 6 : 0; for (int i = 0; i < 6; i++) { d.buf[i] = (i + k) % 8; d.bshow[i] = t; d.bout[i] = st == 4 ? t : -1; }
        for (int s = 0; s < 2; s++) { sz_lay.panel(s, d, v); sz_lay.fx(s, d, v); sz_lay.combo(s, d, v); }
        sz_lay.whole("WBP_Rep_TimeCount", d, v);
        sz_lay.boost(100, 100, t, st == 4 ? t : -1, v);
    }
    for (int b = 1; b <= 9; b++) for (int sp = 0; sp < 2; sp++) {   // every life bar colour; sparking with its gauge running
        SzData d;
        d.hp = b * 10000 - 3000; d.trail = d.hp + 3000; d.ki = 100000; d.kitrail = d.ki; d.energy = 300000; d.senzu = 1;
        d.spark = sp; d.orange = sp ? 15000 : 0; d.now = 101; d.nw = 40; d.nh = 11; d.t_show = 5; d.t_hit = 0.1f; d.t_spark = sp ? 2 : -1;
        for (int s = 0; s < 2; s++) { sz_lay.panel(s, d, v); sz_lay.fx(s, d, v); }
    }
    for (int lk = 0; lk < 2; lk++) {
        SzRow R[6]; SzLKey keys[6];
        static const int TY[5] = {0, 0, 1, 1, 2};
        for (int co = 0; co < 2; co++) {
            for (int i = 0; i < 5; i++) { R[i].type = TY[i]; R[i].tw = 100; R[i].th = 20; R[i].cost = i + 5 * co; R[i].lock = lk != 0; R[i].t_on = 0.1f; }
            for (int k = 0; k <= 30; k++) SzList::build(R, 5, 100.0f + k / 30.0f, k / 30.0f, 1, v, keys);
        }
    }
    std::unordered_set<const std::string*> seen;
    std::set<std::tuple<const void*, const void*, const void*, bool>> gs;
    int nt = 0, ng = 0;
    for (auto& it : v) {
        if (it.tex.empty() || it.tex[0] == '@') continue;
        bool first = seen.insert(it.tex.p).second;
        const BhTex* b = bh_gets(it.tex);
        nt += first;
        if (it.value < 0 || !b) continue;
        const BhTex* m = it.mask.empty() ? nullptr : bh_gets(it.mask);
        const BhTex* f = it.filter.empty() ? nullptr : bh_gets(it.filter);
        if (!gs.insert(std::make_tuple((const void*)b, (const void*)m, (const void*)f, it.vert)).second) continue;
        sz_gtab(b, m, f, it.vert, it.tex.str() + "|" + it.mask.str() + "|" + it.filter.str() + (it.vert ? "|v" : ""));
        ng++;
    }
    sz_lay.cache.clear();
    sz_cpart();
    sz_list_bake();
    bh_get("T_UI_NameBase");
    done = bh_gen;
    printf("hud sz: texturas y medidores preparados en %.0f ms (%d texturas, %d medidores; carga %.0f ms en %d hilos)\n", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), nt, ng, tload, nth);
}
static bool sz_opaque(const BhTex* t) {
    static std::map<uint32_t, bool> M;
    auto it = M.find(t->a);
    if (it != M.end()) return it->second;
    bool op = true;
    for (int y = 0; y < t->oh && op; y += 2) for (int x = 0; x < t->ow; x += 2) if ((psp_l32(t->a + 4u * (uint32_t)(y * t->w + x)) >> 24) < 250) { op = false; break; }
    return M[t->a] = op;
}
// one quad in screen units (480x272), corners in order top left, top right, bottom right, bottom left
static void sz_quad(const BhTex* t, const SzItem& it) {
    if (t->a != bh_cur_tex) {
        bh_w(0xA0, t->a); bh_w(0xA8, ((t->a >> 24) & 0xF) << 16 | (uint32_t)t->w);
        bh_w(0xB8, (uint32_t)(__builtin_ctz(t->h) << 8 | __builtin_ctz(t->w))); bh_w(0xCB, 0);
        bh_cur_tex = t->a;
    }
    if ((int)it.add != bh_cur_add) { bh_w(0xDF, it.add ? 2u | (10u << 4) : 2u | (3u << 4)); if (it.add) bh_w(0xE1, 0xFFFFFF); bh_cur_add = it.add; }
    auto c8 = [](float v) { return (uint32_t)(std::max(0.0f, std::min(1.0f, v)) * 255 + 0.5f); };
    uint32_t col = c8(it.a) << 24 | c8(it.b) << 16 | c8(it.g) << 8 | c8(it.r);
    float uu[4] = {it.u0 * t->ow, it.u1 * t->ow, it.u1 * t->ow, it.u0 * t->ow}, vv[4] = {it.v0 * t->oh, it.v0 * t->oh, it.v1 * t->oh, it.v1 * t->oh};
    uint32_t v = bh_vbuf + bh_vpos;
    bh_vpos = (bh_vpos + 24 * 4) % (4 * 0x8000);
    for (int k = 0; k < 4; k++) {
        uint32_t b = v + 24 * k;
        float f[6] = {uu[k], vv[k], 0, it.x[k], it.y[k], 0};
        memcpy(psp_p(b), &f[0], 8); psp_s32(b + 8, col); memcpy(psp_p(b + 12), &f[3], 12);
    }
    bh_w(0x10, ((v >> 24) & 0xF) << 16); bh_w(0x01, v); bh_w(0x04, (5u << 16) | 4u);
}

// portrait (IMG_Face): the fighter's big render (the generated one when Renders has it) cut around the head and masked
// (T_UI_Face_Mask_P1/P2), built off the game thread once per character and colour; white copy for the AddRate flash
std::string psp_big_png(int sysid);
bool psp_render_big_gen(int s, int c, std::vector<uint32_t>& px, int* w, int* h);
static const int SZ_FS = 256;
struct SzFace { int key = -1; bool have = false; uint32_t buf[2] = {0, 0}; BhTex t{0, 0, 0}, wt{0, 0, 0}; };
struct SzFaceRes { int key = -1; std::vector<uint32_t> px; };
static SzFace sz_face[4];
static std::mutex sz_fmx;
static SzFaceRes sz_fres[4];
static bool sz_png(const std::string& f, std::vector<uint32_t>& px, int* w, int* h) {
    std::vector<uint8_t> d;
    std::string err;
    if (FILE* fp = f.empty() ? nullptr : fopen(f.c_str(), "rb")) {
        fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
        d.resize(n > 0 ? (size_t)n : 0);
        if (fread(d.data(), 1, d.size(), fp) != d.size()) d.clear();
        fclose(fp);
    }
    return !d.empty() && png_decode(d, w, h, px, &err) && *w > 1 && *h > 1 && px.size() >= (size_t)*w * *h;
}
// the BT3 portrait as the panel cuts it (bh_face, left side, top panel), for the render editor's preview; g = screen rect and texture part
bool psp_bt3_face_cut(const std::vector<uint32_t>& fc, int pw, int ph, const float* g, std::vector<uint32_t>& out, int* ow, int* oh) {
    std::vector<uint32_t> mk;
    int mw = 0, mh = 0;
    std::string f = psp_cr_file("BT3Hud/common/gauge_07.png");
    if (f.empty()) f = psp_cr_file("BT3Hud/en/gauge_07.png");
    if (pw < 2 || ph < 2 || !sz_png(f, mk, &mw, &mh)) return false;
    uint32_t mmax = 1;
    for (uint32_t p : mk) mmax = std::max(mmax, p >> 24);
    const int K = 6;
    int W = std::min(1024, (int)std::ceil((g[1] - g[0]) * K)), H = std::min(1024, (int)std::ceil((g[3] - g[2]) * K));
    if (W < 2 || H < 2) return false;
    auto mask = [&](float u, float v) -> float {
        u -= 0.5f; v -= 0.5f;
        int x0 = (int)std::floor(u), y0 = (int)std::floor(v); float fx = u - x0, fy = v - y0, r = 0;
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
            int x = x0 + i, y = y0 + j;
            float a = x >= 0 && y >= 0 && x < mw && y < mh ? (float)(mk[(size_t)y * mw + x] >> 24) : 0.0f;
            r += a * (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
        }
        return r / mmax;
    };
    std::vector<uint32_t> d((size_t)W * H, 0);
    int bx0 = W, by0 = H, bx1 = -1, by1 = -1;
    for (int yy = 0; yy < H; yy++) {
        float sy = g[2] + (yy + 0.5f) / K, tv = g[6] + (sy - g[2]) / (g[3] - g[2]) * (g[7] - g[6]), bv = sy / BH_SY;
        for (int xx = 0; xx < W; xx++) {
            float sx = g[0] + (xx + 0.5f) / K, tu = g[4] + (sx - g[0]) / (g[1] - g[0]) * (g[5] - g[4]), bu = sx / BH_SX;
            float cv = mask(bu + 2, bv - 8) * bh_cov((std::hypot(sx - 21 * BH_SX, sy - 54 * BH_SY) - 8.0f) / 0.25f) * bh_cov((bh_face_edge(bv) - bu) * 0.7f / 0.25f);
            if (cv <= 0 || tu < 0 || tv < 0 || tu >= 1 || tv >= 1) continue;
            uint32_t p = fc[(size_t)std::min(ph - 1, (int)(tv * ph)) * pw + std::min(pw - 1, (int)(tu * pw))];
            uint32_t a = (uint32_t)((p >> 24) * std::min(1.0f, cv));
            if (!a) continue;
            d[(size_t)yy * W + xx] = (p & 0xFFFFFFu) | a << 24;
            bx0 = std::min(bx0, xx); by0 = std::min(by0, yy); bx1 = std::max(bx1, xx); by1 = std::max(by1, yy);
        }
    }
    if (bx1 < bx0) return false;
    *ow = bx1 - bx0 + 1; *oh = by1 - by0 + 1;
    out.resize((size_t)*ow * *oh);
    for (int y = 0; y < *oh; y++) memcpy(&out[(size_t)y * *ow], &d[(size_t)(y + by0) * W + bx0], 4 * (size_t)*ow);
    return true;
}
float psp_render_big_dx(int s, int c);
bool psp_sz_face_cut(const std::vector<uint32_t>& src, int w, int h, int side, float shift, std::vector<uint32_t>& out, int* fs) {   // shift: the editor's horizontal move of the big (subject to the right > 0), kept in the portrait   // the SZ portrait of a big render (also the render editor's preview); false without the mod's mask
    std::vector<uint32_t> mk;
    int mw = 0, mh = 0;
    if (w < 2 || h < 2 || !sz_png(psp_cr_file(side ? "SZHud/common/T_UI_Face_Mask_P2.png" : "SZHud/common/T_UI_Face_Mask_P1.png"), mk, &mw, &mh)) return false;
    auto A = [&](int x, int y) { return (int)(src[(size_t)y * w + x] >> 24); };
    std::vector<int> top(w, h);   // head = where a head-wide object reaches highest (an arm is narrower)
    for (int x = 0; x < w; x++) for (int y = 0; y < h; y++) if (A(x, y) > 128) { top[x] = y; break; }
    int hw = std::max(1, w * 30 / 264), best = h + 1;
    std::vector<int> v(w);
    for (int x = 0; x < w; x++) { int m = 0; for (int k = std::max(0, x - hw); k < std::min(w, x + hw); k++) m = std::max(m, top[k]); v[x] = m; best = std::min(best, m); }
    double sx = 0; int n = 0;
    for (int x = 0; x < w; x++) if (v[x] <= best + 3 * h / 172) { sx += x; n++; }
    float S = (float)h, hx = n ? (float)(sx / n) : w * 0.5f, x0 = (w >= h ? std::max(0.0f, std::min(hx - S * 0.5f, w - S)) : (w - S) * 0.5f) - shift * w;
    int ss = std::max(1, (int)ceilf(S / SZ_FS));   // taps per axis when the render is bigger than the portrait (HD renders)
    out.assign((size_t)SZ_FS * SZ_FS * 2, 0);
    for (int Y = 0; Y < SZ_FS; Y++)
        for (int X = 0; X < SZ_FS; X++) {
            int ma = (int)(mk[(size_t)std::min(mh - 1, Y * mh / SZ_FS) * mw + std::min(mw - 1, X * mw / SZ_FS)] >> 24);
            if (!ma) continue;
            float acc[4] = {0, 0, 0, 0};
            for (int j = 0; j < ss; j++)
                for (int i = 0; i < ss; i++) {
                    float u = (X + (i + 0.5f) / ss) / SZ_FS, vv = (Y + (j + 0.5f) / ss) / SZ_FS;
                    if (side) u = 1 - u;
                    float fx = x0 + u * S - 0.5f, fy = vv * S - 0.5f;
                    int ix = (int)floorf(fx), iy = (int)floorf(fy);
                    float ax = fx - ix, ay = fy - iy;
                    for (int q = 0; q < 4; q++) {
                        int px = ix + (q & 1), py = iy + (q >> 1);
                        if (px < 0 || py < 0 || px >= w || py >= h) continue;
                        float wt = ((q & 1) ? ax : 1 - ax) * ((q >> 1) ? ay : 1 - ay);
                        uint32_t p = src[(size_t)py * w + px];
                        float a = (p >> 24) * wt;
                        acc[0] += (p & 255) * a; acc[1] += ((p >> 8) & 255) * a; acc[2] += ((p >> 16) & 255) * a; acc[3] += a;
                    }
                }
            float a = acc[3] / (ss * ss);
            if (a < 0.5f) continue;
            int oa = (int)(a * ma / 255 + 0.5f);
            uint32_t rgb = (uint32_t)(acc[0] / acc[3] + 0.5f) | (uint32_t)(acc[1] / acc[3] + 0.5f) << 8 | (uint32_t)(acc[2] / acc[3] + 0.5f) << 16;
            out[(size_t)Y * SZ_FS + X] = rgb | (uint32_t)oa << 24;
            out[(size_t)SZ_FS * SZ_FS + (size_t)Y * SZ_FS + X] = 0xFFFFFFu | (uint32_t)oa << 24;
        }
    if (fs) *fs = SZ_FS;
    return true;
}
static void sz_face_make(int slot, int side, int key, int s, int c) {
    auto t0 = std::chrono::steady_clock::now();
    std::vector<uint32_t> src, out;
    int w = 0, h = 0;
    bool gen = psp_render_big_gen(s, c, src, &w, &h);
    if (!gen && !sz_png(psp_big_png(s), src, &w, &h)) w = h = 0;
    bool mask = !w || psp_sz_face_cut(src, w, h, side, gen ? psp_render_big_dx(s, c) : 0.0f, out, nullptr);
    int ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    printf("hud sz: retrato lado %d personaje %d color %d: %s %dx%d en %d ms\n", side, s, c, !w ? "sin render" : !mask ? "falta la mascara" : gen ? "render generado" : "render original", w, h, ms);
    fflush(stdout);
    std::lock_guard<std::mutex> g(sz_fmx);
    sz_fres[slot].key = key; sz_fres[slot].px.swap(out);
}
static const BhTex* sz_face_tex(int slot, int side, uint32_t f, bool white) {
    SzFace& F = sz_face[slot];
    uint32_t m = psp_l32(f + 0x9D0);
    if (!guest_ptr(m)) return nullptr;
    m += 0xA8u * psp_l32(f + 8);
    int s = (int)psp_l32(m + 0xC), c = (int)psp_l32(m + 0x10) + (psp_l8(f + 0x1A58) ? 64 : 0), key = (s & 0xFFFF) << 8 | (c & 0xFF);   // + 64: damaged level 2 costume (renders.h)
    if (key != F.key) { F.key = key; F.have = false; BH_LATE("hilo del retrato", "", std::thread(sz_face_make, slot, side, key, s, c).detach()); }
    if (!F.have) {
        std::unique_lock<std::mutex> g(sz_fmx, std::try_to_lock);
        if (!g.owns_lock() || sz_fres[slot].key != key) return nullptr;
        std::vector<uint32_t>& px = sz_fres[slot].px;
        if (px.empty()) { F.have = true; F.t.a = 0; return nullptr; }
        auto lt0 = std::chrono::steady_clock::now();
        uint32_t n = SZ_FS * SZ_FS * 4;
        if (!F.buf[0]) { F.buf[0] = psp_alloc(n); F.buf[1] = psp_alloc(n); }
        memcpy(psp_p(F.buf[0]), px.data(), n);
        memcpy(psp_p(F.buf[1]), px.data() + SZ_FS * SZ_FS, n);
        static uint32_t ver = 0;   // a stamp in a transparent corner texel: the GE texture cache sees a new portrait in the same buffer
        ver = (ver + 1) & 0xFFFFFF;
        for (int b = 0; b < 2; b++) if (!(psp_l32(F.buf[b]) >> 24)) psp_s32(F.buf[b], ver);
        F.t = BhTex{F.buf[0], SZ_FS, SZ_FS, SZ_FS, SZ_FS}; F.wt = BhTex{F.buf[1], SZ_FS, SZ_FS, SZ_FS, SZ_FS};
        F.have = true;
        bh_late_log("copia del retrato", "", lt0);
    }
    return F.t.a ? (white ? &F.wt : &F.t) : nullptr;
}

// name (GRP_NameBox/UserName): the bar name text (character, or the Steam name online) written with the app font sz_name,
// made off the game thread when the character changes; no name = no name base
static const float SZ_NK = 40.0f / 16.5f;   // canvas units per font px: UE size 30 (40 px em on the 3840 canvas) over the font's 16.5 px em
struct SzName { int key = -1; bool have = false; uint32_t buf = 0; BhTex t{0, 0, 0}; float nw = 0, nh = 0; };
struct SzNameRes { int key = -1, w = 0, h = 0; std::vector<uint32_t> px; };
static SzName sz_nm[4];
static SzNameRes sz_nres[4];
static std::string sz_utf8(uint32_t s) {
    std::string o;
    for (int i = 0; i < 64; i++) {
        uint32_t ch = psp_l16(s + 2 * (uint32_t)i);
        if (!ch) break;
        if (ch < 0x80) o += (char)ch;
        else if (ch < 0x800) { o += (char)(0xC0 | ch >> 6); o += (char)(0x80 | (ch & 63)); }
        else { o += (char)(0xE0 | ch >> 12); o += (char)(0x80 | ((ch >> 6) & 63)); o += (char)(0x80 | (ch & 63)); }
    }
    return o;
}
static void sz_name_make(int side, int key, std::string text) {
    static ft::Font fnt = ft::load("../../games/ttt/fonts/sz_name");
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    if (fnt.ok && !text.empty()) {
        ft::Img im = ft::compose(fnt, text, 256, 22, 4, 0, 1);
        int x0 = im.w, x1 = -1;
        for (int y = 0; y < im.h; y++) for (int x = 0; x < im.w; x++) if (im.px[(size_t)y * im.w + x] >> 24) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
        if (x1 >= x0) {
            x0 = std::max(0, x0 - 2); x1 = std::min(im.w - 1, x1 + 2);
            w = std::min(1024, x1 - x0 + 1); h = std::min(128, im.h);
            px.resize((size_t)w * h);
            for (int y = 0; y < h; y++) memcpy(&px[(size_t)y * w], &im.px[(size_t)y * im.w + x0], 4 * (size_t)w);
        }
    }
    printf("hud sz: nombre lado %d \"%s\" %dx%d%s\n", side, text.c_str(), w, h, fnt.ok ? "" : " (falta games/ttt/fonts/sz_name)");
    fflush(stdout);
    std::lock_guard<std::mutex> g(sz_fmx);
    sz_nres[side].key = key; sz_nres[side].w = w; sz_nres[side].h = h; sz_nres[side].px.swap(px);
}
static void sz_name_poll(int side) {
    SzName& N = sz_nm[side];
    if (N.have) return;
    std::unique_lock<std::mutex> g(sz_fmx, std::try_to_lock);
    if (!g.owns_lock() || sz_nres[side].key != N.key) return;
    SzNameRes& r = sz_nres[side];
    N.have = true; N.t.a = 0; N.nw = 0;
    if (r.px.empty()) return;
    auto lt0 = std::chrono::steady_clock::now();
    if (!N.buf) N.buf = psp_alloc(1024 * 128 * 4);
    for (int y = 0; y < r.h; y++) memcpy(psp_p(N.buf + (uint32_t)(y * 1024 * 4)), &r.px[(size_t)y * r.w], 4 * (size_t)r.w);
    static uint32_t ver = 0;
    ver = (ver + 1) & 0xFFFFFF;
    if (!(psp_l32(N.buf) >> 24)) psp_s32(N.buf, ver);
    N.t = BhTex{N.buf, 1024, 128, r.w, r.h};
    N.nw = r.w / 4.0f * SZ_NK; N.nh = r.h / 4.0f * SZ_NK;
    bh_late_log("copia del nombre", "", lt0);
}

// event times per panel (seconds, steady clock): the state animations are chosen from them
struct SzSide { double bon[8] = {-1, -1, -1, -1, -1, -1, -1, -1}, boff[8] = {-1, -1, -1, -1, -1, -1, -1, -1}; double t0 = 0, hit = -1, son = -1, soff = -1, con = -1, coff = -1, unit = -1, mx = -1, brk = -1, hide = -1, cell[5]; int hp = 0, bars = 0, units = 0, en = 0; double gain = -1, gstart = -1; bool spk = false, chg = false; float tr = -1, trh = 0; uint32_t tvb = 0; };
static SzSide sz_sd[4];   // per bar (0 J1, 1 partner, 2/3 opponents): the state follows the fighter when bars swap panels
// SZ red trail (measured on his SZ video): it stays while hits land, then drains at 3000 HP/s 1.6 s after the last drop; frozen in pause
static void sz_trail(SzSide& e, int hp, bool drop) {
    uint32_t vb = psp_l32(0x08A78E14u);
    float dt = e.tvb && !bp_open ? std::min(8u, vb - e.tvb) / 60.0f : 0;
    e.tvb = vb;
    if (e.tr < hp) { e.tr = (float)hp; e.trh = 0; return; }
    if (drop) { e.trh = 0; return; }
    e.trh += dt;
    if (e.trh >= 1.6f) e.tr = std::max((float)hp, e.tr - 3000.0f * dt);
}
static float sz_alen(const char* cn, const char* an, float def);
static void sz_panel(const Ctx& c, uint32_t f, int side, int slot, float dy) {
    HUD_COST("hud sz: panel");
    SzData d;
    d.hp = std::max(0, (int)psp_l32(f + 0x1A34));
    d.ki = std::max(0, (int)psp_l32(f + 0x1A40));
    d.spark = psp_l8(f + 0xDDB) != 0;
    d.energy = std::max(0, cl_energy(c, f));
    d.senzu = sz_senzu[side];
    d.copy = dy != 0;
    d.orange = std::max(0, (int)psp_l32(f + 0x1A54));
    d.charging = psp_l32(f + 0x984) == 60;
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    SzSide& e = sz_sd[slot];
    int bars = (d.hp + 9999) / 10000, units = d.energy / 100000;
    bool chg = !d.spark && d.orange > 0;
    if (bh_hp_shown[bh_pn] < 0) {   // new battle: the in animation starts, nothing pending
        e = SzSide(); e.t0 = now; sz_face[slot].key = -1; sz_nm[slot].key = -1; e.hp = d.hp; e.bars = bars; e.units = units; e.en = d.energy; e.spk = d.spark; e.chg = chg;
        for (int i = 0; i < 5; i++) e.cell[i] = d.ki >= 20000 * (i + 1) ? now - 10 : -1;
    }
    if (d.energy > e.en) { if (e.gain < 0 || now - e.gain > 0.35) e.gstart = now; e.gain = now; }
    e.en = d.energy;
    sz_trail(e, d.hp, d.hp < e.hp);
    if (d.hp < e.hp) e.hit = now;
    if (bars < e.bars) e.brk = now;
    if (d.spark != e.spk) (d.spark ? e.son : e.soff) = now;
    if (chg != e.chg) (chg ? e.con : e.coff) = now;
    if (units > e.units) { e.unit = now; if (units >= 5) e.mx = now; }
    for (int i = 0; i < 5; i++) { bool full = d.ki >= 20000 * (i + 1); if (full && e.cell[i] < 0) e.cell[i] = now; if (!full) e.cell[i] = -1; }
    if (bh_phase() == 5) { if (e.hide < 0) e.hide = now; } else e.hide = -1;
    e.hp = d.hp; e.bars = bars; e.units = units; e.spk = d.spark; e.chg = chg;
    auto ago = [&](double t) { return t < 0 ? -1.0f : (float)(now - t); };
    d.now = (float)fmod(now, 3600.0); d.t_show = ago(e.t0); d.t_hit = ago(e.hit); d.t_spark = ago(e.son); d.t_unspark = ago(e.soff);
    d.t_charge = ago(e.con); d.t_uncharge = ago(e.coff); d.t_unit = ago(e.unit); d.t_max = ago(e.mx); d.t_break = ago(e.brk); d.t_hide = ago(e.hide);
    for (int i = 0; i < 5; i++) d.cell[i] = ago(e.cell[i]);
    {   // status icons: TTT stat modifiers (BT3 BtlStat_GetMod), raised or lowered, packed left to right
        static const float BOUT = sz_alen("WBP_GRP_BuffIcon", "ANM_Out", 0.3667f);
        int v[4];
        for (int i = 0; i < 4; i++) v[i] = (int)hud_guest(c, 0x089128B4u, f, (uint32_t)i, 0xFFFFFFFFu);
        for (int k = 0; k < 8; k++) {
            int w = v[k / 2];
            bool on = w && (w < 0) == (k & 1);
            if (on && (e.bon[k] < 0 || e.boff[k] >= 0)) { e.bon[k] = now; e.boff[k] = -1; }
            if (!on && e.bon[k] >= 0 && e.boff[k] < 0) e.boff[k] = now;
            if (e.boff[k] >= 0 && now - e.boff[k] >= BOUT) e.bon[k] = e.boff[k] = -1;
            if (e.bon[k] >= 0 && d.nbuf < 6) { d.buf[d.nbuf] = k; d.bshow[d.nbuf] = ago(e.bon[k]); d.bout[d.nbuf] = ago(e.boff[k]); d.nbuf++; }
        }
        static int bl[4] = {-1, -1, -1, -1}, logs = 0;
        int key = 0;
        for (int i = 0; i < 4; i++) key = key * 3 + (v[i] > 0 ? 1 : v[i] < 0 ? 2 : 0);
        if (key != bl[slot] && logs++ < 30) { bl[slot] = key; printf("hud sz: iconos barra %d ataque %d defensa %d ki %d velocidad %d\n", side, v[0], v[1], v[2], v[3]); }
    }
    d.t_egain = ago(e.gain); d.t_estart = ago(e.gstart);
    bh_step(bh_pn, d.hp, d.ki, d.spark);
    d.trail = std::max((int)d.hp, (int)e.tr);
    d.kitrail = std::max((int)d.ki, bh_ki_shown[bh_pn]);
    {
        uint32_t mm = psp_l32(f + 0x9D0);
        int ck = -1;
        if (guest_ptr(mm)) { mm += 0xA8u * psp_l32(f + 8); ck = (int)(psp_l32(mm + 0xC) & 0xFFFF) << 8 | (int)(psp_l32(mm + 0x10) & 0xFF); }
        SzName& N = sz_nm[slot];
        if (ck != N.key) {
            N.key = ck; N.have = false;
            bool player;
            uint32_t str = nm_text(c, f, &player);
            BH_LATE("hilo del nombre", "", std::thread(sz_name_make, slot, ck, str && (player || nm_want()) ? sz_utf8(str) : std::string()).detach());
        }
        sz_name_poll(slot);
        if (N.have && N.t.a) { d.nw = N.nw; d.nh = N.nh; }
    }
    std::vector<SzItem> items;
    items.reserve(64);
    { HUD_COST("hud sz: panel, diseno"); sz_lay.scope = slot; sz_lay.panel(side, d, items); sz_lay.fx(side, d, items); }
    {
        static int n = 0, logs = 0; static long h0 = 0, m0 = 0;
        if (++n >= 1200) {
            long h = sz_lay.c_hit - h0, m = sz_lay.c_miss - m0;
            if (logs++ < 8 && h + m > 0) printf("hud sz: cache del diseno %.1f%% de aciertos (%ld de %ld dibujos)\n", 100.0 * h / (h + m), h, h + m);
            if (logs <= 3) sz_lay.why_print();
            h0 = sz_lay.c_hit; m0 = sz_lay.c_miss; n = 0;
        }
    }
    if (dy != 0) for (auto& it : items) for (int k = 0; k < 4; k++) it.y[k] += dy;
    static int logs = 0, miss = 0;
    if (logs < 4) { logs++; printf("hud sz: panel %d barra %d con %d piezas (vida %d ki %d energia %d semillas %d)\n", side, slot, (int)items.size(), (int)d.hp, (int)d.ki, (int)d.energy, (int)d.senzu); }
    static bool lspk[4], lchg[4];
    if (lspk[slot] != d.spark || lchg[slot] != (d.orange > 0)) {
        lspk[slot] = d.spark; lchg[slot] = d.orange > 0;
        printf("hud sz: panel %d sparking %d naranja %d cargando %d ki %d piezas %d\n", side, (int)d.spark, (int)d.orange, (int)d.charging, (int)d.ki, (int)items.size());
    }
    HUD_COST("hud sz: panel, medidores y dibujo");
    bh_begin();
    for (auto& it : items) {
        if (it.tex[0] == '@') {
            if (it.tex == "@facew") continue;   // no white flash on hits (SZ has none)
            const BhTex* t = it.tex == "@name" ? (sz_nm[slot].t.a ? &sz_nm[slot].t : nullptr) : sz_face_tex(slot, side, f, it.tex == "@facew");
            if (t) sz_quad(t, it);
            continue;
        }
        const BhTex* t = it.value >= 0 ? sz_gauge(it, slot) : bh_gets(it.tex);
        if (t && it.addq) { const BhTex* b = it.value >= 0 ? bh_gets(it.tex) : t; if (b && sz_opaque(b)) it.add = true; }
        if (t) sz_quad(t, it);
        else if (miss++ < 8) printf("hud sz: falta la textura %s\n", it.tex.c_str());
    }
    bh_end();
}

// radial discs of the shader-only materials M_UI_Start_RadioGrad_Base / _Add, profiles measured on SZ footage:
// Base full to r 0.34, gone at 0.65; Add full to 0.3, gone at 0.8
static const BhTex* sz_radial(bool add) {
    static BhTex t[2] = {{0, 0, 0}, {0, 0, 0}};
    BhTex& x = t[add];
    if (!x.a) {
        const int N = 64;
        x = BhTex{psp_reserve(N * N * 4), N, N, N, N};
        float r0 = add ? 0.3f : 0.34f, r1 = add ? 0.8f : 0.65f;
        for (int y = 0; y < N; y++)
            for (int xx = 0; xx < N; xx++) {
                float dx = (xx + 0.5f) / N * 2 - 1, dy = (y + 0.5f) / N * 2 - 1, r = sqrtf(dx * dx + dy * dy), k = std::max(0.0f, std::min(1.0f, (r1 - r) / (r1 - r0))), a = k * k * (3 - 2 * k);
                psp_s32(x.a + 4u * (uint32_t)(y * N + xx), 0xFFFFFFu | (uint32_t)(a * 255 + 0.5f) << 24);
            }
    }
    return &x;
}
// round timer (WBP_OBJ_Timer): the game's counter as SZ digits, infinity without a time limit
static struct { double seen = -10, t0 = 0, hide = -1, chg[3] = {-1, -1, -1}; int dig[3] = {0, 0, 0}, prev[3] = {0, 0, 0}; } sz_tm;
static void sz_timer(int t, bool inf) {
    HUD_COST("hud sz: reloj");
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now - sz_tm.seen > 1.0) { sz_tm = {}; sz_tm.t0 = now; for (int k = 0, p = 1; k < 3; k++, p *= 10) sz_tm.dig[k] = sz_tm.prev[k] = t / p % 10; }
    sz_tm.seen = now;
    for (int k = 0, p = 1; k < 3; k++, p *= 10) { int v = t / p % 10; if (v != sz_tm.dig[k]) { sz_tm.prev[k] = sz_tm.dig[k]; sz_tm.dig[k] = v; sz_tm.chg[k] = now; } }
    if (bh_phase() == 5) { if (sz_tm.hide < 0) sz_tm.hide = now; } else sz_tm.hide = -1;
    auto ago = [&](double x) { return x < 0 ? -1.0f : (float)(now - x); };
    SzData d;
    double tp = sz_sd[0].t0 > 0 && now - sz_sd[0].t0 >= 0 ? now - sz_sd[0].t0 : now - sz_tm.t0;   // since the panels' in animation began
    const float BOLT = 0.55f + SzLayout::FX_LAG;   // the timer's start effect begins 0.55 s into SZ's panel in animation (timed on his 60 fps footage)
    d.tm = inf ? -1 : t; d.now = (float)fmod(now, 3600.0); d.t_show = (float)tp - BOLT; d.t_hide = ago(sz_tm.hide);
    for (int k = 0; k < 3; k++) { d.tprev[k] = sz_tm.prev[k]; d.tchg[k] = ago(sz_tm.chg[k]); }
    bool rep = sz_lay.cls.count("WBP_Rep_TimeCount") > 0;   // the timer SZ shows in battle (kit with it), else WBP_OBJ_Timer at the measured size
    if (inf && d.t_hide < 0 && d.t_show >= 10) { d.t_hide = d.t_show - 10; if (d.t_hide > 0.3f) return; }   // SZ: infinity only the first 10 s
    if (d.t_show < 0) return;
    std::vector<SzItem> items;
    items.reserve(16);
    sz_lay.scope = 5;
    if (rep) sz_lay.whole("WBP_Rep_TimeCount", d, items, d.t_hide >= 0 ? std::max(0.0f, 1 - d.t_hide / 0.2333f) : 1);
    else sz_lay.root("WBP_OBJ_Timer", d, items);
    static int logs = 0;
    if (logs++ < 2) {
        std::string nm;
        if (rep) { auto it = sz_lay.cls.find("WBP_Rep_TimeCountNum"); if (it != sz_lay.cls.end()) for (auto& w : it->second) nm += " " + w.name + (w.img.empty() ? "" : "=" + w.img); }
        printf("hud sz: reloj %s %d%s, %d piezas%s%s\n", rep ? "de SZ" : "del HUD", t, inf ? " (infinito)" : "", (int)items.size(), rep ? ", cifra:" : "", nm.c_str());
    }
    bh_begin(true);
    for (auto& it : items) if (const BhTex* tx = it.tex[0] == '@' ? sz_radial(it.tex == "@radial2") : bh_gets(it.tex)) sz_quad(tx, it);
    bh_end();
}


// combo hits and damage (WBP_Rep_ComboSet_P1/P2): hits from 2; damage counts up along Curve_DamageNum_Float (0.5 s); hits hide 0.5 s
// after the game ends the combo, damage 1.75 s after its last change (DisplaySec of the set); clock frozen in pause
struct SzCombo { double cshow = -1, cend = -1, chide = -1, dshow = -1, dset = -1, dhide = -1; int hits = 0, fd = 0, dfrom = 0, dto = 0; bool con = false, don = false; };
static SzCombo sz_cb[2];
static double sz_cclk = 0;
static float sz_dcurve(float t) {
    float s = std::min(1.0f, std::max(0.0f, t / 0.5f)), s2 = s * s, s3 = s2 * s;
    return (s3 - 2 * s2 + s) * 0.5f * 6.0138f + (-2 * s3 + 3 * s2) + (s3 - s2) * 0.5f * 0.0583f;
}
static int sz_dshown(const SzCombo& e) { return e.dset < 0 ? e.dto : e.dfrom + (int)lroundf((e.dto - e.dfrom) * std::min(1.0f, sz_dcurve((float)(sz_cclk - e.dset)))); }
static float sz_alen(const char* cn, const char* an, float def) {
    auto a = sz_lay.anim.find(cn);
    if (a == sz_lay.anim.end()) return def;
    auto b = a->second.find(an);
    return b == a->second.end() ? def : b->second.len;
}
static const BhTex* sz_cpart() {   // M_UI_Combo_Particle: (R + G) x B of T_UI_Combo_Particle, additive
    static BhTex t{0, 0, 0};
    static bool tried = false;
    if (tried) return t.a ? &t : nullptr;
    tried = true;
    auto lt0 = std::chrono::steady_clock::now();
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    if (!sz_png(psp_cr_file("SZHud/common/T_UI_Combo_Particle.png"), px, &w, &h) || w > 1024 || h > 256) return nullptr;
    int W = w / 2, H = h / 2, TW = 1, TH = 1;
    while (TW < W) TW <<= 1;
    while (TH < H) TH <<= 1;
    t = BhTex{psp_reserve((uint32_t)(TW * TH * 4)), TW, TH, W, H};
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int v = 0;
            for (int k = 0; k < 4; k++) {
                uint32_t p = px[(size_t)(2 * y + (k >> 1)) * w + 2 * x + (k & 1)];
                v += std::min(255u, (p & 255) + ((p >> 8) & 255)) * ((p >> 16) & 255) / 255;
            }
            v /= 4;
            psp_s32(t.a + 4u * (uint32_t)(y * TW + x), 0xFF000000u | (uint32_t)v * 0x010101u);
        }
    bh_late_log("particula del combo", "", lt0);
    return &t;
}
static void sz_combo(const Ctx& c) {
    HUD_COST("hud sz: combo");
    static uint32_t vb0 = 0;
    static double t0 = -2;
    uint32_t vb = psp_l32(0x08A78E14u);
    if (sz_sd[0].t0 != t0) { t0 = sz_sd[0].t0; for (auto& e : sz_cb) e = SzCombo(); vb0 = vb; }
    double dt = std::min(0.1, (double)(uint32_t)(vb - vb0) / 60.0);
    vb0 = vb;
    if (!bp_open) sz_cclk += dt;
    double now = sz_cclk;
    bh_combo_feed(c);
    const float chl = sz_alen("WBP_OBJ_Combo_P1", "ANI_Hide", 0.1667f), dhl = sz_alen("WBP_OBJ_Damage_P1", "ANI_Hide", 0.3333f);
    std::vector<SzItem> items;
    for (int s = 0; s < 2; s++) {
        SzCombo& e = sz_cb[s];
        int h = bh_cb[s].fh, dv = bh_cb[s].fd;
        if (h >= 2) { if (!e.con || e.chide >= 0) { e.con = true; e.cshow = now; e.chide = -1; } e.hits = std::min(h, 999); e.cend = -1; }
        else if (e.con && e.chide < 0) { if (e.cend < 0) e.cend = now; if (now - e.cend >= 0.5) e.chide = now; }
        if (e.con && e.chide >= 0 && now - e.chide >= chl) e.con = false;
        if (dv > 0 && dv != e.fd) {
            int from = e.fd <= 0 ? 0 : sz_dshown(e);   // a new combo counts from 0
            if (!e.don || e.dhide >= 0) { e.don = true; e.dshow = now; e.dhide = -1; }
            e.dfrom = from; e.dto = std::min(dv, 999999); e.dset = now;
        }
        e.fd = dv;
        if (e.don && e.dhide < 0 && dv <= 0 && now - e.dset >= 1.75) e.dhide = now;
        if (e.don && e.dhide >= 0 && now - e.dhide >= dhl) e.don = false;
        if (!e.con && !e.don) continue;
        SzData d;
        d.now = (float)fmod(now, 3600.0);
        d.hits = e.hits; d.dmg = sz_dshown(e);
        d.t_cshow = e.con ? (float)(now - e.cshow) : -1; d.t_chide = e.con && e.chide >= 0 ? (float)(now - e.chide) : -1;
        d.t_dshow = e.don ? (float)(now - e.dshow) : -1; d.t_dhide = e.don && e.dhide >= 0 ? (float)(now - e.dhide) : -1;
        sz_lay.scope = 4;
        sz_lay.combo(s, d, items);
    }
    static int logs = 0;
    if (!items.empty() && logs++ < 2) printf("hud sz: combo con %d piezas (%s)\n", (int)items.size(), sz_lay.cls.count("WBP_Rep_ComboSet_P1") ? "WBP_Rep_ComboSet" : "WBP_GRP_ComboSet");
    if (items.empty()) return;
    bh_begin();
    for (auto& it : items) {
        const BhTex* t = it.tex == "@cpart" ? sz_cpart() : it.tex[0] == '@' ? nullptr : bh_gets(it.tex);
        if (t && it.addq && sz_opaque(t)) it.add = true;
        if (t) sz_quad(t, it);
    }
    bh_end();
}

// power-up notice: TTT's "¡Potenciación!" (prompt 0x089D72F4, holder +0x70 in kind 1) is not drawn; SZ's BBoost disc and text
// (WBP_Rep_BBoost_1P) are drawn centred where it rests while the game shows it
struct SzBoost { double show = -1, out = -1, seen = -1; };
static SzBoost sz_bs[2];   // 0 = player 1 (left), 1 = player 2 in 2 views (right)
static double sz_bs_t0 = -2;
static const float SZ_BX = 50.5f, SZ_BY = 166.3f;   // centre of TTT's title at rest, measured on his capture (1916x1090: 0..403, 645..688)
// ra 0x089D8C14: holder +0x70 in kind 1 (a short flash); ra 0x089D8CCC: holders +0x74..+0x94, the title is the one in state 25
static bool sz_boost_inst(Ctx& c, void (*orig)(Ctx&)) {
    if (!bh_sz || bh_lay == 4 || !sz_lay.cls.count("WBP_Rep_BBoost_1P")) return false;
    bool loop = c.r[31] == 0x089D8CCCu;
    uint32_t i = loop ? c.r[16] : 0, o = loop ? c.r[18] - 4 * i : c.r[16], h = guest_ptr(o) ? psp_l32(o + (loop ? 0x74 + 4 * i : 0x70)) : 0, in = c.r[4];
    if (!guest_ptr(h) || i > 8 || !psp_l32(o + 0xAC) || in != cp_state(h, loop ? 25 : 1)) return false;
    double now = sz_cclk;
    SzBoost& b = sz_bs[bh_lay == 2 && bh_vpass == 1 ? 1 : 0];   // the prompt copy drawn in player 2's view pass
    if (b.show < 0 || b.out >= 0 || now - b.seen > 0.1) { b.show = now; b.out = -1; }
    b.seen = now;
    static int logs = 0;
    if (logs++ < 24) printf("hud sz: aviso de mejora, pieza %s de TTT oculta a los %.2f s (jugador %d)\n", loop ? "titulo" : "destello", now - b.show, (int)(&b - sz_bs) + 1);
    return true;
}
static void sz_boost() {
    if (sz_sd[0].t0 != sz_bs_t0) { sz_bs[0] = sz_bs[1] = SzBoost(); sz_bs_t0 = sz_sd[0].t0; }
    double now = sz_cclk;
    for (int s = 0; s < 2; s++) {
        SzBoost& b = sz_bs[s];
        if (b.show < 0) continue;
        if (b.out < 0 && now - b.seen > 0.1) b.out = now;
        float tout = b.out >= 0 ? (float)(now - b.out) : -1;
        if (tout >= sz_alen("WBP_Rep_BBoost_1P", "ANM_Out", 0.2334f)) { b.show = -1; continue; }
        std::vector<SzItem> items;
        if (!sz_lay.boost(s ? 480 - SZ_BX : SZ_BX, SZ_BY, (float)(now - b.show), tout, items) || items.empty()) continue;
        static int logs = 0;
        if (logs++ < 2) printf("hud sz: aviso de mejora con %d piezas\n", (int)items.size());
        bh_begin();
        for (auto& it : items) if (const BhTex* t = bh_gets(it.tex)) sz_quad(t, it);
        bh_end();
    }
}

// partner command box (P1's CPU partner, mouse wheel): SZ name band (T_UI_NameBase) under P1's panel with the order written in the
// bar-name font; shown when the order changes, held 2 s, faded 0.5 s (as the other bar modes)
static const char* SZ_CMD[2][3] = {{"Normal", "Focus", "Split"}, {"Normal", "Concentración", "Dividido"}};
static const float SZ_CX0 = 4.5f, SZ_CY0 = 75.4f, SZ_CH = 7.6f, SZ_CW = 82.9f, SZ_CTX = 15.0f;   // his choice (mockup B), screen px
static SzName sz_cmd;
static SzNameRes sz_cres;
static void sz_cmd_make(int key, std::string text) {
    static ft::Font fnt = ft::load("../../games/ttt/fonts/sz_name");
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    if (fnt.ok) {
        ft::Img im = ft::compose(fnt, text, 256, 22, 4, 0, 1);
        int x0 = im.w, x1 = -1;
        for (int y = 0; y < im.h; y++) for (int x = 0; x < im.w; x++) if (im.px[(size_t)y * im.w + x] >> 24) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
        if (x1 >= x0) {
            x0 = std::max(0, x0 - 2); x1 = std::min(im.w - 1, x1 + 2);
            w = std::min(1024, x1 - x0 + 1); h = std::min(128, im.h);
            px.resize((size_t)w * h);
            for (int y = 0; y < h; y++) memcpy(&px[(size_t)y * w], &im.px[(size_t)y * im.w + x0], 4 * (size_t)w);
        }
    }
    std::lock_guard<std::mutex> g(sz_fmx);
    sz_cres.key = key; sz_cres.w = w; sz_cres.h = h; sz_cres.px.swap(px);
}
static void sz_cmd_draw(uint32_t o) {
    uint32_t th = psp_l32(o + 0x78);
    if (!guest_ptr(th)) return;
    uint32_t in = psp_l32(th), l = psp_l32(th + 4);
    int n = std::min(16, (int)psp_l32(th + 0xC)), st = -1, rank = 0, keys[16];
    for (int i = 0; guest_ptr(l) && i < n; i++) { keys[i] = (int)psp_l32(l + 8 * i); if (psp_l32(l + 8 * i + 4) == in) st = keys[i]; }
    for (int i = 0; i < n; i++) rank += keys[i] < st;
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    static int lst = -2, logs = 0;
    static double t0 = 0;
    if (st != lst) {
        lst = st; t0 = now;
        if (logs++ < 12) { printf("hud sz: orden del companero estado %d (puesto %d de %d:", st, rank, n); for (int i = 0; i < n; i++) printf(" %d", keys[i]); printf(")\n"); }
    }
    double e = now - t0;
    float a = e < 2.0 ? 1.0f : e < 2.5 ? (float)(1.0 - (e - 2.0) / 0.5) : 0.0f;
    int lg = bh_lang == 1, idx = std::max(0, std::min(2, rank)), key = idx * 2 + lg;
    SzName& N = sz_cmd;
    if (key != N.key) { N.key = key; N.have = false; std::thread(sz_cmd_make, key, std::string(SZ_CMD[lg][idx])).detach(); }
    if (!N.have) {
        std::unique_lock<std::mutex> g(sz_fmx, std::try_to_lock);
        if (g.owns_lock() && sz_cres.key == N.key) {
            N.have = true; N.t.a = 0;
            if (!sz_cres.px.empty()) {
                if (!N.buf) N.buf = psp_alloc(1024 * 128 * 4);
                for (int y = 0; y < sz_cres.h; y++) memcpy(psp_p(N.buf + (uint32_t)(y * 1024 * 4)), &sz_cres.px[(size_t)y * sz_cres.w], 4 * (size_t)sz_cres.w);
                static uint32_t ver = 0;
                ver = (ver + 1) & 0xFFFFFF;
                if (!(psp_l32(N.buf) >> 24)) psp_s32(N.buf, 0x00800000u | ver);
                N.t = BhTex{N.buf, 1024, 128, sz_cres.w, sz_cres.h};
            }
        }
    }
    const BhTex* band = bh_get("T_UI_NameBase");
    if (a <= 0 || !band || !N.have || !N.t.a) return;
    const float k = SZ_NK / 4 * 480.0f / 3840;
    float tw = N.t.ow * k, tht = N.t.oh * k, w = std::max(SZ_CW, SZ_CTX + tw + 8);
    auto quad = [&](const BhTex* t, float x0, float y0, float x1, float y1) {
        SzItem it;
        it.tex = "x";
        it.x[0] = it.x[3] = x0; it.x[1] = it.x[2] = x1; it.y[0] = it.y[1] = y0; it.y[2] = it.y[3] = y1; it.a = a;
        sz_quad(t, it);
    };
    bh_begin();
    quad(band, SZ_CX0, SZ_CY0, SZ_CX0 + w, SZ_CY0 + SZ_CH);
    float ty = SZ_CY0 + (SZ_CH - tht) * 0.5f;
    quad(&N.t, SZ_CX0 + SZ_CTX - 2 * k, ty, SZ_CX0 + SZ_CTX - 2 * k + tw, ty + tht);
    bh_end();
}

// technique list (G6): SZ rows (SzList) in place of the TTT row pieces and names; the key glyphs stay the game's font, placed by the caller
struct SzLText { std::string s; bool have = false; BhTex t{0, 0, 0}; };
struct SzLRes { std::string s; int w = 0, h = 0; std::vector<uint32_t> px; };
static SzLText sz_lt[6];
static SzLRes sz_lres[6];
static uint32_t sz_lbuf = 0;
static void sz_list_make(int r, std::string text) {
    static ft::Font fnt = ft::load("../../games/ttt/fonts/sz_list");
    static std::mutex one;
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> g1(one);
        if (fnt.ok && !text.empty()) {
            ft::Img im = ft::compose(fnt, text, 256, 22, 2, 0, 1);
            int x0 = im.w, x1 = -1;
            for (int y = 0; y < im.h; y++) for (int x = 0; x < im.w; x++) if (im.px[(size_t)y * im.w + x] >> 24) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
            if (x1 >= x0) {
                w = std::min(512, x1 - x0 + 1); h = std::min(64, im.h);
                px.resize((size_t)w * h);
                for (int y = 0; y < h; y++) memcpy(&px[(size_t)y * w], &im.px[(size_t)y * im.w + x0], 4 * (size_t)w);
            }
        }
    }
    std::lock_guard<std::mutex> g(sz_fmx);
    sz_lres[r].s = text; sz_lres[r].w = w; sz_lres[r].h = h; sz_lres[r].px.swap(px);
}
static void sz_list_poll(int r) {
    SzLText& T = sz_lt[r];
    if (T.have) return;
    std::unique_lock<std::mutex> g(sz_fmx, std::try_to_lock);
    if (!g.owns_lock() || sz_lres[r].s != T.s) return;
    SzLRes& q = sz_lres[r];
    T.have = true; T.t.a = 0;
    if (q.px.empty()) return;
    auto lt0 = std::chrono::steady_clock::now();
    if (!sz_lbuf) sz_lbuf = psp_alloc(512 * 64 * 4 * 6);
    uint32_t b = sz_lbuf + (uint32_t)r * 512 * 64 * 4;
    memset(psp_p(b), 0, 512 * 64 * 4);
    for (int y = 0; y < q.h; y++) memcpy(psp_p(b + (uint32_t)(y * 512 * 4)), &q.px[(size_t)y * q.w], 4 * (size_t)q.w);
    static uint32_t ver = 0;
    ver = (ver + 1) & 0xFFFFFF;
    if (!(psp_l32(b) >> 24)) psp_s32(b, ver);
    T.t = BhTex{b, 512, 64, q.w, q.h};
    bh_late_log("copia de fila de la lista", "", lt0);
}
static void sz_list_bake() {   // the flame plate: its own alpha is faint, the material lifts it inside the brush mask
    static uint32_t baked = 0;
    const BhTex* pc = bh_getk("T_UI_SkillGuide_Base_TypeC_01");
    if (!pc || pc->a == baked) return;
    baked = pc->a;
    auto lt0 = std::chrono::steady_clock::now();
    const BhTex* m = bh_get("T_UI_SkillGuide_Base_TypeC_Mask");
    if (m && m->ow == pc->ow && m->oh == pc->oh)
        for (int y = 0; y < pc->oh; y++) for (int x = 0; x < pc->ow; x++) {
            uint32_t p = pc->a + 4u * (uint32_t)(y * pc->w + x), v = psp_l32(p), ma = psp_l32(m->a + 4u * (uint32_t)(y * m->w + x)) >> 24;
            uint32_t a = std::min(255u, (v >> 24) * 9 / 5) * ma / 255;
            psp_s32(p, (v & 0xFFFFFFu) | a << 24);
        }
    bh_late_log("placa de llamas de la lista", "", lt0);
}
static bool sz_list(const Ctx& c, bool spark, float al, SzLKey* keys) {
    HUD_COST("hud sz: lista de tecnicas");
    if (!bh_on || !bh_sz) return false;
    const BhTex *pa = bh_get("T_UI_SkillGuide_Base_TypeA_01"), *pb = bh_get("T_UI_SkillGuide_Base_TypeB_01"), *pc = bh_get("T_UI_SkillGuide_Base_TypeC_01");
    static int said = -1;
    bool ok = pa && pb && pc;
    if ((int)ok != said) { said = ok; printf("hud sz: lista de tecnicas %s\n", ok ? "de SZ" : "de TTT (el kit instalado no trae las piezas T_UI_SkillGuide)"); }
    if (!ok) return false;
    sz_list_bake();
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    static double last = -1, t0 = 0, ton[6];
    static bool lk[6];
    if (now - last > 0.15) { t0 = now; for (int i = 0; i < 6; i++) { ton[i] = -1; lk[i] = cl_lock[i]; } }
    last = now;
    SzRow R[6];
    int map[6], n = 0;
    const float K = SZ_NK / 2;
    for (int r = 0; r < cl_rows && r < 6; r++) {
        if (keys) keys[r] = SzLKey();
        if (lk[r] != cl_lock[r]) { lk[r] = cl_lock[r]; if (!lk[r]) ton[r] = now; }
        if (!cl_name[r] || !psp_l16(cl_name[r]) || (r == cl_rows - 1 && !spark)) continue;
        SzLText& T = sz_lt[r];
        std::string s = sz_utf8(cl_name[r]);
        if (s != T.s) { T.s = s; T.have = false; std::thread(sz_list_make, r, s).detach(); }
        sz_list_poll(r);
        SzRow& w = R[n];
        w.type = cl_type[r]; w.lock = cl_lock[r]; w.text = r; w.t_on = ton[r] < 0 ? -1 : (float)(now - ton[r]);
        w.cost = cl_cost[r] <= 0 ? 0 : w.type == 0 ? (cl_cost[r] + 99999) / 100000 : w.type == 1 ? (cl_cost[r] + CP_KI_BAR - 1) / CP_KI_BAR : 0;
        if (T.have && T.t.a) { w.tw = T.t.ow * K; w.th = T.t.oh * K; }
        map[n++] = r;
    }
    static std::vector<SzItem> items;
    items.clear();
    SzLKey k[6];
    SzList::build(R, n, (float)fmod(now, 3600.0), (float)(now - t0), al, items, k);
    for (int i = 0; i < n; i++) if (keys) keys[map[i]] = k[i];
    bh_begin();
    for (auto& it : items) {
        const BhTex* t = it.tex[0] == '@' ? &sz_lt[it.tex[6] - '0'].t : bh_gets(it.tex);
        if (t && t->a) sz_quad(t, it);
    }
    bh_end();
    static int logs = 0;
    if (logs < 2) { logs++; printf("hud sz: lista de tecnicas con %d filas, %d piezas\n", n, (int)items.size()); }
    return true;
}

// mode change away from Sparking Zero: every buffer of the SZ panels back to the pool (portraits, names, gauges, list texts)
static void sz_release() {
    for (auto& m : sz_g) { for (auto& kv : m) { psp_free(kv.second.buf[0]); psp_free(kv.second.buf[1]); } m.clear(); }
    for (auto& F : sz_face) { psp_free(F.buf[0]); psp_free(F.buf[1]); F = SzFace(); }
    for (auto& N : sz_nm) { psp_free(N.buf); N = SzName(); }
    psp_free(sz_cmd.buf); sz_cmd = SzName();
    psp_free(sz_lbuf); sz_lbuf = 0;
    for (auto& T : sz_lt) T = SzLText();
    static int logs = 0;
    if (logs++ < 10) printf("hud sz: memoria de los paneles devuelta (en uso %u KB)\n", psp_alloc_used() / 1024);
}
