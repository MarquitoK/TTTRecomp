// import census: how often the game calls each one, to see whether it reads any clock
static uint32_t census[4096];
static long census_base = -1;
volatile uint32_t psp_ctrl_inject = 0;

void psp_stack_note(uint32_t lo, uint32_t hi);

void psp_census_report() {
    int idx[4096], n = 0;
    for (int i = 0; i < 4096; i++) if (census[i]) idx[n++] = i;
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++) if (census[idx[b]] > census[idx[a]]) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
    long nc = census_base >= 0 ? cuadros - census_base : 1;
    if (nc < 1) nc = 1;
    printf("censo de imports en %ld cuadros:\n", nc);
    for (int i = 0; i < n && i < 30; i++)
        printf("  censo %-40s %8u  (%.2f por cuadro)\n", psp_import_name(idx[i]), census[idx[i]], (double)census[idx[i]] / nc);
    fflush(stdout);
    for (int i = 0; i < 4096; i++) census[i] = 0;
    psp_jalr_census_report(nc);
    psp_fn_census_report(nc);
    census_base = cuadros;
}

static void psp_hle_(Ctx& c, int id);
int psp_mbx_poll(int id, uint32_t* msg);

#if TTT_PERF
// exclusive hle-zone cycles per import (nested GE/raster work excluded), top 6 per perf period
static uint64_t hc_cyc[4096], hc_n[4096]; static int hc_period = 0, hc_lines = 0;
struct HleCost {
    int id; uint64_t h0;
    explicit HleCost(int i) : id(i >= 0 && i < 4096 && hc_lines < 30 ? i : -1), h0(0) {
        if (id < 0) return;
        if (perf_period != hc_period) {
            if (hc_period > 0) {
                int top[6]; int k = 0;
                for (int j = 0; j < 6; j++) {
                    int b = -1;
                    for (int i2 = 0; i2 < 4096; i2++) { bool u = false; for (int q = 0; q < j; q++) u |= top[q] == i2; if (!u && hc_cyc[i2] && (b < 0 || hc_cyc[i2] > hc_cyc[b])) b = i2; }
                    if (b < 0) break;
                    top[k++] = b;
                }
                double f = (double)(perf_every ? perf_every : 300), tot = 0;
                for (int i2 = 0; i2 < 4096; i2++) tot += (double)hc_cyc[i2];
                printf("hle: periodo %d, llamadas %.2f M/cuadro en total |", hc_period, tot / f / 1e6);
                for (int j = 0; j < k; j++) printf(" %s %.2f M (%.1f)", psp_import_name(top[j]), hc_cyc[top[j]] / f / 1e6, hc_n[top[j]] / f);
                printf("\n"); fflush(stdout);
                hc_lines++;
            }
            hc_period = perf_period;
            memset(hc_cyc, 0, sizeof(hc_cyc)); memset(hc_n, 0, sizeof(hc_n));
        }
        perf_mark(perf_zone); h0 = perf_cycles[PZ_HLE];
    }
    ~HleCost() {
        if (id < 0) return;
        perf_mark(perf_zone);
        uint64_t h1 = perf_cycles[PZ_HLE];
        if (h1 >= h0) hc_cyc[id] += h1 - h0;
        if (h1 >= h0 + 2000000) perf_sub_add(perf_sub_id(psp_import_name(id)), h1 - h0);
        hc_n[id]++;
    }
};
#endif

// TTT_IO_LOG=1: every sceIo/sceUmd call with arguments, result and caller
// ring of the last HLE calls, dumped by hle_ring_dump (an atrac id released twice) and followed for hle_trace_left calls
struct HleRec { int id, th; uint32_t a[4], v, ra; long fr; };
static HleRec hle_ring[8][128];
static unsigned hle_ring_n[8];
static void hle_print(const HleRec& r, const char* tag) {
    printf("hle: %s hilo %d %s(%08X, %08X, %08X, %08X) = %08X desde 0x%08X cuadro %ld\n", tag, r.th, psp_import_name(r.id), r.a[0], r.a[1], r.a[2], r.a[3], r.v, r.ra - 8, r.fr);
}
static void hle_ring_dump() {
    int me = psp_thread_current() & 7;
    for (int t = 0; t < 8; t++) {
        unsigned tot = hle_ring_n[t], n = std::min<unsigned>(tot, t == me ? 128u : 24u);
        for (unsigned k = tot - n; k < tot; k++) hle_print(hle_ring[t][k & 127], "antes");
    }
    for (int i = 0; i < n_blocks; i++) {
        uint32_t a = blocks[i].dir, e = a + blocks[i].size_b;
        if (e > 0x0B300000u && a < 0x0B600000u) printf("hle: bloque %d 0x%08X-0x%08X (%u KB)\n", i, a, e, blocks[i].size_b / 1024);
    }
    printf("hle: brk_free 0x%08X, %d bloques\n", brk_free, n_blocks);
    hle_trace_left = 40;
    fflush(stdout);
}
static void hle_ring_add(Ctx& c, int id, const uint32_t* a, uint32_t ra) {
    int t = psp_thread_current() & 7;
    HleRec& r = hle_ring[t][hle_ring_n[t]++ & 127];
    r.id = id; r.th = psp_thread_current(); memcpy(r.a, a, 16); r.v = c.r[2]; r.ra = ra; r.fr = cuadros;
    if (hle_trace_left > 0) { hle_trace_left--; hle_print(r, "despues"); }
}
static bool hle_ring_set = (hle_ring_dump_fn = hle_ring_dump, true);
void psp_hle_recent(int per_thread) {
    for (int t = 0; t < 8; t++) {
        unsigned tot = hle_ring_n[t], n = std::min<unsigned>(tot, (unsigned)per_thread);
        for (unsigned k = tot - n; k < tot; k++) hle_print(hle_ring[t][k & 127], "reciente");
    }
    fflush(stdout);
}
void psp_hle(Ctx& c, int id) {
    StallHle stall_(id);
#if TTT_PERF
    HleCost hcost_(id);
#endif
    static int io_log = -1;
    if (io_log < 0) { const char* e = getenv("TTT_IO_LOG"); io_log = e ? atoi(e) : 0; }
    if (!io_log) {
        uint32_t a0[4] = {c.r[4], c.r[5], c.r[6], c.r[7]}, ra0 = c.r[31];
        HleCur& hc = hle_cur[psp_thread_current() & 7];
        hc.id = id; memcpy(hc.a, a0, 16); hc.ra = ra0;
        psp_hle_(c, id);
        hc.id = -1;
        hle_ring_add(c, id, a0, ra0);
        return;
    }
    const char* n = psp_import_name(id);
    bool io = !strncmp(n, "sceIo", 5) || !strncmp(n, "sceUmd", 6);
    uint32_t a[4] = {c.r[4], c.r[5], c.r[6], c.r[7]}, ra = c.r[31];
    char txt[96] = "";
    if (io && (!strcmp(n, "sceIoOpen") || !strcmp(n, "sceIoGetstat") || !strncmp(n, "sceIoDopen", 10)) && a[0]) snprintf(txt, sizeof(txt), " \"%.80s\"", (const char*)psp_p(a[0]));
    psp_hle_(c, id);
    if (io && (strcmp(n, "sceUmdGetDriveStat") || io_log > 1))
        printf("io: %s(%08X, %08X, %08X, %08X)%s = %08X desde 0x%08X cuadro %ld\n", n, a[0], a[1], a[2], a[3], txt, c.r[2], ra - 8, cuadros);
}

static void psp_hle_(Ctx& c, int id) {
    PERF(PZ_HLE);
    PERF_N(PC_HLE, 1);
    if (id >= 0 && id < 4096) census[id]++;
    if (psp_cov_on && id >= 0 && id < IMPORTS_N) psp_coverage_note(COV_HLE, IMPORTS[id].stub, c.r[31] - 8);
    if (census_base < 0) census_base = cuadros;
    total_hle++;
    last_hle = id;
    if ((total_hle & 0xFFF) == 0) watch();
    if (patch_hle_fast.val && hle_fast(c, id)) return;
    static int frame_trace = 0, trace_on = -1;   // TTT_TRACE_1500=1: call trace of frames 1500-1501 (it costs ~90 ms there)
    static bool jalr_armed = false;
    if (trace_on < 0) { const char* e = getenv("TTT_TRACE_1500"); trace_on = e && *e == '1'; }
    if (trace_on && cuadros == 1500 && psp_thread_current() == 0 && !jalr_armed) { jalr_armed = true; psp_jalr_trace = 400; }
    if (trace_on && cuadros >= 1500 && cuadros < 1502 && psp_thread_current() == 0 && frame_trace < 120) {
        frame_trace++;
        printf("  [c%ld h%d] %-36s a0=0x%08X a1=0x%08X a2=0x%08X ra=0x%08X\n", cuadros, psp_thread_current(),
               psp_import_name(id), c.r[4], c.r[5], c.r[6], c.r[31]);
    }
    size_t ln = strlen(psp_import_name(id));
    if (ln > 2 && !strcmp(psp_import_name(id) + ln - 2, "CB")) {
        if (!ms_warned && cb_ms >= 0) { ms_warned = true; enqueue_cb(cb_ms, 0, 1); }
        deliver_cbs(c);
    }
    if (order_n < 256) {
        bool seen = false;
        for (long i = 0; i < order_n; i++) if (order[i] == id) { seen = true; break; }
        if (!seen) order[order_n++] = id;
    }
    if (id >= 0 && id < 4096) psp_hle_count[id]++;
    const char* n = psp_import_name(id);
    uint32_t a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
    uint32_t res = 0;

    static bool banner = false;
    if (!banner) { banner = true; printf("runtime compiled %s %s\n", __DATE__, __TIME__); fflush(stdout); }
    if (snd_hle(c, n, &res)) {
        c.r[2] = res;
        c.r[3] = 0;
        return;
    }
    if (net_hle(c, n, &res)) {
        c.r[2] = res;
        c.r[3] = 0;
        return;
    }

    if (!strcmp(n, "sceUtilityLoadModule") || !strcmp(n, "sceUtilityLoadAvModule") ||
        !strcmp(n, "sceUtilityLoadUsbModule") || !strcmp(n, "sceUtilityLoadNetModule")) {
        unsigned b = 1u << (a0 & 31);
        if (modules_loaded & b) res = 0x80111102u;      // already loaded
        else { modules_loaded |= b; res = 0; }
    } else if (!strcmp(n, "sceUtilityUnloadModule") || !strcmp(n, "sceUtilityUnloadAvModule") ||
               !strcmp(n, "sceUtilityUnloadUsbModule") || !strcmp(n, "sceUtilityUnloadNetModule")) {
        unsigned b = 1u << (a0 & 31);
        if (modules_loaded & b) { modules_loaded &= ~b; res = 0; }
        else res = 0x80111102u;                            // not loaded
    } else if (!strcmp(n, "sceIoOpen")) {
        res = io_open((const char*)psp_p(a0));
    } else if (!strcmp(n, "sceIoRead")) {
        res = io_read((int)a0, a1, a2);
    } else if (!strcmp(n, "sceIoLseek32")) {
        res = io_lseek((int)a0, (int32_t)a1, (int)a2);
    } else if (!strcmp(n, "sceIoLseek")) {
        // 64-bit SceOff: offset in a2:a3, mode in t0, result in v0:v1
        res = io_lseek((int)a0, (int32_t)a2, (int)c.r[8]);
        c.r[2] = res;
        c.r[3] = (res == 0xFFFFFFFFu) ? 0xFFFFFFFFu : 0;
        return;
    } else if (!strcmp(n, "sceIoClose")) {
        io_close((int)a0);
        res = 0;
    } else if (!strcmp(n, "sceIoDevctl")) {
        // (device, command, input, input_size, output, output_size)
        const char* dev = a0 ? (const char*)psp_p(a0) : "?";
        static int td = 0;
        if (td++ < 12) printf("devctl %.16s orden 0x%08X ent 0x%08X/%u sal 0x%08X/%u (cuadro %ld)\n",
                              dev, a1, a2, a3, c.r[8], c.r[9], cuadros);
        res = 0;
        switch (a1) {
        case 0x02425818: case 0x02425824: {           // Memory Stick size
            uint32_t p = a2 ? psp_l32(a2) : 0;
            if (p) {
                uint64_t total = 4ull * 1024 * 1024 * 1024, free_mem = 3ull * 1024 * 1024 * 1024;
                psp_s32(p + 0, (uint32_t)(total * 95 / 100 / (32 * 1024)));   // total clusters
                psp_s32(p + 4, (uint32_t)(free_mem / (32 * 1024)));              // free clusters
                psp_s32(p + 8, (uint32_t)(total * 95 / 100 / (32 * 1024)));   // total sectors
                psp_s32(p + 12, 0x200);                                       // sector size
                psp_s32(p + 16, 0x40);                                        // sectors per cluster
            }
            break;
        }
        case 0x02025801: case 0x02425823: case 0x02425802:                    // media inserted
            if (c.r[8]) psp_s32(c.r[8], 1);
            break;
        case 0x02415821:                                    // registers the insertion callback
            if (a2) { cb_ms = (int)psp_l32(a2); ms_warned = false; printf("callback del Memory Stick: %d\n", cb_ms); }
            break;
        case 0x02415822: case 0x02415823: case 0x02415824:
            break;
        default:
            if (c.r[8] && c.r[9] >= 4) psp_s32(c.r[8], 1);
            break;
        }
    } else if (!strcmp(n, "sceIoIoctl")) {
        static int ti = 0;
        if (ti++ < 6) printf("ioctl fd %u orden 0x%08X (cuadro %ld)\n", a0, a1, cuadros);
        res = 0;
    } else if (!strcmp(n, "sceIoGetstat")) {
        // (path, SceIoStat struct): the game only uses the size and st_private[0] = sector
        uint32_t lbn = 0, size_b = 0;
        if (a1 && (disc_find((const char*)psp_p(a0), &lbn, &size_b) || iso_buscar((const char*)psp_p(a0), &lbn, &size_b))) {
            memset(psp_p(a1), 0, 104);
            psp_s32(a1 + 0, 0x2000 | 0x1FF);         // FIO_S_IFREG
            psp_s32(a1 + 4, 0x20);                   // normal file attribute
            psp_s32(a1 + 8, size_b);
            psp_s32(a1 + 64, lbn);                   // st_private[0]
            printf("getstat %.70s -> sector %u, %u bytes\n", (const char*)psp_p(a0), lbn, size_b);
            res = 0;
        } else {
            res = 0x80010002u;                       // does not exist
        }
    } else if (!strcmp(n, "sceUmdGetDriveStat")) {
        res = 0x32;                          // present, initialized and ready
    } else if (!strcmp(n, "sceUmdWaitDriveStat") || !strcmp(n, "sceUmdWaitDriveStatCB") ||
               !strcmp(n, "sceUmdActivate") || !strcmp(n, "sceUmdCheckMedium")) {
        res = !strcmp(n, "sceUmdCheckMedium") ? 1 : 0;
    } else if (!strcmp(n, "sceDisplaySetFrameBuf")) {
        if (with_window) {
            psp_window_lat(1, psp_thread_current());
            psp_gpu_to_window(a0, (int)a1, (int)a2);
            if (psp_window_capture_requested()) psp_gpu_capture(a0, (int)a1, (int)a2);
            psp_window_repeat(logic_rate());
            psp_window_frame_ready();
            perf_frame();
            stall_frame();
            present_thread = psp_thread_current();
            t_last_frame = psp_window_ms();
            if (!psp_window_open()) { psp_request_stop(); psp_block(0); }
            // TTT_PRESENT_NOW=0 defers the frame until the thread blocks again (after the next vblank wait)
            static int already = -1;
            if (already < 0) { const char* e = getenv("TTT_PRESENT_NOW"); already = e ? atoi(e) : 1; printf("presentacion: %s\n", already ? "inmediata en SetFrameBuf" : "diferida al planificador"); }
            if (already) psp_yield();
        } else { psp_gpu_frame(a0, (int)a1, (int)a2); perf_frame(); stall_frame(); }
        res = 0;
    } else if (!strcmp(n, "sceUtilityGetSystemParamInt")) {
        // 8 language: 1 English, 2 French, 3 Spanish; 9 accept button: 1 cross
        uint32_t v = a0 == 8 ? (uint32_t)psp_window_language() : a0 == 9 ? 1u : 0u;
        if (a1) psp_s32(a1, v);
        static int warnings = 0;
        if (warnings++ < 6) printf("system: parametro %u -> %u\n", a0, v);
        res = 0;
    } else if (!strncmp(n, "sceUtility", 10) && strstr(n, "InitStart")) {
        util_state = 1; util_param = a0; util_frames = 0;
        strncpy(util_type, n + 10, 15); util_type[15] = 0;
        char* q = strstr(util_type, "Init"); if (q) *q = 0;
        uint32_t mode = a0 ? psp_l32(a0 + 48) : 0;
        printf("dialogo del sistema: %s (modo %u) en el cuadro %ld, llamado desde 0x%08X\n", util_type, mode, cuadros, c.r[31]);
        if (a0 && !strcmp(util_type, "Savedata")) dump_param(a0);
        osk_mode = 0;
        if (!strcmp(util_type, "Osk")) {
            int b = psp_bank_current(), idx = b >= 0 ? BANKS[b].indice : -1;
            if (c.r[31] == 0x08A8D230u && idx == 20) { osk_mode = 2; psp_account_settings_request(); }
            else if (c.r[31] == 0x08A8BE4Cu && idx == 12 && psp_account_name(osk_name, sizeof(osk_name))) { osk_mode = 1; printf("perfil: partida nueva sin teclado, nombre \"%s\"\n", osk_name); }
        }
        res = 0;
    } else if (!strncmp(n, "sceUtility", 10) && strstr(n, "GetStatus")) {
        bool mine = util_state && strstr(n, util_type) != nullptr;
        if (!mine) res = 0;
        else if (util_state == 1) { util_state = 2; res = 1; }
        else if (util_state == 2) {
            if (!strcmp(util_type, "Osk") && osk_native() && util_param && !osk_mode) {
                static bool osk_open = false;
                if (!osk_open && util_frames == 0) {
                    uint32_t data_ptr = psp_l32(util_param + 52);
                    uint32_t cap = data_ptr ? psp_l32(data_ptr + 48) : 0;
                    psp_window_osk_open("Nombre del jugador", player_name, (int)(cap ? cap : 31));
                    osk_open = true;
                }
                if (osk_open && psp_window_osk_state(nullptr, 0) == 0) { res = 2; c.r[2] = res; c.r[3] = 0; return; }
                osk_open = false;
            }
            if (++util_frames > 3) {
                util_state = 3;
                if (util_param) {
                    uint32_t r = 0;
                    if (!strcmp(util_type, "Savedata")) {
                        uint32_t mode = psp_l32(util_param + 48);
                        r = sav_hle(util_param, mode);
                        printf("savedata modo %u -> resultado 0x%08X\n", mode, r);
                    } else if (!strcmp(util_type, "Osk")) {
                        uint32_t data_ptr = psp_l32(util_param + 52);
                        uint32_t out_buf = data_ptr ? psp_l32(data_ptr + 40) : 0;
                        uint32_t cap = data_ptr ? psp_l32(data_ptr + 48) : 0;
                        if (out_buf) {
                            char text[64];
                            int st = osk_mode == 1 ? 1 : osk_mode == 2 ? 2 : osk_native() ? psp_window_osk_state(text, sizeof(text)) : 1;
                            if (osk_mode == 1) snprintf(text, sizeof(text), "%s", osk_name);
                            else if (!osk_native() || osk_mode == 2) snprintf(text, sizeof(text), "%s", player_name);
                            bool accepted = st == 1;
                            if (accepted && text[0]) { strncpy(player_name, text, 31); player_name[31] = 0; }
                            const char* q = accepted ? text : player_name;
                            uint32_t n = (uint32_t)strlen(q);
                            if (cap && n > cap) n = cap;
                            for (uint32_t i = 0; i < n; i++) psp_s16(out_buf + i * 2, (uint8_t)q[i]);
                            psp_s16(out_buf + n * 2, 0);
                            psp_s32(data_ptr + 36, n);
                            psp_s32(data_ptr + 44, accepted ? 1u : 0u);      // 1 accepted, 0 cancelled
                        }
                        printf("teclado en pantalla -> \"%s\"\n", player_name);
                        r = 0;
                        // cancel: the game checks result==1 in the dialog and asks to return to scene 5
                        if (osk_mode == 2 || (!osk_mode && osk_native() && psp_window_osk_state(nullptr, 0) == 2)) r = 1;
                    } else if (!strcmp(util_type, "MsgDialog")) {
                        psp_s32(util_param + 0x38, 1);                  // button: yes
                    }
                    psp_s32(util_param + 28, r);
                }
            }
            res = util_state;
        } else if (util_state == 3) res = 3;
        else if (util_state == 4) { util_state = 0; res = 4; }
        else res = 0;
    } else if (!strncmp(n, "sceUtility", 10) && strstr(n, "ShutdownStart")) {
        printf("dialogo del sistema %s cierre pedido desde 0x%08X\n", util_type, c.r[31]);
        util_state = 4;
        printf("dialogo del sistema %s cerrado en el cuadro %ld\n", util_type, cuadros);
        res = 0;
    } else if (!strncmp(n, "sceUtility", 10) && strstr(n, "Update")) {
        res = 0;
    } else if (!strcmp(n, "sceGeListEnQueue") || !strcmp(n, "sceGeListEnQueueHead")) {
        psp_ge_list(a0, a1);
        res = 0x10;
    } else if (!strcmp(n, "sceGeListUpdateStallAddr")) {
        psp_ge_stall(a1);
        res = 0;
    } else if (!strcmp(n, "sceGeDrawSync") || !strcmp(n, "sceGeListSync")) {
        psp_ge_finish();
        res = 0;
    } else if (!strcmp(n, "sceDisplayGetVcount")) {
        res = vblank_real() ? (uint32_t)vb_current() : (uint32_t)cuadros;
    } else if (!strncmp(n, "sceCtrl", 7) && (strstr(n, "ReadBuffer") || strstr(n, "PeekBuffer"))) {
        // 16-byte SceCtrlData: timestamp, buttons, axes
        uint32_t nbuf = a1 ? a1 : 1;
        bool neg = strstr(n, "Negative") != nullptr;
        // simulated pad: from frame 120, X for 6 frames every 90; START for 6 frames every 450
        uint32_t buttons = 0;
        uint8_t sx = 128, sy = 128;
        if (with_window) {
            // TTT_FRESH_INPUT=0 uses the sample taken by the message loop again
            static int fresh = -1;
            if (fresh < 0) { const char* e = getenv("TTT_FRESH_INPUT"); fresh = e ? atoi(e) : 1; printf("input: muestreo %s\n", fresh ? "en la lectura del juego" : "en el bucle de mensajes"); }
            if (fd_read < 0) {
                double r = read_delay();
                if (r > 0) psp_window_lat(4, (int)(r * 1000.0));
            }
            if (fresh) psp_window_sample();
            psp_window_lat(0, psp_thread_current());
            if (psp_thread_current() == present_thread && fd_read < 0) fd_read = psp_window_clock();
            buttons = psp_window_buttons() | psp_ctrl_inject; psp_window_stick(&sx, &sy);
        }
        else {
            long fx = cuadros >= 120 ? (cuadros - 120) % 90 : -1;
            long fs = cuadros >= 300 ? (cuadros - 300) % 450 : -1;
            if (fx >= 0 && fx < 6) buttons |= 0x4000;
            if (fs >= 0 && fs < 6) buttons |= 0x0008;
            if (fx == 0 || fs == 0) printf("pad: %s en el cuadro %ld\n", (buttons & 8) ? "START" : "X", cuadros);
        }
        for (uint32_t i = 0; i < nbuf && a0; i++) {
            uint32_t b = a0 + i * 16;
            psp_s32(b + 0, (uint32_t)psp_time_us());
            psp_s32(b + 4, neg ? ~buttons : buttons);
            psp_s8(b + 8, sx);
            psp_s8(b + 9, sy);
        }
        res = nbuf;
    } else if (!strcmp(n, "sceKernelCheckThreadStack")) {
        res = 0x4000;
    } else if (!strcmp(n, "sceKernelPrintf")) {
        const char* f = (const char*)psp_p(a0);
        printf("[juego] %.200s\n", f);
        res = 0;
    } else if (!strcmp(n, "sceKernelExitDeleteThread") || !strcmp(n, "sceKernelExitThread") ||
               !strcmp(n, "sceKernelTerminateDeleteThread")) {
        printf("hilo %d terminado con estado 0x%08X\n", psp_thread_current(), a0);
        psp_thread_exit();
        res = 0;
    } else if (!strcmp(n, "sceKernelMemset")) {
        if ((uint64_t)(a0 & psp_mem_mask) + a2 > psp_mem_mask) {
            printf("memset fuera de rango 0x%08X+%u (hilo %d, ra 0x%08X)\n", a0, a2, psp_thread_current(), c.r[31]);
            a2 = 0;
        }
        memset(psp_p(a0), (int)(a1 & 0xFF), a2);
        res = a0;
    } else if (!strcmp(n, "sceKernelMemcpy") || !strcmp(n, "sceKernelMemmove")) {
        // TTT_TRACE_QUEUE=1: copies from the load ring 0x08B2F580 to the volatile queue
        static int queue_trace = -1, queue_n = 0;
        if (queue_trace < 0) { const char* e = getenv("TTT_TRACE_QUEUE"); queue_trace = e ? atoi(e) : 0; }
        if (queue_trace && a1 >= 0x08B2F580u && a1 < 0x08B3F580u && queue_n++ < 24)
            printf("queue: memcpy 0x%08X <- anillo+0x%X, %u bytes (ra 0x%08X)\n", a0, a1 - 0x08B2F580u, a2, c.r[31]);
        if ((uint64_t)(a0 & psp_mem_mask) + a2 > psp_mem_mask || (uint64_t)(a1 & psp_mem_mask) + a2 > psp_mem_mask) {
            printf("memcpy fuera de rango 0x%08X <- 0x%08X, %u (hilo %d, ra 0x%08X)\n", a0, a1, a2, psp_thread_current(), c.r[31]);
            a2 = 0;
        }
        memmove(psp_p(a0), psp_p(a1), a2);
        res = a0;
    } else if (!strcmp(n, "sceKernelExtendThreadStack")) {
        // (size, entry, argument): runs the entry on a larger stack
        Fn f = psp_resolve(a1);
        if (f) {
            // runs on the thread's real context: only the stack changes
            uint32_t sp = c.r[29], ra = c.r[31];
            uint32_t size_b = a0 ? a0 : 0x8000;
            c.r[29] = reserve_mem(size_b, 256) + size_b - 0x40;
            psp_stack_note(c.r[29] + 0x40 - size_b, c.r[29] + 0x40);
            c.r[4] = a2;
            c.r[31] = 0;
            { PERF(PZ_GEN); f(c); }
            res = c.r[2];
            c.r[29] = sp;
            c.r[31] = ra;
        }
    } else if (!strcmp(n, "sceKernelAllocPartitionMemory")) {
        // (partition, name, type, size, address)
        uint32_t size_b = a3;
        uint32_t d = reserve_mem(size_b ? size_b : 64, 256);
        res = 0x1000 + n_blocks;            // block id
    } else if (!strcmp(n, "sceKernelCreateThread")) {
        uint32_t size_b = a3 ? a3 : 0x10000;
        uint32_t stack = reserve_mem(size_b, 256) + size_b - 0x40;
        psp_stack_note(stack + 0x40 - size_b, stack + 0x40);
        res = 0x2000 + (uint32_t)psp_thread_create(a1, stack, (int)a2);
    } else if (!strcmp(n, "sceKernelStartThread")) {
        {int hid = (int)a0 - 0x2000;
         printf("arranca hilo %d en 0x%08X prioridad %d  (arglen %u, argp 0x%08X, *argp 0x%08X)\n", hid,
                psp_thread_entry(hid), psp_thread_prio(hid), a1, a2, a2 ? psp_l32(a2) : 0);}
        psp_thread_start((int)a0 - 0x2000, a1, a2);
        res = 0;
    } else if (!strcmp(n, "sceKernelDelayThread") || !strcmp(n, "sceKernelDelayThreadCB")) {
        psp_block(psp_time_us() + a0);
        res = 0;
    } else if (!strcmp(n, "sceKernelSleepThread") || !strcmp(n, "sceKernelSleepThreadCB")) {
        psp_block(0);
        res = 0;
    } else if (!strcmp(n, "sceKernelWakeupThread") || !strcmp(n, "sceKernelResumeThread")) {
        psp_wake((int)a0 - 0x2000);
        res = 0;
    } else if (!strcmp(n, "sceKernelWaitThreadEnd") || !strcmp(n, "sceKernelWaitThreadEndCB")) {
        while (psp_thread_state((int)a0 - 0x2000) != 3) psp_block(psp_time_us() + 1000);
        res = 0;
    } else if (!strcmp(n, "sceKernelCreateMbx")) {
        res = 0x4000 + (uint32_t)psp_mbx_create();
    } else if (!strcmp(n, "sceKernelSendMbx")) {
        psp_mbx_send((int)a0 - 0x4000, a1);
        res = 0;
    } else if (!strcmp(n, "sceKernelPollMbx")) {
        uint32_t msg = 0;
        int r = psp_mbx_poll((int)a0 - 0x4000, &msg);
        if (r == 0 && a1) psp_s32(a1, msg);
        res = r == 0 ? 0 : 0x800201B2u;
    } else if (!strcmp(n, "sceKernelReceiveMbx") || !strcmp(n, "sceKernelReceiveMbxCB")) {
        uint32_t msg = 0;
        uint64_t wait_us = a2 ? psp_l32(a2) : 0;
        int r = psp_mbx_receive((int)a0 - 0x4000, &msg, wait_us);
        if (r == 0 && a1) psp_s32(a1, msg);
        res = r == 0 ? 0 : 0x800201A8u;
    } else if (!strcmp(n, "sceKernelCreateCallback")) {
        if (ncbs < 32) {
            cbs[ncbs].fn = a1; cbs[ncbs].arg = a2; cbs[ncbs].id = 0x7000 + ncbs;
            res = 0x7000 + ncbs;
            printf("callback creado %u -> 0x%08X arg 0x%08X (%.20s)\n", res, a1, a2, a0 ? (const char*)psp_p(a0) : "");
            ncbs++;
        } else res = 0;
    } else if (!strcmp(n, "sceKernelCheckCallback")) {
        if (!ms_warned && cb_ms >= 0) { ms_warned = true; enqueue_cb(cb_ms, 0, 1); }
        int avail = nqueue;
        deliver_cbs(c);
        res = (uint32_t)avail;
    } else if (!strcmp(n, "sceKernelNotifyCallback")) {
        enqueue_cb((int)a0, 0, a1);
        res = 0;
    } else if (!strcmp(n, "sceKernelCreateSema")) {
        res = 0x5000 + (uint32_t)psp_sema_create((int)a2, (int)a3);
    } else if (!strcmp(n, "sceKernelWaitSema") || !strcmp(n, "sceKernelWaitSemaCB")) {
        res = psp_sema_wait((int)a0 - 0x5000, (int)a1, a2 ? psp_l32(a2) : 0) == 0 ? 0 : 0x800201A8u;
    } else if (!strcmp(n, "sceKernelSignalSema")) {
        psp_sema_signal((int)a0 - 0x5000, (int)a1);
        res = 0;
    } else if (!strcmp(n, "sceKernelCreateEventFlag")) {
        res = 0x6000 + (uint32_t)psp_flag_create(a2);
    } else if (!strcmp(n, "sceKernelSetEventFlag")) {
        psp_flag_set((int)a0 - 0x6000, a1);
        res = 0;
    } else if (!strcmp(n, "sceKernelClearEventFlag")) {
        psp_flag_clear((int)a0 - 0x6000, a1);
        res = 0;
    } else if (!strcmp(n, "sceKernelWaitEventFlag") || !strcmp(n, "sceKernelWaitEventFlagCB")) {
        uint32_t outp = 0;
        int r = psp_flag_wait((int)a0 - 0x6000, a1, (int)a2, &outp, c.r[8] ? psp_l32(c.r[8]) : 0);
        if (r == 0 && a3) psp_s32(a3, outp);
        res = r == 0 ? 0 : 0x800201A8u;
    } else if (!strncmp(n, "sceDisplayWaitVblank", 20)) {
        cuadros++;
        PERF_N(PC_VBLANK, 1);
        int nvb = strstr(n, "Multi") ? (int)a0 : 1;
        if (nvb < 1) nvb = 1;
        static int vbmax = -1;   // TTT_VBMAX=1 trims the game's wait and runs it at 60
        if (vbmax < 0) { const char* e = getenv("TTT_VBMAX"); vbmax = e ? atoi(e) : 0; }
        if (vbmax > 0 && nvb > vbmax) nvb = vbmax;
        static int seen_list[8]; static int n_seen = 0;
        bool is_new = true;
        for (int i = 0; i < n_seen; i++) if (seen_list[i] == nvb) is_new = false;
        if (is_new && n_seen < 8) { seen_list[n_seen++] = nvb; printf("vblank: %s espera %d\n", n, nvb); fflush(stdout); }
        if (psp_thread_current() == present_thread) { watch_write(); find_position(); find_rate(); half_step(); patch_counters(); set_values(); two_to_one(); }
        nvb *= logic_rate();
        if (psp_thread_current() == present_thread) {
            nvb_last = nvb;
            if (fd_read >= 0) { fd_span[fd_i++ % 32] = (psp_window_clock() - fd_read) * 1000.0; fd_read = -1; }
        }
        psp_window_lat(2, psp_thread_current());
        if (psp_thread_current() == present_thread) load_check();
        double t_wait = clock_s();
        if (vblank_real() && load_turbo() && with_window && psp_thread_current() == present_thread && vb_origin > 0) {
            long target = vb_last + nvb;
            double missing = vb_origin + (double)target * vb_period() - psp_window_clock();
            if (missing > 0) vb_origin -= missing + 1e-6;
            vb_last = target;
        } else if (vblank_real()) vblank_rate_multi(nvb);
        else vblank_rate();
        if (cg.active && psp_thread_current() == present_thread) cg.wait_for += clock_s() - t_wait;
        psp_window_lat(3, psp_thread_current());
        psp_block(psp_time_us() + 16667 * nvb);
        res = 0;
    } else if (!strcmp(n, "sceKernelGetBlockHeadAddr")) {

        int i = (int)a0 - 0x1000 - 1;
        res = (i >= 0 && i < n_blocks) ? blocks[i].dir : 0;
    } else if (!strcmp(n, "sceKernelMaxFreeMemSize") || !strcmp(n, "sceKernelTotalFreeMemSize")) {
        res = 0x01000000u;
    } else if (!strcmp(n, "sceKernelCreateSema") || !strcmp(n, "sceKernelCreateEventFlag") ||
               !strcmp(n, "sceKernelCreateMbx") || !strcmp(n, "sceKernelCreateCallback") ||
               !strcmp(n, "sceKernelCreateVpl") || !strcmp(n, "sceKernelCreateFpl")) {
        res = 0x3000 + (uint32_t)total_hle;
    } else if (!strcmp(n, "sceKernelDevkitVersion")) {
        res = 0x06060010u;
    } else if (!strcmp(n, "sceGeEdramGetAddr")) {
        res = 0x04000000u;
    } else if (!strcmp(n, "sceGeEdramGetSize")) {
        res = 0x00200000u;
    } else if (!strcmp(n, "sceKernelVolatileMemLock") || !strcmp(n, "sceKernelVolatileMemTryLock")) {
        // 32 MB volatile zone at 0x0E000000: the GE only addresses 28 bits, so it must stay below 0x10000000
        static int vol_psp = -1, vol_warned = 0;
        if (vol_psp < 0) { const char* e = getenv("TTT_VOLATILE"); vol_psp = e && !strcmp(e, "psp"); }
        if (a1) psp_s32(a1, vol_psp ? 0x08400000u : 0x0E000000u);
        if (a2) psp_s32(a2, vol_psp ? 0x00400000u : 0x02000000u);
        if (!vol_warned++) printf("volatile: base 0x%08X, %u MB\n", vol_psp ? 0x08400000u : 0x0E000000u, vol_psp ? 4u : 32u);
        res = 0;
    } else if (!strcmp(n, "sceKernelLibcClock")) {
        res = (uint32_t)psp_time_us();
    } else if (!strcmp(n, "sceKernelGetSystemTimeLow")) {
        res = (uint32_t)psp_time_us();
    } else if (!strcmp(n, "sceKernelGetSystemTimeWide")) {
        uint64_t t = psp_time_us();
        c.r[2] = (uint32_t)t;
        c.r[3] = (uint32_t)(t >> 32);
        return;
    } else if (!strcmp(n, "sceKernelLibcTime")) {
        res = 1700000000u;
    } else if (!strcmp(n, "sceKernelExitGame") || !strcmp(n, "sceKernelSelfStopUnloadModule")) {
        longjmp(exit_jmp, 2);
    } else if (!strcmp(n, "sceIoOpen")) {
        res = 0xFFFFFFFFu;                  // no file system yet
    }
    c.r[2] = res;
    c.r[3] = 0;
    (void)a0; (void)a1; (void)a2; (void)a3;
}

