// map select 3D (TTT_MAP_3D): the big render of the map under the cursor becomes that map, live, through the viewer engine
// (owner 4, no objects), seen through the map's 3 intro cameras (pak entries 03..05) in a loop on camera slot 0, as the
// battle's camera intro phase (0x089087A8: 0x089AADC4(i) -> 0x089BA450(i) -> 0x089AA460(cma, 0))
static uint32_t m3d_obj = 0;                  // map select object (constructor 0x08A03C0C .. destructor 0x08A03C9C)
static int m3d_rn = -1, m3d_seen = -1000, m3d_frame = 0, m3d_want_f = 0, m3d_cma = 0;
static bool m3d_started = false, m3d_ready = false, m3d_rect_ok = false;
static int m3d_x0 = 180, m3d_y0 = 40, m3d_x1 = 480, m3d_y1 = 192;   // panel (480x272); taken from the big render's slot when it is valid
static uint64_t m3d_t_req = 0;
static uint32_t m3d_nbuf = 0, m3d_nsz = 0;   // next map read while the current one stays on screen
static int m3d_npend = -1, m3d_swaps = 0;
static int m3d_cframes = 0, m3d_cur = -1, m3d_cur_f = 0, m3d_st = -1, m3d_st_f = 0;
bool psp_map_big_rect(int* r);
static bool m3d_live();
static bool m3d_enabled();
// map renders from the 3D (TTT_GEN_MAP_RENDERS): each map's 3 entry cameras at their first frame, captured on the page
void psp_cap_request_map(uint32_t tag);
bool psp_map_render_have(const std::string& k, int q);
void psp_map_render_begin(const std::string& k, int q);
void psp_map_render_save(const std::string& k, int i, int q, std::vector<uint32_t>&& px, int w, int h);
int psp_render_quality();
std::string psp_map_render_key(int i);
bool psp_map_render_fix(const std::string& k, int q);
void psp_map_chooser_request();
static bool mg_run = false;
static std::vector<int> mg_jobs;
static size_t mg_i = 0;
static int mg_k = 0, mg_state = 0, mg_q = 2, mg_f = 0, mg_jf = 0, mg_tries = 0, mg_go_seen = 0, mg_cfg_last = -1, mg_caps = 0;
static uint32_t mg_tag = 0x40000000u, mg_page = 0;
static uint64_t mg_t0 = 0, mg_tj = 0;
static const int MG_SETTLE = 6;
static bool m3d_blocks_room() { return (m3d_enabled() || mg_run) && m3d_live(); }
static int m3d_map = -1, m3d_want = -1;
static uint32_t m3d_entry(int i);
static std::vector<uint8_t> m3d_rnd;   // pristine copy: the stage relocates the buffer it lives in
static uint32_t m3d_rbuf = 0;
static bool m3d_rnd_have() {
    static int st = -1;
    if (st < 0) {
        st = 0;
        FILE* fp = fopen("../../games/ttt/map3d/random_map.pak", "rb");
        if (fp) {
            fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
            if (n > 16 && n <= 0x400000) { m3d_rnd.resize((size_t)n); if (fread(m3d_rnd.data(), 1, (size_t)n, fp) == (size_t)n) st = 1; }
            fclose(fp);
        }
        printf("map3d: mapa de Random %s (%u bytes)\n", st ? "cargado de games/ttt/map3d/random_map.pak" : "no encontrado en games/ttt/map3d/random_map.pak", (unsigned)m3d_rnd.size()); fflush(stdout);
        if (st) m3d_rbuf = psp_reserve((uint32_t)m3d_rnd.size() + 64);
        if (!m3d_rbuf) st = 0;
    }
    return st > 0;
}
static bool m3d_show() { return m3d_map >= 0 && m3d_want >= 0 && (m3d_want == m3d_map || m3d_entry(m3d_want) != 0); }   // the old map stays until the next one is ready

static bool m3d_enabled() {
    static int v = 0, n = 0;
    if ((n++ & 63) == 0) { const char* e = getenv("TTT_MAP_3D"); v = e && *e ? atoi(e) : 0; }
    return v > 0;
}
static uint32_t m3d_entry(int i) {
    int n = st_get().n;
    if (i == n) return m3d_rnd_have() ? 0xFFFFFFFFu : 0;   // Random: the app's own map (games/ttt/map3d/random_map.pak)
    if (i < 0 || i > n) return 0;
    if (i < stages::OLD) return 0x495u + 2u * (uint32_t)i;
    int b = psp_stage_base();
    return b >= 0 ? (uint32_t)b + 2u * (uint32_t)(i - stages::OLD) : 0;
}
static uint32_t m3d_big(int cell) { return m3d_obj ? psp_l32(m3d_obj + ST_OBJ + 4 * (ST_CELLS + (uint32_t)cell)) : 0; }
static bool m3d_live() { return m3d_obj && m3d_frame - m3d_seen <= 3; }

static void m3d_ctor(uint32_t o) {
    m3d_obj = o; m3d_rn = -1; m3d_seen = -1000; m3d_rect_ok = false; m3d_cur = m3d_st = -1;
    printf("map3d: pagina de mapas 0x%08X\n", o); fflush(stdout);
    if (!m3d_enabled()) return;
    int n = st_get().n;
    for (int i = 0; i < n; i++) if (uint32_t e = m3d_entry(i)) psp_disc_hold(e, false);
    printf("map3d: %d mapas en cola para memoria\n", n); fflush(stdout);
}
static void m3d_dtor(uint32_t o) { if (o == m3d_obj) { m3d_obj = 0; printf("map3d: pagina de mapas destruida\n"); fflush(stdout); } }

volatile int m3d_q_on = 0;
static int m3d_mark_f = -1;
// one GE NOP marker into the list the page has open (the libgu write pointer), where the render would be drawn
static void m3d_mark_inline(uint32_t word) {
    uint32_t ctx = psp_l32(0x08B478D0u), p = ctx && ctx < 0x10000000u ? psp_l32(ctx + 8) : 0;
    if (p && p < 0x10000000u) { psp_s32(p, word); psp_s32(ctx + 8, p + 4); }
}
// AGS instance draw: a map's big render. While the 3D is shown: the first render of the frame leaves the marker that pastes the
// 3D there (gpu.cpp 0xF3) and none is drawn, except Random's own, drawn by the game at rest (its texture is the "?", renders.h)
static bool m3d_rnd_in = false;   // Random's render being drawn by the port: the nested instance draw goes to the game
static bool m3d_inst(Ctx& c) {
    if (!m3d_obj || m3d_rnd_in) return false;
    uint32_t inst = c.r[4];
    int n = st_get().n;
    for (int k = 0; k <= n; k++) {
        uint32_t e = m3d_big(k);
        if (!e || (e != inst && psp_l32(e) != inst)) continue;
        static int logs = 0;
        if (m3d_rn != k && logs++ < 8) { printf("map3d: render grande %d dibujado (%s)\n", k, e == inst ? "elemento" : "contenedor"); fflush(stdout); }
        m3d_rn = k; m3d_seen = m3d_frame;
        if (!m3d_rect_ok) {
            int r[4];
            if (psp_map_big_rect(r) && r[2] - r[0] >= 64 && r[3] - r[1] >= 32 && r[0] >= -2 && r[1] >= -2 && r[2] <= 482 && r[3] <= 274) {
                m3d_x0 = std::max(0, r[0]) & ~1; m3d_y0 = std::max(0, r[1]) & ~1; m3d_x1 = (std::min(480, r[2]) + 1) & ~1; m3d_y1 = (std::min(272, r[3]) + 1) & ~1;
            }
            m3d_rect_ok = true;
        }
        if (mg_run) return false;
        if (!(cv_owner() == 4 && m3d_ready && m3d_started && m3d_show())) return false;
        if (m3d_mark_f != m3d_frame) { m3d_mark_f = m3d_frame; m3d_mark_inline(0x00F30000u); }
        if (k != n || m3d_map != n) return true;
        if (e == inst) return true;   // Random's element alone: only the whole holder is drawn at rest
        int last = s3d_last_frame(inst);   // Random: its render ("?") at rest, no entrance
        if (last >= 0) cv_wf(inst + 0x2C, (float)last);
        uint32_t sl = psp_l32(e + 8);
        if (sl && sl < 0x10000000u) {
            uint32_t kx = psp_l32(sl + 0xE4), ky = psp_l32(sl + 0xE8);
            psp_s32(sl + 0xE4, 0); psp_s32(sl + 0xE8, 0);
            m3d_rnd_in = true;
            Ctx c2 = c; c2.r[31] = 0; psp_call(c2, 0x089D1B94u);
            m3d_rnd_in = false;
            psp_s32(sl + 0xE4, kx); psp_s32(sl + 0xE8, ky);
            return true;
        }
        return false;
    }
    return false;
}

static void m3d_close(const Ctx& c, const char* why) {
    if (cv_owner() != 4) return;
    if (m3d_npend >= 0) {
        if (mpm_call(c, 0x08852768u) != 1) return;
        if (m3d_nbuf) mpm_call(c, 0x089BFDB0u, m3d_nbuf);
        m3d_nbuf = 0; m3d_npend = -1;
    }
    if (cv.mapbuf == m3d_rbuf) cv.mapbuf = 0;   // reserved for good, never freed
    cv_term(c);
    m3d_map = -1; m3d_ready = m3d_started = false;
    printf("map3d: cerrado (%s)\n", why); fflush(stdout);
}

// the stage moves to buf (scene and stage effects rebuilt with it); the old buffer is freed unless it is Random's
static void m3d_bind(const Ctx& c, uint32_t buf, uint32_t sz, int map) {
    m3d_swaps++;
    mpm_call(c, 0x0887B770u);
    if (psp_l32(0x08A7E678u)) mpm_call(c, 0x089B733Cu);
    mpm_call(c, 0x089BDE00u);
    psp_s32(CV_R + 0xC, 0); psp_s32(CV_R + 0x3C, 0);
    if (cv.mapbuf && cv.mapbuf != m3d_rbuf) mpm_call(c, 0x089BFDB0u, cv.mapbuf);
    cv.mapbuf = buf; cv.mapsz = sz;
    mpm_call(c, 0x0887B340u, 0, 0, 0);
    mpm_call(c, 0x089BDD88u);
    psp_s32(CV_R + 0xC, cv.mapbuf); psp_s32(CV_R + 0x3C, cv.mapsz);
    mpm_call(c, 0x08804DA0u);
    m3d_map = map; m3d_cma = 0; m3d_started = false;
    m3d_ready = psp_l32(0x08A7E678u) != 0;
    if (m3d_swaps <= 40 || m3d_swaps % 50 == 0) { printf("map3d: cambio %d al mapa %d en %.0f ms, escenario %s\n", m3d_swaps, m3d_map, (psp_time_us() - m3d_t_req) / 1000.0, m3d_ready ? "si" : "no"); fflush(stdout); }
    if (!m3d_ready) m3d_map = -2;
}

void psp_window_busy(int kind, int done, int total);
static int mg_env(const char* k, int d) { const char* e = getenv(k); return e && *e ? atoi(e) : d; }
static std::string mg_key(int i) { return psp_map_render_key(i); }
static void mg_end(const char* why) {
    printf("render: generacion de mapas %s, %d mapas, %d capturas en %.1f s\n", why, (int)mg_i, mg_caps, (psp_time_us() - mg_t0) / 1e6); fflush(stdout);
    mg_run = false; mg_jobs.clear(); mg_i = 0; mg_k = 0; mg_state = 0; m3d_started = false;
    psp_window_busy(0, 0, 0);
}
static void mg_job_start() {
    mg_k = 0; mg_state = 0; mg_jf = m3d_frame; mg_tj = psp_time_us();
    if (mg_i < mg_jobs.size()) { psp_map_render_begin(mg_key(mg_jobs[mg_i]), mg_q); if (uint32_t e = m3d_entry(mg_jobs[mg_i])) psp_disc_hold(e, true); }
}
static void mg_next_cap() {
    mg_state = 0;
    if (++mg_k < 3) return;
    printf("render: mapa %d (%s) capturado en %llu ms (%d de %d)\n", mg_jobs[mg_i], mg_key(mg_jobs[mg_i]).c_str(), (unsigned long long)((psp_time_us() - mg_tj) / 1000), (int)mg_i + 1, (int)mg_jobs.size()); fflush(stdout);
    mg_i++;
    if (mg_i >= mg_jobs.size()) { mg_end("terminada"); psp_map_chooser_request(); return; }
    mg_job_start();
}
// jobs when the page opens, on Regenerate or when the switch or the quality change: every map with a 3D and no render of this quality
static void mg_check(const Ctx& c) {
    if (mg_run || (m3d_frame % 30 && mg_page == m3d_obj)) return;
    int on = mg_env("TTT_GEN_MAP_RENDERS", 0), go = mg_env("TTT_GEN_MAP_RENDERS_GO", 0), q = psp_render_quality(), cfg = on | q << 1;
    bool force = go != mg_go_seen, page = mg_page != m3d_obj, changed = cfg != mg_cfg_last;
    mg_go_seen = go; mg_page = m3d_obj; mg_cfg_last = cfg;
    if (!on || (!force && !page && !changed)) return;
    mg_q = q; mg_jobs.clear(); mg_i = 0;
    int n = st_get().n, all = 0;
    for (int i = 0; i < n; i++) {
        std::string k = mg_key(i);
        uint32_t e = m3d_entry(i);
        if (k.empty() || !e) continue;
        uint32_t sz = mpm_call(c, 0x08831CACu, e);
        if (!sz || sz > psp_map_buf_size()) { printf("render: mapa %d mide %u bytes, sin 3D: se queda su imagen\n", i, sz); continue; }
        all++;
        if (force || !psp_map_render_have(k, q)) mg_jobs.push_back(i);
        else psp_map_render_fix(k, q);
    }
    printf("render: %d de %d mapas por generar (calidad x%d%s)\n", (int)mg_jobs.size(), all, q, force ? ", regenerar" : ""); fflush(stdout);
    if (mg_jobs.empty()) return;
    for (int i : mg_jobs) psp_disc_hold(m3d_entry(i), false);
    mg_run = true; mg_caps = 0; mg_t0 = psp_time_us();
    mg_job_start();
}
static void mg_tick(const Ctx& c) {
    psp_window_busy(2, (int)mg_i, (int)mg_jobs.size());
    if (mg_state != 2 && m3d_frame - mg_jf > 900) {
        printf("render: mapa %d sin escenario listo en 900 cuadros, se salta\n", mg_jobs[mg_i]); fflush(stdout);
        mg_k = 2; mg_next_cap(); return;
    }
    if (mg_state != 2) return;
    std::vector<uint32_t> px; int w = 0, h = 0; float pts[9];
    if (psp_cap_take(mg_tag, px, &w, &h, pts)) {
        if (!w) { mg_end("cancelada: la captura necesita Graphics > System > GPU rendering"); return; }
        psp_map_render_save(mg_key(mg_jobs[mg_i]), mg_k, mg_q, std::move(px), w, h);
        mg_caps++;
        mg_next_cap();
    } else if (++mg_tries > 30) { mg_state = 1; mg_f = m3d_frame; mg_tries = 0; printf("render: captura de mapa sin respuesta, se repite (mapa %d camara %d)\n", mg_jobs[mg_i], mg_k); fflush(stdout); }
}

static void m3d_tick(const Ctx& c) {
    m3d_frame++;
    m3d_q_on = !mg_run && cv_owner() == 4 && m3d_ready && m3d_started && m3d_map == st_get().n && m3d_show() ? 1 : 0;
    if (m3d_live()) mg_check(c);
    if (mg_run && !m3d_live()) mg_end("interrumpida (pagina cerrada)");
    if (!(m3d_enabled() || mg_run) || !m3d_live()) { if (cv_owner() == 4 && !cv_busy()) m3d_close(c, m3d_enabled() ? "pagina oculta" : "opcion apagada"); return; }
    if (cv_owner() == 3 && !cv_busy() && !rm_pending()) { printf("map3d: la sala de batalla libre cede el visor\n"); fflush(stdout); rm_close(c); }
    if (cv_owner() != 0 && cv_owner() != 4) {
        static int logs = 0;
        if (logs++ < 3) { printf("map3d: el visor esta ocupado por el dueno %d\n", cv_owner()); fflush(stdout); }
        return;
    }
    if (mg_run) mg_tick(c);
    if (!mg_run && !m3d_enabled()) return;
    // the cell under the cursor: the object's selected index (+0x11C) moves at once, the render's cell (st_cur) after its slide
    int n = st_get().n, oc = (int)psp_l32(m3d_obj + 0x11C);
    if (oc >= 0 && oc <= n && oc != m3d_cur) { m3d_cur = oc; m3d_cur_f = m3d_frame; }
    if (st_cur != m3d_st) { m3d_st = st_cur; m3d_st_f = m3d_frame; }
    int want = m3d_cur >= 0 && (m3d_st < 0 || m3d_cur_f >= m3d_st_f) ? m3d_cur : m3d_st >= 0 && m3d_st <= n ? m3d_st : m3d_rn;
    if (mg_run && mg_i < mg_jobs.size()) want = mg_jobs[mg_i];
    if (want != m3d_want) {
        static int logs = 0;
        if (logs++ < 80) { printf("map3d: cursor %d (objeto %d a los %d cuadros, render %d a los %d cuadros)\n", want, m3d_cur, m3d_frame - m3d_cur_f, m3d_st, m3d_frame - m3d_st_f); fflush(stdout); }
    }
    if (want != m3d_want) {
        m3d_want = want; m3d_want_f = m3d_frame;
        for (int d : {1, -1, 2, -2}) if (uint32_t e = m3d_entry(want + d)) psp_disc_hold(e, true);
        if (uint32_t e = m3d_entry(want)) psp_disc_hold(e, true);
    }
    if (cv_owner() == 4 && cv.map_wait) return;
    if (cv_owner() == 4 && !m3d_ready && m3d_map >= 0 && m3d_npend < 0) {
        m3d_ready = psp_l32(0x08A7E678u) != 0;
        printf("map3d: mapa %d listo en %.0f ms, escenario %s\n", m3d_map, (psp_time_us() - m3d_t_req) / 1000.0, m3d_ready ? "si" : "no"); fflush(stdout);
        if (!m3d_ready) m3d_map = -2;
    }
    if (m3d_npend >= 0) {   // the next map is read: the stage moves to it
        if (mpm_call(c, 0x08852768u) != 1) return;
        m3d_bind(c, m3d_nbuf, m3d_nsz, m3d_npend);
        m3d_nbuf = 0; m3d_npend = -1;
        return;
    }
    if (want == m3d_map || m3d_frame - m3d_want_f < 3) return;   // the cursor rests 3 frames before a map loads
    uint32_t e = m3d_entry(want);   // Random: the app's own map when it ships one, else its render
    if (!e) return;
    m3d_t_req = psp_time_us();
    if (e == 0xFFFFFFFFu) {
        if (cv_owner() == 0) cv_init(c, -1, 4);
        memcpy(psp_p(m3d_rbuf), m3d_rnd.data(), m3d_rnd.size());
        m3d_bind(c, m3d_rbuf, (uint32_t)m3d_rnd.size(), want);
        return;
    }
    uint32_t sz = mpm_call(c, 0x08831CACu, e);
    if (!sz || sz > psp_map_buf_size()) {
        static int logs = 0;
        if (logs++ < 4) { printf("map3d: mapa %d mide %u bytes: se queda su imagen\n", want, sz); fflush(stdout); }
        return;
    }
    m3d_t_req = psp_time_us();
    if (cv_owner() == 0) {
        cv_init(c, (int)e, 4);
        m3d_map = want; m3d_cma = 0; m3d_started = false;
        printf("map3d: mapa %d (entrada %u), primero\n", want, e); fflush(stdout);
        return;
    }
    m3d_nsz = sz; m3d_nbuf = mpm_call(c, 0x08852854u, e, 0, 0);
    if (m3d_nbuf) m3d_npend = want;
}

// camera slot 0 plays the map's intro cameras 0, 1, 2 and again
static uint32_t m3d_slot_cam() { return psp_l32(0x08A7E248u); }
static bool m3d_cman(Ctx& c) {
    for (int tries = 0; tries < 3; tries++) {
        if (m3d_started && cl_call(c, 0x089AA7BCu, 0, 0, 0, 0, 0, 0) == 1) {
            cl_call(c, 0x089AA5D4u, 0, 0, 0, 0, 0, 0);
            cl_call(c, 0x089AAE1Cu, 0, 0, 0, 0, 0, 0);
            m3d_cframes++;
            return true;
        }
        if (m3d_started) {
            static int logs = 0;
            if (logs++ < 12) { printf("map3d: camara %d terminada en %d cuadros\n", m3d_cma, m3d_cframes); fflush(stdout); }
            m3d_cma = (m3d_cma + 1) % 3;
        }
        uint32_t p = cl_call(c, 0x089BA450u, (uint32_t)m3d_cma, 0, 0, 0, 0, 0);
        m3d_started = true; m3d_cframes = 0;
        static int logs = 0;
        if (logs++ < 12) { printf("map3d: camara %d del mapa %d: cma 0x%08X (%s), ranuras 0x%08X\n", m3d_cma, m3d_map, p, p && psp_l32(p) == 0x6E414D43u ? "CMAn" : "?", m3d_slot_cam()); fflush(stdout); }
        if (!p) { m3d_started = false; m3d_cma = (m3d_cma + 1) % 3; continue; }
        cl_call(c, 0x089AA460u, p, 0, 0, 0, 0, 0);
        { Ctx c2 = c; c2.r[4] = 0; c2.f[12].f = 0.0f; c2.r[31] = 0; psp_call(c2, 0x089AA57Cu); }
        cl_call(c, 0x089AA96Cu, 0, 0, 0, 0, 0, 0);
    }
    return false;
}

uint32_t psp_reserve(uint32_t n);
void psp_gpu_layer_scratch(uint32_t a, uint32_t n);
static void ly_scratch_ensure() {   // guest scratch of the BT3 stage layers (GPU), reserved once from the game thread
    static bool done = false;
    if (done) return;
    done = true;
    psp_gpu_layer_scratch(psp_reserve(0x300000), 0x300000);
}
static void m3d_scene(Ctx& c, uint32_t mgr, uint32_t cam) {
    ly_scratch_ensure();
    uint32_t sel = psp_l32(mgr + 0x1160u);
    psp_s32(mgr + 0x1160u, cam);
    cl_call(c, 0x089AF124u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089AF33Cu, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089AF48Cu, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BAA70u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0887B9ACu, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x088068FCu, cam, 0, 0, 0, 0, 0);
    cl_call(c, 0x0880740Cu, cam, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BDE94u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BDFB4u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0880BCF0u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0887BAA4u, 1, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BDFE4u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BE034u, 0, 0, 0, 0, 0, 0);
    psp_s32(mgr + 0x1160u, sel);
}
// generation: the job's map, camera mg_k held at its first frame, drawn hidden (copied out and the screen restored) until it settles,
// then captured on the whole 300:152 rect of the screen (gpu.cpp EB arg 1)
static void mg_draw(Ctx& c) {
    if (cv_owner() != 4 || !m3d_ready || m3d_npend >= 0 || mg_i >= mg_jobs.size() || m3d_map != mg_jobs[mg_i] || mg_state == 2) return;
    uint32_t mgr = psp_l32(0x08A7E260u);
    if (!mgr || !psp_l32(0x08A7E678u)) return;
    cv_flags();
    psp_s32(CV_W + 0x5BD0, 0x1F); psp_s32(CV_W + 0x5BD4, 0);
    if (mg_state == 0) {
        uint32_t p = cl_call(c, 0x089BA450u, (uint32_t)mg_k, 0, 0, 0, 0, 0);
        if (!p || psp_l32(p) != 0x6E414D43u) { printf("render: mapa %d sin camara %d (0x%08X)\n", m3d_map, mg_k, p); fflush(stdout); mg_next_cap(); return; }
        cl_call(c, 0x089AA460u, p, 0, 0, 0, 0, 0);
        { Ctx c2 = c; c2.r[4] = 0; c2.f[12].f = 0.0f; c2.r[31] = 0; psp_call(c2, 0x089AA57Cu); }
        cl_call(c, 0x089AA96Cu, 0, 0, 0, 0, 0, 0);
        mg_state = 1; mg_f = m3d_frame;
    }
    cl_call(c, 0x089AAE1Cu, 0, 0, 0, 0, 0, 0);
    uint32_t cam = m3d_slot_cam();
    if (!cam) return;
    bool cap = m3d_frame - mg_f >= MG_SETTLE;
    s3d_mark_list(c, 0x00F40001u, 0);
    s3d_mark_list(c, 0x00E70000u | 240u << 8, 0x00E80000u | 136u << 8);
    if (cap) { mg_tag++; psp_cap_request_map(mg_tag); s3d_mark_list(c, 0x00EB0001u, 0); }
    m3d_scene(c, mgr, cam);
    if (cap) { s3d_mark_list(c, 0x00EC0000u, 0); mg_state = 2; mg_tries = 0; }
    s3d_mark_list(c, 0x00E90000u, 0);
    s3d_mark_list(c, 0x00F40000u, 0);
}

static void m3d_draw(Ctx& c) {
    if (mg_run) { mg_draw(c); return; }
    if (cv_owner() != 4 || !m3d_ready || !m3d_live() || m3d_map < 0 || !m3d_show()) return;   // Random without its own map: its render
    uint32_t mgr = psp_l32(0x08A7E260u);
    if (!mgr || !psp_l32(0x08A7E678u)) return;
    cv_flags();
    psp_s32(CV_W + 0x5BD0, 0x1F); psp_s32(CV_W + 0x5BD4, 0);
    if (!m3d_cman(c)) return;
    uint32_t cam = m3d_slot_cam();
    if (!cam) return;
    static int logs = 0;
    if (logs++ < 3) {
        float m[4]; for (int i = 0; i < 4; i++) m[i] = cv_rf(cam + 0x40 + 4 * (uint32_t)(12 + i));
        printf("map3d: camara 0x%08X, gestor 0x%08X, actual 0x%08X, vista fila 3 (%.2f, %.2f, %.2f, %.2f)\n", cam, mgr, psp_l32(mgr + 0x1160u), m[0], m[1], m[2], m[3]); fflush(stdout);
    }
    s3d_mark_list(c, 0x00F40001u, 0);   // gpu.cpp: this panel is copied out and the screen restored; the page pastes it (0xF3)
    s3d_mark_list(c, 0x00E70000u | (uint32_t)(m3d_x0 / 2) | (uint32_t)(m3d_x1 / 2) << 8, 0x00E80000u | (uint32_t)(m3d_y0 / 2) | (uint32_t)(m3d_y1 / 2) << 8);
    m3d_scene(c, mgr, cam);
    s3d_mark_list(c, 0x00E90000u, 0);
    static int first = 0;
    if (first++ < 1) { printf("map3d: primer dibujo, mapa %d, panel (%d, %d)-(%d, %d)\n", m3d_map, m3d_x0, m3d_y0, m3d_x1, m3d_y1); fflush(stdout); }
}
