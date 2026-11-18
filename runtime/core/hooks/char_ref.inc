uint32_t psp_map_buf_size();
// Character Reference (G 63): 8th item of the main menu wheel (17_11) and the BT3 screen from a mod
// (Packfile/Menus/menu_chara_reference, codes: 1/2 cr_main, 3/4 cr_parts, 5/6 cr_message, 7/8 cr_bg; texts in CharRef/<lang>.txt)
int psp_cr_pak();
std::string psp_cr_file(const std::string& rel);
int psp_cr_voice(const std::string& name);
std::string psp_cr_voice_info(int idx);
int psp_cr_big(int sysid);
bool psp_disc_blink(uint32_t idx);
int psp_lip_dir(const std::string& dir);
int psp_cr_map();
int psp_col_count(int s);
bool psp_disc_lv1(uint32_t idx);
bool psp_disc_dmg(uint32_t idx, uint32_t* out);
bool psp_disc_held(uint32_t idx);
void psp_disc_hold(uint32_t idx, bool front, bool fast);
void psp_disc_prebuild(uint32_t idx);
bool psp_input_pad();
void psp_input_viewer(float* ax, float* ay, float* zoom, int* wheel, int* wasd, int* dirs);

static const int CR_ITEM = 7;
enum { CRN_LOAD = 40, CRN_PAK, CRN_ANIM, CRN_DRAW, CRN_PAK2 };
enum { CK_BG, CK_MAIN, CK_ROW0, CK_BAR = CK_ROW0 + 5, CK_POINT, CK_PLATE0, CK_PLATE5 = CK_PLATE0 + 4, CK_NUM, CK_PBOX, CK_VBOX, CK_GUIDE, CK_EYE, CK_MOUTH,
       CK_AUP, CK_ADOWN, CK_ALEFT, CK_ARIGHT, CK_MSG, CK_ART, CK_FRAME, CK_VICON, CK_VROW, CK_VINFO, CK_SLEFT = CK_VINFO + 3, CK_SRIGHT, CK_N };
enum { CP_LOAD, CP_ANIM, CP_LIST, CP_DET, CP_PROF, CP_VOICE, CP_COMM, CP_MODEL, CP_DONE, CP_VIEW };
enum { CS_ROW, CS_BAR, CS_POINT, CS_GUIDE, CS_AUP, CS_ADOWN, CS_A_EYE, CS_A_MOUTH, CS_B_EYE, CS_B_MOUTH, CS_NAME, CS_FORM, CS_MSG, CS_GUIDE_BAR,
       CS_PLATE1, CS_PLATE5 = CS_PLATE1 + 4, CS_NUM, CS_PBOX, CS_VBOX, CS_ALEFT, CS_ARIGHT, CS_NAME1, CS_FORM1, CS_VOICE, CS_PROFILE, CS_ART, CS_FRAME, CS_VICON, CS_LABEL, CS_VALUE, CS_N };

static uint32_t cr_mem = 0, cr_list = 0, cr_list_alloc = 0, cr_w = 0, cr_loader = 0, cr_loader2 = 0, cr_fb = 0, cr_rmem = 0, cr_rhnd = 0;
static int cr_rfor = -1, cr_rbuf = 0;
static uint32_t cr_hnd[4], cr_hld[CK_N], cr_hdr = 0, cr_nav = 0;
static uint32_t cr_vnav = 0;   // button bar of the 3D viewer (pak entry 24): in the widget tree for its load and animation, hidden there, drawn by the viewer
static std::vector<uint32_t> cr_vnav_cur;
static int cr_vtxt[7] = {-1, -1, -1, -1, -1, -1, -1};   // its texts: rotate, zoom, colour, face, form, damage, voice
static int cr_line = 0, cr_idle = 0, cr_last = -1;
static bool cr_wheel = false, cr_want = false, cr_leave = false, cr_ret = false;
static int cr_phase = CP_LOAD, cr_cursor = 0, cr_row = 0, cr_scroll = 0, cr_pose = 0, cr_blink = 120, cr_rowst[5];

struct CrTexts { std::vector<std::string> name, form, voice, profile, comment; std::vector<int> first, count, pose; };
static CrTexts cr_tx;
static int cr_tx_lang = -1;
static uint32_t cr_fno = 0, cr_seen = 0, cr_ups = 0, cr_draws = 0;
static bool cr_self = false, cr_talk = false;
static int cr_next_st = -1, cr_next_ph = CP_LIST, cr_menu = 0, cr_comm = 0, cr_ctime = 0, cr_costume = 0, cr_pscroll = 0;
static int cr_vreq = -1, cr_vkind = 0;   // pending Chi-Chi line; 0 none, 1 Chi-Chi, 2 character sample
static bool cr_vplay = false, cr_vend = false;

// voices: AT3 entries of the mod played like the game's voices (category 1, stream 1: voices volume and voice language packs)
static const uint32_t CR_SND = 0x08A78EE8u;   // holds the sound manager
static std::string cr_vname;
static bool cr_vkill = false, cr_vseen = false;
static int cr_vwait = 0;
static void cr_vstop() { if (cr_vplay) cr_vkill = true; cr_vplay = false; cr_vend = false; cr_vreq = -1; cr_vkind = 0; cr_vname.clear(); }
static void cr_vplay_file(const std::string& name, int kind) { cr_vstop(); cr_vname = name; cr_vkind = kind; }
static std::string cv_sample_name(int b, int k) { char n[32]; snprintf(n, sizeof n, "chara_%03d_%d", b, k); return n; }
static void cr_sample() { cr_vplay_file(cv_sample_name(cr_cursor, rand() % 2), 2); }
// screen music: Music/4410_ost_sm_4 on channel 0 once the main menu's track has faded (as every menu screen, only when nothing plays)
static const uint32_t CR_MUSIC = 4410;
static int cr_mus = 0;   // 0 off, 1 waiting for channel 0, 2 playing
static void cr_music(const Ctx& c, bool on) {
    uint32_t mgr = psp_l32(CR_SND);
    if (!mgr) return;
    if (!on) {
        if (cr_mus == 2) { Ctx c2 = c; c2.r[4] = mgr; c2.r[5] = 0; c2.f[12].f = 1000.0f; c2.r[31] = 0; psp_call(c2, 0x0883A6E8u); }
        cr_mus = 0;
        return;
    }
    static int wait = 0;
    if (cr_mus == 0) { cr_mus = 1; wait = 0; }
    if (cr_mus != 1) return;
    bool busy = mpm_call(c, 0x0883A7F4u, mgr, 0) != 0;
    if (busy && ++wait < 150) return;
    if (busy) mpm_call(c, 0x0883A5A4u, mgr, 0);
    Ctx c2 = c;
    c2.r[4] = mgr; c2.r[5] = 0; c2.r[6] = 0; c2.r[7] = CR_MUSIC; c2.r[8] = 0; c2.r[9] = 0; c2.f[12].f = 0.0f; c2.r[31] = 0;
    psp_call(c2, 0x0883A430u);
    cr_mus = 2;
    printf("cr: musica %u en el canal 0%s\n", CR_MUSIC, busy ? " (se detuvo la anterior)" : ""); fflush(stdout);
}
static void cr_vtick(const Ctx& c) {
    uint32_t mgr = psp_l32(CR_SND);
    if (cr_vkill && mgr) mpm_call(c, 0x0883A5A4u, psp_l32(CR_SND), 1);
    cr_vkill = false;
    if (!cr_vname.empty()) {
        int idx = psp_cr_voice(cr_vname);
        static int logs = 0;
        if (idx >= 0 && mgr) {
            Ctx c2 = c;
            c2.r[4] = mgr; c2.r[5] = 1; c2.r[6] = 1; c2.r[7] = (uint32_t)idx; c2.r[8] = 0; c2.r[9] = 0; c2.f[12].f = 0.0f; c2.r[31] = 0;
            psp_call(c2, 0x0883A430u);
            cr_vplay = true; cr_vseen = false; cr_vwait = 0;
            if (logs++ < 24) printf("cr: voz %s (entrada %d, %s)\n", cr_vname.c_str(), idx, psp_cr_voice_info(idx).c_str());
        } else {
            cr_vkind = 0;
            if (logs++ < 24) printf("cr: sin voz %s en el mod\n", cr_vname.c_str());
        }
        cr_vname.clear();
    } else if (cr_vplay) {
        bool on = mgr && mpm_call(c, 0x0883A7F4u, mgr, 1) != 0;
        if (on) cr_vseen = true;
        // Chi-Chi: also ended when her lip sync has been silent for 0.75 s after she spoke (the stream can stay "playing")
        static int quiet = 0;
        static bool heard = false;
        if (cr_vkind == 1) { if (psp_lip_dir("menus/chara_reference_voices") >= 0) { heard = true; quiet = 0; } else if (heard) quiet++; }
        bool silent = cr_vkind == 1 && heard && quiet > 45;
        if ((cr_vseen && !on) || silent || (!cr_vseen && ++cr_vwait > 90)) {
            static int logs = 0;
            if (logs++ < 12) printf("cr: voz terminada (%s)\n", silent && on ? "silencio" : cr_vseen ? "fin del audio" : "no empezo");
            cr_vplay = false; cr_vend = true; heard = false; quiet = 0;
        }
    }
    cr_talk = cr_vplay && cr_vkind == 1;
}

static uint32_t cr_name(int k) { return cr_mem + 0x2800 + 0x20 * k; }
static uint32_t cr_vt() { return cr_mem + 0x200; }
static uint32_t cr_buf(int k) { return cr_mem + 0x400 + 0x100 * k; }
static uint32_t cr_scratch() { return cr_mem + 0x300; }
static uint32_t cr_navt() { return cr_mem + 0x380; }
static uint32_t cr_titles() { return cr_mem + 0x3C0; }

static void cr_setup() {
    if (cr_mem) return;
    cr_mem = psp_reserve(0x3000);
    static const char* N[CS_N] = {"list_row", "scroll_bar", "scroll_point", "guide_chichi", "arrow_up", "arrow_down", "chichi_a_eye", "chichi_a_mouth",
                                  "chichi_b_eye", "chichi_b_mouth", "text_name", "text_form", "text_message", "guide",
                                  "menu_plate_1", "menu_plate_2", "menu_plate_3", "menu_plate_4", "menu_plate_5", "num", "profile_box", "voice_box",
                                  "arrow_left", "arrow_right", "text_name_1", "text_form_1", "text_voice", "text_profile", "chara_art", "chara_art_frame", "voice_icon", "text_label", "text_value"};
    for (int k = 0; k < CS_N; k++) memcpy(psp_p(cr_name(k)), N[k], strlen(N[k]) + 1);
    memcpy(psp_p(cr_vt()), psp_p(0x08A70E08u), 0xB0);
    psp_s32(cr_vt() + 0x14, MPM_NAT + 4 * CRN_LOAD);
    psp_s32(cr_vt() + 0x64, MPM_NAT + 4 * CRN_ANIM);
    psp_s32(cr_vt() + 0x6C, MPM_NAT + 4 * CRN_DRAW);
}

static bool cr_on() { return psp_cr_pak() >= 0; }

// CharRef/<lang>.txt: key.index=value, \n escaped
static void cr_texts(uint32_t lang) {
    int li = mpm_li(lang);
    if (cr_tx_lang == li) return;
    cr_tx_lang = li;
    cr_tx = CrTexts{};
    static const char* L[2] = {"en", "es"};
    std::string path = psp_cr_file(std::string("CharRef/") + L[li] + ".txt");
    if (path.empty()) path = psp_cr_file("CharRef/es.txt");
    if (path.empty()) path = psp_cr_file("CharRef/en.txt");
    FILE* f = path.empty() ? nullptr : fopen(path.c_str(), "rb");
    if (!f) { printf("cr: sin textos (CharRef/%s.txt en el mod)\n", L[li]); return; }
    char line[8192];
    bool first = true;
    auto put = [](std::vector<std::string>& v, int i, const std::string& s) { if (i < 0 || i > 4096) return; if ((int)v.size() <= i) v.resize(i + 1); v[i] = s; };
    auto puti = [](std::vector<int>& v, int i, int x) { if (i < 0 || i > 4096) return; if ((int)v.size() <= i) v.resize(i + 1, 0); v[i] = x; };
    while (fgets(line, sizeof(line), f)) {
        char* p = line;
        if (first && (uint8_t)p[0] == 0xEF && (uint8_t)p[1] == 0xBB && (uint8_t)p[2] == 0xBF) p += 3;
        first = false;
        size_t n = strlen(p);
        while (n && (p[n - 1] == '\n' || p[n - 1] == '\r')) p[--n] = 0;
        char *dot = strchr(p, '.'), *eq = strchr(p, '=');
        if (p[0] == '#' || !dot || !eq || dot > eq) continue;
        std::string key(p, dot - p), v;
        int i = atoi(dot + 1);
        for (char* s = eq + 1; *s; s++) {
            if (s[0] == '\\' && s[1] == 'n') { v += '\n'; s++; }
            else v += *s;
        }
        if (key == "name") put(cr_tx.name, i, v);
        else if (key == "form") put(cr_tx.form, i, v);
        else if (key == "voice") put(cr_tx.voice, i, v);
        else if (key == "profile") put(cr_tx.profile, i, v);
        else if (key == "comment") put(cr_tx.comment, i, v);
        else if (key == "first") puti(cr_tx.first, i, atoi(v.c_str()));
        else if (key == "count") puti(cr_tx.count, i, atoi(v.c_str()));
        else if (key == "pose") puti(cr_tx.pose, i, atoi(v.c_str()));
    }
    fclose(f);
    printf("cr: textos %s: %d nombres, %d perfiles, %d comentarios\n", path.c_str(), (int)cr_tx.name.size(), (int)cr_tx.profile.size(), (int)cr_tx.comment.size());
}

static int cr_game_char(int b);
static std::vector<int> cr_ids;   // BT3 ids shown in the list: the ones with a model in the port
static int cr_pos = 0;             // list position of cr_cursor
static int cr_all() { int n = (int)cr_tx.profile.size(); return n > 0 ? n : (int)cr_tx.name.size(); }
static int cr_count() { return cr_ids.empty() ? cr_all() : (int)cr_ids.size(); }
static int cr_wrapi(int n) { int c = cr_count(); return c ? ((n % c) + c) % c : 0; }
static int cr_id(int pos) { int p = cr_wrapi(pos); return cr_ids.empty() ? p : cr_ids[p]; }
static void cr_goto(int pos) { cr_pos = cr_wrapi(pos); cr_cursor = cr_id(cr_pos); }

static uint32_t cr_u16(const std::string& s, int buf) {
    uint32_t a = cr_buf(buf);
    const unsigned char* p = (const unsigned char*)s.c_str();
    int n = 0;
    while (*p && n < 126) {
        uint32_t u = *p++;
        if (u >= 0xE0 && p[0] && p[1]) { u = ((u & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
        else if (u >= 0xC0 && *p) u = ((u & 0x1F) << 6) | (*p++ & 0x3F);
        if (u == '\n') break;
        psp_s16(a + 2 * n++, u);
    }
    psp_s16(a + 2 * n, 0);
    return a;
}

// name of the root node of a holder's state (empty when the AGS does not have it)
static std::string cr_state_name(uint32_t h, int st) {
    uint32_t inst = h ? au_record(h, st) : 0, sc = inst ? psp_l32(inst + 0x30) : 0, node = sc ? psp_l32(sc) : 0, nm = node ? psp_l32(node + 8) : 0;
    if (nm < 0x08800000u || nm >= 0x0A000000u) return "";
    std::string r;
    for (int i = 0; i < 48; i++) { uint8_t ch = psp_l8(nm + i); if (!ch) break; if (ch < 32 || ch > 126) return ""; r += (char)ch; }
    return r;
}

// ---- main menu wheel: 7 items -> 8 (holder of item 7 at list +0xA4, AGS 8 states 14/15 of menu_main)
static bool cr_mm_ready(const Ctx& c, uint32_t w) {
    uint32_t h = psp_l32(w + 0x7C);
    if (!h) return false;
    return !psp_l32(h) || mpm_ended(c, h);
}

static uint32_t cr_desc(uint32_t lang) {
    static const char* T[2] = {"Browse the characters' profiles, voices and models.", "Consulta los perfiles, voces y modelos de los personajes."};
    cr_setup();
    return cr_u16(T[mpm_li(lang)], 15);
}

// wheel order of the 8 items: the Character Reference (7) right before Options (6)
static const int CR_ORD[8] = {0, 1, 2, 3, 4, 5, 7, 6};
static int cr_ordpos(int v) { for (int i = 0; i < 8; i++) if (CR_ORD[i] == v) return i; return 0; }
static bool cr_menu_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (dir == 0x089BFDF4u) {   // main menu list object (0x08A83220): one more word for the 8th holder
        if (c.r[31] != 0x08A83224u || c.r[4] != 0xA4u || !cr_on()) return false;
        c.r[4] = 0xA8u;
        orig(c);
        cr_list_alloc = c.r[2];
        return true;
    }
    if (dir == 0x08A83554u) {   // list load: 7 holders, then the 8th one
        uint32_t L = c.r[4];
        Ctx c0 = c;
        bool big = L && L == cr_list_alloc;
        if (big) psp_s32(L + 0xA4, 0);
        cr_wheel = false;
        orig(c);
        if (!big) return true;
        uint32_t h = au_new(c0, psp_l32(L + 0x68), 14, 15);
        std::string nm = cr_state_name(h, 14);
        if (h && nm.empty()) { mpm_call(c0, 0x089D248Cu, h, 3); h = 0; }
        if (h) {
            psp_s32(L + 0xA4, h);
            cr_wheel = true; cr_list = L;
            uint32_t cur = psp_l32(L + 0x70);
            int k = cr_ordpos((int)(psp_l32(L + 0x74) % 7));
            if (cr_ret && cur < 5) k = (cr_ordpos(CR_ITEM) + 8 - (int)cur) % 8;   // back from the screen: its item under the cursor, as the game does for the other modes
            for (int r = 0; r < 5; r++) psp_s32(L + 0x74 + 4 * r, (uint32_t)CR_ORD[(k + r) % 8]);
        }
        cr_ret = false;
        printf("cr: menu principal %s (lista 0x%08X, giro %u, estado 14 '%s')\n", h ? "con 8 elementos, Referencia de personajes antes de Opciones" : "sin los estados 14/15 en la entrada 8 (menu_main sin el parche): 7 elementos", L, psp_l32(L + 0x74), nm.c_str());
        return true;
    }
    if (dir == 0x08A83508u) {   // wheel rotation modulo 8
        uint32_t L = c.r[4];
        if (!cr_wheel || L != cr_list) return false;
        int d = (int32_t)c.r[5];
        for (int r = 0; r < 5; r++) { int v = cr_ordpos((int)psp_l32(L + 0x74 + 4 * r)) + d; psp_s32(L + 0x74 + 4 * r, (uint32_t)CR_ORD[((v % 8) + 8) % 8]); }
        return true;
    }
    if (dir == 0x08A836F0u) {   // list unload
        uint32_t L = c.r[4];
        if (L && L == cr_list) {
            uint32_t h = psp_l32(L + 0xA4);
            if (h) mpm_call(c, 0x089D248Cu, h, 3);
            psp_s32(L + 0xA4, 0);
            cr_wheel = false; cr_list = 0;
        }
        return false;
    }
    if (dir == 0x08A81DC0u) {   // main menu input: blocked while the screen is open; confirm on item 7 opens it
        if (cr_w || cr_want) return true;
        uint32_t w = c.r[4], L = psp_l32(w + 0x94);
        if (!cr_on() || !L || !cr_mm_ready(c, w) || psp_l8(w + 0x90)) return false;
        bool conf = (psp_l32(0x08B4E990u + 0x30) & psp_l32(0x08A793D4u)) != 0;
        if (cr_wheel && L == cr_list && conf && mpm_call(c, 0x08A846D0u, L) == (uint32_t)CR_ITEM) {
            cr_leave = true;
            printf("cr: elegida en el menu principal, el menu se cierra como con los otros modos\n");
        }
        return false;
    }
    if (dir == 0x08830274u && cr_leave) return true;   // item 7 preload of the main menu table (title paks): not needed
    if (dir == 0x089C34ACu && cr_leave && c.r[5] == (uint32_t)(CR_ITEM + 1)) {   // its scene request: the screen opens instead
        cr_leave = false; cr_want = true;
        printf("cr: menu principal cerrado, abre la referencia de personajes\n");
        return true;
    }
    if (dir == 0x0883EE74u) {   // item 7 description (text id 0x4E27 from 0x08A82438)
        if (c.r[4] != 0x4E20u + CR_ITEM || c.r[31] != 0x08A82440u || !cr_wheel) return false;
        c.r[2] = cr_desc(mpm_lang(c));
        return true;
    }
    return false;
}

// ---- screen
static uint32_t cr_slot(const Ctx& c, uint32_t h, int name) {
    uint32_t inst = h ? psp_l32(h) : 0;
    if (!inst || !psp_l32(inst + 0x30)) return 0;
    return mpm_call(c, 0x08840DC4u, psp_l32(inst + 0x30), cr_name(name));
}

// n-th node with that name, in the order of the game's own search (0x088208E8: self, siblings, then children)
static uint32_t cr_find(uint32_t n, const char* want, int& left, int depth) {
    for (; n && depth < 64; n = psp_l32(n + 0xC8)) {
        uint32_t nm = psp_l32(n + 8);
        if (nm && !strcmp((const char*)psp_p(nm), want) && left-- == 0) return n;
    }
    return 0;
}
static uint32_t cr_find_tree(uint32_t n, const char* want, int& left, int depth) {
    if (!n || depth > 16) return 0;
    uint32_t r = cr_find(n, want, left, depth);
    if (r) return r;
    for (uint32_t s = n; s; s = psp_l32(s + 0xC8)) if ((r = cr_find_tree(psp_l32(s + 0xC4), want, left, depth + 1))) return r;
    return 0;
}
static uint32_t cr_slot_nth(const Ctx& c, uint32_t h, int name, int nth) {
    uint32_t inst = h ? psp_l32(h) : 0, sc = inst ? psp_l32(inst + 0x30) : 0;
    uint32_t node = sc ? psp_l32(sc) : 0;
    int left = nth;
    (void)c;
    return node ? cr_find_tree(node, (const char*)psp_p(cr_name(name)), left, 0) : 0;
}

static void cr_bind(const Ctx& c, int k, uint32_t parent, int name, int nth = 0) {
    if (cr_hld[k]) psp_s32(cr_hld[k] + 8, nth ? cr_slot_nth(c, parent, name, nth) : cr_slot(c, parent, name));
}

static int cr_frames(uint32_t h) {
    uint32_t inst = h ? psp_l32(h) : 0;
    if (!inst || !psp_l32(inst + 0x30)) return 0;
    return (int)psp_l32(psp_l32(psp_l32(psp_l32(inst + 0x30)) + 0x198) + 0x3C);
}
static bool cr_done(uint32_t h) {
    uint32_t inst = h ? psp_l32(h) : 0;
    return inst && (int)mpm_f(inst + 0x2C) >= cr_frames(h) - 1;
}
// one step of a holder: stops on the last frame, or back to 0 when loop
static void cr_tick(const Ctx& c, uint32_t h, bool loop) {
    uint32_t inst = h ? psp_l32(h) : 0;
    if (!inst) return;
    mpm_call(c, 0x089D19D0u, inst);
    float n = (float)(cr_frames(h) - 1), f = mpm_f(inst + 0x2C) + mpm_f(h + 0x10);
    if (f > n) f = loop ? 0.0f : n;
    mpm_wf(inst + 0x2C, f < 0.0f ? 0.0f : f);
}

static int cr_item(int occ) { return cr_id(cr_pos - cr_row + 4 - occ); }

static void cr_attach(const Ctx& c) {
    uint32_t m = cr_hld[CK_MAIN];
    for (int k = 0; k < 5; k++) psp_s32(cr_hld[CK_ROW0 + k] + 8, cr_slot_nth(c, m, CS_ROW, k));
    cr_bind(c, CK_BAR, m, CS_BAR);
    cr_bind(c, CK_GUIDE, m, CS_GUIDE);
    cr_bind(c, CK_AUP, m, CS_AUP);
    cr_bind(c, CK_ADOWN, m, CS_ADOWN);
    cr_bind(c, CK_ALEFT, m, CS_ALEFT);
    cr_bind(c, CK_ARIGHT, m, CS_ARIGHT);
    for (int k = 0; k < 5; k++) cr_bind(c, CK_PLATE0 + k, m, CS_PLATE1 + k);
    cr_bind(c, CK_NUM, cr_hld[CK_PLATE5], CS_NUM);
    cr_bind(c, CK_PBOX, m, CS_PBOX);
    cr_bind(c, CK_VBOX, m, CS_VBOX);
    cr_bind(c, CK_FRAME, m, CS_FRAME);
    cr_bind(c, CK_VICON, m, CS_VICON);
    cr_bind(c, CK_EYE, cr_hld[CK_GUIDE], cr_pose ? CS_B_EYE : CS_A_EYE);
    cr_bind(c, CK_MOUTH, cr_hld[CK_GUIDE], cr_pose ? CS_B_MOUTH : CS_A_MOUTH);
}

static void cr_loaded(const Ctx& c, uint32_t w) {
    uint32_t pak = psp_l32(cr_loader + 0x14);
    if (!psp_l32(0x08B48A08u)) { psp_s32(0x08B48A08u, 1); mpm_call(c, 0x088402C0u, 0x08B48200u); }
    static const int AGS[4] = {7, 1, 3, 5};   // codes are the 1-based file numbers (0x088403F0 reads [pak + 4 * code])
    uint32_t lg = mpm_lang(c), prhg = 20 + (lg == 2 ? 2 : 1);   // parts textures by language (21 en, 22 es), else 4
    if (!hud_has(pak, prhg)) prhg = 4;
    for (int i = 0; i < 4; i++) cr_hnd[i] = mpm_call(c, 0x088404D8u, 0x08B48200u, pak, AGS[i], i == 2 ? prhg : AGS[i] + 1);
    for (int k = 0; k < CK_N; k++) {
        int src = k == CK_BG ? 0 : k == CK_MAIN ? 1 : k == CK_MSG ? 3 : 2, first = 0, last = 0;
        if (k == CK_MAIN) last = 23;
        else if (k >= CK_ROW0 && k < CK_BAR) { first = 10; last = 14; }
        else if (k == CK_BAR) first = last = 56;
        else if (k == CK_POINT) first = last = 51;
        else if (k == CK_GUIDE) { first = 8; last = 9; }
        else if (k == CK_EYE || k == CK_MOUTH) last = 7;
        else if (k == CK_AUP) first = last = 55;
        else if (k == CK_ADOWN) first = last = 54;
        else if (k == CK_MSG) last = 5;
        else if (k >= CK_PLATE0 && k < CK_PLATE5) { first = 16 + 5 * (k - CK_PLATE0); last = first + 4; }
        else if (k == CK_PLATE5) { first = 46; last = 50; }
        else if (k == CK_NUM) { first = 36; last = 45; }
        else if (k == CK_PBOX) first = last = 57;
        else if (k == CK_VBOX) first = last = 58;
        else if (k == CK_FRAME) first = last = 59;
        else if (k == CK_VICON) first = last = 60;
        else if (k == CK_ALEFT) first = last = 53;
        else if (k == CK_ARIGHT) first = last = 52;
        else if (k == CK_VROW) { first = 10; last = 62; }
        else if (k >= CK_VINFO && k < CK_VINFO + 3) first = last = 63;
        else if (k == CK_SLEFT) first = last = 64;
        else if (k == CK_SRIGHT) first = last = 65;
        uint32_t h = cr_hnd[src] && k != CK_ART ? mpm_alloc(c, 0x54) : 0;
        if (h) mpm_call(c, 0x089D1EE8u, h, cr_hnd[src], (uint32_t)first, (uint32_t)last);
        cr_hld[k] = h;
    }
    cr_texts(mpm_lang(c));
    cr_ids.clear();
    for (int b = 0; b < cr_all(); b++) if (cr_game_char(b) >= 0) cr_ids.push_back(b);
    printf("cr: lista con %d de %d personajes (los que tienen modelo en el port)\n", (int)cr_ids.size(), cr_all());
    if ((int)cr_ids.size() == cr_all()) cr_ids.clear();
    cr_row = cr_scroll = cr_pose = 0;
    cr_goto(0);
    for (int k = 0; k < 5; k++) cr_rowst[k] = -1;
    mpm_state(c, cr_hld[CK_BG], 0);
    mpm_state(c, cr_hld[CK_MAIN], 0);
    mpm_state(c, cr_hld[CK_GUIDE], 8);
    mpm_state(c, cr_hld[CK_EYE], 0);
    mpm_state(c, cr_hld[CK_MOUTH], 2);
    mpm_state(c, cr_hld[CK_BAR], 56);
    mpm_state(c, cr_hld[CK_POINT], 51);
    mpm_state(c, cr_hld[CK_AUP], 55);
    mpm_state(c, cr_hld[CK_ADOWN], 54);
    mpm_state(c, cr_hld[CK_MSG], 0);
    for (int k = 0; k < 4; k++) mpm_state(c, cr_hld[CK_PLATE0 + k], 16 + 5 * k);
    mpm_state(c, cr_hld[CK_PLATE5], 46);
    mpm_state(c, cr_hld[CK_NUM], 36);
    mpm_state(c, cr_hld[CK_PBOX], 57);
    mpm_state(c, cr_hld[CK_VBOX], 58);
    if (mpm_has_state(cr_hld[CK_FRAME], 59)) mpm_state(c, cr_hld[CK_FRAME], 59);
    if (mpm_has_state(cr_hld[CK_VICON], 60)) mpm_state(c, cr_hld[CK_VICON], 60);
    mpm_state(c, cr_hld[CK_ALEFT], 53);
    mpm_state(c, cr_hld[CK_ARIGHT], 52);
    if (mpm_has_state(cr_hld[CK_VROW], 62)) mpm_state(c, cr_hld[CK_VROW], 62);   // the viewer's own name plate, else the list row
    else if (mpm_has_state(cr_hld[CK_VROW], 12)) mpm_state(c, cr_hld[CK_VROW], 12);
    for (int k = 0; k < 3; k++) if (mpm_has_state(cr_hld[CK_VINFO + k], 63)) mpm_state(c, cr_hld[CK_VINFO + k], 63);
    if (mpm_has_state(cr_hld[CK_SLEFT], 64)) mpm_state(c, cr_hld[CK_SLEFT], 64);   // small arrows of the viewer (Options style)
    if (mpm_has_state(cr_hld[CK_SRIGHT], 65)) mpm_state(c, cr_hld[CK_SRIGHT], 65);
    cr_menu = cr_comm = cr_costume = cr_pscroll = 0;
    cr_line = 0; cr_idle = 0; cr_last = -1;
    cr_vstop(); cr_vreq = 0;
    // TTT header (codes 10/11, title 12 + 1 + language) and button bar (nested pak 9), the game's own classes as in the multiplayer menu
    uint32_t lang = mpm_lang(c), t = cr_titles();
    psp_s32(t, 12); psp_s32(t + 4, 12 + 1 + lang); psp_s32(t + 8, 0); psp_s32(t + 12, 0);
    bool tttp = hud_has(pak, 16);
    cr_hdr = tttp ? mpm_alloc(c, 0x1FC) : 0;
    if (cr_hdr) { mpm_call(c, 0x08A2D7B4u, cr_hdr, pak, 10, 11, t, 1); mpm_call(c, 0x089D0310u, w, cr_hdr, 0x80, 0); mpm_call(c, 0x08A2E0BCu, cr_hdr, 0, 0); }
    uint32_t nt = cr_navt();
    psp_s32(nt, 6); psp_s32(nt + 4, 1); mpm_wf(nt + 8, 1.0f);
    psp_s32(nt + 12, 5); psp_s32(nt + 16, 0); mpm_wf(nt + 20, 1.0f);
    cr_nav = tttp ? mpm_alloc(c, 0x25C) : 0;
    if (cr_nav) { mpm_call(c, 0x08A104B4u, cr_nav, pak, 9); mpm_call(c, 0x089D0310u, w, cr_nav, 0x80, 0); mpm_call(c, 0x08A10CFCu, cr_nav, cr_slot(c, cr_hld[CK_BG], CS_GUIDE_BAR), 2, nt); }
    cr_vnav = hud_has(pak, 24) ? mpm_alloc(c, 0x25C) : 0;
    if (cr_vnav) {
        mpm_call(c, 0x08A104B4u, cr_vnav, pak, 24);
        mpm_call(c, 0x089D0310u, w, cr_vnav, 0x80, 0);
        cr_vnav_cur.clear();   // no entries outside the viewer
        cr_vtxt[0] = -2;   // texts read once its pak is in (cr_vnav_ok)
    }
    printf("cr: barra del visor %s, placa del nombre %s, placas de datos %s\n", cr_vnav ? "si (entrada 24)" : "no (pak sin la entrada 24)",
           mpm_cur_state(cr_hld[CK_VROW]) == 62 ? "propia (estado 62)" : "fila de la lista (estado 12)", mpm_cur_state(cr_hld[CK_VINFO]) == 63 ? "si (estado 63)" : "no");
    uint32_t fbi = 24 + (lang >= 1 && lang <= 3 ? lang : 1);
    cr_loader2 = mpm_alloc(c, 0x28);
    if (cr_loader2) {
        mpm_call(c, 0x089D26A0u, cr_loader2);
        mpm_call(c, 0x089D2828u, cr_loader2, fbi);
        mpm_call(c, 0x089D28A0u, cr_loader2, MPM_NAT + 4 * CRN_PAK2, w);
        psp_s32(w + 0x60, cr_loader2);
    }
    cr_fb = 0; cr_rfor = -1; cr_rhnd = 0;
    mpm_state(c, cr_hld[CK_MAIN], 0);
    cr_phase = CP_ANIM; cr_next_st = 1; cr_next_ph = CP_LIST;
    bool full = mpm_has_state(cr_hld[CK_MAIN], 21) && mpm_has_state(cr_hld[CK_ROW0], 14) && mpm_has_state(cr_hld[CK_BG], 0);
    printf("cr: piezas con el RHG %u, marco del render %s\n", prhg, mpm_cur_state(cr_hld[CK_FRAME]) == 59 ? "si (estado 59)" : "no (pak sin el estado 59)");
    printf("cr: pak cargado 0x%08X (entrada %d), handles fondo 0x%08X principal 0x%08X piezas 0x%08X, estados %s, %d personajes\n", pak, psp_cr_pak(),
           cr_hnd[0], cr_hnd[1], cr_hnd[2], full ? "completos" : "incompletos", cr_count());
    printf("cr: cabecera 0x%08X, barra de botones 0x%08X%s (slot guide del fondo 0x%08X), ventana de mensaje 0x%08X\n", cr_hdr, cr_nav, tttp ? "" : " (pak sin las entradas 9..16)", cr_slot(c, cr_hld[CK_BG], CS_GUIDE_BAR), cr_hnd[3]);
}

static void cv_term(const Ctx& c);
static int cv_owner();
static void cr_destroy(const Ctx& c) {
    if (!cr_w) return;
    if (cr_vplay && psp_l32(CR_SND)) mpm_call(c, 0x0883A5A4u, psp_l32(CR_SND), 1);
    cr_vstop(); cr_vkill = false; cr_talk = false;
    cr_music(c, false);
    uint32_t w = cr_w;
    mpm_call(c, 0x089CFD08u, mpm_root(), w);
    for (uint32_t ch : {cr_hdr, cr_nav}) {
        if (!ch) continue;
        mpm_call(c, 0x089CFD08u, w, ch);
        uint32_t v = psp_l32(ch + 0x40) + 8;
        mpm_call(c, psp_l32(v + 4), ch + (uint32_t)(int32_t)(int16_t)psp_l16(v), 3);
    }
    cr_hdr = cr_nav = 0;
    if (cr_vnav) { mpm_call(c, 0x089CFD08u, w, cr_vnav); uint32_t v = psp_l32(cr_vnav + 0x40) + 8; mpm_call(c, psp_l32(v + 4), cr_vnav + (uint32_t)(int32_t)(int16_t)psp_l16(v), 3); }
    cr_vnav = 0;
    for (int k = 0; k < CK_N; k++) { if (cr_hld[k]) mpm_call(c, 0x089D248Cu, cr_hld[k], 3); cr_hld[k] = 0; }
    for (int i = 0; i < 4; i++) { if (cr_hnd[i]) mpm_call(c, 0x08840640u, 0x08B48200u, cr_hnd[i]); cr_hnd[i] = 0; }
    if (cr_rhnd) mpm_call(c, 0x08840640u, 0x08B48200u, cr_rhnd);
    cr_rhnd = 0; cr_rfor = -1; cr_fb = 0;
    if (cr_loader2) mpm_call(c, 0x089D27D0u, cr_loader2, 3);
    cr_loader2 = 0;
    if (cr_loader) mpm_call(c, 0x089D27D0u, cr_loader, 3);
    psp_s32(w + 0x60, 0);
    mpm_call(c, 0x089D1590u, w, 3);
    cr_w = cr_loader = 0;
    cr_phase = CP_LOAD;
    if (cv_owner() == 1) cv_term(c);
    uint32_t mgr = psp_l32(0x08A7EA50u);
    if (mgr) psp_s32(mgr, 0);
    cr_ret = true;   // the scene manager rebuilds the current scene (main menu) with its own entry
    printf("cr: referencia de personajes cerrada, el menu principal se vuelve a crear (escena %u, pagina %u)\n", mgr ? psp_l32(mgr + 0x10) : 0, mgr ? psp_l32(mgr + 0x14) : 0);
}

// ---- 3D viewer, as BT3's ChrView: the battle's object, stage and scene modules without a battle, inside this screen.
// The map (Menus/chara_reference_map) is read once; each model is only its colour pak, loaded while the previous one stays shown
static const uint32_t CV_W = 0x08B48A20u, CV_R = 0x08B41A08u;   // battle work, battle resources (map buffer +0xC, size +0x3C)
struct CrView {
    bool on = false, map_wait = false, visible = false, want = false, framed = false, back = false;
    int owner = 0, sys = -1;           // 1 Character Reference, 2 character select (33); character shown
    uint32_t mapbuf = 0, mapsz = 0, buf[2] = {0, 0}, rec[2] = {0, 0}, cam = 0;
    int obj[2] = {-1, -1}, cur = -1, pend = -1, bt3 = 0, colour = 0, ncol = 1, dirs = 0, edge = 0, wheel = 0;
    int dmg = 0, face = -1;            // damage shown (0 none, 1 textures, 2 damaged model), extra face shown (-1 the model's own)
    bool has1 = false, has2 = false, hasb = false;   // this colour has level 1 textures / a level 2 model / a blink texture
    uint32_t ctrl = 0;                 // face control block of the port (the object has none)
    float ax = 0, ay = 0, zoom = 0, yaw = 0, pitch = 0.15f, zf = 1.0f, h = 20.0f, cy = -10.0f, ang[2] = {0, 0};
    uint64_t t = 0;
};
static CrView cv;
static int cv_owner() { return cv.on ? cv.owner : 0; }
static int cv_guard = 0;
bool psp_cr_viewer() { return cv.visible || (cv.on && cv.owner == 3); }
static bool cv_is(uint32_t ent);
static bool cv_lv1(uint32_t ent);
static bool cv_blink(uint32_t ent);

// BT3 id of the list -> character of the game (originals, mod characters, renamed originals); -1 = no model in the port
static int cr_game_char(int b) {
    if (b < 0 || b >= roster::SYS) return -1;
    for (auto& r : roster::defs().renames) if (r.newid == b && r.sysid != b) return r.sysid;
    return roster::vanilla(b) || roster::find(b) ? b : -1;
}
static int cr_ncol(int b) { int s = cr_game_char(b), n = s >= 0 ? psp_col_count(s) : 0; return n > 0 ? n : 1; }
// forms = the list entries with the same name that have a model
static int cr_form(int b, int d) {
    int n = (int)cr_tx.name.size();
    if (b >= (int)cr_tx.name.size()) return b;
    for (int k = 1; k < n; k++) {
        int i = ((b + d * k) % n + n) % n;
        if (i < (int)cr_tx.name.size() && cr_tx.name[i] == cr_tx.name[b] && cr_game_char(i) >= 0) return i;
    }
    return b;
}
static float cv_rf(uint32_t a) { uint32_t v = psp_l32(a); float f; memcpy(&f, &v, 4); return f; }
static void cv_wf(uint32_t a, float f) { uint32_t v; memcpy(&v, &f, 4); psp_s32(a, v); }
static uint32_t cv_objp(int id) { uint32_t b = psp_l32(0x08A7E278u), o = b ? b + 0x16E0u * (uint32_t)id + 0x10u : 0; return o && id >= 0 && (int)psp_l32(o + 0xC) == id ? o : 0; }

static bool cv_is(uint32_t ent) { return cv.visible && cv.cur >= 0 && ent && ent == cv_objp(cv.obj[cv.cur]); }
static bool cv_lv1(uint32_t ent) { return cv.dmg == 1 && cv_is(ent); }
// mouth level for the lip sync of the viewer model while its sample plays (-2 = not the viewer, the normal rule)
static int cv_lip(uint32_t ent) {
    if (!cv_is(ent)) return -2;
    return cv.face < 0 && cr_vkind == 2 && (cr_vplay || !cr_vname.empty()) ? psp_lip_dir("menus/chara_reference_samples") : -1;
}
static bool cv_blink(uint32_t ent) { return cv.face == 8 && cv_is(ent); }   // eyes held closed: the port's blink texture swap (marker 0xE3 bit 10)
static void cv_flags() {   // battle flags as the modules read them: no pause (0x100), not loading (0x2000)
    psp_s32(CV_W + 0x5C60, psp_l32(CV_W + 0x5C60) & ~0x2100u);
    psp_s32(CV_W + 0x24, 0); psp_s32(CV_W + 0x2C, 0);
}

static void cv_init(const Ctx& c, int map = -2, int owner = 1) {
    if (cv.on) return;
    static uint32_t cam = 0, rec[2] = {0, 0}, ctrl = 0;
    if (!cam) { cam = psp_reserve(0x2A0); rec[0] = psp_reserve(0x40); rec[1] = psp_reserve(0x40); ctrl = psp_reserve(0x40); }
    cv = CrView();
    cv.on = true; cv.cam = cam; cv.rec[0] = rec[0]; cv.rec[1] = rec[1]; cv.ctrl = ctrl; cv.owner = owner;
    cv_flags();
    mpm_call(c, 0x089AE428u, 0, 0);   // objects (BtlObj_Init, no model buffers)
    mpm_call(c, 0x089AC58Cu);         // cameras (template for the port's own)
    mpm_call(c, 0x0887B228u, 0);
    mpm_call(c, 0x0887B340u, 0, 0, 0); // scene, small arenas (as the story field does)
    mpm_call(c, 0x089BDD88u);         // stage effects
    int m = map == -2 ? psp_cr_map() : map;
    uint32_t msz = m >= 0 ? mpm_call(c, 0x08831CACu, (uint32_t)m) : 0;
    if (msz > psp_map_buf_size()) { printf("cr: visor: el mapa del visor mide %u bytes, mas que el limite de un mapa (%u): sin mapa\n", msz, psp_map_buf_size()); fflush(stdout); m = -1; }
    if (m >= 0) {
        cv.mapsz = mpm_call(c, 0x08831CACu, (uint32_t)m);
        cv.mapbuf = mpm_call(c, 0x08852854u, (uint32_t)m, 0, 0);
        cv.map_wait = cv.mapbuf != 0;
    }
    printf("cr: visor iniciado (objetos 0x%08X, mapa entrada %d, %u bytes)\n", psp_l32(0x08A7E278u), m, cv.mapsz); fflush(stdout);
}

// pak buffers kept by another owner (the character select's cache) are not freed with their object
static bool (*cv_keep)(uint32_t buf) = nullptr;
static uint32_t (*cv_cached)(uint32_t idx) = nullptr;
static void (*cv_cache_put)(uint32_t idx, uint32_t buf) = nullptr;
static void (*cv_predraw)(uint32_t obj) = nullptr;
static void (*cv_objmark)(Ctx& c, bool before) = nullptr;
static void (*cv_dropmark)(uint32_t buf) = nullptr;   // a kept pak whose object was just ended (the game tears it down in later frames)   // around the object draw (the character select hides it while it measures the pose)
// pak buffers of dropped objects are freed some frames later: the game ends an object after the call, a freed pak reused at once got corrupted
struct CvLate { uint32_t buf; uint32_t frame; };
static std::vector<CvLate> cv_late;
static uint32_t cv_frame = 0;
static int cv_late_n() { static int v = -1; if (v < 0) { const char* e = getenv("TTT_CV_LATE_FREE"); v = e && *e ? atoi(e) : 0; } return v; }
static void cv_free(const Ctx& c, uint32_t buf) {
    if (!buf) return;
    if (cv_late_n() <= 0) { mpm_call(c, 0x089BFDB0u, buf); return; }
    for (auto& e : cv_late) if (e.buf == buf) return;
    cv_late.push_back({buf, cv_frame});
}
static void cv_free_late(const Ctx& c, bool all) {
    size_t j = 0;
    for (auto& e : cv_late) {
        bool live = e.buf == cv.buf[0] || e.buf == cv.buf[1] || (cv_keep && cv_keep(e.buf));
        if (live) continue;
        if (all || cv_frame - e.frame >= (uint32_t)cv_late_n()) mpm_call(c, 0x089BFDB0u, e.buf);
        else cv_late[j++] = e;
    }
    cv_late.resize(all ? 0 : j);
}
static void cv_drop(const Ctx& c, int k) {
    if (cv.obj[k] >= 0) mpm_call(c, 0x089AE1FCu, (uint32_t)cv.obj[k]);
    if (cv.buf[k] && !(cv.owner == 2 && cv_keep && cv_keep(cv.buf[k]))) cv_free(c, cv.buf[k]);
    else if (cv.buf[k] && cv_dropmark) cv_dropmark(cv.buf[k]);
    cv.obj[k] = -1; cv.buf[k] = 0;
    if (cv.cur == k) cv.cur = -1;
}
static bool cv_busy() { return cv.map_wait || cv.pend >= 0; }

static void cv_hide(const Ctx& c) {
    cv.want = false; cv.visible = false;
    if (cv.cur >= 0) cv_drop(c, cv.cur);
}

static void cv_term(const Ctx& c) {
    if (!cv.on) return;
    cv_hide(c);
    cv_free_late(c, true);
    mpm_call(c, 0x0887B770u);
    if (psp_l32(0x08A7E678u)) mpm_call(c, 0x089B733Cu);
    psp_s32(CV_R + 0xC, 0); psp_s32(CV_R + 0x3C, 0);
    if (cv.mapbuf) mpm_call(c, 0x089BFDB0u, cv.mapbuf);
    mpm_call(c, 0x089BDE00u);
    mpm_call(c, 0x089AC5C8u);
    mpm_call(c, 0x089AE4F0u);
    cv.on = false; cv.mapbuf = 0;
    printf("cr: visor cerrado\n"); fflush(stdout);
}

// load the colour pak of a BT3 id; it replaces the model on screen when it is ready
// dmg 2 loads the colour's damaged model (1_p_dmg entry) when it has one
static void cv_show_sys(const Ctx& c, int s, int colour, int dmg, int b);
static void cv_show(const Ctx& c, int b, int colour, int dmg = 0) {
    int s = cr_game_char(b);
    if (s < 0 || cv.pend >= 0) return;
    cv_init(c);
    cv_show_sys(c, s, colour, dmg, b);
}
static void cv_show_sys(const Ctx& c, int s, int colour, int dmg, int b) {
    if (s < 0 || cv.pend >= 0 || !cv.on) return;
    int nc = psp_col_count(s), ncol = nc > 0 ? nc : 1, col = std::max(0, std::min(colour, ncol - 1));
    uint32_t idx = mpm_call(c, 0x089C6CA0u, (uint32_t)s, (uint32_t)col) & 0xFFFFu, d = 0;
    bool has2 = psp_disc_dmg(idx, &d);
    int dm = dmg == 2 && has2 ? 2 : 0;
    uint32_t ridx = dm == 2 ? d : idx, hit = cv.owner == 2 && cv_cached ? cv_cached(ridx) : 0;
    static uint32_t w_idx = 0xFFFFFFFFu, w_f0 = 0;
    if (!hit && cv.owner == 2 && patch_sel_wait.val && !psp_disc_held(ridx)) {   // select: the pak is built by the hold worker, the screen never waits for a build
        if (w_idx != ridx) { w_idx = ridx; w_f0 = cv_frame; psp_disc_hold(ridx, true, true); }
        if (cv_frame - w_f0 < 600) return;   // a build already running in the worker comes first (BT3 colours take 1-2.5 s)
        static int logs = 0;
        if (logs++ < 10) { printf("cr: visor: entrada %u sin preparar tras 600 cuadros, se lee igual\n", ridx); fflush(stdout); }
    }
    w_idx = 0xFFFFFFFFu;
    cv.bt3 = b; cv.sys = s; cv.ncol = ncol; cv.colour = col; cv.want = true;
    int k = cv.cur == 0 ? 1 : 0;
    cv_drop(c, k);
    cv.has2 = has2; cv.has1 = psp_disc_lv1(idx); cv.hasb = psp_disc_blink(idx);
    cv.dmg = dm;
    if (cv.has2 && cv.owner == 1) psp_disc_prebuild(cv.dmg == 2 ? idx : d);   // the other variant ready for the first toggle
    if (cv.dmg == 2) idx = d;
    uint32_t sz = mpm_call(c, 0x08831CACu, idx);
    cv.buf[k] = hit ? hit : mpm_call(c, 0x08852854u, idx, 0, 0);
    if (!cv.buf[k]) { printf("cr: visor: no se pudo leer la entrada %u\n", idx); fflush(stdout); return; }
    if (!hit && cv.owner == 2 && cv_cache_put) cv_cache_put(idx, cv.buf[k]);
    for (uint32_t o = 0; o < 0x40; o += 4) psp_s32(cv.rec[k] + o, 0);
    psp_s32(cv.rec[k], cv.buf[k]); psp_s32(cv.rec[k] + 4, sz); psp_s32(cv.rec[k] + 8, idx);
    cv.pend = k;
    printf("cr: visor: id BT3 %d (personaje %d) color %d de %d, entrada %u, %u bytes, rotura nivel 1 %s, nivel 2 %s, parpadeo %s%s\n", b, s, cv.colour + 1, cv.ncol, idx, sz,
           cv.has1 ? "si" : "no", cv.has2 ? "si" : "no", cv.hasb ? "si" : "no", cv.dmg == 2 ? " (traje roto cargado)" : ""); fflush(stdout);
}

static void cv_created(const Ctx& c, int k) {
    cv_flags();
    printf("cr: visor: creando el objeto (pak 0x%08X, %u bytes)\n", psp_l32(cv.rec[k]), psp_l32(cv.rec[k] + 4)); fflush(stdout);
    int id = (int)mpm_call(c, 0x089AE0A4u, 2, cv.rec[k], 1, 0xFFFFFFFFu);
    uint32_t o = cv_objp(id);
    if (!o) { printf("cr: visor: no se pudo crear el objeto (id %d)\n", id); fflush(stdout); cv_drop(c, k); return; }
    cv.obj[k] = id;
    uint32_t an = psp_l32(o + 0x738 + 4 * 7);
    if (!an || !psp_l16(an + 2)) {   // no 035_stand_loop.anm in this pak: the mod's default one
        static uint32_t def = 0;
        if (!def) {
            std::string f = psp_cr_file("CharRef/stand_loop.anm");
            if (f.empty()) f = "../../games/ttt/select3d/stand_loop.anm";
            FILE* fp = fopen(f.c_str(), "rb");
            if (fp) {
                fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
                if (n > 4 && n < 0x100000) { def = psp_reserve((uint32_t)n + 64); if (fread(psp_p(def), 1, (size_t)n, fp) != (size_t)n) def = 0; }
                fclose(fp);
            }
        }
        printf("cr: visor: el personaje no tiene 035_stand_loop.anm, %s\n", def ? "se usa el stand_loop.anm por defecto" : "y no hay stand_loop.anm por defecto"); fflush(stdout);
        if (def) psp_s32(o + 0x738 + 4 * 7, def);
    }
    printf("cr: visor: objeto creado, animacion 0x%08X\n", psp_l32(o + 0x738 + 4 * 7)); fflush(stdout);
    if (psp_l32(o + 0x738 + 4 * 7)) mpm_call(c, 0x089B14C8u, o, 7, 2);   // 035_stand_loop, looping (BT3 plays model animation 7)
    float sp = cv_rf(o + 0xCA0);
    cv_wf(o + 0xCA0, sp * 0.5f);         // the battle steps objects at 30 Hz, this screen at 60
    bool keep = cv.owner == 1 && cv.cur >= 0 && cv.framed;   // Character Reference: a colour or form change inside the view keeps the camera
    if (cv.cur >= 0 && cv.cur != k) cv_drop(c, cv.cur);
    cv.cur = k; cv.framed = keep; cv.face = -1;
    uint32_t pm = psp_l32(o + 0x14);
    printf("cr: visor: objeto %d en 0x%08X, pMdl 0x%08X, %u huesos, velocidad %.2f\n", id, o, pm, pm ? psp_l32(pm + 8) : 0, sp); fflush(stdout);
    if (!cv.want) cv_hide(c);
    else cv.visible = true;
}

// TTT_HEAP_CHECK=1: walks the game heap (newlib chunks) once per frame and reports the first broken chunk, with what the port knows
bool psp_heap_arena(uint32_t* a, uint32_t* e);
const char* psp_io_last(uint32_t* dst, uint32_t* bytes, long* frame);
void psp_hle_recent(int per_thread);
static void heap_check() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_HEAP_CHECK"); on = e && *e ? atoi(e) : 0; }
    if (!on) return;
    static int reports = 0;
    static uint32_t last_bad = 0;
    if (reports >= 3) return;
    uint32_t a, e;
    if (!psp_heap_arena(&a, &e)) return;
    uint32_t p = a, q = 0, qsz = 0, n = 0;
    if ((psp_l32(p + 4) & ~7u) == 0 && (psp_l32(p + 12) & ~7u) >= 16) p = a + 8;   // newlib puts the first chunk 8 bytes in
    const char* why = nullptr;
    while (p + 8 <= e && n < 400000) {
        uint32_t szf = psp_l32(p + 4), sz = szf & ~7u;
        if (sz == 0 || (sz < 16 && psp_l32(p) == 0)) {
            bool zeros = true;
            for (uint32_t o = 0; o < 4096 && p + o + 4 <= e && zeros; o += 4) if (psp_l32(p + o)) zeros = false;
            if (zeros) break;   // beyond the heap top
            why = "tamano cero"; break;
        }
        if (sz < 16) { why = "tamano menor que 16"; break; }
        if (p + sz > e) { why = "pasa del final del monton"; break; }
        if (p + sz + 8 <= e && !(psp_l32(p + sz + 4) & 1) && psp_l32(p + sz) != sz) { why = "pie del bloque libre distinto"; break; }
        q = p; qsz = sz; p += sz; n++;
    }
    { static bool once = false; if (!once) { once = true; printf("heap: monton 0x%08X-0x%08X, %u bloques, tope 0x%08X%s%s\n", a, e, n, p, why ? ", roto: " : "", why ? why : ""); } }
    if (!why || p == last_bad) return;
    last_bad = p; reports++;
    printf("heap: bloque roto en 0x%08X (%s): cabecera %08X %08X, tras %u bloques, cuadro %u\n", p, why, psp_l32(p), psp_l32(p + 4), n, cv_frame);
    if (q) {
        printf("heap:   bloque anterior 0x%08X de %u bytes%s, datos en 0x%08X:", q, qsz, psp_l32(q + 4) & 1 ? "" : " (libre)", q + 8);
        for (int k = 0; k < 8; k++) printf(" %08X", psp_l32(q + 8 + 4u * k));
        printf("\n");
        for (int k = 0; k < 2; k++) if (cv.buf[k] && cv.buf[k] >= q + 8 && cv.buf[k] < p + 8) printf("heap:   cv.buf[%d] = 0x%08X vive en ese bloque\n", k, cv.buf[k]);
    }
    printf("heap:   datos en el bloque roto 0x%08X:", p);
    for (int k = 0; k < 8; k++) printf(" %08X", psp_l32(p + 4u * k));
    printf("\n");
    uint32_t dst, bytes; long fr;
    const char* nm = psp_io_last(&dst, &bytes, &fr);
    printf("heap:   ultima lectura: %s, %u bytes a 0x%08X (hasta 0x%08X), cuadro %ld\n", nm, bytes, dst, dst + bytes, fr);
    printf("heap:   visor: cur %d pend %d buf0 0x%08X buf1 0x%08X obj0 %d obj1 %d, %d liberaciones pendientes\n", cv.cur, cv.pend, cv.buf[0], cv.buf[1], cv.obj[0], cv.obj[1], (int)cv_late.size());
    psp_hle_recent(10);
    fflush(stdout);
}

// once per frame from the pad read: requests, input
static void cv_tick(const Ctx& c) {
    if (!cv.on) return;
    cv_frame++;
    heap_check();
    if (!cv_late.empty()) cv_free_late(c, false);
    if (cv_busy() && mpm_call(c, 0x08852768u) == 1) {
        if (cv.map_wait) {
            cv.map_wait = false;
            psp_s32(CV_R + 0xC, cv.mapbuf); psp_s32(CV_R + 0x3C, cv.mapsz);
            printf("cr: visor: mapa leido (0x%08X), preparando el escenario\n", cv.mapbuf); fflush(stdout);
            mpm_call(c, 0x08804DA0u);   // StgModel_InitStage: the stage object lives in the map buffer
            printf("cr: visor: mapa listo (escenario 0x%08X)\n", psp_l32(0x08A7E678u)); fflush(stdout);
        }
        if (cv.pend >= 0) { int k = cv.pend; cv.pend = -1; cv_created(c, k); }
    }
    int wasd = 0, dirs = 0, wheel = 0;
    psp_input_viewer(&cv.ax, &cv.ay, &cv.zoom, &wheel, &wasd, &dirs);
    cv.wheel += wheel;
    cv.edge |= dirs & ~cv.dirs;
    cv.dirs = dirs;
}

// camera angles (+0x270) toward ctr: formula 4 of the split screen search, the one the battle test chose (pitch negated, yaw atan2(dx, dz))
static void cv_aim(Ctx& c, const float* eye, const float* ctr) {
    float dx = ctr[0] - eye[0], dy = ctr[1] - eye[1], dz = ctr[2] - eye[2];
    cv.ang[0] = -atan2f(dy, sqrtf(dx * dx + dz * dz)); cv.ang[1] = atan2f(dx, dz);
    cv_wf(cv.cam + 0x270, cv.ang[0]); cv_wf(cv.cam + 0x274, cv.ang[1]); cv_wf(cv.cam + 0x278, 0.0f);
    cl_call(c, 0x089AC4B8u, cv.cam, 1, 0, 0, 0, 0);
    static int logs = 0;
    if (logs++ < 1) { float v[3]; lmp_view_pt(cv.cam, ctr, v); printf("cr: visor: centro en la vista (%.2f, %.2f, %.2f)\n", v[0], v[1], v[2]); fflush(stdout); }
}

static void cr_text_at(const Ctx& c, uint32_t font, int x, int y, float s, uint32_t str, uint32_t col);
static void cr_text(const Ctx& c, uint32_t font, uint32_t slot, uint32_t str, uint32_t col);
static bool cv_faces(uint32_t o) {
    for (int f = 0; o && f < 8; f++) if (psp_l32(o + 0x54 + 4 * (uint32_t)f)) return true;
    return cv.hasb;
}
// the viewer bar is hidden in the tree's draw (root holder +0x80, visible byte +0x15); only the viewer shows it
static void cv_bar_show(bool on) { uint32_t h = cr_vnav ? psp_l32(cr_vnav + 0x80) : 0; if (h) psp_s8(h + 0x15, on ? 1 : 0); }
// the viewer bar's texts (states named cr_rotate ... cr_voice in its text AGS), read once its nested pak is in
static bool cr_vnav_ok() {
    if (!cr_vnav) return false;
    if (cr_vtxt[0] == -2) {
        bool any = false;
        for (int k = 0; k < 22; k++) if (psp_l32(cr_vnav + 0xB0 + 4 * (uint32_t)k)) any = true;
        static int wait = 0;
        if (!any) { if (++wait == 600) { printf("cr: la barra del visor no tiene textos tras 600 fotogramas\n"); fflush(stdout); } return false; }
        static const char* N[7] = {"cr_rotate", "cr_zoom", "cr_colour", "cr_face", "cr_form", "cr_damage", "cr_voice"};
        for (int i = 0; i < 7; i++) cr_vtxt[i] = -1;
        for (int k = 0; k < 22; k++) {
            uint32_t h = psp_l32(cr_vnav + 0xB0 + 4 * (uint32_t)k), list = h ? psp_l32(h + 4) : 0, n = h ? psp_l32(h + 0xC) : 0;
            for (uint32_t j = 0; list && j < n && j < 64; j++) {
                std::string nm = cr_state_name(h, (int)psp_l32(list + 8 * j));
                for (int i = 0; i < 7; i++) if (nm == N[i] && cr_vtxt[i] < 0) cr_vtxt[i] = k;
            }
        }
        printf("cr: textos de la barra del visor: girar %d, zoom %d, color %d, cara %d, forma %d, rotura %d, voz %d\n", cr_vtxt[0], cr_vtxt[1], cr_vtxt[2], cr_vtxt[3], cr_vtxt[4], cr_vtxt[5], cr_vtxt[6]);
        fflush(stdout);
    }
    for (int i = 0; i < 7; i++) if (cr_vtxt[i] < 0) return false;
    return true;
}
// its entries, only what this character has (entry 0 is drawn rightmost); once per frame outside the walks, plus its animation
static void cr_vnav_frame(const Ctx& c) {
    if (!cr_vnav) return;
    cv_bar_show(false);
    if (!cr_vnav_ok()) return;
    std::vector<uint32_t> t;
    auto add = [&](int ic, int tx) { t.push_back((uint32_t)ic); t.push_back((uint32_t)tx); t.push_back(0x3F800000u); };
    if (!(cr_phase == CP_VIEW && cv.visible)) {
        if (!cr_vnav_cur.empty()) { cr_vnav_cur.clear(); mpm_call(c, 0x08A10CFCu, cr_vnav, cr_slot(c, cr_hld[CK_BG], CS_GUIDE_BAR), 0, cr_navt() + 0x18); }
        return;
    }
    uint32_t o = cv.cur >= 0 ? cv_objp(cv.obj[cv.cur]) : 0;
    add(6, 1);
    if (cv.has1 || cv.has2) add(5, cr_vtxt[5]);
    if (cv.face < 0 && psp_cr_voice(cv_sample_name(cv.bt3, 0)) >= 0) add(7, cr_vtxt[6]);
    if (cr_form(cv.bt3, 1) != cv.bt3) add(4, cr_vtxt[4]);
    if (cv_faces(o)) add(9, cr_vtxt[3]);
    if (cv.ncol > 1) add(10, cr_vtxt[2]);
    add(13, cr_vtxt[1]);
    add(11, cr_vtxt[0]);
    if (t != cr_vnav_cur) {
        bool first = cr_vnav_cur.empty();
        cr_vnav_cur = t;
        static uint32_t tb = 0;
        if (!tb) tb = psp_reserve(0x60);
        for (size_t i = 0; i < t.size() && i < 24; i++) psp_s32(tb + 4 * (uint32_t)i, t[i]);
        uint32_t slot = cr_slot(c, cr_hld[CK_BG], CS_GUIDE_BAR);
        mpm_call(c, first ? 0x08A10CFCu : 0x08A10DE8u, cr_vnav, slot, (uint32_t)(t.size() / 3), tb);
    }
}

// a part of the screen pak drawn at a point of the screen with a scale (no slot)
static void cv_part(const Ctx& c, int k, int x, int y, float sc, float sy = -1.0f) {
    uint32_t h = cr_hld[k];
    if (!h || !psp_l32(h)) return;
    uint32_t keep[0x54 / 4];
    for (uint32_t i = 0; i < 0x54 / 4; i++) keep[i] = psp_l32(h + 4 * i);
    psp_s32(h + 8, 0); psp_s32(h + 0x18, (uint32_t)x); psp_s32(h + 0x1C, (uint32_t)y);
    mpm_wf(h + 0x20, sc); mpm_wf(h + 0x24, sy < 0.0f ? sc : sy);
    for (int i = 0; i < 4; i++) mpm_wf(h + 0x40 + 4 * i, 1.0f);
    mpm_draw_holder(c, h);
    for (uint32_t i = 2; i < 0x54 / 4; i++) if (i != 0x10 / 4 && i != 0x14 / 4) psp_s32(h + 4 * i, keep[i]);   // its state and frame stay as they ran
}

// HUD of the viewer over the 3D: name plate, data plates (colour, damage, face), animated arrows when there are more colours or faces,
// and the viewer's own button bar (pak entry 24), else the controls as text
static void cv_hud(Ctx& c) {
    uint32_t font = psp_l32(0x08A78EECu);
    if (!font) return;
    bool es = mpm_lang(c) == 2, pad = psp_input_pad();
    uint32_t list = mpm_call(c, 0x08838054u, 0);
    mpm_call(c, 0x08A6495Cu, 0, 0, 480, 272);
    bool zt = (psp_l32(0x08B47888u + 0x44) & 2) != 0;   // the 3D leaves the depth test on: the map would hide the 2D
    mpm_call(c, 0x08A64240u, 1);
    int b = cv.bt3, buf = 0;   // cr_draw is off in the view: its text buffers are free (0..35 only, 0x2800 holds the slot names)
    uint32_t o = cv.cur >= 0 ? cv_objp(cv.obj[cv.cur]) : 0;
    bool own = mpm_cur_state(cr_hld[CK_VROW]) == 62, plates = mpm_cur_state(cr_hld[CK_VINFO]) == 63, faces = cv_faces(o);
    // one text size for the whole HUD; plates at 60 % height, widened (only in x) when a text needs it; text rects of the pak (canvas units)
    const float TS = 0.6f, SY = 0.6f, TH = 15.0f * TS;
    const uint32_t W = 0xFFFFFFFFu, Y = 0xFF00F0FFu, OR = 0xFF2A9BFFu;
    mpm_call(c, 0x08832448u, font);
    auto width = [&](const std::string& t) { return t.empty() ? 0 : (int)((float)(int)mpm_call(c, 0x08831F68u, font, cr_u16(t, 19)) * TS); };
    auto put = [&](const std::string& t, float x, float rowy, float rowh, uint32_t col) {
        if (t.empty() || buf >= 18) return;
        cr_text_at(c, font, (int)x, (int)(rowy + (rowh - TH) * 0.5f), TS, cr_u16(t, buf++), col);
    };
    mpm_call(c, 0x08832500u, font);
    const int px = 6, py = 4;
    const std::string nm = b < (int)cr_tx.name.size() ? cr_tx.name[b] : "", fm = b < (int)cr_tx.form.size() ? cr_tx.form[b] : "";
    mpm_call(c, 0x08832448u, font);
    int nw = width(nm), fw = width(fm);
    mpm_call(c, 0x08832500u, font);
    float ph = own ? 40.0f * SY : 67.0f * 0.45f;
    // data lines: Color, Traje dañado (always), Cara (when the model has faces); same plate width for all, label and value columns
    static const char* FES[9] = {"Da\xC3\xB1" "o", "Hablar 1", "Hablar 2", "Hablar 3", "Expresi\xC3\xB3n 1", "Expresi\xC3\xB3n 2", "Expresi\xC3\xB3n 3", "Cara extra", "Parpadeo"};
    static const char* FEN[9] = {"Damage", "Talk 1", "Talk 2", "Talk 3", "Expression 1", "Expression 2", "Expression 3", "Extra face", "Blink"};
    struct Line { std::string l, v; uint32_t col; bool lr; };
    std::vector<Line> ls;
    char t[48];
    snprintf(t, sizeof t, "%d / %d", cv.colour + 1, cv.ncol);
    ls.push_back({"Color", t, Y, false});
    if (cv.dmg) snprintf(t, sizeof t, es ? "Nivel %d" : "Level %d", cv.dmg); else snprintf(t, sizeof t, "No");
    ls.push_back({es ? "Da\xC3\xB1" "o" : "Damage", t, cv.dmg ? OR : Y, false});
    if (faces) ls.push_back({es ? "Cara" : "Face", cv.face >= 0 && cv.face < 9 ? (es ? FES[cv.face] : FEN[cv.face]) : (es ? "Por defecto" : "Default"), Y, true});
    const int AW = 8;   // room for a small arrow on each side of a value
    // one width for every possible text of these lines (per language), so the plates never change size
    static int lw = 0, vw = 0, wlang = -1;
    if (wlang != (int)es) {
        wlang = (int)es; lw = vw = 0;
        mpm_call(c, 0x08832448u, font);
        for (const char* l : {"Color", es ? "Da\xC3\xB1" "o" : "Damage", es ? "Cara" : "Face"}) lw = std::max(lw, width(l));
        for (const char* v : {"10 / 10", "No", es ? "Nivel 2" : "Level 2"}) vw = std::max(vw, width(v));
        for (int f = 0; f < 9; f++) vw = std::max(vw, width(es ? FES[f] : FEN[f]) + 2 * AW);
        vw = std::max(vw, width(es ? "Por defecto" : "Default") + 2 * AW);
        mpm_call(c, 0x08832500u, font);
    }
    const int LP = 5, GAP = 6;   // left padding, gap between the columns, same right padding
    // every plate ends at the same x: the data plates' width, wider only when the name or the form needs it
    float wc = (float)(LP + lw + GAP + vw + LP);
    if (own) wc = std::max(wc, std::max(nw + 16.0f, fw + 26.0f));
    float ix = wc / 150.0f, sx = own ? wc / 200.0f : 0.45f;
    cv_part(c, CK_VROW, px, py, sx, own ? SY : 0.45f);
    mpm_call(c, 0x08832448u, font);
    put(nm, px + 9.0f, py + (own ? 1.5f * SY : 0), own ? 19.0f * SY : ph * 0.5f, W);
    put(fm, px + 20.0f, py + (own ? 20.5f * SY : ph * 0.5f), own ? 19.0f * SY : ph * 0.5f, Y);
    mpm_call(c, 0x08832500u, font);
    float rh = 18.0f * SY, y = py + ph + 3;
    bool small = mpm_cur_state(cr_hld[CK_SLEFT]) == 64 && mpm_cur_state(cr_hld[CK_SRIGHT]) == 65;
    for (size_t i = 0; i < ls.size() && i < 3; i++) {
        const Line& e = ls[i];
        if (plates) cv_part(c, CK_VINFO + (int)i, px, (int)y, ix, SY);
        float lx = px + LP, vx = lx + lw + GAP;
        mpm_call(c, 0x08832448u, font);
        put(e.l, lx, y, rh, W);
        put(e.v, vx + (e.lr ? AW : 0), y, rh, e.col);
        int ew = width(e.v);
        mpm_call(c, 0x08832500u, font);
        if (e.lr) {
            int ay = (int)(y + (rh - 8.0f) * 0.5f);
            if (small) { cv_part(c, CK_SLEFT, (int)vx, ay, 1.0f); cv_part(c, CK_SRIGHT, (int)vx + AW + ew + 2, ay, 1.0f); }
            else { cv_part(c, CK_ALEFT, (int)vx, ay, 0.22f); cv_part(c, CK_ARIGHT, (int)vx + AW + ew + 1, ay, 0.22f); }
        }
        y += rh + 2;
    }
    // more colours: small animated arrows at the very top and bottom of the screen
    if (cv.ncol > 1) { cv_part(c, CK_AUP, 240 - 4, 1, 0.25f); cv_part(c, CK_ADOWN, 240 - 4, cr_vnav_ok() ? 251 - 11 : 272 - 11, 0.25f); }   // above the bar when it is shown
    if (cr_vnav_ok() && !cr_vnav_cur.empty()) { cv_bar_show(true); mpm_call(c, 0x08A11EE0u, cr_vnav); cv_bar_show(false); }
    else {   // controls as text: label white, key orange, right-aligned at the bottom
        mpm_call(c, 0x08832448u, font);
        std::vector<std::pair<std::string, std::string>> hl;
        hl.push_back({es ? "Girar" : "Rotate", pad ? (es ? "stick derecho" : "right stick") : "WASD"});
        hl.push_back({"Zoom", pad ? "L1 / R1" : (es ? "rueda del rat\xC3\xB3n" : "mouse wheel")});
        if (cv.ncol > 1) hl.push_back({"Color", pad ? (es ? "cruceta arriba / abajo" : "d-pad up / down") : (es ? "flechas arriba / abajo" : "up / down arrows")});
        if (faces) hl.push_back({es ? "Cara" : "Face", pad ? (es ? "cruceta izquierda / derecha" : "d-pad left / right") : (es ? "flechas izquierda / derecha" : "left / right arrows")});
        if (cr_form(b, 1) != b) hl.push_back({es ? "Transformaci\xC3\xB3n" : "Transformation", pad ? "L2 / R2" : "Q / E"});
        if (cv.has1 || cv.has2) hl.push_back({es ? "Traje da\xC3\xB1" "ado" : "Damaged costume", pad ? (es ? "confirmar" : "confirm") : (es ? "clic" : "click")});
        if (cv.face < 0 && psp_cr_voice(cv_sample_name(b, 0)) >= 0) hl.push_back({es ? "Voz" : "Voice", pad ? (es ? "Cuadrado / X" : "Square / X") : "F"});
        hl.push_back({es ? "Volver" : "Back", pad ? (es ? "cancelar" : "cancel") : "Esc"});
        const float hs = 0.8f;
        int ly = 266 - 13 * (int)hl.size();
        for (auto& e : hl) {
            if (buf >= 19) break;
            uint32_t uk = cr_u16(e.second, buf++), ul = cr_u16(e.first + ": ", buf++);
            int wk = (int)((float)(int)mpm_call(c, 0x08831F68u, font, uk) * hs), wl = (int)((float)(int)mpm_call(c, 0x08831F68u, font, ul) * hs);
            cr_text_at(c, font, 470 - wk, ly, hs, uk, OR);
            cr_text_at(c, font, 470 - wk - wl, ly, hs, ul, W);
            ly += 13;
        }
        mpm_call(c, 0x08832500u, font);
    }
    static int logs = 0;
    if (logs++ < 1) { printf("cr: visor: placa del nombre %s, placas de datos %s (ancho x%.2f), flechas pequenas %s, barra %s\n", own ? "propia" : "fila", plates ? "si" : "no", ix,
                             small ? "si" : "no (flechas de la pantalla reducidas)", cr_vnav_ok() ? "del visor" : "no (ayudas de texto)"); fflush(stdout); }
    if (zt) mpm_call(c, 0x08A641E8u, 1);
    mpm_call(c, 0x08838120u, list);
    mpm_call(c, 0x08837F54u, 0x08B4E690u, list, 0);
}

// after the scene draw, before the frame is composed (0x0880BBF4): what BT3's ChrView_Update does
// model height from the bind positions of its bones (pMdl +0x50 table, 0xA0 per bone, position +0x10, up is -Y): giants are framed whole
static float cv_height(uint32_t o) {
    uint32_t hdr = psp_l32(o + 0x14), bt = hdr ? psp_l32(hdr + 0x50) : 0, nb = hdr ? psp_l32(hdr + 8) : 0;
    if (bt && bt < 0x08000000u) bt += hdr;
    float top = 0.0f;
    for (uint32_t i = 0; bt && i < nb && i < 256; i++) top = std::max(top, -cv_rf(bt + 0xA0 * i + 0x14));
    return top > 8.0f && top < 200.0f ? top * 1.08f : 19.0f;
}
static bool cv_scene(Ctx& c, uint32_t o, const float* eye, const float* ctr);
static void cv_draw3d(Ctx& c) {
    if (!cv.on || !cv.visible || cv.cur < 0 || cv.owner != 1) return;
    uint32_t o = cv_objp(cv.obj[cv.cur]), mgr = psp_l32(0x08A7E260u);
    if (!o || !mgr) return;
    uint64_t now = psp_time_us();
    if (!cv.framed) {   // full body: a TTT fighter is about 19 units tall, up is -Y
        if (cv.t == 0) { cv.yaw = 0.0f; cv.pitch = 0.04f; cv.zf = 1.0f; }
        cv.h = 19.0f;
        // model height from the bind positions of its bones (pMdl +0x50 table, 0xA0 per bone, position +0x10, up is -Y): giants are framed whole
        uint32_t hdr = psp_l32(o + 0x14), bt = hdr ? psp_l32(hdr + 0x50) : 0, nb = hdr ? psp_l32(hdr + 8) : 0;
        if (bt && bt < 0x08000000u) bt += hdr;
        float top = 0.0f;
        for (uint32_t i = 0; bt && i < nb && i < 256; i++) top = std::max(top, -cv_rf(bt + 0xA0 * i + 0x14));
        if (top > 8.0f && top < 200.0f) cv.h = top * 1.08f;
        cv.cy = -cv.h * 0.5f;
        printf("cr: visor: altura del modelo %.1f (hueso mas alto %.1f, %u huesos)\n", cv.h, top, nb); fflush(stdout);
        cv.framed = true; cv.t = now;
    }
    float dt = std::min(0.1f, (now - cv.t) / 1e6f);
    cv.t = now;
    cv.yaw += cv.ax * 2.4f * dt;
    cv.pitch += cv.ay * 1.6f * dt;
    if (cv.zoom != 0.0f) cv.zf *= expf(-cv.zoom * 1.2f * dt);
    for (; cv.wheel > 0; cv.wheel--) cv.zf *= 0.88f;
    for (; cv.wheel < 0; cv.wheel++) cv.zf *= 1.12f;
    cv.zf = std::min(2.5f, std::max(0.3f, cv.zf));
    float dist = cv.h * 2.5f * cv.zf, m = cv.h * 0.03f;
    float pmin = asinf(std::min(1.0f, std::max(-1.0f, (cv.cy + m) / dist)));   // the eye stays above the floor (y 0)
    cv.pitch = std::min(1.4f, std::max(pmin, cv.pitch));
    float ctr[3] = {0.0f, cv.cy, 0.0f};
    float eye[3] = {dist * cosf(cv.pitch) * sinf(cv.yaw), cv.cy - dist * sinf(cv.pitch), dist * cosf(cv.pitch) * cosf(cv.yaw)};
    bool stage = cv_scene(c, o, eye, ctr);
    cv_hud(c);
    static int logs = 0;
    if (logs++ < 1) printf("cr: visor: primer dibujo, escenario %s, ojo (%.1f, %.1f, %.1f), distancia %.1f\n", stage ? "si" : "no", eye[0], eye[1], eye[2], dist); fflush(stdout);
}

// camera at eye looking at ctr, the battle modules' update and the draw of the stage and the objects (BT3 ChrView_Update)
static bool cv_scene(Ctx& c, uint32_t o, const float* eye, const float* ctr) {
    uint32_t mgr = psp_l32(0x08A7E260u);
    for (uint32_t k = 0; k < 0x290; k += 4) psp_s32(cv.cam + k, psp_l32(mgr + 0x720u + k));
    cv_wf(cv.cam + 0x260, eye[0]); cv_wf(cv.cam + 0x264, eye[1]); cv_wf(cv.cam + 0x268, eye[2]); cv_wf(cv.cam + 0x26C, 1.0f);
    cv_flags();
    psp_s32(CV_W + 0x5BD0, 0x1F); psp_s32(CV_W + 0x5BD4, 0);
    cv_aim(c, (float*)eye, (float*)ctr);
    psp_s32(mgr + 0x1160u, cv.cam);
    // extra face: 0x089B40F4 reads [obj+0x16A8]+0x1C (face + 1); the object's own block is borrowed for this frame, else the port's
    uint32_t rc = psp_l32(o + 0x16A8), keep = 0;
    bool xf = cv.face >= 0 && cv.face < 8, fown = xf && rc, fset = xf && !rc;
    if (fown) { keep = psp_l32(rc + 0x1C); psp_s32(rc + 0x1C, (uint32_t)cv.face + 1); }
    if (fset) { for (uint32_t k = 0; k < 0x40; k += 4) psp_s32(cv.ctrl + k, 0); psp_s32(cv.ctrl + 0x1C, (uint32_t)cv.face + 1); psp_s32(o + 0x16A8, cv.ctrl); }
    static int steps = 0;
    auto mark = [&](const char* w) { if (steps < 12) { printf("cr: visor: paso %s\n", w); fflush(stdout); } };
    mark("actualizar objetos");
    cl_call(c, 0x089AF124u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089AF33Cu, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089AF48Cu, 0, 0, 0, 0, 0, 0);
    bool stage = psp_l32(0x08A7E678u) != 0;
    if (stage) cl_call(c, 0x089BAA70u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0887B9ACu, 0, 0, 0, 0, 0, 0);
    if (stage) { cl_call(c, 0x088068FCu, cv.cam, 0, 0, 0, 0, 0); cl_call(c, 0x0880740Cu, cv.cam, 0, 0, 0, 0, 0); }
    cl_call(c, 0x089BDE94u, 0, 0, 0, 0, 0, 0);
    if (xf) {
        psp_s32((fown ? rc : cv.ctrl) + 0x1C, (uint32_t)cv.face + 1);
    }
    if (cv_predraw) cv_predraw(o);
    mark("dibujar modelo");
    if (cv_objmark) cv_objmark(c, true);
    cl_call(c, 0x0880BD3Cu, 0, 0, 0, 0, 0, 0);
    if (cv_objmark) cv_objmark(c, false);
    if (fset && psp_l32(o + 0x16A8) == cv.ctrl) psp_s32(o + 0x16A8, 0);
    if (fown) psp_s32(rc + 0x1C, keep);
    mark("modelo dibujado");
    steps++;
    cl_call(c, 0x089BDFB4u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0880BCF0u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x0887BAA4u, 1, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BDFE4u, 0, 0, 0, 0, 0, 0);
    cl_call(c, 0x089BE034u, 0, 0, 0, 0, 0, 0);
    return stage;
}

static void cr_open(const Ctx& c) {
    cr_setup();
    uint32_t w = mpm_alloc(c, 0x80);
    if (!w) { printf("cr: sin memoria para la pantalla\n"); return; }
    mpm_call(c, 0x089D1538u, w);
    psp_s32(w + 0x40, cr_vt());
    cr_w = w;
    cr_phase = CP_LOAD;
    cr_self = false; cr_ups = cr_draws = 0; cr_seen = cr_fno;
    for (int k = 0; k < CK_N; k++) cr_hld[k] = 0;
    for (int i = 0; i < 4; i++) cr_hnd[i] = 0;
    cr_hdr = cr_nav = 0;
    mpm_call(c, 0x089D0310u, mpm_root(), w, 0xFF, 0);
    printf("cr: pantalla abierta (widget 0x%08X, entrada %d)\n", w, psp_cr_pak());
}

// once per frame from the pad read, outside the widget walks
// the main menu does not walk the root's update: when the game has not called the widget's update in the last frame, the port does
static void cr_anim(const Ctx& c);
static void cr_frame(const Ctx& c) {
    cr_fno++;
    if (cr_want && !cr_w) { cr_want = false; if (cr_on()) cr_open(c); }
    cv_tick(c);
    if (cr_w) cr_vnav_frame(c);
    if (cv_guard > 0 && !(psp_l32(0x08B4E990u + 0x2C) & psp_l32(0x08A793D0u))) cv_guard--;   // counts once the key is up
    if (cr_w && cr_phase == CP_MODEL && cv.visible) { cr_phase = CP_VIEW; printf("cr: visor visible\n"); fflush(stdout); }
    if (cr_w && cr_phase == CP_DONE && cv_busy()) return;
    if (cr_w && cr_phase == CP_DONE) { printf("cr: %u fotogramas, %u dibujos, %u actualizaciones del juego%s\n", cr_fno - cr_seen, cr_draws, cr_ups, cr_self ? ", actualizada por el port" : ""); cr_destroy(c); return; }
    if (!cr_w || cr_phase == CP_LOAD) return;
    if (!cr_self && cr_fno - cr_seen > 2) { cr_self = true; printf("cr: el juego no actualiza la pantalla (dibujos %u): la actualiza el port cada fotograma\n", cr_draws); }
    if (cr_self) cr_anim(c);
    static int logs = 0;
    if ((cr_fno % 300) == 0 && logs++ < 4) printf("cr: abierta, %u dibujos, %u actualizaciones del juego, fase %d, estado principal %d\n", cr_draws, cr_ups, cr_phase, mpm_cur_state(cr_hld[CK_MAIN]));
}

static uint32_t cr_be(uint32_t a) { return __builtin_bswap32(psp_l32(a)); }
static bool cr_entry(uint32_t pak, int code, uint32_t* a, uint32_t* n) {
    uint32_t cnt = cr_be(pak);
    if (!pak || code < 1 || (uint32_t)code > cnt || cnt > 4096) return false;
    uint32_t o0 = cr_be(pak + 4 * code) & ~3u, o1 = cr_be(pak + 4 * code + 4) & ~3u;
    if (o1 <= o0) return false;
    *a = pak + o0; *n = o1 - o0;
    return true;
}

// TTT big render of the character in chara_art: a pak made in port memory with the screen's AGS 19 and the render RHG
// (char_select of the free battle pak: RHG 184 + menu number, 255 = no render)
static void cr_render(const Ctx& c) {
    if (cr_rfor == cr_cursor || !cr_fb || !cr_loader) return;
    cr_rfor = cr_cursor;
    uint32_t pak = psp_l32(cr_loader + 0x14), cs = 0, csn = 0, ag = 0, agn = 0, rh = 0, rhn = 0;
    if (!cr_entry(cr_fb, 12, &cs, &csn) || !cr_entry(pak, 19, &ag, &agn)) { static int l = 0; if (l++ < 1) printf("cr: sin render (pak de la pantalla sin la entrada 19 o batalla libre sin char_select)\n"); return; }
    int n = roster::rank_vanilla(cr_cursor), big = psp_cr_big(cr_game_char(cr_cursor)), code = big > 0 ? big : n >= 0 ? 184 + n : 255;   // a mod character: its big.png entry
    if (!cr_entry(cs, code, &rh, &rhn)) return;
    if (!cr_rmem) cr_rmem = psp_reserve(0x100000);
    uint32_t base = cr_rmem + 0x80000 * cr_rbuf, o1 = 64, o2 = (o1 + agn + 63) & ~63u, end = (o2 + rhn + 63) & ~63u;
    if (end > 0x80000) { printf("cr: render %d demasiado grande (%u bytes)\n", code, rhn); return; }
    cr_rbuf ^= 1;
    psp_s32(base, __builtin_bswap32(2u)); psp_s32(base + 4, __builtin_bswap32(o1)); psp_s32(base + 8, __builtin_bswap32(o2)); psp_s32(base + 12, __builtin_bswap32(end));
    memcpy(psp_p(base + o1), psp_p(ag), agn);
    memcpy(psp_p(base + o2), psp_p(rh), rhn);
    if (cr_hld[CK_ART]) mpm_call(c, 0x089D248Cu, cr_hld[CK_ART], 3);
    cr_hld[CK_ART] = 0;
    if (cr_rhnd) mpm_call(c, 0x08840640u, 0x08B48200u, cr_rhnd);
    cr_rhnd = mpm_call(c, 0x088404D8u, 0x08B48200u, base, 1, 2);
    uint32_t h = cr_rhnd ? mpm_alloc(c, 0x54) : 0;
    if (h) { mpm_call(c, 0x089D1EE8u, h, cr_rhnd, 0, 0); mpm_state(c, h, 0); }
    cr_hld[CK_ART] = h;
    static int logs = 0;
    if (logs++ < 6) printf("cr: render del personaje %d: RHG %d (%u bytes), handle 0x%08X\n", cr_cursor, code, rhn, cr_rhnd);
}

// main screen animation st, then state next and phase ph (BT3 Flash_GotoLabel; input waits like MFLASH_PAD)
static void cr_play(const Ctx& c, int st, int next, int ph) {
    mpm_state(c, cr_hld[CK_MAIN], st);
    cr_phase = CP_ANIM; cr_next_st = next; cr_next_ph = ph;
}

// BT3 plate states: off, on_start, on, off_start, ok
static void cr_plate(const Ctx& c, int k, int base, bool lit, bool ok) {
    uint32_t h = cr_hld[k];
    int cur = mpm_cur_state(h) - base, want;
    if (ok) want = 4;
    else if (lit) want = cur == 1 && !cr_done(h) ? 1 : cur == 1 || cur == 2 ? 2 : 1;
    else want = cur == 3 && !cr_done(h) ? 3 : cur == 1 || cur == 2 || cur == 4 ? 3 : 0;
    if (cur != want) mpm_state(c, h, base + want);
}

static void cr_msg(const Ctx& c, int st) { if (mpm_cur_state(cr_hld[CK_MSG]) != st) mpm_state(c, cr_hld[CK_MSG], st); }

static int cr_first() { return cr_cursor < (int)cr_tx.first.size() ? cr_tx.first[cr_cursor] : 0; }
static int cr_ncomm() { return cr_cursor < (int)cr_tx.count.size() ? cr_tx.count[cr_cursor] : 1; }
static void cr_say(int k) {
    int f = cr_first();
    cr_comm = k; cr_ctime = 0;
    cr_line = 9 + f + k;
    cr_vstop(); cr_vreq = cr_line;
    cr_pose = f + k < (int)cr_tx.pose.size() ? (cr_tx.pose[f + k] ? 1 : 0) : 0;
}

static void cr_input_list(const Ctx& c, uint32_t pr, uint32_t rep) {
    int n = cr_count();
    if (pr & psp_l32(0x08A793D0u)) { cr_vstop(); mpm_sfx(c, 1, 0); cr_msg(c, 2); cr_play(c, 4, -1, CP_DONE); printf("cr: atras\n"); return; }
    if (n <= 0) return;
    if (pr & psp_l32(0x08A793D4u)) {
        mpm_sfx(c, 1, 6);
        cr_vstop();
        cr_msg(c, 2);
        cr_menu = 0;
        cr_play(c, 5, 6, CP_DET);
        printf("cr: detalle de %d\n", cr_cursor);
        return;
    }
    if (rep & 8) {
        mpm_sfx(c, 1, 3);
        if (--cr_row < 0) { cr_row = 0; cr_scroll = 8; cr_play(c, 3, 1, CP_LIST); }
        cr_goto(cr_pos - 1);
    } else if (rep & 4) {
        mpm_sfx(c, 1, 3);
        if (++cr_row > 3) { cr_row = 3; cr_scroll = 4; cr_play(c, 2, 1, CP_LIST); }
        cr_goto(cr_pos + 1);
    } else if (rep & 1) {
        mpm_sfx(c, 1, 3);
        cr_goto(cr_pos - 4 < 0 ? n - 1 : cr_pos - 4);
    } else if (rep & 2) {
        mpm_sfx(c, 1, 3);
        cr_goto(cr_pos + 4 > n - 1 ? 0 : cr_pos + 4);
    }
}

static void cr_input_det(const Ctx& c, uint32_t pr, uint32_t rep) {
    if (pr & psp_l32(0x08A793D0u)) {
        mpm_sfx(c, 1, 0);
        cr_pose = 0; cr_line = 1; cr_idle = 0;
        cr_vstop(); cr_vreq = 1;
        cr_msg(c, 0);
        cr_play(c, 7, 1, CP_LIST);
        return;
    }
    if (pr & psp_l32(0x08A793D4u)) {
        mpm_sfx(c, 1, 6);
        if (cr_menu == 0) { cr_pscroll = 0; cr_play(c, 8, 9, CP_PROF); }
        else if (cr_menu == 1) { cr_play(c, 11, 12, CP_VOICE); cr_sample(); }
        else if (cr_menu == 2) { cr_say(0); cr_msg(c, 3); cr_play(c, 14, 15, CP_COMM); }
        else { cr_costume = 0; cr_play(c, 17, 18, CP_MODEL); }
        printf("cr: %s de %d\n", cr_menu == 0 ? "perfil" : cr_menu == 1 ? "voz" : cr_menu == 2 ? "comentarios" : "modelo", cr_cursor);
        return;
    }
    if (rep & 8) { mpm_sfx(c, 1, 3); cr_menu = (cr_menu + 3) % 4; }
    else if (rep & 4) { mpm_sfx(c, 1, 3); cr_menu = (cr_menu + 1) % 4; }
    else if (rep & 1) { mpm_sfx(c, 1, 3); cr_goto(cr_pos - 1); cr_play(c, 21, 6, CP_DET); }
    else if (rep & 2) { mpm_sfx(c, 1, 3); cr_goto(cr_pos + 1); cr_play(c, 20, 6, CP_DET); }
}

static int cr_prof_n = 0, cr_prof_vis = 4;
static int cr_prof_lines() { return cr_prof_n; }

static void cr_input_sub(const Ctx& c, uint32_t pr, uint32_t rep) {
    bool back = (pr & psp_l32(0x08A793D0u)) != 0, conf = (pr & psp_l32(0x08A793D4u)) != 0;
    if (cr_phase == CP_PROF) {
        int lh = 19, vis = cr_prof_vis, maxs = (cr_prof_lines() - vis) * lh;
        if ((rep & 8) && cr_pscroll > 0) cr_pscroll -= lh;
        else if ((rep & 4) && cr_pscroll < maxs) cr_pscroll += lh;
        if (back) { mpm_sfx(c, 1, 0); cr_play(c, 10, 6, CP_DET); }
    } else if (cr_phase == CP_VOICE) {
        if (conf) cr_sample();
        if (back) { cr_vstop(); mpm_sfx(c, 1, 0); cr_play(c, 13, 6, CP_DET); }
    } else if (cr_phase == CP_COMM) {
        bool wait = cr_vreq >= 0 || cr_vplay || !cr_vname.empty(), next = conf || (!wait && (cr_vend || ++cr_ctime > 300));
        if (back || (next && cr_comm >= cr_ncomm() - 1)) {
            mpm_sfx(c, 1, back ? 0 : 6);
            cr_vstop();
            cr_msg(c, 5); cr_pose = 0;
            cr_play(c, 16, 6, CP_DET);
        } else if (next) { if (conf) mpm_sfx(c, 1, 6); cr_say(cr_comm + 1); }
    } else if (cr_phase == CP_MODEL) {
        int nc = cr_ncol(cr_cursor);
        if (rep & 8) { mpm_sfx(c, 1, 3); cr_costume = (cr_costume + nc - 1) % nc; }
        else if (rep & 4) { mpm_sfx(c, 1, 3); cr_costume = (cr_costume + 1) % nc; }
        bool sq = (pr & 0x800u) != 0;   // Square (F on the keyboard): straight to the damaged model
        if ((conf || sq) && !cv.want) {
            if (cr_game_char(cr_cursor) < 0) { mpm_sfx(c, 1, 0); printf("cr: el id BT3 %d no tiene modelo en el port\n", cr_cursor); }
            else if (!cv_busy()) { mpm_sfx(c, 1, 6); cr_vstop(); cv_show(c, cr_cursor, cr_costume, sq ? 2 : 0); cv.edge = 0; }
        }
        if (back && cv_guard > 0) back = false;   // the Esc that closed the viewer is not this screen's
        if (back) { cv.want = false; mpm_sfx(c, 1, 0); cr_play(c, 19, 6, CP_DET); }
    } else if (cr_phase == CP_VIEW) {
        int e = cv.edge;
        cv.edge = 0;
        if (back) { cr_vstop(); mpm_sfx(c, 1, 0); cv_hide(c); cr_phase = CP_MODEL; cr_costume = cv.colour; cv_guard = 20; printf("cr: visor oculto\n"); fflush(stdout); return; }
        if (cv_busy()) return;
        uint32_t o = cv.cur >= 0 ? cv_objp(cv.obj[cv.cur]) : 0;
        if (conf && o) {   // X / click: normal -> level 1 -> level 2 -> normal, only the levels this colour has
            int nd = cv.dmg;
            do nd = (nd + 1) % 3; while (nd != cv.dmg && ((nd == 1 && !cv.has1) || (nd == 2 && !cv.has2)));
            if (nd == cv.dmg) return;
            mpm_sfx(c, 1, 6);
            printf("cr: visor: rotura %d\n", nd); fflush(stdout);
            if (nd == 2 || cv.dmg == 2) { cv_show(c, cv.bt3, cv.colour, nd); if (nd == 1) cv.dmg = 1; }
            else cv.dmg = nd;
            return;
        }
        if ((pr & 0x800u) && o && cv.face < 0) {   // Square / F: one of the character's voice samples, with lip sync on the model
            cr_vplay_file(cv_sample_name(cv.bt3, rand() % 2), 2);
            printf("cr: visor: voz del personaje %d\n", cv.bt3); fflush(stdout);
            return;
        }
        if ((e & 12) && o) {   // left / right: the model's own face, then each extra face it has
            int f = cv.face;
            bool bl = cv.hasb;   // blink only when the model has a blink texture
            for (int n = 0; n < 10; n++) {
                f = (e & 4) ? (f <= -1 ? 8 : f - 1) : (f >= 8 ? -1 : f + 1);
                if (f < 0 || (f == 8 ? bl : psp_l32(o + 0x54 + 4 * (uint32_t)f) != 0)) break;
            }
            if (f != cv.face) { mpm_sfx(c, 1, 3); cv.face = f; printf("cr: visor: cara %d\n", f); fflush(stdout); }
            return;
        }
        int nb = cv.bt3, nc = cv.colour;
        if (e & 1) nc = (cv.colour + cv.ncol - 1) % cv.ncol;
        else if (e & 2) nc = (cv.colour + 1) % cv.ncol;
        else if (e & 16) nb = cr_form(cv.bt3, -1);
        else if (e & 32) nb = cr_form(cv.bt3, 1);
        if (nb == cv.bt3 && nc == cv.colour) return;
        mpm_sfx(c, 1, 3);
        if (nb != cv.bt3) { cr_cursor = nb; for (int i = 0; i < (int)cr_ids.size(); i++) if (cr_ids[i] == nb) cr_pos = i; if (cr_ids.empty()) cr_pos = nb; }
        cv_show(c, nb, nc);
    }
}

static void cr_anim(const Ctx& c) {
    if (cr_phase == CP_LOAD || cr_phase == CP_DONE || !cr_hld[CK_MAIN]) return;
    uint32_t m = cr_hld[CK_MAIN];
    if (cr_phase == CP_ANIM && cr_done(m)) {
        if (cr_next_st >= 0) mpm_state(c, m, cr_next_st);
        cr_phase = cr_next_ph; cr_scroll = 0;
        if (cr_phase == CP_DONE) return;
    }
    int msg = mpm_cur_state(cr_hld[CK_MSG]);
    if ((msg == 0 || msg == 3) && cr_done(cr_hld[CK_MSG])) cr_msg(c, msg + 1);
    if (cr_phase != CP_ANIM) {
        uint32_t pad = 0x08B4E990u, pr = psp_l32(pad + 0x30), rep = psp_l32(pad + 0x34);
        if (cr_phase == CP_LIST) {
            int cur = cr_cursor;
            cr_input_list(c, pr, rep);
            if (cur != cr_cursor) cr_idle = 0;
            else if (cr_phase == CP_LIST && !cr_vplay && cr_vreq < 0 && cr_vname.empty() && ++cr_idle > 300) {
                cr_idle = 0;
                int n;
                do n = 2 + rand() % 7; while (n == cr_last);
                cr_last = cr_line = n;
                cr_vreq = n;
            }
        } else if (cr_phase == CP_DET) cr_input_det(c, pr, rep);
        else cr_input_sub(c, pr, rep);
    }
    if (cr_vreq >= 0 && (msg == 1 || msg == 4)) {
        char n[32];
        snprintf(n, sizeof n, "chichi_%03d", cr_vreq);
        int k = cr_vreq;
        cr_vplay_file(n, 1);
        cr_line = k;
    }
    cr_vtick(c);
    cr_music(c, true);
    if (--cr_blink <= 0) cr_blink = 90 + rand() % 150;
    int eye = (cr_pose ? 4 : 0) + (cr_blink <= 4), mouth = (cr_pose ? 6 : 2) + !(cr_talk && psp_lip_dir("menus/chara_reference_voices") > 0);
    if (mpm_cur_state(cr_hld[CK_GUIDE]) != 8 + cr_pose) mpm_state(c, cr_hld[CK_GUIDE], 8 + cr_pose);
    if (mpm_cur_state(cr_hld[CK_EYE]) != eye) mpm_state(c, cr_hld[CK_EYE], eye);
    if (mpm_cur_state(cr_hld[CK_MOUTH]) != mouth) mpm_state(c, cr_hld[CK_MOUTH], mouth);
    bool to_list = cr_phase == CP_LIST || (cr_phase == CP_ANIM && cr_next_ph == CP_LIST);
    for (int k = 0; k < 5; k++) cr_plate(c, CK_ROW0 + k, 10, to_list && k == 4 - cr_row, false);
    bool det = cr_phase == CP_DET || (cr_phase == CP_ANIM && cr_next_ph == CP_DET);
    int sub = cr_phase == CP_PROF ? 0 : cr_phase == CP_VOICE ? 1 : cr_phase == CP_COMM ? 2 : cr_phase == CP_MODEL ? 3 : -1;
    if (cr_phase == CP_ANIM && cr_next_ph >= CP_PROF && cr_next_ph <= CP_MODEL) sub = cr_next_ph - CP_PROF;
    for (int k = 0; k < 4; k++) cr_plate(c, CK_PLATE0 + k, 16 + 5 * k, det && k == cr_menu, sub == k);
    cr_plate(c, CK_PLATE5, 46, sub == 3, false);
    int num = cr_costume < 9 ? 37 + cr_costume : mpm_has_state(cr_hld[CK_NUM], 61) ? 61 : 36;   // 1..9, 10 (state 61 of the pak)
    if (mpm_cur_state(cr_hld[CK_NUM]) != num) mpm_state(c, cr_hld[CK_NUM], num);
    int ms = mpm_cur_state(m);
    bool mloop = ms == 1 || ms == 6 || ms == 9 || ms == 12 || ms == 15 || ms == 18;
    for (int k = 0; k < CK_N; k++) {
        int st = mpm_cur_state(cr_hld[k]);
        bool loop = k == CK_MAIN ? mloop : (k >= CK_ROW0 && k < CK_BAR) ? st == 12 : (k >= CK_PLATE0 && k <= CK_PLATE5) ? (st - 16) % 5 == 2 || st == 48
                  : k == CK_MSG ? st == 1 || st == 4 : k != CK_NUM;
        cr_tick(c, cr_hld[k], loop);
    }
    if (cr_phase != CP_LIST) cr_render(c);
    cr_attach(c);
}

static bool cr_scissor_on() { const char* e = getenv("TTT_CR_SCISSOR"); return !(e && *e == '0'); }

static void cr_point(const Ctx& c) {
    uint32_t h = cr_hld[CK_POINT], s = cr_slot(c, cr_hld[CK_MAIN], CS_POINT);
    if (!h || !s) { if (h) psp_s32(h + 8, 0); return; }
    uint32_t f = cr_scratch(), i = cr_scratch() + 0x20;
    for (int k = 0; k < 4; k++) { psp_s32(i + 4 * k, 0); mpm_wf(f + 4 * k, 0.0f); }
    mpm_call(c, 0x08840BA4u, s, f, f + 4, f + 8, f + 12);
    mpm_call(c, 0x08840B04u, s, i, i + 4, i + 8, i + 12);
    int n = cr_count(), y = n ? (cr_pos - cr_row) * 0xE0 / n : 0;
    if (y < 0) y = 0;
    if (y > 0xDA) y = 0xDA;
    psp_s32(h + 8, 0);
    psp_s32(h + 0x18, psp_l32(i));
    float sy = (int)psp_l32(i + 12) > 0 ? (float)(int)psp_l32(i + 12) / 32.0f : 0.5417f, sx = (int)psp_l32(i + 8) > 0 ? (float)(int)psp_l32(i + 8) / 32.0f : 0.6319f;
    uint32_t bs = cr_hld[CK_BAR] ? psp_l32(cr_hld[CK_BAR] + 8) : 0;
    float by = bs && (int)psp_l32(bs + 0xF0) > 0 ? (float)(int)psp_l32(bs + 0xF0) / 257.0f : sy;   // BT3 offset in the bar's units
    float len = n > 28 ? 28.0f / (float)n : 1.0f;   // BT3: the point is 4 / count * 7 of its height
    psp_s32(h + 0x1C, psp_l32(i + 4) + (uint32_t)(int)((float)y * by));
    mpm_wf(h + 0x20, sx); mpm_wf(h + 0x24, sy * len);
    for (int k = 0; k < 4; k++) psp_s32(h + 0x40 + 4 * k, psp_l32(f + 4 * k));
    static int logs = 0;
    if (logs++ < 1) printf("cr: punto de scroll en (%d, %d) + %d, escala %.3f %.3f, barra %.3f\n", (int)psp_l32(i), (int)psp_l32(i + 4), (int)((float)y * by), sx, sy * len, by);
}

// a part in a slot: the port scales it to the slot's rectangle on each axis (the game's own slot scale reads 1.0 for these slots)
static void cr_fit(int k, float cw, float chh) {
    uint32_t h = cr_hld[k], s = h ? psp_l32(h + 8) : 0;
    if (!s) return;
    int w = (int)psp_l32(s + 0xEC), hh = (int)psp_l32(s + 0xF0);
    if (w <= 0 || hh <= 0) return;
    psp_s32(h + 0x50, 3);
    mpm_wf(h + 0x20, (float)w / cw);
    mpm_wf(h + 0x24, (float)hh / chh);
}

// text with the HUD name style at a point, font scaled by s (the charge list's vertex scale)
static void cr_text_at(const Ctx& c, uint32_t font, int x, int y, float s, uint32_t str, uint32_t col) {
    uint32_t st = cr_scratch() + 0x40;
    psp_s32(st, 0); psp_s32(st + 4, 0); psp_s32(st + 8, col); psp_s32(st + 0xC, 1);
    psp_s32(st + 0x10, 1); psp_s32(st + 0x14, 1); psp_s32(st + 0x18, 0xFF404040u); psp_s32(st + 0x1C, 2);
    mpm_call(c, 0x088321ECu, font, st);
    float ksx = cl_sx, ksy = cl_sy, kax = cl_ax, kay = cl_ay;
    cl_sx = s; cl_sy = s; cl_ax = (float)x; cl_ay = (float)y; cl_gu_on = s != 1.0f;
    mpm_call(c, 0x08832530u, font, (uint32_t)x, (uint32_t)y, str, 0xFFFFFFFFu, 0xFFFFFFFFu);
    cl_gu_on = false; cl_sx = ksx; cl_sy = ksy; cl_ax = kax; cl_ay = kay;
    psp_s32(st + 8, 0xFFFFFFFFu); psp_s32(st + 0xC, 0); psp_s32(st + 0x18, 0xFF404040u); psp_s32(st + 0x1C, 0);
    mpm_call(c, 0x088321ECu, font, st);
}

// text in a BT3 text slot: scaled to the slot height, narrowed to its width (the game's measure 0x08831F68), a small left
// margin, vertically centred; hidden while the slot is transparent
static const int CR_PAD = 4;
static void cr_text(const Ctx& c, uint32_t font, uint32_t slot, uint32_t str, uint32_t col) {
    if (!slot || !str || mpm_f(slot + 0x2C) < 0.5f) return;
    int x = (int)psp_l32(slot + 0xE4), y = (int)psp_l32(slot + 0xE8), w = (int)psp_l32(slot + 0xEC), hh = (int)psp_l32(slot + 0xF0);
    float s = hh > 0 ? (float)hh / 12.5f : 1.0f;
    if (s > 1.15f) s = 1.15f;
    if (s < 0.6f) s = 0.6f;
    float tw = (float)(int)mpm_call(c, 0x08831F68u, font, str) * s, room = (float)w;
    if (tw > room && room > 8.0f) s *= room / tw;
    cr_text_at(c, font, x, y + (int)((hh - 15.0f * s) * 0.5f), s, str, col);
}

// a text wrapped to a width in px at scale s (original line breaks become spaces), cached per source
struct CrWrap { std::string src; int w = -1; float s = 0; std::vector<std::string> lines; };
static const std::vector<std::string>& cr_wrap(const Ctx& c, uint32_t font, const std::string& t, int maxw, float s, CrWrap& k) {
    if (k.src == t && k.w == maxw && k.s == s) return k.lines;
    k.src = t; k.w = maxw; k.s = s; k.lines.clear();
    std::string cur, word, flat;
    for (char ch : t) flat += ch == '\n' ? ' ' : ch;
    flat += ' ';
    for (char ch : flat) {
        if (ch != ' ') { word += ch; continue; }
        if (word.empty()) continue;
        std::string cand = cur.empty() ? word : cur + " " + word;
        float px = (float)(int)mpm_call(c, 0x08831F68u, font, cr_u16(cand, 19)) * s;
        if (px > maxw && !cur.empty()) { k.lines.push_back(cur); cur = word; }
        else cur = cand;
        word.clear();
    }
    if (!cur.empty()) k.lines.push_back(cur);
    return k.lines;
}

static void cr_profile(const Ctx& c, uint32_t font) {
    uint32_t s = cr_slot(c, cr_hld[CK_MAIN], CS_PROFILE);
    if (!s || mpm_f(s + 0x2C) < 0.5f || cr_cursor >= (int)cr_tx.profile.size()) return;
    int x = (int)psp_l32(s + 0xE4), y = (int)psp_l32(s + 0xE8), w = (int)psp_l32(s + 0xEC), h = (int)psp_l32(s + 0xF0), lh = 19;
    const float ps = 1.25f;
    if (w <= 0 || h <= 0) return;
    mpm_call(c, 0x08A6495Cu, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
    // paragraphs (separated by blank lines) are kept; the lines of a paragraph are reflowed to the width of the box
    static CrWrap wk[24];
    std::vector<std::string> ls, paras(1);
    const std::string& t = cr_tx.profile[cr_cursor];
    size_t a = 0;
    while (a <= t.size()) {
        size_t b = t.find('\n', a);
        std::string line = t.substr(a, b == std::string::npos ? std::string::npos : b - a);
        if (line.empty()) { if (!paras.back().empty()) paras.push_back(""); }
        else paras.back() += (paras.back().empty() ? "" : " ") + line;
        if (b == std::string::npos) break;
        a = b + 1;
    }
    for (size_t k = 0; k < paras.size() && k < 24; k++) {
        if (paras[k].empty()) continue;
        if (!ls.empty()) ls.push_back("");
        for (auto& part : cr_wrap(c, font, paras[k], w - 2 * CR_PAD, ps, wk[k])) ls.push_back(part);
    }
    cr_prof_n = (int)ls.size(); cr_prof_vis = std::max(1, h / lh);
    for (int l = 0, buf = 20; l < (int)ls.size() && buf < 36; l++) {
        int ly = y + l * lh - cr_pscroll;
        if (ly > y - lh && ly < y + h && !ls[l].empty()) cr_text_at(c, font, x + CR_PAD, ly, ps, cr_u16(ls[l], buf++), 0xFFFFFFFFu);
    }
    mpm_call(c, 0x08A6495Cu, 0, 0, 480, 272);
}

// render filling the chara_art window (uniform scale that covers it, centred) and cut to the window, so its edges never show
static void cr_art(const Ctx& c) {
    uint32_t h = cr_hld[CK_ART], s = cr_slot(c, cr_hld[CK_MAIN], CS_ART);
    if (!h || !s) return;
    uint32_t f = cr_scratch() + 0x60, i = cr_scratch() + 0x80;
    mpm_call(c, 0x08840BA4u, s, f, f + 4, f + 8, f + 12);
    mpm_call(c, 0x08840B04u, s, i, i + 4, i + 8, i + 12);
    int x = (int)psp_l32(i), y = (int)psp_l32(i + 4), w = (int)psp_l32(i + 8), hh = (int)psp_l32(i + 12);
    if (w <= 0 || hh <= 0 || mpm_f(f + 12) <= 0.01f) return;
    float sc = std::max((float)w / 272.0f, (float)hh / 176.0f) * 1.04f;   // a little over: the renders have empty edge pixels
    psp_s32(h + 8, 0);
    psp_s32(h + 0x18, (uint32_t)(x + (int)((w - 272.0f * sc) * 0.5f)));
    psp_s32(h + 0x1C, (uint32_t)(y + (int)((hh - 176.0f * sc) * 0.5f)));
    mpm_wf(h + 0x20, sc); mpm_wf(h + 0x24, sc);
    for (int k = 0; k < 4; k++) psp_s32(h + 0x40 + 4 * k, psp_l32(f + 4 * k));
    mpm_call(c, 0x08A6495Cu, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)hh);
    mpm_draw_holder(c, h);
    mpm_call(c, 0x08A6495Cu, 0, 0, 480, 272);
    static int logs = 0;
    if (logs++ < 1) printf("cr: render en la ventana (%d, %d, %d, %d), escala %.3f\n", x, y, w, hh, sc);
}

static void cv_bar_show(bool on);
static void cr_draw(const Ctx& c) {
    cv_bar_show(false);
    if (cr_phase == CP_LOAD || cr_phase == CP_DONE || cr_phase == CP_VIEW) return;
    mpm_draw_holder(c, cr_hld[CK_BG]);
    mpm_draw_holder(c, cr_hld[CK_MAIN]);
    bool sc = cr_scissor_on(), moving = cr_phase == CP_ANIM;
    if (sc) mpm_call(c, 0x08A6495Cu, 78, 48, 324, 131);
    uint32_t font = psp_l32(0x08A78EECu);
    for (int k = 0; k < 5; k++) if (k || moving) { cr_fit(CK_ROW0 + k, 515, 67); mpm_draw_holder(c, cr_hld[CK_ROW0 + k]); }
    if (font && cr_count()) {
        mpm_call(c, 0x08832448u, font);
        for (int k = 0; k < 5; k++) {
            if (!k && !moving) continue;
            int it = k == 0 && cr_scroll == 4 ? cr_id(cr_pos - cr_row - 1) : cr_item(k);
            uint32_t h = cr_hld[CK_ROW0 + k];
            const std::string* nm = it < (int)cr_tx.name.size() ? &cr_tx.name[it] : nullptr;
            const std::string* fm = it < (int)cr_tx.form.size() ? &cr_tx.form[it] : nullptr;
            if (nm && !nm->empty()) cr_text(c, font, cr_slot(c, h, CS_NAME), cr_u16(*nm, 2 * k), 0xFFFFFFFFu);
            if (fm && !fm->empty()) cr_text(c, font, cr_slot(c, h, CS_FORM), cr_u16(*fm, 2 * k + 1), 0xFF00F0FFu);
        }
        mpm_call(c, 0x08832500u, font);
    }
    if (sc) mpm_call(c, 0x08A6495Cu, 0, 0, 480, 272);
    cr_fit(CK_BAR, 129, 257);
    mpm_draw_holder(c, cr_hld[CK_BAR]);
    cr_point(c);
    mpm_draw_holder(c, cr_hld[CK_POINT]);
    cr_art(c);
    if (mpm_cur_state(cr_hld[CK_FRAME]) == 59 && psp_l32(cr_hld[CK_FRAME] + 8)) { cr_fit(CK_FRAME, 268, 216); mpm_draw_holder(c, cr_hld[CK_FRAME]); }   // frame over the render (pak with state 59)
    for (int k = 0; k < 4; k++) { cr_fit(CK_PLATE0 + k, 319, 87); mpm_draw_holder(c, cr_hld[CK_PLATE0 + k]); }
    cr_fit(CK_PLATE5, 259, 67);
    mpm_draw_holder(c, cr_hld[CK_PLATE5]);
    cr_fit(CK_NUM, 32, 32);
    mpm_draw_holder(c, cr_hld[CK_NUM]);
    cr_fit(CK_PBOX, 512, 256);
    mpm_draw_holder(c, cr_hld[CK_PBOX]);
    cr_fit(CK_VBOX, 256, 128);
    mpm_draw_holder(c, cr_hld[CK_VBOX]);
    if (font && cr_count()) {
        mpm_call(c, 0x08832448u, font);
        int i = cr_cursor;
        uint32_t m = cr_hld[CK_MAIN];
        if (i < (int)cr_tx.name.size() && !cr_tx.name[i].empty()) cr_text(c, font, cr_slot(c, m, CS_NAME1), cr_u16(cr_tx.name[i], 16), 0xFFFFFFFFu);
        if (i < (int)cr_tx.form.size() && !cr_tx.form[i].empty()) cr_text(c, font, cr_slot(c, m, CS_FORM1), cr_u16(cr_tx.form[i], 17), 0xFF00F0FFu);
        if (i < (int)cr_tx.voice.size() && !cr_tx.voice[i].empty()) cr_text(c, font, cr_slot(c, m, CS_VOICE), cr_u16(cr_tx.voice[i], 18), 0xFFFFFFFFu);
        cr_profile(c, font);
        mpm_call(c, 0x08832500u, font);
    }
    cr_fit(CK_GUIDE, 256, 512);
    mpm_draw_holder(c, cr_hld[CK_GUIDE]);
    cr_fit(CK_EYE, 64, 32);
    if (cr_blink <= 8) mpm_draw_holder(c, cr_hld[CK_EYE]);
    cr_fit(CK_MOUTH, 32, 32);
    mpm_draw_holder(c, cr_hld[CK_MOUTH]);
    static const int AR[4][3] = {{CK_AUP, 32, 40}, {CK_ADOWN, 32, 40}, {CK_ALEFT, 37, 32}, {CK_ARIGHT, 37, 32}};
    for (auto& a : AR) { cr_fit(a[0], (float)a[1], (float)a[2]); mpm_draw_holder(c, cr_hld[a[0]]); }
    mpm_draw_holder(c, cr_hld[CK_MSG]);
    uint32_t ms = cr_slot(c, cr_hld[CK_MSG], CS_MSG);
    int mst = mpm_cur_state(cr_hld[CK_MSG]);
    if (font && ms && (mst == 1 || mst == 4) && cr_line < (int)cr_tx.comment.size()) {
        mpm_call(c, 0x08832448u, font);
        const std::string& t = cr_tx.comment[cr_line];
        int x = (int)psp_l32(ms + 0xE4), y = (int)psp_l32(ms + 0xE8), w = (int)psp_l32(ms + 0xEC), hh = (int)psp_l32(ms + 0xF0);
        static CrWrap wk;
        (void)hh;
        const std::vector<std::string>& ls = cr_wrap(c, font, t, w - 2 * CR_PAD, 1.0f, wk);
        for (int l = 0; l < 3 && l < (int)ls.size(); l++)
            mpm_call(c, 0x08832530u, font, (uint32_t)(x + CR_PAD), (uint32_t)(y + 15 * l), cr_u16(ls[l], 10 + l), 0xFFFFFFFFu, 0xFFFFFFFFu);
        mpm_call(c, 0x08832500u, font);
    }
    uint32_t vi = cr_hld[CK_VICON], vs = vi ? psp_l32(vi + 8) : 0;   // play icon of the voice page, over everything (pak state 60, slot voice_icon)
    if (vs && mpm_cur_state(vi) == 60 && mpm_f(vs + 0x2C) >= 0.01f) { cr_fit(CK_VICON, (float)(int)psp_l32(vs + 0xEC), (float)(int)psp_l32(vs + 0xF0)); mpm_draw_holder(c, vi); }
}

static void cr_native(Ctx& c, int k) {
    switch (k) {
        case CRN_LOAD: {
            int idx = psp_cr_pak();
            cr_loader = idx >= 0 ? mpm_alloc(c, 0x28) : 0;
            if (!cr_loader) { cr_phase = CP_DONE; break; }
            mpm_call(c, 0x089D26A0u, cr_loader);
            mpm_call(c, 0x089D2828u, cr_loader, (uint32_t)idx);
            mpm_call(c, 0x089D28A0u, cr_loader, MPM_NAT + 4 * CRN_PAK, c.r[4]);
            psp_s32(c.r[4] + 0x60, cr_loader);
            break;
        }
        case CRN_PAK: cr_loaded(c, c.r[4]); break;
        case CRN_PAK2: cr_fb = psp_l32(cr_loader2 + 0x14); printf("cr: batalla libre cargada para los renders (0x%08X, char_select %s)\n", cr_fb, cr_be(cr_fb) >= 12 ? "si" : "no"); break;
        case CRN_ANIM: cr_ups++; cr_seen = cr_fno; if (!cr_self) cr_anim(c); break;
        case CRN_DRAW: cr_draws++; cr_draw(c); break;
    }
}
