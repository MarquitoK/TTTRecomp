// item 33, game room: each player's model in its colour instead of its big render (rows found by the GPU, markers 0xEE k / 0xE9)
void psp_room_track(bool on);
void psp_room_set(int k, int menu, bool live, const float* hd, const float* ws, const float* nk, float rw, const float* tweak);
int psp_room_quads(int* menus, int max);
struct RmModel { int obj = -1, sys = -1, col = 0, dmg = 0, menu = -1, seen = 0, calm = 0; uint32_t buf = 0; bool shown = false, nok = false; float hd[3] = {0, 0, 0}, ws[3] = {0, 0, 0}, nk[3] = {0, 0, 0}, rw = 1, last = 0; };
struct RmSlot { int want = -1, want_f = 0, load_f = -999; bool pend = false; RmModel cur, nxt; };   // cur on screen, nxt loading and settling hidden
static int RM_WAIT = 6, RM_GAP = 16;   // TTT_ROOM3D_RATE=wait,gap: frames a row's choice must hold before it loads; while Random runs, frames between two loads
static RmSlot rm[4];
static bool rm_on = false;
static bool rm_active() { return rm_on; }
static int rm_gone = 0, rm_last_load = -999;
static uint32_t rm_rec[4] = {0, 0, 0, 0}, rm_w = 0, rm_wvt = 0;

// colour of the nth player whose choice is this menu number (selection records of the free battle widget, +4 menu, +0xC colour)
static int rm_colour(const Ctx& c, int menu, int nth, int* row = nullptr) {
    uint32_t vt = rm_w && rm_w < 0x10000000u ? psp_l32(rm_w + 0x40) : 0;
    if (!vt || vt >= 0x10000000u) return 0;
    uint32_t off = (uint32_t)(int32_t)(int16_t)psp_l16(vt + 0xA0), fn = psp_l32(vt + 0xA4);
    if (fn < 0x08804000u || fn >= 0x0A000000u) return 0;
    for (int p = 0, k = 0; p < 4; p++) {
        uint32_t r = mpm_call(c, fn, rm_w + off, (uint32_t)p);
        if (!r || r >= 0x10000000u || (int)psp_l32(r + 4) != menu) continue;
        if (k++ == nth) { int col = (int)psp_l32(r + 0xC); if (row) *row = p; return col >= 0 && col < 20 ? col : 0; }
    }
    return 0;
}
static bool rm_lv1(uint32_t ent) {   // a room model whose player chose the level 1 costume
    for (auto& s : rm) for (const RmModel* m : {&s.cur, &s.nxt}) if (m->dmg == 1 && m->obj >= 0 && cv_objp(m->obj) == ent) return true;
    return false;
}
static std::vector<uint32_t> rm_late;   // buffers of models still being read when the room was closed: freed once the loader is idle
static void rm_free(const Ctx& c, RmModel& m, bool reading = false) {
    if (m.obj >= 0) mpm_call(c, 0x089AE1FCu, (uint32_t)m.obj);
    if (m.buf && reading) rm_late.push_back(m.buf);
    else if (m.buf) mpm_call(c, 0x089BFDB0u, m.buf);
    m = RmModel();
}
static void rm_close(const Ctx& c) {
    for (int k = 0; k < 4; k++) { rm_free(c, rm[k].cur); rm_free(c, rm[k].nxt, rm[k].pend); rm[k] = RmSlot(); psp_room_set(k, -1, false, nullptr, nullptr, nullptr, 1.0f, nullptr); }
    if (cv_owner() == 3) cv_term(c);
    rm_on = false; rm_gone = 0;
    printf("room3d: cerrada\n"); fflush(stdout);
}
static void rm_create(const Ctx& c, int k) {
    RmModel& m = rm[k].nxt;
    rm[k].pend = false;
    cv_flags();
    int id = (int)mpm_call(c, 0x089AE0A4u, 2, rm_rec[k], 1, 0xFFFFFFFFu);
    uint32_t o = cv_objp(id);
    if (!o) { printf("room3d: fila %d: no se pudo crear el objeto (id %d)\n", k, id); fflush(stdout); rm_free(c, m); return; }
    m.obj = id;
    uint32_t an = psp_l32(o + 0x738 + 4 * 7);
    if ((!an || !psp_l16(an + 2)) && s3d_idle) psp_s32(o + 0x738 + 4 * 7, s3d_idle);
    if (psp_l32(o + 0x738 + 4 * 7)) mpm_call(c, 0x089B14C8u, o, 7, 2);
    cv_wf(o + 0xCA0, cv_rf(o + 0xCA0) * 0.5f);
}
static void rm_load(const Ctx& c, int k, int m, int nth) {
    RmSlot& s = rm[k];
    rm_free(c, s.nxt);
    RmModel& x = s.nxt;
    x.menu = m; x.sys = s3d_sys_of_menu(m);
    s.load_f = s3d_frame; rm_last_load = s3d_frame;
    if (x.sys < 0) return;
    int nc = psp_col_count(x.sys);
    int row = -1;
    x.col = std::max(0, std::min(rm_colour(c, m, nth, &row), (nc > 0 ? nc : 1) - 1));
    uint32_t idx = mpm_call(c, 0x089C6CA0u, (uint32_t)x.sys, (uint32_t)x.col) & 0xFFFFu, d = 0;
    int lv = cs_level(row, idx);   // costume chosen in the select for this player and entry
    x.dmg = lv == 2 ? (psp_disc_dmg(idx, &d) ? 2 : 0) : lv == 1 && psp_disc_lv1(idx) ? 1 : 0;
    if (x.dmg == 2) idx = d;
    uint32_t sz = mpm_call(c, 0x08831CACu, idx);
    x.buf = mpm_call(c, 0x08852854u, idx, 0, 0);
    static int logs = 0;
    if (logs++ < 40 || !x.buf) { printf("room3d: fila %d: menu %d, personaje %d, color %d, traje %d, entrada %u%s\n", k, m, x.sys, x.col + 1, x.dmg, idx, x.buf ? "" : ", no se pudo leer"); fflush(stdout); }
    if (!x.buf) return;
    for (uint32_t o = 0; o < 0x40; o += 4) psp_s32(rm_rec[k] + o, 0);
    psp_s32(rm_rec[k], x.buf); psp_s32(rm_rec[k] + 4, sz); psp_s32(rm_rec[k] + 8, idx);
    s.pend = true;
}

static bool m3d_blocks_room();
static bool rm_pending() { for (auto& s : rm) if (s.pend) return true; return false; }
// once per frame from the pad read, after the select's tick
static void rm_tick(const Ctx& c) {
    bool en = s3d_enabled() && !gen_run && gen_env("TTT_ROOM_3D", 1);
    psp_room_track(en);
    if (s3d_alive() && s3d_frame - s3d_seen <= 3) { uint32_t w = psp_l32(s3d_obj + 0xC); if (w && w < 0x10000000u) { rm_w = w; rm_wvt = psp_l32(w + 0x40); } }
    bool fb = rm_w && psp_l32(rm_w + 0x40) == rm_wvt && !cr_w;   // the free battle widget alive, not the Character Reference (its render is a big render too)
    int menus[4], n = en ? psp_room_quads(menus, 4) : 0;
    bool sel = s3d_alive() && s3d_frame - s3d_seen <= 3, room = n > 0 && !sel && fb;
    bool pend = false;
    for (auto& s : rm) pend |= s.pend;
    if (!rm_late.empty() && mpm_call(c, 0x08852768u) == 1) { for (uint32_t b : rm_late) mpm_call(c, 0x089BFDB0u, b); rm_late.clear(); }
    if (rm_on) {
        if (pend && mpm_call(c, 0x08852768u) == 1) { for (int k = 0; k < 4; k++) if (rm[k].pend) { rm_create(c, k); break; } pend = false; for (auto& s : rm) pend |= s.pend; }   // one model per frame
        if (!room) { if ((++rm_gone > 90 || sel || !fb) && !pend) rm_close(c); return; }
        rm_gone = 0;
    } else {
        if (!room || cv_owner() != 0 || m3d_blocks_room()) return;
        if (!rm_rec[0]) for (auto& r : rm_rec) r = psp_reserve(0x40);
        static bool cfg = false;
        if (!cfg) { cfg = true; if (const char* e = getenv("TTT_ROOM3D_RATE")) sscanf(e, "%d,%d", &RM_WAIT, &RM_GAP); }
        cv_init(c, -1, 3);
        rm_on = true; rm_gone = 0; rm_last_load = -999;
        printf("room3d: abierta, %d filas\n", n); fflush(stdout);
    }
    if (pend) return;
    // a row whose choice holds loads now; while Random runs, the row that has waited longest loads once every RM_GAP frames
    int best = -1, nth_best = 0;
    for (int k = 0; k < 4; k++) {
        int m = menus[k];
        if (m < 0) continue;
        RmSlot& s = rm[k];
        if (m != s.want) { s.want = m; s.want_f = s3d_frame; }
        int have = s.nxt.menu >= 0 ? s.nxt.menu : s.cur.menu;
        if (m == have) continue;
        int nth = 0;
        for (int j = 0; j < k; j++) nth += menus[j] == m;
        if (s3d_frame - s.want_f >= RM_WAIT) { rm_load(c, k, m, nth); return; }
        if (best < 0 || s.load_f < rm[best].load_f) { best = k; nth_best = nth; }
    }
    if (best >= 0 && s3d_frame - rm_last_load >= RM_GAP) rm_load(c, best, menus[best], nth_best);
}

// the idle pose, once it has settled (the first frames still hold the bind pose), places the model once: it does not follow the animation
static bool rm_settle(int k, RmModel& m, uint32_t o) {
    uint32_t tb = s3d_mtx(o);
    float hd[3] = {0, 0, 0}, ws[3] = {0, 0, 0}, nk[3] = {0, 0, 0};
    bool ok = tb && tb < 0x10000000u && s3d_bone_pos(o, tb, 0x2F, hd) && s3d_bone_pos(o, tb, 0x03, ws) && s3d_valid(hd, ws, cv_height(o));
    m.seen = ok ? m.seen + 1 : 0;
    float span = ws[1] - hd[1];
    m.calm = ok && m.seen > 1 && std::fabs(hd[1] - m.last) < 0.015f * std::max(1.0f, span) ? m.calm + 1 : 0;
    m.last = hd[1];
    if (!ok || !((m.seen >= 10 && m.calm >= 4) || m.seen > 40)) return false;
    m.shown = true;
    m.nok = s3d_bone_pos(o, tb, 0x2E, nk) && nk[1] - hd[1] > 0.02f && nk[1] - hd[1] < 6.0f;
    m.rw = span > 0.5f && span < 40.0f ? span / 5.5f : 1.0f;
    memcpy(m.hd, hd, sizeof hd); memcpy(m.ws, ws, sizeof ws); memcpy(m.nk, nk, sizeof nk);
    static int logs = 0;
    if (logs++ < 40) { printf("room3d: fila %d visible (personaje %d), distancia cabeza-cintura %.2f%s\n", k, m.sys, span, m.nok ? "" : ", sin cuello"); fflush(stdout); }
    return true;
}

// frame end: one camera, the modules' update once, then each model drawn alone into its row
static void rm_draw(Ctx& c) {
    if (!rm_on || cv_owner() != 3) return;
    uint32_t mgr = psp_l32(0x08A7E260u), oc[4] = {0, 0, 0, 0}, on[4] = {0, 0, 0, 0};
    float h = 19.0f;
    int n = 0;
    for (int k = 0; k < 4; k++) {
        oc[k] = rm[k].cur.obj >= 0 ? cv_objp(rm[k].cur.obj) : 0; on[k] = rm[k].nxt.obj >= 0 ? cv_objp(rm[k].nxt.obj) : 0;
        for (uint32_t o : {oc[k], on[k]}) if (o) { n++; h = std::max(h, cv_height(o)); }
    }
    if (!n || !mgr) return;
    for (uint32_t k = 0; k < 0x290; k += 4) psp_s32(cv.cam + k, psp_l32(mgr + 0x720u + k));
    float dist = h * 2.5f, ctr[3] = {0.0f, -h * 0.5f, 0.0f}, pitch = 0.04f;
    float eye[3] = {dist * cosf(pitch) * sinf(s3d_yaw), ctr[1] - dist * sinf(pitch), dist * cosf(pitch) * cosf(s3d_yaw)};
    cv_wf(cv.cam + 0x260, eye[0]); cv_wf(cv.cam + 0x264, eye[1]); cv_wf(cv.cam + 0x268, eye[2]); cv_wf(cv.cam + 0x26C, 1.0f);
    cv_flags();
    psp_s32(CV_W + 0x5BD0, 0x1F); psp_s32(CV_W + 0x5BD4, 0);
    cv_aim(c, eye, ctr);
    psp_s32(mgr + 0x1160u, cv.cam);
    cl_call(c, 0x089AF124u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089AF33Cu, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089AF48Cu, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0887B9ACu, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BDE94u, 0, 0, 0, 0, 0, 0);
    uint32_t L = mpm_call(c, 0x089AD8A8u, 0), cnt = L ? psp_l32(L + 0xC0) : 0, keep[48];
    bool lok = L && L < 0x10000000u && cnt <= 48;
    for (uint32_t i = 0; lok && i < cnt; i++) keep[i] = psp_l32(L + 4 * i);
    auto draw = [&](uint32_t o, uint32_t mark) {
        if (lok) { psp_s32(L, o); psp_s32(L + 0xC0, 1); }
        s3d_mark_list(c, mark, 0);
        cl_call(c, 0x0880BD3Cu, 0, 0, 0, 0, 0, 0);
        s3d_mark_list(c, 0x00E90000u, 0);
    };
    for (int k = 0; k < 4; k++) {
        RmSlot& s = rm[k];
        if (on[k] && rm_settle(k, s.nxt, on[k])) {   // the new model takes the row
            rm_free(c, s.cur);
            s.cur = s.nxt; s.nxt = RmModel();
            oc[k] = on[k]; on[k] = 0;
        }
        float tw[6];
        psp_render_tweak(s.cur.sys, tw);
        psp_room_set(k, s.cur.menu, s.cur.shown, s.cur.hd, s.cur.ws, s.cur.nok ? s.cur.nk : nullptr, s.cur.rw, tw);
        if (oc[k]) draw(oc[k], 0x00EE0000u | (uint32_t)k);
        if (on[k]) draw(on[k], 0x00EE0010u | (uint32_t)k);   // hidden: drawn only for its bone matrices
    }
    if (lok) { for (uint32_t i = 0; i < cnt; i++) psp_s32(L + 4 * i, keep[i]); psp_s32(L + 0xC0, cnt); }
    cl_call(c, 0x089BDFB4u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0880BCF0u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0887BAA4u, 1, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BDFE4u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BE034u, 0, 0, 0, 0, 0, 0);
}
