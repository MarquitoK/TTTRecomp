// Local multiplayer in one process (item 20b): extra players' pads into the game's pad table, their fighters human.
// Raw pads 0x08AF31A0 + k*16 (sceCtrl data) -> 0x08819FA0(k) -> table 0x08B4E990 + k*0x58; a fighter reads entry
// +0x18 and takes the AI word instead when +0x1690 is set (0x08858830: setup +0x13C == 2, CPU)
int psp_input_local(int k, uint32_t* b, uint8_t* x, uint8_t* y);

static int lmp_n() {
    uint32_t mode = psp_l32(0x08B48A28u);
    if (mode >= 11 && mode <= 13) return 1;
    uint32_t b; uint8_t x, y;
    return psp_input_local(0, &b, &x, &y);
}

// player of a fighter: 1 opponent 1, 2 partner, 3 opponent 2 (player 0 is the local fighter, team 0 member 0)
static int lmp_player(uint32_t team, uint32_t member) {
    if (team > 1 || member > 1) return -1;
    for (int k = 0; k < 4; k++) if (LMP_SLOT[k] == (int)(2 * team + member)) return k;
    return -1;
}

// players to fighters from the team sizes of the battle setup ([0x08B48A20 + team*0x1D0 + 0xC0], 0x08858974): player 1 the
// leader of team A, then opponent 1, partner, opponent 2, skipping slots without a fighter (1 vs 2: J3 is opponent 2)
// local select screen (mp_menu.inc): fighter slot of each human player and the team sizes it chose
static int lmp_cfg_on = 0, lmp_cfg_n = 0, lmp_cfg_slot[4], lmp_cfg_cnt[2];

static void lmp_map_update(int n) {
    int cnt[2] = {(int)psp_l32(0x08B48A20u + 0xC0), (int)psp_l32(0x08B48A20u + 0x1D0 + 0xC0)};
    if (cnt[0] < 1 || cnt[0] > 2 || cnt[1] < 1 || cnt[1] > 2) return;
    static const int pref[4] = {0, 2, 1, 3};
    int m[4], nm = 0;
    bool cfg = lmp_cfg_on && cnt[0] == lmp_cfg_cnt[0] && cnt[1] == lmp_cfg_cnt[1] && n == lmp_cfg_n;
    if (lmp_cfg_on && !cfg) {
        static int warn = 0;
        if (warn++ < 4) printf("local: la seleccion local pedia %d contra %d con %d jugadores, el combate tiene %d contra %d con %d: reparto por defecto\n",
                               lmp_cfg_cnt[0], lmp_cfg_cnt[1], lmp_cfg_n, cnt[0], cnt[1], n);
    }
    if (cfg) for (int k = 0; k < n; k++) m[nm++] = lmp_cfg_slot[k];
    for (int i = 0; i < 4; i++) {
        bool used = false;
        for (int j = 0; j < nm; j++) used |= m[j] == pref[i];
        if (!used && (pref[i] & 1) < cnt[pref[i] >> 1]) m[nm++] = pref[i];
    }
    for (int i = 0; i < 4; i++) {
        bool used = false;
        for (int j = 0; j < nm; j++) used |= m[j] == pref[i];
        if (!used) m[nm++] = pref[i];
    }
    if (!memcmp(m, LMP_SLOT, sizeof(m))) return;
    memcpy(LMP_SLOT, m, sizeof(m));
    static int logs = 0;
    if (logs++ < 8) printf("local: equipos %d contra %d, %d jugadores: J1 luchador %d, J2 luchador %d, J3 luchador %d, J4 luchador %d\n", cnt[0], cnt[1], n, m[0], m[1], m[2], m[3]);
}

static void lmp_log_fighters() {
    static uint32_t last_base = 0;
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!t) return;
    int n = (int)psp_l32(t);
    uint32_t base = psp_l32(t + 4);
    if (base == last_base || n < 1 || n > 8) return;
    last_base = base;
    printf("local: batalla, modo %u, %d luchadores\n", psp_l32(0x08B48A28u), n);
    for (int i = 0; i < n; i++) {
        uint32_t e = base + 0x1A90u * i;
        printf("local:   luchador %d equipo %u miembro %u indice %u mando %u cpu %u\n", i, psp_l32(e + 4), psp_l32(e + 8), psp_l32(e + 0x2C), psp_l32(e + 0x18), psp_l8(e + 0x1690));
    }
}

// per view visibility and detail: 0x089AC060(cam, 1) copies the camera matrices the culling test reads; 0x089AF33C(0) fills each model's render block (+0xB10: visible, detail) and the
// draw list from the current camera; 0x089AF48C(0) merges the blocks into the model flags. The tick runs them with
// player 1's camera, so each pass runs them again with its own (player 1 last leaves the tick's result)
// split screen (20c, 20d): the game keeps a camera per fighter (manager [0x08A7E260] + 0x720 + slot*0x290, all
// updated each tick); the battle draw 0x0885AD48 uses the current one [0x08A7E25C]. The draw runs once per view,
// player 1 last; the HUD packets come after all of them and cover the whole screen
static int lmp_local = -1;

void psp_ge_split_range(int slot, int pass, uint32_t lo, uint32_t hi);

// packet buffer of the frame: [0x08AF3200 + parity*4] + offset [0x08AF3218] (0x0882F838 / 0x0882F8BC)
static uint32_t lmp_pk() { uint32_t par = psp_l32(0x08AF3214u) & 1; return psp_l32(0x08AF3200u + 4 * par) + psp_l32(0x08AF3218u); }

void psp_gpu_split_views(int n);
void psp_gpu_battle_frame();
void psp_ge_split_skip(int slot, int mask);
int psp_gpu_split_alt_ok();

// fighter slot of each local player (2*team + member) and whether a slot's fighter is on the field

static bool lmp_slot_active(int slot) {
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!t) return false;
    int n = (int)psp_l32(t);
    uint32_t base = psp_l32(t + 4);
    for (int i = 0; i < n && i < 8; i++) {
        uint32_t e = base + 0x1A90u * i;
        if (psp_l32(e + 0xC) && (int)psp_l32(e + 0x2C) == slot) return true;
    }
    return false;
}

static uint32_t lmp_cam_of(Ctx& c, uint32_t mgr, int slot) {
    lmp_local = slot;
    uint32_t k = cl_call(c, 0x089ACA2Cu, 0, 0, 0, 0, 0, 0);
    lmp_local = -1;
    return mgr + 0x720u + 0x290u * k;
}

// slot seen in view v: the players' fighters, then a CPU fighter left over, else each fighter in turn every 6 s
static bool lmp_view_free(int v, int n) {
    if (v < n) return !lmp_slot_active(LMP_SLOT[v]);
    for (int k = n; k < 4; k++) if (lmp_slot_active(LMP_SLOT[k])) return false;
    return true;
}
static int lmp_view_slot(int v, int n) {
    if (v < n) return LMP_SLOT[v];
    for (int k = n; k < 4; k++) if (lmp_slot_active(LMP_SLOT[k])) return LMP_SLOT[k];
    int act[4], na = 0;
    for (int s = 0; s < 4; s++) if (lmp_slot_active(s)) act[na++] = s;
    return na ? act[(psp_time_us() / 6000000ull) % na] : 0;
}

// free view (3 humans, no fighter left): a map camera orbiting the fight like the map intro. A copy of player 1's
// camera with its own eye (+0x260) and target (+0x270), matrices rebuilt by 0x089AC4B8 (it also sets the current camera)
static uint32_t lmp_mcam = 0;
static float lmp_mc[3];
static bool lmp_mc_ok = false;
static int lmp_mcam_on = -1;
static float lmp_rf(uint32_t a) { uint32_t v = psp_l32(a); float f; memcpy(&f, &v, 4); return f; }
static void lmp_wf(uint32_t a, float f) { uint32_t v; memcpy(&v, &f, 4); psp_s32(a, v); }
static uint32_t lmp_mtpl = 0;
// view-space point of p through the camera matrix +0x40 (row or column layout, told by where the translation is)
static void lmp_view_pt(uint32_t cam, const float* p, float* v) {
    float m[16];
    for (int i = 0; i < 16; i++) m[i] = lmp_rf(cam + 0x40 + 4 * i);
    bool row = fabsf(m[12]) + fabsf(m[13]) + fabsf(m[14]) >= fabsf(m[3]) + fabsf(m[7]) + fabsf(m[11]);
    for (int i = 0; i < 3; i++) v[i] = row ? p[0] * m[i] + p[1] * m[4 + i] + p[2] * m[8 + i] + m[12 + i] : m[4 * i] * p[0] + m[4 * i + 1] * p[1] + m[4 * i + 2] * p[2] + m[4 * i + 3];
}
static float lmp_view_up(uint32_t cam) {
    float o[3] = {0, 0, 0}, u[3] = {0, -100.0f, 0}, a[3], b[3];
    lmp_view_pt(cam, o, a); lmp_view_pt(cam, u, b);
    return b[1] - a[1];
}
static float lmp_view_z(uint32_t cam, const float* p) { float v[3]; lmp_view_pt(cam, p, v); return v[2]; }
static uint32_t lmp_map_cam(Ctx& c, uint32_t mgr, uint32_t cam1) {
    if (lmp_mcam_on < 0) { const char* e = getenv("TTT_MAP_VIEW"); lmp_mcam_on = !(e && *e == '0'); }
    if (!lmp_mcam_on) return 0;
    if (!lmp_mcam) { lmp_mcam = psp_reserve(0x2A0); lmp_mtpl = psp_reserve(0x2A0); }
    if (!lmp_mcam || !lmp_mtpl) return 0;
    uint32_t tb = psp_l32(0x08A7DBC8u), base = tb ? psp_l32(tb + 4) : 0;
    static uint32_t last_base = 0;
    if (base != last_base) { last_base = base; lmp_mc_ok = false; }
    float ctr[3] = {0, 0, 0}, lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    int na = 0;
    for (int s = 0; s < 4; s++) {
        if (!lmp_slot_active(s)) continue;
        uint32_t t = mgr + 0x720u + 0x290u * s + 0x260u;
        for (int i = 0; i < 3; i++) { float v = lmp_rf(t + 4 * i); ctr[i] += v; if (v < lo[i]) lo[i] = v; if (v > hi[i]) hi[i] = v; }
        na++;
    }
    if (!na) return 0;
    for (int i = 0; i < 3; i++) ctr[i] /= na;
    if (!lmp_mc_ok) {   // centre and camera template taken once per battle: nothing the fighters do reaches this view later
        memcpy(lmp_mc, ctr, sizeof(ctr)); lmp_mc_ok = true;
        for (uint32_t o = 0; o < 0x290; o += 4) psp_s32(lmp_mtpl + o, psp_l32(cam1 + o));
    }
    float ex = lmp_rf(cam1 + 0x260) - lmp_rf(cam1 + 0x270), ey = lmp_rf(cam1 + 0x264) - lmp_rf(cam1 + 0x274), ez = lmp_rf(cam1 + 0x268) - lmp_rf(cam1 + 0x278);
    float cd = sqrtf(ex * ex + ey * ey + ez * ez), sx = hi[0] - lo[0], sz = hi[2] - lo[2];
    (void)sx; (void)sz;
    const float r = 650.0f;   // fixed: the shot must not follow the fighters
    // panoramic shots only: orbit 10 s, high view 8 s, wide sweep 10 s (the game's up is -Y)
    static const int DUR[3] = {10, 8, 10};
    uint64_t now = psp_time_us(), cyc = now / 28000000ull, pos = now % 28000000ull, beg = 0;
    int kind = 0;
    while (kind < 2 && pos >= beg + DUR[kind] * 1000000ull) { beg += DUR[kind] * 1000000ull; kind++; }
    float t = (float)((pos - beg) / (DUR[kind] * 1000000.0)), a0 = (float)(cyc % 9) * 0.7f + kind * 1.3f;
    kind = kind == 1 ? 2 : kind == 2 ? 3 : 0;
    float ex2, ey2, ez2;
    if (kind == 0) { float a = a0 + t * 0.9f; ex2 = r * cosf(a); ey2 = -r * 0.45f; ez2 = r * sinf(a); }
    else if (kind == 2) { float a = a0 + t * 0.5f; ex2 = r * 0.35f * cosf(a); ey2 = -r * 1.5f; ez2 = r * 0.35f * sinf(a); }
    else { float a = a0 + t * 1.3f; ex2 = r * 2.0f * cosf(a); ey2 = -r * 0.4f; ez2 = r * 2.0f * sinf(a); }
    for (uint32_t o = 0; o < 0x290; o += 4) psp_s32(lmp_mcam + o, psp_l32(lmp_mtpl + o));
    lmp_wf(lmp_mcam + 0x260, lmp_mc[0] + ex2); lmp_wf(lmp_mcam + 0x264, lmp_mc[1] + ey2); lmp_wf(lmp_mcam + 0x268, lmp_mc[2] + ez2); lmp_wf(lmp_mcam + 0x26C, 1.0f);
    // +0x270 holds the camera angles (X rotation, Y rotation): aim at the centre by trying the angle formulas with the
    // game's own matrix (centre in front, centred, upright), then keep the formula that worked
    static float ang[2];
    static int conv = -1;
    float fz = lmp_view_z(lmp_mtpl, lmp_mc) >= 0 ? 1.0f : -1.0f, upj = lmp_view_up(lmp_mtpl);
    float dx = -ex2, dy = -ey2, dz = -ez2, hz = sqrtf(dx * dx + dz * dz), pt = atan2f(dy, hz);
    uint32_t cur = psp_l32(0x08A7E25Cu);
    auto cand = [&](int k, float* g) {
        float sx = (k & 1) ? -1.0f : 1.0f, sz = (k & 2) ? -1.0f : 1.0f;
        g[0] = (k & 4) ? -pt : pt; g[1] = atan2f(sx * dx, sz * dz);
    };
    auto score = [&](const float* g) {
        lmp_wf(lmp_mcam + 0x270, g[0]); lmp_wf(lmp_mcam + 0x274, g[1]); lmp_wf(lmp_mcam + 0x278, 0.0f);
        cl_call(c, 0x089AC4B8u, lmp_mcam, 0, 0, 0, 0, 0);
        float v[3];
        lmp_view_pt(lmp_mcam, lmp_mc, v);
        float f = v[2] * fz;
        if (f <= 1.0f || lmp_view_up(lmp_mcam) * upj <= 0) return 1e9f;
        return (fabsf(v[0]) + fabsf(v[1])) / f;
    };
    float best = 1e9f;
    if (conv >= 0) { cand(conv, ang); best = score(ang); }
    if (best > 0.05f) {
        int bk = -1;
        float g[2];
        for (int k = 0; k < 8; k++) { cand(k, g); float e = score(g); if (e < best) { best = e; bk = k; } }
        if (bk >= 0) {
            static int logs3 = 0;
            if (conv != bk && logs3++ < 4) printf("local: vista libre, formula de angulos %d (error %.3f)\n", bk, best);
            conv = bk; cand(bk, ang);
        }
    }
    lmp_wf(lmp_mcam + 0x270, ang[0]); lmp_wf(lmp_mcam + 0x274, ang[1]); lmp_wf(lmp_mcam + 0x278, 0.0f);
    cl_call(c, 0x089AC4B8u, lmp_mcam, 0, 0, 0, 0, 0);
    psp_s32(0x08A7E25Cu, cur);
    static uint64_t next = 0;
    static int logs = 0;
    if (psp_time_us() >= next && logs < 6) {
        next = psp_time_us() + 5000000; logs++;
        float vc[3];
        lmp_view_pt(lmp_mcam, lmp_mc, vc);
        printf("local: vista libre, camara del mapa: centro (%.1f, %.1f, %.1f), toma %d, angulos (%.2f, %.2f), centro en la vista (%.1f, %.1f, %.1f), frente %.0f\n",
               lmp_mc[0], lmp_mc[1], lmp_mc[2], kind, ang[0], ang[1], vc[0], vc[1], vc[2], fz);
        (void)cd;
    }
    return lmp_mcam;
}

// views follow the fighter slots like the BT3 bars (4 views: A1 top left, B1 top right, A2 bottom left, B2 bottom
// right; 2 views: P1 left, P2 right); a view whose slot has no player shows its CPU or, with no fighter, the free view
static int lmp_vslot[4] = {0, 2, 1, 3}, lmp_vplayer[4] = {0, 1, 2, 3}, lmp_vview[4] = {0, 1, 2, 3};
static void lmp_views_map(int views, int n) {
    static const int ord[4] = {0, 2, 1, 3};
    for (int v = 0; v < 4; v++) { lmp_vslot[v] = views == 2 ? (v == 0 ? 0 : v == 1 ? LMP_SLOT[1] : -1) : ord[v]; lmp_vplayer[v] = -1; }
    for (int k = 0; k < 4; k++) lmp_vview[k] = -1;
    for (int v = 0; v < views; v++)
        for (int k = 0; k < n && k < 4; k++) if (LMP_SLOT[k] == lmp_vslot[v]) { lmp_vplayer[v] = k; lmp_vview[k] = v; }
}
int psp_local_view_player(int v) { return v >= 0 && v < 4 && lmp_vplayer[v] >= 0 ? lmp_vplayer[v] : v; }

static uint64_t lmp_now_us() { return (uint64_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

// the frame's packet buffers (two of [0x08AF320C] bytes, 0x68000, allocated by 0x0882F6F4) hold one view; libgu does
// not check the end, so several views ran past it and corrupted the heap (music thread object, hangs). Both buffers
// move to reserved memory LMP_PK_MUL times as large; packets already queued stay valid where they are. The free
// 0x0882F68C gets the game's own block back
static const uint32_t LMP_PK_MUL = 5;
static uint32_t lmp_pk_new[2] = {0, 0}, lmp_pk_cap = 0, lmp_pk_peak = 0, lmp_pk_orig = 0;

static void lmp_pk_grow() {
    uint32_t cap = psp_l32(0x08AF320Cu), b0 = psp_l32(0x08AF3200u);
    if (!cap || !b0 || (b0 == lmp_pk_new[0] && cap == lmp_pk_cap)) return;
    uint32_t big = ((cap * LMP_PK_MUL) + 63) & ~63u;
    lmp_pk_orig = b0;
    if (lmp_pk_cap != big) { lmp_pk_new[0] = psp_reserve(big); lmp_pk_new[1] = psp_reserve(big); lmp_pk_cap = big; }
    psp_s32(0x08AF3200u, lmp_pk_new[0]); psp_s32(0x08AF3204u, lmp_pk_new[1]); psp_s32(0x08AF320Cu, big);
    printf("local: paquetes reubicados, capacidad %u KB -> %u KB\n", cap / 1024, big / 1024);
}

// per-player HUD (20.9): the HUD container (class 0x089D3930) children that follow the local player get a copy per extra
// human, the manager's target rings two per extra human; every node runs with its owner as the local player. The HUD
// root draw 0x089D070C runs full screen (shared pieces) and then once per player in his view (GE passes 5..12)
static const uint32_t LMP_HUD_OFF[8] = {0x6C, 0x70, 0x74, 0x78, 0x7C, 0x80, 0x84, 0x88};
static const bool LMP_HUD_SHARED[8] = {false, false, false, false, false, true, false, false};   // battle_text: one for everyone
static const uint32_t LMP_HUD_SIZE[8] = {0xA4, 0x94, 0x94, 0x94, 0x94, 0x74, 0x74, 0xDC};
static const uint32_t LMP_HUD_CTOR[8] = {0x089D6390u, 0x089F1824u, 0x089E9E78u, 0x089E9E78u, 0x089EAA14u, 0x089DAC90u, 0x089EEA00u, 0x089D72F4u};
static uint32_t lmp_hcont = 0, lmp_hmgr = 0, lmp_hcopy[4][8], lmp_hring[4][2], lmp_harrow[4];
static struct { uint32_t o; int k; bool ring; int idx; } lmp_hown[48];
static int lmp_hlay = 0;               // split layout AGS variants loaded: 0 none, 2 or 4 views
static int lmp_hown_n = 0, lmp_hk = -1, lmp_hpass = -1, lmp_hview = 0, lmp_hkind = -1, lmp_hviews = 0;
static bool lmp_hr = false, lmp_hlive = false, lmp_hsplit = false, lmp_hcupd = false;
static uint32_t lmp_lists_peak = 0;
// per-frame list table of 0x08838054: 128 lists ([0x08B2E448] counts them); past it lists come back null
static uint32_t lmp_lists() { return psp_l32(0x08B2E448u); }
static uint32_t lmp_vcam[4], lmp_vsel = 0;

// each per-player node's list range goes to the GE, which measures the piece and has the GPU place it in its view
void psp_ge_hud_node(int slot, uint32_t lo, uint32_t hi, int code);
static float lmp_crop[2] = {120.0f, 120.0f};
static int lmp_hmode = 1;               // TTT_SPLIT_HUD: 0 off, 1 on, 2 copies without per-player passes, 3 passes without copies
// TTT_SPLIT_HUD_MASK (hex): bits 0..7 copy of that container child, 8 ring copies, 9 container update replay, 10 ring camera, 11 enemy arrows
static uint32_t lmp_hmask = 0xFFFFFFFFu;
static uint32_t lmp_gu_ptr() { uint32_t g = psp_l32(0x08B478D0u); return g ? psp_l32(g + 8) : 0; }
void psp_gpu_split_crop(int v, float start);

// 2 views: each view shows 240 of the full render's 480 columns; the window follows its fighter when he nears an edge
static uint32_t lmp_scr = 0;
static void lmp_crop_update(const Ctx& c, int views, bool shared, const uint32_t* cams, const int* slots) {
    static int calls = 0;
    float fxs[2] = {-1, -1};
    for (int v = 0; v < 2; v++) {
        float target = 120.0f;
        if (views == 2 && !shared) {
            if (!lmp_scr) lmp_scr = psp_reserve(64);
            uint32_t t = psp_l32(0x08A7DBC8u), base = t ? psp_l32(t + 4) : 0, ent = 0;
            int n = t ? (int)psp_l32(t) : 0;
            for (int i = 0; i < n && i < 8; i++) { uint32_t e = base + 0x1A90u * i; if (psp_l32(e + 0xC) && (int)psp_l32(e + 0x2C) == slots[v]) ent = entity_of(e); }
            if (ent) {
                FReg one; one.f = 1.0f;
                for (int k = 0; k < 3; k++) psp_s32(lmp_scr + 4 * k, psp_l32(ent + 0x950 + 4 * k));
                psp_s32(lmp_scr + 12, one.u);
                if (cl_call(c, 0x089D3800u, lmp_scr + 0x20, cams[v] + 0x140u, lmp_scr, 1, 0, 0)) {
                    FReg fx; fx.u = psp_l32(lmp_scr + 0x20);
                    fxs[v] = fx.f;
                    if (fx.f < 176.0f) target = fx.f - 56.0f;
                    else if (fx.f > 304.0f) target = fx.f - 184.0f;
                }
            }
            target = target < 0 ? 0 : target > 240.0f ? 240.0f : target;
            lmp_crop[v] += (target - lmp_crop[v]) * 0.12f;
        } else lmp_crop[v] = 120.0f;
        psp_gpu_split_crop(v + 1, lmp_crop[v]);
    }
    if (views == 2 && !shared && ++calls % 300 == 0) printf("local: recorte v1 luchador x %.0f inicio %.0f | v2 luchador x %.0f inicio %.0f\n", fxs[0], lmp_crop[0], fxs[1], lmp_crop[1]);
}

// the copies reuse what the originals loaded (0x088404D8 by caller and entries): loading it again per player
// filled guest memory with 4 players (effects gone, crashes)
static struct { uint32_t ra, a2, a3, v; } lmp_res[512];
static int lmp_res_n = 0;
static bool lmp_hrec = false, lmp_hbuild = false, lmp_hdel = false;
static bool lmp_res_wrap(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t ra = c.r[31], a2 = c.r[6], a3 = c.r[7];
    if (lmp_hbuild) {
        for (int i = 0; i < lmp_res_n; i++) if (lmp_res[i].ra == ra && lmp_res[i].a2 == a2 && lmp_res[i].a3 == a3) { c.r[2] = lmp_res[i].v; return true; }
        static int miss = 0;
        if (miss++ < 20) printf("local: recurso no compartido, ags %u rhg %u desde 0x%08X\n", a2, a3, ra - 8);
        return false;
    }
    if (!hud_wrap(0x088404D8u, c, orig)) orig(c);
    if (lmp_res_n < 512) lmp_res[lmp_res_n++] = {ra, a2, a3, c.r[2]};
    return true;
}

// technique window in a right view: the GPU flips the whole piece, so its text calls are flipped here first around the
// box of all of them (the name comes in several calls) and read normally
static bool lmp_tflip = false;
static struct { uint32_t v, n; int st, po, ps; } lmp_tcall[64];
static int lmp_tcall_n = 0;
static void lmp_text_flip(const Ctx& c) {
    uint32_t n = c.r[6], v = c.r[8], vt = c.r[5] ? c.r[5] : cl_vt;
    if (!v || c.r[7] || !(vt & 0x800000u) || n < 2 || n > 4096 || lmp_tcall_n >= 64) return;
    int po, ps, st = cl_vlayout(vt, &po, &ps);
    if (ps == 2 || ps == 4) lmp_tcall[lmp_tcall_n++] = {v, n, st, po, ps};
}
static float lmp_tx(int i, uint32_t k) {
    uint32_t a = lmp_tcall[i].v + k * lmp_tcall[i].st + lmp_tcall[i].po;
    if (lmp_tcall[i].ps == 2) return (float)(int16_t)psp_l16(a);
    FReg f; f.u = psp_l32(a); return f.f;
}
static void lmp_text_apply() {
    float x0 = 1e9f, x1 = -1e9f;
    for (int i = 0; i < lmp_tcall_n; i++)
        for (uint32_t k = 0; k < lmp_tcall[i].n; k++) { float x = lmp_tx(i, k); x0 = std::fmin(x0, x); x1 = std::fmax(x1, x); }
    for (int i = 0; i < lmp_tcall_n; i++)
        for (uint32_t k = 0; k < lmp_tcall[i].n; k++) {
            uint32_t a = lmp_tcall[i].v + k * lmp_tcall[i].st + lmp_tcall[i].po;
            float x = x0 + x1 - lmp_tx(i, k);
            if (lmp_tcall[i].ps == 2) psp_s16(a, (uint16_t)(int16_t)lroundf(x)); else { FReg f; f.f = x; psp_s32(a, f.u); }
        }
    lmp_tcall_n = 0;
}

static const int LMP_ARROW = 9;
static int lmp_hud_owner(uint32_t o, bool* ring, int* idx = nullptr) {
    for (int i = 0; i < lmp_hown_n; i++) if (lmp_hown[i].o == o) { *ring = lmp_hown[i].ring; if (idx) *idx = lmp_hown[i].idx; return lmp_hown[i].k; }
    return -1;
}
static void lmp_hud_add(uint32_t o, int k, bool ring, int idx = -1) { if (o && lmp_hown_n < 48) lmp_hown[lmp_hown_n++] = {o, k, ring, idx}; }

static void lmp_virt(const Ctx& c, uint32_t o, uint32_t off, uint32_t a1) {
    uint32_t v = psp_l32(o + 0x40) + off;
    cl_call(c, psp_l32(v + 4), o + (uint32_t)(int32_t)(int16_t)psp_l16(v), a1, 0, 0, 0, 0);
}

static void lmp_hud_destroy(const Ctx& c) {
    lmp_hdel = true;
    for (int k = 1; k < 4; k++) {
        for (int i = 0; i < 8; i++) if (lmp_hcopy[k][i]) { cl_call(c, 0x089CFD08u, lmp_hcont, lmp_hcopy[k][i], 0, 0, 0, 0); lmp_virt(c, lmp_hcopy[k][i], 8, 3); }
        for (int j = 0; j < 2; j++) if (lmp_hring[k][j] && lmp_hmgr) { cl_call(c, 0x089CFD08u, lmp_hmgr, lmp_hring[k][j], 0, 0, 0, 0); lmp_virt(c, lmp_hring[k][j], 8, 3); }
        if (lmp_harrow[k] && lmp_hmgr) { cl_call(c, 0x089CFD08u, lmp_hmgr, lmp_harrow[k], 0, 0, 0, 0); lmp_virt(c, lmp_harrow[k], 8, 3); }
    }
    lmp_hdel = false;
    memset(lmp_hcopy, 0, sizeof(lmp_hcopy)); memset(lmp_hring, 0, sizeof(lmp_hring)); memset(lmp_harrow, 0, sizeof(lmp_harrow)); memset(hud_kids_extra, 0, sizeof(hud_kids_extra));
    lmp_hown_n = 0; lmp_hcont = lmp_hmgr = 0;
}

static void lmp_hud_build(const Ctx& c, uint32_t ct) {
    memset(lmp_hcopy, 0, sizeof(lmp_hcopy)); memset(lmp_hring, 0, sizeof(lmp_hring)); memset(lmp_harrow, 0, sizeof(lmp_harrow)); memset(hud_kids_extra, 0, sizeof(hud_kids_extra));
    lmp_hown_n = 0; lmp_hcont = lmp_hmgr = 0;
    int n = lmp_n();
    if (n < 2 || !patch_split_screen.val) return;
    lmp_hmode = patch_split_hud.val >= 0 && patch_split_hud.val <= 3 ? patch_split_hud.val : 1;
    if (!lmp_hmode) { printf("local: HUD por jugador desactivado (TTT_SPLIT_HUD=0)\n"); return; }
    const char* em = getenv("TTT_SPLIT_HUD_MASK");
    lmp_hmask = em && *em ? (uint32_t)strtoul(em, nullptr, 16) : 0xFFFFFFFFu;
    lmp_hcont = ct; lmp_hmgr = psp_l32(ct + 0x68);
    lmp_hlay = hud_split_layout();
    for (int i = 0; i < 8; i++) if (!LMP_HUD_SHARED[i]) lmp_hud_add(psp_l32(ct + LMP_HUD_OFF[i]), 0, false, i);
    if (lmp_hmgr) { lmp_hud_add(psp_l32(lmp_hmgr + 0x84), 0, true); lmp_hud_add(psp_l32(lmp_hmgr + 0x88), 0, true); }
    if (lmp_hmgr && psp_l32(lmp_hmgr + 0xAC) && (lmp_hmask & 0x800)) lmp_hud_add(psp_l32(lmp_hmgr + 0xAC), 0, true, LMP_ARROW);
    uint32_t tab = psp_l32(0x08A7DBC8u), base = tab ? psp_l32(tab + 4) : 0;
    for (int k = 1; k < n && k < 4 && lmp_hmode != 3; k++) {
        int slot = LMP_SLOT[k];
        lmp_local = slot; lmp_hk = k; lmp_hbuild = true;
        for (int i = 0; i < 8; i++) {
            if (LMP_HUD_SHARED[i] || !(lmp_hmask & (1u << i)) || !psp_l32(ct + LMP_HUD_OFF[i])) continue;
            uint32_t o = cl_call(c, 0x089BFDF4u, LMP_HUD_SIZE[i], 0, 0, 0, 0, 0);
            if (!o) continue;
            cl_call(c, LMP_HUD_CTOR[i], o, i == 2 ? 1 : 0, 0, 0, 0, 0);
            lmp_hcopy[k][i] = o;
        }
        if (lmp_hcopy[k][1] && base) lmp_virt(c, lmp_hcopy[k][1], 0x88, base + 0x1A90u * slot);
        for (int i = 0; i < 8; i++) if (lmp_hcopy[k][i]) { cl_call(c, 0x089D0310u, ct, lmp_hcopy[k][i], 0x80, 0, 0, 0); lmp_hud_add(lmp_hcopy[k][i], k, false, i); }
        for (int j = 0; j < 2 && lmp_hmgr && (lmp_hmask & 0x100); j++) {
            uint32_t o = cl_call(c, 0x089BFDF4u, 0x78, 0, 0, 0, 0, 0);
            if (!o) continue;
            cl_call(c, 0x089F283Cu, o, 0, 0, 0, 0, 0);
            cl_call(c, 0x089D0310u, lmp_hmgr, o, 0x80, 0, 0, 0);
            lmp_hring[k][j] = o; lmp_hud_add(o, k, true);
        }
        if (lmp_hmgr && psp_l32(lmp_hmgr + 0xAC) && (lmp_hmask & 0x800)) {   // enemy arrows (0x089D9B10): one per player, from his fighter and camera
            uint32_t o = cl_call(c, 0x089BFDF4u, 0x94, 0, 0, 0, 0, 0);
            if (o) {
                cl_call(c, 0x089D9B10u, o, 0, 0, 0, 0, 0);
                lmp_virt(c, o, 0x10, 0);
                cl_call(c, 0x089D0310u, lmp_hmgr, o, 0x80, 0, 0, 0);
                uint32_t oh = psp_l32(psp_l32(lmp_hmgr + 0xAC) + 0x70);
                bool set = oh && psp_l32(oh + 8);
                if (set) lmp_virt(c, o, 0x88, lmp_hmgr + 0x70);
                lmp_harrow[k] = o; lmp_hud_add(o, k, true, LMP_ARROW);
                hud_kids_extra[k - 1] = o;
            }
        }
        lmp_local = -1; lmp_hk = -1; lmp_hbuild = false;
    }
    printf("local: HUD por jugador, %d jugadores, %d piezas (flechas %d), modo %d, mascara %X, %d recursos compartidos, diseno %d vistas\n", n, lmp_hown_n, (lmp_harrow[1] ? 1 : 0) + (lmp_harrow[2] ? 1 : 0) + (lmp_harrow[3] ? 1 : 0), lmp_hmode, lmp_hmask, lmp_res_n, lmp_hlay);
}

// ring copy: bound to its player's opponents (member 0, 1), shown like the original ring
static void lmp_ring_prep(const Ctx& c, uint32_t o, int k) {
    int j = lmp_hring[k][0] == o ? 0 : 1;
    if (!psp_l32(o + 0x70)) {
        uint32_t t = psp_l32(0x08A7DBC8u);
        int n = t ? (int)psp_l32(t) : 0, team = LMP_SLOT[k] >> 1;
        uint32_t base = t ? psp_l32(t + 4) : 0;
        for (int i = 0; i < n && i < 8; i++) {
            uint32_t e = base + 0x1A90u * i;
            if (psp_l32(e + 0xC) && (int)psp_l32(e + 4) != team && (int)psp_l32(e + 8) == j) { lmp_virt(c, o, 0x88, e); break; }
        }
    }
    uint32_t orig_ring = lmp_hmgr ? psp_l32(lmp_hmgr + 0x84 + 4 * j) : 0;
    if (orig_ring && psp_l8(orig_ring + 0x75) != psp_l8(o + 0x75)) lmp_virt(c, o, 0x90, psp_l8(orig_ring + 0x75));
}

// item 58, Graphics > Battle HUD off: the HUD container's children are not drawn except the command prompt (and its
// per-player copies), the mode/pause pages and the clash HUD, which gameplay needs; re-read live once per 15 root draws
static bool hud_hide = false, hud_back = false, hud_back_frame = false, hud_back_drawing = false;
static int hud_pass = 0;
static void hud_hide_read() {
    static int n = 0;
    if (n++ % 15) return;
    const char* e = getenv("TTT_BATTLE_HUD");
    bool h = e && *e && !atoi(e);
    if (h != hud_hide) { hud_hide = h; printf("hud: interfaz de combate %s\n", h ? "oculta" : "visible"); }
    e = getenv("TTT_BARS_BEHIND");
    h = e && *e && atoi(e);
    if (h != hud_back) { hud_back = h; printf("hud: barras %s\n", h ? "detras de los luchadores" : "encima de todo"); }
}

// Graphics > Bars behind fighters: the four life bars (BT3 bars, one view) go into their own packet queued right before the
// fighters' packet (0x0880BD3C from the battle draw), between the map and the fighters/effects; GE state is saved and given back
// around it (port marker 0xEF) so the fighters draw as before; the HUD walk then skips them
// pieces that go behind: nodes whose draw (virtual +0x68) is the life bar, union counter, senzu, timer/defeated counter or
// ability icons; collected by the HUD walk of the previous frame, dropped when the HUD container unloads
static uint32_t hud_back_ctx = 0, hb_cur[24], hb_next[24];
static int hb_ncur = 0, hb_nnext = 0;
static bool hud_back_class(uint32_t node) {
    uint32_t vt = psp_l32(node + 0x40);
    if (!guest_ptr(vt)) return false;
    uint32_t fn = psp_l32(vt + 0x6C);
    return fn == 0x089E5674u || fn == 0x089F6060u || fn == 0x089E4354u || fn == 0x089F4990u || fn == 0x089D5F44u;
}
static void hud_back_draw(const Ctx& c) {
    if (!hud_back || hud_hide || !hud_on || !hud_mgr || lmp_hown_n || c.r[31] != 0x0885AD7Cu || !psp_l32(0x08B48A08u)) return;
    uint32_t bars[24]; int nb = 0;
    for (int k = 0; k < hb_ncur; k++) if (hud_back_class(hb_cur[k])) bars[nb++] = hb_cur[k];
    if (!nb) return;
    if (!hud_back_ctx) hud_back_ctx = psp_reserve(16);
    psp_s32(hud_back_ctx, (uint32_t)hud_pass);
    uint32_t mask = psp_l32(0x08B47888u + 0x44);
    hud_guest(c, 0x088403B8u, 0x08B48200u);
    uint32_t list = hud_guest(c, 0x08838054u, 0);
    uint32_t gc = psp_l32(0x08B478D0u), p = gc ? psp_l32(gc + 8) : 0;
    if (!list || !p) { if (list) { hud_guest(c, 0x08838120u, list); hud_guest(c, 0x08837F54u, 0x08B4E690u, list, 0); } hud_guest(c, 0x088403D4u, 0x08B48200u); return; }
    psp_s32(p, 0x00EF0001u); psp_s32(p + 4, 0x23000000u); psp_s32(p + 8, 0x24000000u); psp_s32(p + 12, 0xE7000001u); psp_s32(p + 16, 0xE90000FFu);
    psp_s32(gc + 8, p + 20);
    hud_guest(c, 0x08A64240u, 0x15); hud_guest(c, 0x08A641E8u, 9); hud_guest(c, 0x08A64630u, 1, 1);
    hud_back_drawing = true;
    for (int i = 0; i < nb; i++) hud_guest(c, 0x089CFFFCu, bars[i], hud_back_ctx);
    hud_back_drawing = false;
    p = psp_l32(gc + 8);
    psp_s32(p, 0x00EF0000u); psp_s32(gc + 8, p + 4);
    hud_guest(c, 0x08838120u, list);
    hud_guest(c, 0x08837F54u, 0x08B4E690u, list, 0);
    psp_s32(0x08B47888u + 0x44, mask);
    hud_guest(c, 0x088403D4u, 0x08B48200u);
    hud_back_frame = true;
    static int logs = 0;
    if (logs++ < 3) printf("hud: %d piezas dibujadas antes de los luchadores (pasada %d)\n", nb, hud_pass);
}
// item 62 measurement: local fighter, its target and the game camera (fighter +0x460 eye, +0x470 angles), every 30 draws
static bool hud_back_skip(uint32_t node) {
    if (hud_back_drawing || !hud_back || !hud_mgr) return false;
    bool mine = false;
    for (int k = 0; k < hb_ncur && !mine; k++) mine = hb_cur[k] == node;
    if (hb_nnext < 24 && (mine || hud_back_class(node))) hb_next[hb_nnext++] = node;
    return mine && hud_back_frame;
}
static bool hud_hide_node(uint32_t node) {
    uint32_t ct = psp_l32(0x08A7B850u);
    if (!ct || !node || psp_l32(node + 0xC) != ct) return false;
    static const uint32_t keep[] = {0x88, 0x8C, 0x90, 0x98, 0xA0, 0xA4, 0xA8};
    for (uint32_t k : keep) if (psp_l32(ct + k) == node) return false;
    for (int k = 1; k < 4; k++) if (lmp_hcopy[k][7] == node) return false;
    return true;
}

static bool lmp_hud_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (dir == 0x089D070Cu) { hud_hide_read(); hud_pass = (int)c.r[5]; }
    if (dir == 0x089CFFFCu) bh_vpass = lmp_hpass;
    if (dir == 0x089CFFFCu && ((hud_hide && hud_hide_node(c.r[4])) || hud_back_skip(c.r[4]) || bh_skip_node(c.r[4]))) return true;
    if (dir == 0x089CFFFCu && bh_result_wrap(c, orig)) return true;
    if (dir == 0x089D4CD4u) { uint32_t ct = c.r[4]; lmp_res_n = 0; lmp_hrec = true; orig(c); lmp_hrec = false; lmp_hud_build(c, ct); return true; }
    if (dir == 0x089D39B4u) { hb_ncur = hb_nnext = 0; if (lmp_hcont && c.r[4] == lmp_hcont) lmp_hud_destroy(c); orig(c); return true; }
    if (!lmp_hown_n) {
        if (dir == 0x089D070Cu) {
            lmp_hlive = false; hb_nnext = 0;
            orig(c);
            if (hud_back) { memcpy(hb_cur, hb_next, sizeof hb_cur); hb_ncur = hb_nnext; } else hb_ncur = 0;
            hud_back_frame = false;
            return true;
        }
        return false;
    }
    if (dir == 0x089D5164u) {                 // container update: its events again for each player's copies
        if (c.r[4] != lmp_hcont || !(lmp_hmask & 0x200)) return false;
        lmp_hcupd = true;
        { Ctx c2 = c; orig(c2); }
        for (int k = 1; k < 4; k++) {
            if (!lmp_hcopy[k][0] && !lmp_hcopy[k][7]) continue;
            uint32_t keep[8];
            for (int i = 0; i < 8; i++) { keep[i] = psp_l32(lmp_hcont + LMP_HUD_OFF[i]); if (lmp_hcopy[k][i]) psp_s32(lmp_hcont + LMP_HUD_OFF[i], lmp_hcopy[k][i]); }
            lmp_local = LMP_SLOT[k]; lmp_hk = k;
            Ctx c2 = c; orig(c2);
            lmp_local = -1; lmp_hk = -1;
            for (int i = 0; i < 8; i++) psp_s32(lmp_hcont + LMP_HUD_OFF[i], keep[i]);
        }
        lmp_hcupd = false;
        return true;
    }
    if (dir == 0x089D4030u) {                 // container event for battle_text (Ready, Fight, round flow): every player's copy too
        if (c.r[4] != lmp_hcont || lmp_hcupd || lmp_hk >= 0) return false;
        { Ctx c2 = c; orig(c2); }
        for (int k = 1; k < 4; k++) {
            if (!lmp_hcopy[k][5]) continue;
            uint32_t keep = psp_l32(lmp_hcont + 0x80);
            psp_s32(lmp_hcont + 0x80, lmp_hcopy[k][5]);
            lmp_local = LMP_SLOT[k]; lmp_hk = k;
            Ctx c2 = c; orig(c2);
            lmp_local = -1; lmp_hk = -1;
            psp_s32(lmp_hcont + 0x80, keep);
        }
        return true;
    }
    if (dir == 0x089CFEA4u) {                 // update walk: owner as local player; ring copies project with their player's camera
        bool ring = false;
        int idx = -1;
        int own = lmp_hud_owner(c.r[4], &ring, &idx);
        if (own < 0) return false;
        int pk = lmp_hk, pl = lmp_local;
        lmp_hk = own; lmp_local = LMP_SLOT[own];
        uint32_t cm = psp_l32(0x08A7E260u), cur = psp_l32(0x08A7E25Cu);
        bool cam = ring && own > 0 && cm && (lmp_hmask & 0x400);
        if (ring && own > 0 && idx != LMP_ARROW) lmp_ring_prep(c, c.r[4], own);
        if (cam) psp_s32(0x08A7E25Cu, cm + 0x720u + 0x290u * cl_call(c, 0x089ACA2Cu, 0, 0, 0, 0, 0, 0));
        orig(c);
        if (cam) psp_s32(0x08A7E25Cu, cur);
        lmp_hk = pk; lmp_local = pl;
        return true;
    }
    if (dir == 0x089CFFFCu) {                 // draw walk: a node is drawn only in the pass of its owner (shared: full screen)
        bool ring = false;
        uint32_t node = c.r[4];
        int idx = -1;
        int own = lmp_hud_owner(node, &ring, &idx);
        int pk = lmp_hk, pl = lmp_local; bool pr = lmp_hr;
        if (own >= 0) { lmp_hk = own; lmp_hr = ring; lmp_local = LMP_SLOT[own]; }
        int e = lmp_hk;
        bool show = lmp_hpass < 0 ? (e < 0 || (e == 0 && !lmp_hsplit)) : (e == lmp_hpass && (lmp_hkind < 0 || lmp_hkind == (lmp_hr ? 1 : 0)));
        uint32_t m = psp_l32(node + 0x34);
        if (!show) psp_s32(node + 0x34, 0);
        bool col = show && own >= 0 && lmp_hpass >= 0;
        uint32_t lo = col ? lmp_gu_ptr() : 0;
        bool tf = lmp_tflip, mine = col && lmp_hlay && idx == 1 && (lmp_hview & 1);
        if (mine) { lmp_tflip = true; lmp_tcall_n = 0; }
        orig(c);
        if (mine) { lmp_text_apply(); lmp_tflip = tf; }
        if (col && lo) psp_ge_hud_node((int)(psp_l32(0x08AF3214u) & 1), lo, lmp_gu_ptr(), (lmp_hview + 1) | (ring ? 8 : 0) | (lmp_hlay && !ring && idx >= 0 && idx != 6 ? 16 : 0) | (idx == 1 && (lmp_hview & 1) ? 32 : 0) | (idx == 2 || idx == 3 ? 64 : 0) | (idx == 4 ? 128 : 0));
        if (!show) psp_s32(node + 0x34, m);
        lmp_hk = pk; lmp_hr = pr; lmp_local = pl;
        return true;
    }
    if (dir == 0x089D070Cu) {                 // HUD root draw after the battle draw: full screen, then each player in his view
        int slot = (int)(psp_l32(0x08AF3214u) & 1);
        for (int p = 5; p <= 12; p++) psp_ge_split_range(slot, p, 0, 0);
        psp_ge_hud_node(slot, 0, 0, -1);
        bool live = lmp_hlive && (c.r[31] == 0x0885AE74u || c.r[31] == 0x0885AE88u);
        lmp_hlive = false;
        if (!live) { lmp_hsplit = false; return false; }
        lmp_hpass = -1; lmp_hkind = -1;
        { Ctx c2 = c; orig(c2); }
        if (lmp_hsplit && lmp_hmode != 2) {
            uint32_t cm = psp_l32(0x08A7E260u), cur = psp_l32(0x08A7E25Cu), sel = psp_l32(cm + 0x1160u);
            int n = lmp_n(), kinds = 1;
            static int full_logs = 0;
            for (int k = 0; k < n && k < 4; k++)
                for (int kd = 0; kd < kinds; kd++) {
                    int vk = lmp_vview[k];
                    if (vk < 0 || vk >= lmp_hviews) continue;
                    if (lmp_lists() >= 0x7E) { if (full_logs++ < 10) printf("local: HUD del jugador %d omitido, %u listas ocupadas\n", k + 1, lmp_lists()); continue; }
                    uint32_t cam = lmp_vcam[vk], s2 = vk ? cam : lmp_vsel;
                    psp_s32(0x08A7E25Cu, cam); psp_s32(cm + 0x1160u, s2);
                    cl_call(c, 0x089AC060u, s2, 1, 0, 0, 0, 0);
                    lmp_hpass = k; lmp_hview = vk; lmp_hkind = kinds == 2 ? kd : -1;
                    uint32_t lo = lmp_pk();
                    Ctx c2 = c; orig(c2);
                    psp_ge_split_range(slot, 5 + vk + (kd ? 4 : 0), lo, lmp_pk());
                }
            psp_s32(0x08A7E25Cu, cur); psp_s32(cm + 0x1160u, sel);
            cl_call(c, 0x089AC060u, sel, 1, 0, 0, 0, 0);
        }
        lmp_hpass = -1; lmp_hkind = -1;
        if (lmp_lists() > lmp_lists_peak) lmp_lists_peak = lmp_lists();
        return true;
    }
    return false;
}

static void lmp_draw(Ctx& c, void (*orig)(Ctx&)) {
    int n = lmp_n(), views = n == 2 ? 2 : 4;
    if (patch_split_packets.val) lmp_pk_grow();
    uint32_t mgr = psp_l32(0x08A7E260u), cur = psp_l32(0x08A7E25Cu), sel = psp_l32(mgr + 0x1160u);
    uint32_t cam1 = mgr + 0x720u + 0x290u * cl_call(c, 0x089ACA2Cu, 0, 0, 0, 0, 0, 0);
    int spk = (int)cl_call(c, 0x089ACCC4u, 0, 0, 0, 0, 0, 0);
    bool special = spk >= 0 && spk < 4;
    bool shared = cur != cam1 || special;
    static int last_kind = -2, kind_logs = 0;
    int kind = special ? spk : cur != cam1 ? 4 : -1;
    if (kind != last_kind && kind_logs < 40) {
        kind_logs++;
        if (kind >= 0 && kind < 4) printf("local: camara especial %d (prioridad %d), pantalla completa\n", kind, (int)psp_l32(mgr + 0x9A8u + 0x290u * kind));
        else if (kind == 4) printf("local: camara de escena, pantalla completa\n");
        else printf("local: vuelve la pantalla dividida\n");
    }
    last_kind = kind;
    uint32_t cams[4] = {cur, cur, cur, cur};
    int slots[4] = {0, 0, 0, 0};
    lmp_views_map(views, n);
    for (int v = 1; v < views; v++) {
        slots[v] = lmp_vslot[v];
        if (shared) continue;
        uint32_t mc = !lmp_slot_active(slots[v]) ? lmp_map_cam(c, mgr, cam1) : 0;
        cams[v] = mc ? mc : lmp_cam_of(c, mgr, slots[v]);
    }

    static uint32_t logged_mgr = 0;
    static int logged_views = 0;
    int slot = (int)(psp_l32(0x08AF3214u) & 1);
    if ((mgr != logged_mgr || views != logged_views) && !shared) {
        logged_mgr = mgr; logged_views = views;
        printf("local: pantalla dividida, %d vistas:", views);
        for (int v = 0; v < views; v++) { if (cams[v] == lmp_mcam && v) printf(" vista %d camara del mapa", v + 1); else printf(" vista %d luchador %d camara %d", v + 1, slots[v], (int)((cams[v] - mgr - 0x720u) / 0x290u)); }
        printf("\n");
    }
    static int alt_last = 0;
    lmp_crop_update(c, views, shared, cams, slots);
    psp_gpu_battle_frame();
    lmp_hlive = true; lmp_hsplit = !shared; lmp_hviews = views; lmp_vsel = sel;
    for (int v = 0; v < 4; v++) lmp_vcam[v] = v ? cams[v] : cur;
    if (shared) {
        for (int v = 0; v < 4; v++) psp_ge_split_range(slot, v + 1, 0, 0);
        psp_ge_split_skip(slot, 0);
        alt_last = 0;
        orig(c);
        return;
    }
    psp_gpu_split_views(views);
    uint32_t bb = psp_l32(0x08AF3200u + 4 * (uint32_t)slot);
    psp_ge_split_range(slot, 0, bb, bb + psp_l32(0x08AF320Cu));
    for (int v = views; v < 4; v++) psp_ge_split_range(slot, v + 1, 0, 0);
    bool elig = views == 4 && patch_split_alt.val && psp_fps60() && psp_gpu_split_alt_ok();
    int draw = (1 << views) - 1;
    if (elig) draw = alt_last == 0x3 ? 0xC : (alt_last == 0xC || alt_last == 0xF) ? 0x3 : 0xF;
    alt_last = elig ? draw : 0;
    psp_ge_split_skip(slot, (elig ? 0x10 : 0) | (~draw & 0xF));
    static int alt_logs = 0;
    if (elig && alt_logs < 3) { alt_logs++; printf("local: vistas alternas, este cuadro dibuja 0x%X\n", draw); }
    static uint64_t us[4], frames = 0;
    static int lines = 0;
    for (int v = views - 1; v >= 0; v--) {
        if (!(draw & (1 << v))) { psp_ge_split_range(slot, v + 1, 0, 0); continue; }
        uint32_t cam = v ? cams[v] : cur;
        psp_s32(0x08A7E25Cu, cam); psp_s32(mgr + 0x1160u, v ? cam : sel);
        if (patch_split_visibility.val && !shared) { cl_call(c, 0x089AC060u, v ? cam : sel, 1, 0, 0, 0, 0); cl_call(c, 0x089AF33Cu, 0, 0, 0, 0, 0, 0); cl_call(c, 0x089AF48Cu, 0, 0, 0, 0, 0, 0); }
        uint32_t a = lmp_pk();
        uint64_t t0 = lmp_now_us();
        if (v) { Ctx c2 = c; orig(c2); } else orig(c);
        us[v] += lmp_now_us() - t0;
        psp_ge_split_range(slot, v + 1, a, lmp_pk());
    }
    if (!(draw & 1)) {
        psp_s32(0x08A7E25Cu, cur); psp_s32(mgr + 0x1160u, sel);
        if (patch_split_visibility.val && !shared) { cl_call(c, 0x089AC060u, sel, 1, 0, 0, 0, 0); cl_call(c, 0x089AF33Cu, 0, 0, 0, 0, 0, 0); cl_call(c, 0x089AF48Cu, 0, 0, 0, 0, 0, 0); }
    }
    uint32_t used = psp_l32(0x08AF3218u);
    if (used > lmp_pk_peak) lmp_pk_peak = used;
    if (++frames % 300 == 0) {
        if (lines++ < 20) {
            printf("split: invitado %d vistas, ms por cuadro:", views);
            for (int v = 0; v < views; v++) printf(" v%d %.2f", v + 1, us[v] / 300000.0);
            printf(" | paquetes max %u KB de %u KB | listas max %u de 128\n", lmp_pk_peak / 1024, psp_l32(0x08AF320Cu) / 1024, lmp_lists_peak);
        }
        for (int v = 0; v < 4; v++) us[v] = 0;
        lmp_pk_peak = 0; lmp_lists_peak = 0;
    }
}

// beam clash: the game shows the button to press only for the local player, on his side; a human of the other
// team gets the same button drawn again on the mirrored side, with his own device's icons
void psp_ge_icon_range(uint32_t lo, uint32_t hi, int k);
static void lmp_clash_draw(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t pg = c.r[4];
    orig(c);
    int n = lmp_n();
    if (n < 2) return;
    uint32_t h = psp_l32(pg + 0x9C), in = h ? psp_l32(h) : 0, sl = h ? psp_l32(h + 8) : 0;
    uint32_t t = psp_l32(0x08A7DBC8u), base = t ? psp_l32(t + 4) : 0;
    int nf = t ? (int)psp_l32(t) : 0;
    if (!in || !sl || !psp_l8(h + 0x15) || !base) return;
    for (int e = 0; e < 2; e++) {
        uint32_t en = psp_l32(pg + 0xC8 + 4 * e);
        int fi = en ? (int)psp_l32(en + 0x34) : -1;
        if (fi < 0 || fi >= nf || fi > 7) continue;
        uint32_t f = base + 0x1A90u * fi;
        int k = lmp_player(psp_l32(f + 4), psp_l32(f + 8));
        if (k < 1 || k >= n || psp_l32(f + 4) == 0) continue;
        int x = (int)psp_l32(sl + 0xE4), w = (int)psp_l32(sl + 0xEC);
        uint32_t o8 = psp_l32(in + 8), lo = lmp_gu_ptr();
        psp_s32(in + 8, o8 + (uint32_t)(480 - 2 * x - w));
        cl_call(c, 0x089D1B94u, in, 0, 0, 0, 0, 0);
        psp_s32(in + 8, o8);
        psp_ge_icon_range(lo, lmp_gu_ptr(), k);
        static int logs = 0;
    }
}

// sound volume and pan by distance (0x0893CFD8, from the current camera): with several players each one's camera is
// tried and the nearest listener wins, so a player far from player 1 is not heard as distant; scene cameras are shared
static bool lmp_sfx_vol(Ctx& c, void (*orig)(Ctx&)) {
    int n = lmp_n();
    uint32_t mgr = psp_l32(0x08A7E260u), cur = psp_l32(0x08A7E25Cu), a1 = c.r[5], a2 = c.r[6];
    if (n < 2 || !mgr || !cur || !a1 || !a2) return false;
    uint32_t cam1 = lmp_cam_of(c, mgr, LMP_SLOT[0]);
    if (cur != cam1) return false;
    int bv = -1; uint32_t bp = 0; Ctx best = c;
    for (int k = 0; k < n && k < 4; k++) {
        if (k && !lmp_slot_active(LMP_SLOT[k])) continue;
        psp_s32(0x08A7E25Cu, k ? lmp_cam_of(c, mgr, LMP_SLOT[k]) : cam1);
        Ctx c2 = c; orig(c2);
        int v = (int)psp_l32(a1);
        if (v > bv) { bv = v; bp = psp_l32(a2); best = c2; }
    }
    psp_s32(0x08A7E25Cu, cur);
    psp_s32(a1, (uint32_t)bv); psp_s32(a2, bp);
    c = best;
    return true;
}

// pause from any player: the battle Start checks 0x08904F30/0x08905008 walk [0x08B40A60] pads (1 outside WLAN) and
// 0x08904F30 keeps who pressed at +4; while that player's pause runs, pad 0 (the menus' pad) gets his word and the
// local player (0x0885862C) is his fighter. The pause ends when the fighters animate again
static int lmp_pause_k = -1;
int psp_local_pause_view() { return lmp_pause_k >= 0 && lmp_pause_k < 4 ? lmp_vview[lmp_pause_k] : -1; }
static bool lmp_pause_stop = false;
static uint32_t lmp_pause_vb = 0;
static int lmp_pause_on = -1;
static bool lmp_pause_enabled() {
    if (lmp_pause_on < 0) { const char* e = getenv("TTT_PAUSE_ANY"); lmp_pause_on = !(e && *e == '0'); }
    return lmp_pause_on;
}
static void lmp_pause_end(const char* why) {
    static int logs = 0;
    if (logs++ < 16) printf("local: pausa del jugador %d terminada (%s)\n", lmp_pause_k + 1, why);
    lmp_pause_k = -1;
}
static void lmp_pause_tick() {
    if (lmp_pause_k < 0) return;
    uint32_t vb = psp_l32(0x08A78E14u);
    if (vb - walker_vb > 8u) lmp_pause_end("fin del combate");
    else if (!lmp_pause_stop) {
        if (vb - anim_vb >= 6u) lmp_pause_stop = true;
        else if (vb - lmp_pause_vb > 90u) lmp_pause_end("no se abrio");
    } else if (vb - anim_vb <= 2u) lmp_pause_end("reanudada");
}

static bool lmp_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (dir == 0x089F55E4u && hud_union && c.r[4] == hud_union) {   // opponent's union counter: its state binds the bar's fighter
        uint32_t sl = hud_union_slot();
        if (sl > 7u) return false;
        int pl = lmp_local;
        lmp_local = (int)sl;
        orig(c);
        lmp_local = pl;
        return true;
    }
    if (dir == 0x08A16E60u && lmp_pause_k > 0) {   // pause title: holder +0xA4 (AGS 162, states 0..3 = 1P..4P), the game picks state 0
        uint32_t h = psp_l32(c.r[4] + 0xA4), l = h ? psp_l32(h + 4) : 0;
        int n = h ? (int)psp_l32(h + 0xC) : 0, j = -1;
        for (int i = 0; i < n && i < 8 && l; i++) if ((int)psp_l32(l + 8 * i) == lmp_pause_k) j = i;
        static int logs = 0;
        if (logs < 8) { logs++; printf("local: pausa, titulo del jugador %d: %d estados, %s\n", lmp_pause_k + 1, n, j >= 0 ? "cambiado" : "sin estado propio"); }
        if (j < 0) return false;
        uint32_t z = 0xFFFFFFFFu;
        for (int i = 0; i < n && i < 8; i++) if (psp_l32(l + 8 * i) == 0) z = (uint32_t)i;
        psp_s32(l + 8 * j, 0);
        if (z != 0xFFFFFFFFu) psp_s32(l + 8 * z, (uint32_t)lmp_pause_k);
        orig(c);
        psp_s32(l + 8 * j, (uint32_t)lmp_pause_k);
        if (z != 0xFFFFFFFFu) psp_s32(l + 8 * z, 0);
        return true;
    }
    if (dir == 0x08904F30u || dir == 0x08905008u) {
        int n = lmp_n();
        if (n < 2 || !lmp_pause_enabled()) {
            if (dir != 0x08904F30u) return false;
            orig(c);
            bp_note(c.r[2] == 1);
            return true;
        }
        uint32_t cnt = psp_l32(0x08B40A60u);
        if (dir == 0x08904F30u) psp_s32(0x08B40A64u, 0);
        psp_s32(0x08B40A60u, (uint32_t)n);
        orig(c);
        psp_s32(0x08B40A60u, cnt);
        if (dir == 0x08904F30u && c.r[2] == 1) {
            int k = (int)psp_l32(0x08B40A64u);
            if (k < 0 || k >= n) k = 0;
            lmp_pause_k = k; lmp_pause_stop = false; lmp_pause_vb = psp_l32(0x08A78E14u);
            static int logs = 0;
            if (logs++ < 16) printf("local: pausa del jugador %d\n", k + 1);
        }
        if (dir == 0x08904F30u) bp_note(c.r[2] == 1);
        return true;
    }
    if (dir == 0x089DFBD0u) { lmp_clash_draw(c, orig); return true; }
    if (dir == 0x0893CFD8u) return lmp_sfx_vol(c, orig);
    if (dir == 0x088404D8u && (lmp_hrec || lmp_hbuild)) return lmp_res_wrap(c, orig);
    if (dir == 0x08840640u) {                 // a copy's destructor would free what the original still uses
        if (!lmp_hdel) return false;
        for (int i = 0; i < lmp_res_n; i++) if (lmp_res[i].v == c.r[5]) { c.r[2] = 0; return true; }
        return false;
    }
    if ((dir >= 0x089CFEA4u && dir <= 0x089D5164u) && lmp_hud_wrap(dir, c, orig)) return true;
    if (dir == 0x0882F68Cu) {
        if (lmp_pk_orig && psp_l32(0x08AF3200u) == lmp_pk_new[0]) psp_s32(0x08AF3200u, lmp_pk_orig);
        orig(c);
        return true;
    }
    if (dir == 0x0885862Cu) {
        int lo = lmp_local >= 0 ? lmp_local : lmp_pause_k > 0 ? LMP_SLOT[lmp_pause_k] : -1;
        if (lo < 0) return false;
        orig(c);
        c.r[2] = (uint32_t)lo;
        return true;
    }
    if (dir != 0x0881A1A0u && dir != 0x08858830u && dir != 0x08931BE4u && dir != 0x0885AD48u) return false;
    int n = lmp_n();
    if (n < 2) return false;
    if (dir == 0x0885AD48u) {
        if (!patch_split_screen.val) return false;
        lmp_draw(c, orig);
        return true;
    }
    if (dir == 0x08858830u) {                 // fighter creation: players' fighters human, every other fighter CPU
        lmp_map_update(n);
        int k = lmp_player(c.r[4], c.r[5]);
        if (k == 0 || k < 0) return false;
        c.r[2] = k < n ? 0 : 1;
        static int logs = 0;
        if (logs++ < 8) printf("local: luchador equipo %u miembro %u %s\n", c.r[4], c.r[5], k < n ? "humano" : "CPU");
        return true;
    }
    if (dir == 0x08931BE4u) {
        uint32_t f = c.r[4];
        int k = lmp_player(psp_l32(f + 4), psp_l32(f + 8));
        if (k >= 1 && k < n) { psp_s32(f + 0x18, (uint32_t)k); psp_s8(f + 0x1690, 0); }
        lmp_log_fighters();
        orig(c);
        return true;
    }
    lmp_map_update(n);
    lmp_pause_tick();
    int pk = lmp_pause_k > 0 && lmp_pause_k < n ? lmp_pause_k : 0;
    if (pk) { Ctx c1 = c; c1.r[31] = 0; psp_call(c1, 0x0881A140u); }
    else orig(c);
    uint32_t stamp = psp_l32(0x08AF31A0u);
    for (int k = 1; k < n; k++) {
        uint32_t b = 0; uint8_t x = 128, y = 128;
        psp_input_local(k, &b, &x, &y);
        for (int r = 0; r < 2; r++) {
            if (r == 1 && k != pk) break;
            uint32_t raw = 0x08AF31A0u + 16u * (r ? 0 : k);
            psp_s32(raw, stamp); psp_s32(raw + 4, b); psp_s8(raw + 8, x); psp_s8(raw + 9, y);
            for (int i = 10; i < 16; i++) psp_s8(raw + i, 0);
            Ctx c2 = c;
            c2.r[4] = r ? 0u : (uint32_t)k; c2.r[31] = 0;
            psp_call(c2, 0x08819FA0u);
        }
    }
    return true;
}
