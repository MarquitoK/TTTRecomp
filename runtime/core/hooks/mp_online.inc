// En linea screens (57_menu_multiplayer AGS 125 rooms, 139 keyboard, 146 create room) over the online service (net/online.cpp)
#include "net/online.h"
void psp_net_mods_ensure();
int psp_net_poll(Ctx& c);
void psp_window_text_capture(int on);
int psp_window_text_pop();

enum { KB_NONE, KB_SEARCH_ID, KB_GROUP, KB_JOIN_PW, KB_ROOM_NAME, KB_ROOM_PW, KB_A_NAME };
static void ac_kdone(const Ctx& c, int kind, bool ok, const std::string& t);
static void ac_rows(const Ctx& c);
static void ac_attach(const Ctx& c);
static void ac_input(const Ctx& c, uint32_t e);
static void ac_draw(const Ctx& c, uint32_t font, uint32_t lang);
static void ac_draw_dlg(const Ctx& c, uint32_t font, uint32_t lang);
enum { KP_OFF, KP_OPEN, KP_NORMAL, KP_CLOSE };
enum { MOP_NONE, MOP_HOST, MOP_JOIN, MOP_HOSTED };
enum { MM_LIST, MM_EMPTY, MM_LOADING, MM_SVC, MM_CREATE, MM_SEARCH_ID, MM_CONNECTING, MM_CREATING, MM_NOTFOUND, MM_BADPW, MM_FULL, MM_VERSION,
       MM_ICE, MM_MODS, MM_ACC_BAD, MM_C_NAME, MM_C_PLAYERS, MM_C_PW, MM_C_CREATE, MM_C_MODE, MM_C_CUSTOM, MM_R_CLOSED, MM_R_DENIED, MM_R_PREP,
       MM_NEEDACC, MM_RANK_INFO, MM_RK_LOADING, MM_RK_EMPTY, MM_MM_NONE, MM_MM_SVC, MM_K_MODE, MM_K_FMT, MM_K_GROUP, MM_K_LEAVE, MM_K_FIND,
       MM_K_CANCEL, MM_K_FIXED, MM_K_LEADER, MM_K_WAIT, MM_K_NOGROUP, MM_K_RMODS, MM_N };
enum { MS_TAB1, MS_UP = MS_TAB1 + 3, MS_DOWN, MS_ROW1, MS_EMPTY = MS_ROW1 + 6, MS_ACCOUNT, MS_BTN1, MS_NAME = MS_BTN1 + 2, MS_HOST, MS_ID,
       MS_PLAYERS, MS_PROMPT, MS_INPUT, MS_KSPEC, MS_ONAME = MS_KSPEC + 4, MS_OPLAYERS, MS_OPW, MS_TRNAME, MS_TPW, MS_BCREATE,
       MS_OMODE, MS_OCUSTOM, MS_TMODE, MS_TCUSTOM, MS_HEAD, MS_KEYCAP = MS_HEAD + 5, MS_KEY = MS_KEYCAP + 40, MS_RSERVER = MS_KEY + 40, MS_RROLE,
       MS_RDLG, MS_RDL1, MS_RDL2, MS_RDB1, MS_RDB2, MS_RDBC, MS_RBTXT, MS_RICON, MS_RPOS, MS_RELO, MS_RW, MS_RL, MS_RTITLE, MS_AVATAR, MS_TLABEL, MS_TVALUE, MS_TSTATUS, MS_N };

static int mo_sn[MS_N];
static int mo_tab = 0, mo_focus = 1, mo_row = 0, mo_top = 0, mo_btn = 0, mo_nrooms = 0, mo_lstate = 0, mo_refresh = 0;
static OnRoomInfo mo_rooms[64];
static int mo_kb = KB_NONE, mo_kph = KP_OFF, mo_kcur = 0, mo_kshift = 0, mo_kok = 0, mo_blink = 0;
static std::string mo_ktext, mo_join_id, mo_join_pw, mo_room_id;
static int mo_csel = 0, mo_cplayers = 2, mo_cpriv = 0, mo_cmode = 0, mo_ccust = 0;
static OnRankRow mo_rk[50], mo_rkme;
static int mo_nrk = 0, mo_rkst = 0, mo_rkcur = 0, mo_rktop = 0, mo_rkb = 0, mo_mode_c = 16, mo_rmode_w = 0;
static bool mpm_has_state(uint32_t h, int st) {
    if (!h) return false;
    uint32_t list = psp_l32(h + 4), n = psp_l32(h + 0xC);
    for (uint32_t i = 0; i < n; i++) if ((int)psp_l32(list + 8 * i) == st && psp_l32(list + 8 * i + 4)) return true;
    return false;
}

static bool mo_create_public() { return false; }
static std::string mo_cname, mo_cpw;
static int mo_msg = -1, mo_msg_t = 0, mo_op = MOP_NONE, mo_op_t = 0;
static bool mo_noauto = false;
static int mo_back_msg = -1;
static void mo_profile_set(const char* n);
static int mo_role = 0, mo_dlg = 0;
static void mo_room_begin(int role);
static bool mo_hide[H_N];
static void mo_mv(const Ctx& c) { mpm_sfx(c, 1, 3); }
static const char* MO_KEYS = "1234567890QWERTYUIOPASDFGHJKL-ZXCVBNM_.@";

static int mpm_sname(const char* n) {
    static int count = S_N;
    for (int k = 0; k < count; k++) if (!strcmp((const char*)psp_p(mpm_str(k)), n)) return k;
    if (count >= 384) return 0;
    memcpy(psp_p(mpm_str(count)), n, strlen(n) + 1);
    return count++;
}

static void mo_names() {
    static const char* fixed[] = {"tab_01", "tab_02", "tab_03", "arrow_up", "arrow_down", "row_01", "row_02", "row_03", "row_04", "row_05", "row_06",
                                  "text_empty", "text_account", "button_01", "button_02", "text_name", "text_host", "text_id", "text_players",
                                  "text_prompt", "text_input", "key_shift", "key_space", "key_delete", "key_ok", "opt_name", "opt_players",
                                  "opt_password", "text_room_name", "text_password", "btn_create",
                                  "opt_mode", "opt_custom", "text_mode", "text_custom", "head_name", "head_host", "head_id", "head_mode", "head_players"};
    for (int k = 0; k < MS_KEYCAP; k++) mo_sn[k] = mpm_sname(fixed[k]);
    char b[16];
    for (int k = 0; k < 40; k++) {
        snprintf(b, sizeof(b), "keycap_%02d", k); mo_sn[MS_KEYCAP + k] = mpm_sname(b);
        snprintf(b, sizeof(b), "key_%02d", k); mo_sn[MS_KEY + k] = mpm_sname(b);
    }
    static const char* room[] = {"text_server", "text_role", "dialog", "text_dlg_1", "text_dlg_2", "dlg_btn_01", "dlg_btn_02", "dlg_btn_c", "text_button", "mode_icon",
                                 "text_pos", "text_elo", "text_w", "text_l", "text_title", "avatar", "text_label", "text_value", "text_status"};
    for (int k = 0; k < MS_N - MS_RSERVER; k++) mo_sn[MS_RSERVER + k] = mpm_sname(room[k]);
}

static const char* mo_t(int id, uint32_t lang) {
    static const char* T[MM_N][2] = {
        {"Choose a room to join.", "Elige una sala para unirte."},
        {"No public rooms yet.", "No hay salas p\u00FAblicas."},
        {"Searching for rooms...", "Buscando salas..."},
        {"Online service unreachable.", "Sin conexi\u00F3n con el servicio."},
        {"Create a new room.", "Crea una sala nueva."},
        {"Join a room with its ID.", "\u00DAnete a una sala con su ID."},
        {"Connecting...", "Conectando..."},
        {"Creating room...", "Creando sala..."},
        {"That room does not exist.", "La sala no existe."},
        {"Wrong password.", "Contrase\u00F1a incorrecta."},
        {"The room is full.", "La sala est\u00E1 llena."},
        {"Room uses another version.", "La sala usa otra versi\u00F3n."},
        {"Could not reach the host.", "No se pudo unir al anfitri\u00F3n."},
        {"That room uses other character or map mods.", "Esa sala usa otros mods de personajes o mapas."},
        {"Session expired. Sign in again.", "Sesi\u00F3n caducada. Entra otra vez."},
        {"Name other players will see.", "Nombre que ver\u00E1n los dem\u00E1s jugadores."},
        {"Maximum number of players.", "N\u00FAmero m\u00E1ximo de jugadores."},
        {"With a password the room is private (join by ID).", "Con contrase\u00F1a la sala es privada (solo por ID)."},
        {"Create the room and enter it.", "Crea la sala y entra en ella."},
        {"Free Battle or Battle Select.", "Batalla libre o Selecci\u00F3n de batalla."},
        {"Allow custom characters in the room.", "Permitir personajes personalizados en la sala."},
        {"The host closed the room.", "El anfitri\u00F3n cerr\u00F3 la sala."},
        {"Could not enter the room.", "No se pudo entrar a la sala."},
        {"Could not set up the room.", "No se pudo preparar la sala."},
        {"Sign in with Steam in Options.", "Entra con Steam en Opciones."},
        {"Top 50 of ranked rooms.", "Mejores 50 de las clasificatorias."},
        {"Loading ranking...", "Cargando clasificaci\u00F3n..."},
        {"No ranked results yet.", "A\u00FAn no hay resultados clasificatorios."},
        {"No opponent found. Try again.", "Sin rival. Int\u00E9ntalo de nuevo."},
        {"Ranked unavailable.", "Clasificatoria no disponible."},
        {"Choose the game mode.", "Elige el modo de juego."},
        {"1 vs 1 or 2 vs 2.", "1 contra 1 o 2 contra 2."},
        {"Select your match partner.", "Selecciona compa\u00F1ero de partida."},
        {"Confirm to leave the group.", "Confirma para salir del grupo."},
        {"Find a ranked match.", "Busca una partida clasificatoria."},
        {"Stop searching.", "Deja de buscar."},
        {"Leave the group to play 1 vs 1.", "Sal del grupo para jugar 1 vs 1."},
        {"The group leader chooses.", "El l\u00EDder del grupo elige."},
        {"Wait for your partner.", "Espera a tu compa\u00F1ero."},
        {"That group does not exist.", "Ese grupo no existe."},
        {"Ranked allows cosmetic mods only.", "En clasificatoria solo se permiten mods est\u00E9ticos."}};
    return T[id][mpm_li(lang)];
}

static const char* mo_prompt(uint32_t lang) {
    static const char* T[5][2] = {{"Player name", "Nombre de jugador"}, {"Room ID", "ID de la sala"},
                                  {"Password", "Contrase\u00F1a"}, {"Room name", "Nombre de la sala"}, {"Group code", "C\u00F3digo del grupo"}};
    int k = mo_kb == KB_A_NAME ? 0 : mo_kb == KB_SEARCH_ID ? 1 : mo_kb == KB_GROUP ? 4 : mo_kb == KB_ROOM_NAME ? 3 : 2;
    return T[k][mpm_li(lang)];
}

static void mo_say(int id) { mo_msg = id; mo_msg_t = 300; }

static bool mo_kname() { return mo_kb == KB_A_NAME; }
static int mo_kmax() { return mo_kname() ? 10 : mo_kb == KB_SEARCH_ID || mo_kb == KB_GROUP ? 6 : mo_kb == KB_ROOM_NAME ? 20 : 16; }
static bool mo_kpw() { return mo_kb == KB_JOIN_PW || mo_kb == KB_ROOM_PW; }

static char mo_kchar(int i) {
    char ch = MO_KEYS[i];
    if (mo_kshift && ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
    return ch;
}

static bool mo_kallowed(char ch) {
    if (mo_kb == KB_SEARCH_ID || mo_kb == KB_GROUP) return isalnum((unsigned char)ch) != 0;
    if (mo_kname()) return isalnum((unsigned char)ch) || ch == '-' || ch == '_' || ch == '.';
    return true;
}

static void mo_kopen(const Ctx& c, int kind, const std::string& init) {
    mo_kb = kind; mo_kph = KP_OPEN; mo_kcur = 0; mo_kshift = 0; mo_ktext = init;
    mpm_state(c, mpm_hld[K_ROOT], 0);
    psp_window_text_capture(1);
    printf("menu: en linea, teclado abierto (%d)\n", kind);
}

static void mo_kclose(const Ctx& c, bool ok) {
    mo_kok = ok; mo_kph = KP_CLOSE;
    psp_window_text_capture(0);
    mpm_state(c, mpm_hld[K_ROOT], 2);
}

enum { TB_RANKED, TB_ROOMS, TB_RANK };
static int mo_mm = 0, mo_mm_surv = 0, mo_mm_fmt = 1, mo_mm_go = 0, mo_mm_gt = 0, mo_mm_re = 0;
static bool mo_mm_room = false, mo_mm_wgrp = false, mo_mm_follow = false, mo_mm_grp = false, mo_mm_auto = false;
static int mo_osel = 2, mo_omode = 0, mo_ogrp = 0;

static bool mo_grp_in(OnGroup* g) { return mo_mm_grp && net_mm_group_state(g) == MG_OK; }
static std::string mo_mm_done;

static void mo_mm_begin() {
    OnGroup g;
    bool member = net_mm_group_state(&g) == MG_OK && !g.lead;
    if (member) net_mm_follow(); else net_mm_queue(mo_mm_surv, mo_mm_fmt);
    mo_mm_follow = member;
    mo_mm = 1; mo_mm_go = 0; mo_mm_done.clear(); mo_mm_wgrp = false;
    printf("menu: en linea, emparejamiento %s (%s)\n", member ? "siguiendo al lider del grupo" : "buscando", mo_mm_surv ? "supervivencia" : mo_mm_fmt == 2 ? "batalla libre 2vs2" : "batalla libre 1vs1");
}

static void mo_mm_stop(const char* why) {
    if (!mo_mm && !mo_mm_wgrp) return;
    net_mm_cancel();
    mo_mm = 0; mo_mm_wgrp = false; mo_mm_re = 0;
    printf("menu: en linea, emparejamiento detenido: %s\n", why);
}

static void mo_mm_test() {
    static bool once = false;
    const char* t = getenv("TTT_MM_TEST");
    if (once || !t || !*t || !*net_online_account()) return;
    once = true;
    mo_mm_surv = !strcmp(t, "surv"); mo_mm_fmt = !strcmp(t, "free2") || mo_mm_surv ? 2 : 1;
    mo_omode = mo_mm_surv ? 2 : mo_mm_fmt - 1;
    const char* g = getenv("TTT_MM_GROUP");
    if (g && *g) { bool nw = !strcmp(g, "new"); net_mm_group(nw ? 1 : 2, g); mo_mm_grp = mo_mm_auto = true; mo_mm_wgrp = !nw; mo_mm_gt = 0; printf("menu: en linea, emparejamiento: grupo %s\n", g); }
    else mo_mm_begin();
}

static std::string mo_mm_text(uint32_t lang) {
    bool es = mpm_li(lang) == 1;
    char b[96];
    OnGroup g;
    bool in = mo_grp_in(&g);
    if (mo_mm_wgrp) return es ? "Entrando al grupo..." : "Joining the group...";
    if (!mo_mm) {
        if (!in) return "";
        if (g.n < 2) snprintf(b, sizeof(b), es ? "C\u00F3digo del grupo: %s" : "Group code: %s", g.code);
        else snprintf(b, sizeof(b), "%s", es ? "Grupo listo para buscar partida." : "Group ready to search.");
        return b;
    }
    OnMatch m;
    int st = net_mm_state(&m);
    if (mo_mm_follow && st != MMS_MATCH && (st != MMS_WAIT || !m.n)) return es ? "Esperando al l\u00EDder del grupo." : "Waiting for the group leader.";
    if (st == MMS_WAIT) snprintf(b, sizeof(b), es ? (m.back ? "Un jugador sali\u00F3. Buscando (%d s)." : "Buscando partida (%d s).") : (m.back ? "A player left. Searching (%d s)." : "Searching for a match (%d s)."), m.s);
    else if (st == MMS_MATCH) snprintf(b, sizeof(b), "%s", es ? "Partida encontrada. Conectando..." : "Match found. Connecting...");
    else snprintf(b, sizeof(b), "%s", es ? "Servicio no disponible." : "Service unavailable.");
    return b;
}

static void mo_join(const char* id, const char* pw);
static void mo_grp_new() { net_mm_group(1, nullptr); mo_mm_grp = true; mo_mm_gt = 0; printf("menu: en linea, emparejamiento: crea un grupo\n"); }
static void mo_grp_join(const char* code) { net_mm_group(2, code); mo_mm_grp = mo_mm_wgrp = true; mo_mm_gt = 0; printf("menu: en linea, emparejamiento: entra al grupo %s\n", code); }
static void mo_grp_leave(const char* why) {
    if (!mo_mm_grp) return;
    mo_mm_stop(why);
    net_mm_group(3, nullptr);
    mo_mm_grp = mo_mm_wgrp = mo_mm_auto = false;
    printf("menu: en linea, emparejamiento: sale del grupo (%s)\n", why);
}

static void mo_mm_tick(const Ctx& c) {
    if (mo_mm_grp && !mo_mm) {
        OnGroup g;
        int st = net_mm_group_state(&g);
        if (++mo_mm_gt % 120 == 0 && st != MG_BUSY) net_mm_group(0, nullptr);
        if (st == MG_ERROR || st == MG_NONE) {
            if (mo_mm_wgrp || st == MG_ERROR) mo_say(MM_K_NOGROUP);
            mo_mm_grp = mo_mm_wgrp = mo_mm_auto = false;
            printf("menu: en linea, emparejamiento: sin grupo (%s)\n", st == MG_ERROR ? "error del servicio" : "el grupo ya no existe");
            return;
        }
        if (st == MG_OK && !g.lead) { mo_mm_wgrp = false; mo_mm_begin(); return; }
        if (st == MG_OK) mo_mm_wgrp = false;
        if (st == MG_OK && mo_mm_auto && g.n >= 2) { mo_mm_auto = false; mo_mm_begin(); }
        return;
    }
    if (!mo_mm || mo_op != MOP_NONE) return;
    OnMatch m;
    int st = net_mm_state(&m);
    if (st == MMS_ERROR) { mo_mm = 0; mo_say(MM_MM_SVC); printf("menu: en linea, emparejamiento terminado: servicio (%s)\n", m.why); return; }
    if (st == MMS_IDLE && !(mo_mm_follow && !m.why[0])) { mo_mm = 0; if (!strcmp(m.why, "timeout")) mo_say(MM_MM_NONE); printf("menu: en linea, emparejamiento terminado (%s)\n", m.why[0] ? m.why : "sin motivo"); return; }
    if (st != MMS_MATCH || mo_mm_done == m.mid) return;
    if (m.host) {
        mo_mm_done = m.mid;
        mo_cmode = m.mode; mo_ccust = 0; mo_cplayers = m.np; mo_cname = "Ranked"; mo_cpriv = 1; mo_cpw.clear();
        net_online_host_mm(m.mid, m.np, m.mode);
        mo_op = MOP_HOST; mo_op_t = 0; mo_mm_room = true;
        printf("menu: en linea, emparejamiento: crea la sala (%d jugadores, %s)\n", m.np, m.mode ? "supervivencia" : "batalla libre");
    } else if (m.room[0] && m.rn >= m.ord) {
        mo_mm_done = m.mid;
        mo_mm_room = true;
        mo_join(m.room, "");
        printf("menu: en linea, emparejamiento: entra a la sala %s (orden %d, en sala %d)\n", m.room, m.ord, m.rn);
    }
}

static void mo_join(const char* id, const char* pw) {
    mo_join_id = id; mo_join_pw = pw;
    net_online_join(id, pw);
    mo_op = MOP_JOIN; mo_op_t = 0;
    mo_say(MM_CONNECTING);
    printf("menu: en linea, union a la sala %s%s\n", id, *pw ? " con contrasena" : "");
}

static void mo_kdone(const Ctx& c) {
    int kind = mo_kb;
    std::string t = mo_ktext;
    mo_kb = KB_NONE;
    if (kind == KB_A_NAME) { ac_kdone(c, kind, mo_kok != 0, t); return; }
    if (!mo_kok) return;
    if (kind == KB_SEARCH_ID) { if (t.size() == 6) mo_join(t.c_str(), ""); else mo_say(MM_NOTFOUND); }
    else if (kind == KB_GROUP) { if (t.size() == 6) mo_grp_join(t.c_str()); else mo_say(MM_K_NOGROUP); }
    else if (kind == KB_JOIN_PW) mo_join(mo_join_id.c_str(), t.c_str());
    else if (kind == KB_ROOM_NAME) { if (!t.empty()) mo_cname = t; }
    else if (kind == KB_ROOM_PW) mo_cpw = t;
}

static void mo_kinput(const Ctx& c, uint32_t e) {
    int cur = mo_kcur;
    static const int up_col[4] = {0, 3, 6, 8};
    if (e & 0x10) cur = cur >= 40 ? 30 + up_col[cur - 40] : cur >= 10 ? cur - 10 : cur;
    else if (e & 0x40) { if (cur < 30) cur += 10; else if (cur < 40) { int col = cur % 10; cur = col <= 1 ? 40 : col <= 5 ? 41 : col <= 7 ? 42 : 43; } }
    else if (e & 0x80) cur = cur >= 40 ? 40 + (cur - 40 + 3) % 4 : cur / 10 * 10 + (cur % 10 + 9) % 10;
    else if (e & 0x20) cur = cur >= 40 ? 40 + (cur - 40 + 1) % 4 : cur / 10 * 10 + (cur % 10 + 1) % 10;
    if (cur != mo_kcur) { mo_kcur = cur; mo_mv(c); return; }
    int act = e & 0x4000 ? (mo_kcur < 40 ? 0 : mo_kcur - 39) : e & 0x8000 ? 3 : e & 0x1000 ? 1 : e & 0x8 ? 4 : e & 0x2000 ? 5 : -1;
    if (act < 0) return;
    if (act == 0 || act == 2) {
        char ch = act == 2 ? ' ' : mo_kchar(mo_kcur);
        if ((int)mo_ktext.size() >= mo_kmax() || !mo_kallowed(ch) || (act == 2 && (mo_kname() || mo_kb == KB_SEARCH_ID || mo_kb == KB_GROUP))) { mpm_sfx(c, 1, 3); return; }
        if (mo_kb == KB_SEARCH_ID || mo_kb == KB_GROUP) ch = (char)toupper((unsigned char)ch);
        mo_ktext += ch; mpm_sfx(c, 1, 6);
    } else if (act == 1) { mo_kshift ^= 1; mo_mv(c); }
    else if (act == 3) { if (mo_ktext.empty()) mpm_sfx(c, 1, 3); else { mo_ktext.pop_back(); mpm_sfx(c, 2, 0); } }
    else if (act == 4) { mpm_sfx(c, 1, 6); mo_kclose(c, true); }
    else { mpm_sfx(c, 2, 0); mo_kclose(c, false); }
}

static void mo_ktype(const Ctx& c) {
    for (int ch; (ch = psp_window_text_pop()) != 0;) {
        if (ch == 13) { mpm_sfx(c, 1, 6); mo_kclose(c, true); return; }
        if (ch == 27) { mpm_sfx(c, 2, 0); mo_kclose(c, false); return; }
        if (ch == 8) { if (mo_ktext.empty()) mpm_sfx(c, 1, 3); else { mo_ktext.pop_back(); mpm_sfx(c, 2, 0); } continue; }
        bool idk = mo_kb == KB_SEARCH_ID || mo_kb == KB_GROUP;
        if ((int)mo_ktext.size() >= mo_kmax() || !mo_kallowed((char)ch) || (ch == ' ' && (idk || mo_kname()))) { mpm_sfx(c, 1, 3); continue; }
        mo_ktext += idk ? (char)toupper(ch) : (char)ch;
        mpm_sfx(c, 1, 6);
    }
}

// game profile name: [0x08A7F708] + 0x40, 10 UTF-16 chars; the online account follows it
static bool mo_name_ok(const std::string& n) {
    if (n.size() < 3 || n.size() > 10) return false;
    for (char ch : n) if (!isalnum((unsigned char)ch) && ch != '-' && ch != '_' && ch != '.') return false;
    return true;
}

static std::string mo_profile() {
    uint32_t g = psp_l32(0x08A7F708u);
    std::string n;
    if (!g) return n;
    for (int i = 0; i < 10; i++) {
        uint16_t ch = psp_l16(g + 0x40 + 2 * i);
        if (!ch) break;
        n += ch < 0x80 ? (char)ch : '?';
    }
    return n;
}

static void mo_profile_set(const char* n) {
    uint32_t g = psp_l32(0x08A7F708u);
    if (!g || !*n || mo_profile() == n) return;
    int len = (int)strlen(n) < 10 ? (int)strlen(n) : 10;
    for (int i = 0; i < 10; i++) psp_s16(g + 0x40 + 2 * i, i < len ? (uint8_t)n[i] : 0);
    printf("menu: en linea, nombre del jugador = cuenta \"%s\"\n", n);
}

static std::string mo_name_seed() {
    std::string n, p = mo_profile();
    for (char ch : p) if ((isalnum((unsigned char)ch) || ch == '-' || ch == '_' || ch == '.') && n.size() < 10) n += ch;
    return n;
}

static bool mo_compat(const OnRoomInfo& r) { return r.rank ? !strcmp(net_online_rank_mods(), "0") : !strcmp(r.mods[0] ? r.mods : "0", net_online_mods()); }

static void mo_enter(const Ctx& c, bool fresh) {
    psp_net_mods_ensure();
    if (fresh) { mo_tab = TB_RANKED; mo_osel = 2; mo_focus = 1; mo_row = mo_top = mo_btn = 0; mo_msg = -1; mo_noauto = false; }
    if (mo_mm_grp && !mo_mm) net_mm_group(0, nullptr);
    mo_op = MOP_NONE; mo_refresh = 0;
    net_online_refresh();
    std::string pn = mo_profile();
    const char* acc = net_online_account();
    printf("menu: en linea, cuenta \"%s\", nombre del jugador \"%s\", mods %s\n", acc, pn.c_str(), net_online_mods());
    if (!*acc) { mo_say(MM_NEEDACC); mo_noauto = true; }
    if (mo_back_msg >= 0) { mo_say(mo_back_msg); mo_back_msg = -1; }
    if (mo_mm_re) { mo_mm_re = 0; mo_mm_begin(); }
    mo_mm_test();
}

static void mo_need_account(const Ctx& c) {
    if (!*net_online_account() && mo_msg != MM_NEEDACC) mo_say(MM_NEEDACC);
}

static void mo_close_to(const Ctx& c, int next) {
    mpm_next = next;
    mpm_state(c, mpm_hld[mpm_rootk()], 2);
    mpm_phase = PH_CLOSE;
}

// online service results while a request is pending
static void mo_tick(const Ctx& c) {
    mo_blink = (mo_blink + 1) % 30;
    if (mo_msg_t > 0 && --mo_msg_t == 0) mo_msg = -1;
    if (mpm_scr == 2 && mo_tab == TB_ROOMS && mo_op == MOP_NONE && ++mo_refresh >= 600) { mo_refresh = 0; net_online_refresh(); }
    mo_nrooms = net_online_rooms(mo_rooms, 64, &mo_lstate);
    mo_nrk = net_online_ranking(mo_rkb, mo_rk, 50, &mo_rkst, &mo_rkme);
    if (mo_rkcur >= mo_nrk) mo_rkcur = mo_nrk ? mo_nrk - 1 : 0;
    if (mo_row >= mo_nrooms) mo_row = mo_nrooms ? mo_nrooms - 1 : 0;
    if (mo_nrooms == 0 && mo_focus == 0) mo_focus = 1;
    if (mpm_scr == 2) mo_mm_tick(c);
    if (mo_op != MOP_HOST && mo_op != MOP_JOIN && mo_op != MOP_HOSTED) return;
    mo_op_t++;
    int err = 0;
    char id[8] = "";
    int st = net_online_status(&err, id);
    if (st == OS_HOSTING && mo_op == MOP_HOST) {
        mo_room_id = id; mo_op = MOP_NONE; mo_msg = -1;
        printf("menu: en linea, sala creada con ID %s, entra a la sala propia\n", id);
        mo_room_begin(1);
        mo_close_to(c, MA_TOROOM);
        return;
    }
    if (st == OS_CONNECTED && mo_op == MOP_JOIN) {
        printf("menu: en linea, conectado (sala %s), entra a la sala propia\n", mo_join_id.c_str());
        mo_op = MOP_NONE; mo_msg = -1;
        mo_room_begin(2);
        mo_close_to(c, MA_TOROOM);
    } else if (st == OS_FAILED) {
        int m = err == 404 ? MM_NOTFOUND : err == 403 ? MM_BADPW : err == 409 ? MM_FULL : err == 426 ? MM_VERSION : err == -2 ? MM_ICE : err == 401 ? MM_ACC_BAD : MM_SVC;
        printf("menu: en linea, fallo (codigo %d)\n", err);
        net_online_stop();
        bool join = mo_op == MOP_JOIN;
        mo_op = MOP_NONE;
        if (mo_mm_room) { mo_mm_room = false; net_mm_cancel(); mo_mm_begin(); printf("menu: en linea, emparejamiento: la sala fallo, vuelve a buscar\n"); return; }
        mo_say(m);
        if (join && err == 403) mo_kopen(c, KB_JOIN_PW, "");
    }
}

// mode icons of the rooms list (AGS 125 states 25 Batalla libre, 26 Seleccion de batalla); text BL/SB without them
static bool mo_icons() {
    static int on = -1;
    if (on < 0 && mpm_hld[O_ICON0]) { on = mpm_has_state(mpm_hld[O_ICON0], 26) && mpm_has_state(mpm_hld[O_ICON0], 25); printf("menu: en linea, iconos de modo %s\n", on ? "en la AGS 125" : "ausentes, se usan las siglas"); }
    return on == 1;
}

static int mo_btn_state(int i) { return mo_tab == TB_RANKED ? (mo_mm || mo_mm_wgrp ? 21 : 19) : i == 0 ? 15 : 17; }

// ranking tab (AGS 125 states 27/28 tab, 29/30 row, 31 own plate in row_06) and ranked row of Crear sala (146 states 19/20)
static bool mo_rank_ui() {
    static int on = -1;
    if (on < 0 && mpm_hld[O_TAB3]) {
        on = mpm_has_state(mpm_hld[O_TAB3], 27) && mpm_has_state(mpm_hld[O_KROW0], 31) && mpm_has_state(mpm_hld[O_RICON0], 32);
        printf("menu: en linea, clasificacion en la AGS 125 %s\n", on ? "(estados 27 a 32)" : "ausente: pak sin aplicar");
    }
    return on == 1;
}
static bool mo_rk_mine() { return mo_rkme.elo >= 0; }
static int mo_rk_vis() { return mo_rk_mine() ? 5 : 6; }
static int mo_ntabs() { return mo_rank_ui() ? 3 : 2; }
static int mo_navx() { return mo_tab == TB_RANK ? 1 : 0; }
// rooms list columns moved left of their AGS slots for equal gaps (12-char name up to x 110, ID 49 px, mode group 49 px, En sala at 262)
static const int MO_DX_ID = -11, MO_DX_MODE = -3;
static void mo_font_dx(const Ctx& c, uint32_t font, uint32_t slot, uint32_t str, int dx) {
    if (!slot || !str) return;
    mpm_call(c, 0x08832530u, font, (uint32_t)((int)psp_l32(slot + 0xE4) + dx), psp_l32(slot + 0xE8), str, 0xFFFFFFFFu, 0xFFFFFFFFu);
}
static bool mo_title_plate() { return mpm_has_state(mpm_hld[O_RTITLE], 33); }

static void mo_room_rows(const Ctx& c);
static void mo_room_attach(const Ctx& c);

static void mo_rows(const Ctx& c) {
    for (int k = O_ROOT; k < H_N; k++) mo_hide[k] = false;
    if (mpm_scr == 4) mo_room_rows(c);
    else if (mpm_scr >= 5) ac_rows(c);
    else if (mpm_scr == 2) {
        mpm_set(c, O_TAB0, 11 + (mo_tab == TB_RANKED));
        mpm_set(c, O_TAB0 + 1, 9 + (mo_tab == TB_ROOMS));
        mo_hide[O_TAB0 + 2] = true;
        mo_hide[O_TAB3] = !mo_rank_ui();
        if (!mo_hide[O_TAB3]) mpm_set(c, O_TAB3, 27 + (mo_tab == TB_RANK));
        for (int i = 0; i < 3; i++) {
            mo_hide[O_OPT0 + i] = mo_tab != TB_RANKED || i == 2 || (i == 1 && !mo_omode);
            if (!mo_hide[O_OPT0 + i]) mpm_set(c, O_OPT0 + i, 13 + (mo_osel == i));
        }
        for (int i = 0; i < 6; i++) {
            int idx = mo_top + i;
            if (mo_tab != TB_ROOMS || idx >= mo_nrooms) { mo_hide[O_ROW0 + i] = true; continue; }
            bool cur = mo_focus == 0 && mo_row == idx, ok = mo_compat(mo_rooms[idx]), full = mo_rooms[idx].n >= mo_rooms[idx].max;
            mpm_set(c, O_ROW0 + i, !ok ? (cur ? 8 : 7) : full && !cur ? 6 : cur ? 5 : 4);
        }
        for (int i = 0; i < 6; i++) {
            int idx = mo_top + i;
            mo_hide[O_ICON0 + i] = mo_hide[O_ROW0 + i] || !mo_icons();
            if (!mo_hide[O_ICON0 + i]) mpm_set(c, O_ICON0 + i, mo_rooms[idx].mode ? 26 : 25);
            mo_hide[O_RICON0 + i] = mo_hide[O_ROW0 + i] || !mo_rooms[idx].rank || !mo_rank_ui();
            if (!mo_hide[O_RICON0 + i]) mpm_set(c, O_RICON0 + i, 32);
        }
        for (int i = 0; i < 6; i++) {
            int idx = mo_rktop + i;
            if (mo_tab != TB_RANK) mo_hide[O_KROW0 + i] = true;
            else if (i < mo_rk_vis() && idx < mo_nrk) mpm_set(c, O_KROW0 + i, idx == mo_rkcur ? 30 : 29);
            else if (i == 5 && mo_rk_mine()) mpm_set(c, O_KROW0 + i, 31);
            else mo_hide[O_KROW0 + i] = true;
        }
        mo_hide[O_RTITLE] = mo_tab != TB_RANK || !mo_title_plate();
        if (!mo_hide[O_RTITLE]) mpm_set(c, O_RTITLE, 33);
        mo_hide[O_RTICON] = mo_tab != TB_RANK;
        if (!mo_hide[O_RTICON]) mpm_set(c, O_RTICON, mo_rkb ? 26 : 25);
        for (int i = 0; i < 2; i++) {
            bool sel = mo_tab == TB_RANKED ? mo_osel == 2 : mo_focus == 1 && mo_btn == i;
            mo_hide[O_BTN0 + i] = mo_tab == TB_RANK || (mo_tab == TB_RANKED && i == 1);
            if (!mo_hide[O_BTN0 + i]) mpm_set(c, O_BTN0 + i, mo_btn_state(i) + sel);
        }
        mpm_set(c, O_UP, 23); mpm_set(c, O_DOWN, 24);
        mo_hide[O_UP] = !(mo_tab == TB_ROOMS ? mo_top > 0 : mo_tab == TB_RANK && mo_rktop > 0);
        mo_hide[O_DOWN] = !(mo_tab == TB_ROOMS ? mo_top + 6 < mo_nrooms : mo_tab == TB_RANK && mo_rktop + mo_rk_vis() < mo_nrk);
        mpm_set(c, O_BOX, 3);
        mo_hide[O_AV] = net_account_kind() != AK_ACCOUNT || !av_want() || !mpm_has_state(mpm_hld[O_AV], 34);
        if (!mo_hide[O_AV]) mpm_set(c, O_AV, 34);
    } else if (mpm_scr == 3) {
        static const int st[5] = {4, 12, 6, 14, 8};
        for (int k = 0; k < 5; k++) mpm_set(c, C_OPT0 + k, st[k] + (mo_csel == k));
        mo_hide[C_OPT0 + 5] = true;
        bool pub = mo_create_public();
        mo_hide[C_ROOT] = pub; mo_hide[C_ROOTP] = !pub;
        mpm_set(c, C_BTN, 10 + (mo_csel == 6));
        mpm_set(c, C_BOX, 3);
    }
    if (mo_kph != KP_OFF) {
        for (int i = 0; i < 40; i++) mpm_set(c, K_KEY0 + i, 4 + (mo_kcur == i));
        for (int j = 0; j < 4; j++) mpm_set(c, K_SPEC0 + j, 6 + 2 * j + (mo_kcur == 40 + j));
    }
}

static void mo_attach(const Ctx& c) {
    if (mpm_scr == 4) mo_room_attach(c);
    else if (mpm_scr >= 5) ac_attach(c);
    else if (mpm_scr == 2) {
        uint32_t r = mpm_hld[O_ROOT];
        for (int k = 0; k < 2; k++) mpm_hslot(c, O_TAB0 + k, r, mo_sn[MS_TAB1 + k]);
        for (int i = 0; i < 3; i++) mpm_hslot(c, O_OPT0 + i, r, mo_sn[MS_ROW1 + i]);
        for (int i = 0; i < 6; i++) mpm_hslot(c, O_ROW0 + i, r, mo_sn[MS_ROW1 + i]);
        for (int i = 0; i < 6; i++) mpm_hslot(c, O_ICON0 + i, mpm_hld[O_ROW0 + i], mo_sn[MS_RICON]);
        mpm_hslot(c, O_TAB3, r, mo_sn[MS_TAB1 + 2]);
        for (int i = 0; i < 6; i++) mpm_hslot(c, O_KROW0 + i, r, mo_sn[MS_ROW1 + i]);
        for (int i = 0; i < 6; i++) {
            bool rk = mo_top + i < mo_nrooms && mo_rooms[mo_top + i].rank && mo_rank_ui();
            int left = MO_DX_MODE + (rk ? mo_mode_c - 24 : mo_mode_c - 12);
            mpm_hslot(c, O_RICON0 + i, mpm_hld[O_ROW0 + i], mo_sn[MS_RICON]);
            if (mpm_hld[O_ICON0 + i]) { psp_s32(mpm_hld[O_ICON0 + i] + 0x18, (uint32_t)(left - 2)); psp_s32(mpm_hld[O_ICON0 + i] + 0x1C, 0); }
            if (mpm_hld[O_RICON0 + i]) { psp_s32(mpm_hld[O_RICON0 + i] + 0x18, (uint32_t)(left + 25 + 3 - 4)); psp_s32(mpm_hld[O_RICON0 + i] + 0x1C, 0); }
        }
        uint32_t tp = mo_title_plate() ? mpm_hld[O_RTITLE] : 0;
        mpm_hslot(c, O_RTITLE, r, mo_sn[MS_BTN1]);
        if (tp) mpm_hslot(c, O_RTICON, tp, mo_sn[MS_RICON]);
        else mpm_hslot(c, O_RTICON, r, mo_sn[MS_BTN1]);
        if (mpm_hld[O_RTICON]) { psp_s32(mpm_hld[O_RTICON] + 0x18, tp ? 0 : 4); psp_s32(mpm_hld[O_RTICON] + 0x1C, tp ? 0 : 2); }
        for (int i = 0; i < 2; i++) mpm_hslot(c, O_BTN0 + i, r, mo_sn[MS_BTN1 + i]);
        mpm_hslot(c, O_UP, r, mo_sn[MS_UP]);
        mpm_hslot(c, O_DOWN, r, mo_sn[MS_DOWN]);
        mpm_hslot(c, O_BOX, r, S_SETSUMEI);
        mpm_hslot(c, O_AV, r, mo_sn[MS_AVATAR]);
    } else if (mpm_scr == 3) {
        uint32_t r = mpm_hld[mpm_rootk()];
        static const int sl[5] = {MS_ONAME, MS_OMODE, MS_OPLAYERS, MS_OCUSTOM, MS_OPW};
        for (int k = 0; k < 5; k++) mpm_hslot(c, C_OPT0 + k, r, mo_sn[sl[k]]);
        mpm_hslot(c, C_BTN, r, mo_sn[MS_BCREATE]);
        mpm_hslot(c, C_BOX, r, S_SETSUMEI);
    }
    if (mo_kph != KP_OFF) {
        uint32_t r = mpm_hld[K_ROOT];
        for (int i = 0; i < 40; i++) mpm_hslot(c, K_KEY0 + i, r, mo_sn[MS_KEYCAP + i]);
        for (int j = 0; j < 4; j++) mpm_hslot(c, K_SPEC0 + j, r, mo_sn[MS_KSPEC + j]);
    }
}

static void mo_activate(const Ctx& c) {
    if (mo_focus == 0) {
        const OnRoomInfo& r = mo_rooms[mo_row];
        if (!mo_compat(r)) { mpm_sfx(c, 1, 3); mo_say(MM_MODS); return; }
        if (r.n >= r.max) { mpm_sfx(c, 1, 3); mo_say(MM_FULL); return; }
        mpm_sfx(c, 1, 6);
        mo_join(r.id, "");
        return;
    }
    if (mo_btn == 0) {
        mpm_sfx(c, 1, 6);
        mo_cpriv = 0; mo_csel = 0; mo_cplayers = 2; mo_cmode = 0; mo_ccust = 0; mo_cname = net_online_account(); mo_cpw.clear();
        mo_close_to(c, MA_TOCREATE);
        return;
    }
    mpm_sfx(c, 1, 6);
    mo_kopen(c, KB_SEARCH_ID, "");
}

static void mo_deny(const Ctx& c, int msg) { mpm_sfx(c, 1, 3); if (msg >= 0) mo_say(msg); }

// Ranked tab: row 0 mode (1 vs 1, 2 vs 2, Supervivencia), row 1 partner (only for 2 vs 2 and Supervivencia), 2 the button
static void mo_input_ranked(const Ctx& c, uint32_t e) {
    OnGroup g;
    bool in = mo_grp_in(&g), member = in && !g.lead, busy = mo_mm || mo_mm_wgrp;
    if (e & 0x50) {
        int d = e & 0x40 ? 1 : 2;
        do mo_osel = (mo_osel + d) % 3; while (mo_osel == 1 && !mo_omode);
        mo_msg = -1; mo_mv(c);
        return;
    }
    if (mo_osel < 2 && e & 0xA0) {
        if (member) { mo_deny(c, MM_K_LEADER); return; }
        if (busy) { mo_deny(c, -1); return; }
        int d = e & 0x20 ? 1 : 2;
        if (mo_osel == 0) {
            int m = mo_omode;
            do m = (m + d) % 3; while (m == 0 && (in || mo_mm_grp));
            if (m == mo_omode) { mo_deny(c, MM_K_FIXED); return; }
            mo_omode = m; mo_mv(c);
        } else if (in) mo_deny(c, MM_K_LEAVE);
        else { mo_ogrp = (mo_ogrp + d) % 3; mo_mv(c); }
        return;
    }
    if (!(e & 0x4000)) return;
    if (mo_osel == 0) { mo_osel = mo_omode ? 1 : 2; mo_mv(c); return; }
    if (mo_osel == 1) {
        if (in || mo_mm_wgrp) { if (mo_mm && !member) { mo_deny(c, -1); return; } mpm_sfx(c, 2, 0); mo_grp_leave("salida del jugador"); return; }
        if (busy) { mo_deny(c, -1); return; }
        if (mo_ogrp == 0) { mo_osel = 2; mo_mv(c); return; }
        mpm_sfx(c, 1, 6);
        if (mo_ogrp == 1) mo_grp_new(); else mo_kopen(c, KB_GROUP, "");
        return;
    }
    if (busy) { mpm_sfx(c, 2, 0); if (member) mo_grp_leave("cancelado por el jugador"); else mo_mm_stop("cancelado por el jugador"); return; }
    if (member) { mo_deny(c, MM_K_LEADER); return; }
    if (mo_mm_grp && (!in || g.n < 2)) { mo_deny(c, MM_K_WAIT); return; }
    if (strcmp(net_online_rank_mods(), "0")) { mo_deny(c, MM_K_RMODS); printf("menu: en linea, clasificatoria bloqueada por mods de personajes o mapas\n"); return; }
    mo_mm_surv = mo_omode == 2; mo_mm_fmt = mo_omode ? 2 : 1;
    mpm_sfx(c, 1, 6);
    mo_mm_begin();
}

static void mo_input_rooms(const Ctx& c, uint32_t e) {
    if (e & 0x300) {
        int n = mo_ntabs();
        mo_tab = (mo_tab + (e & 0x200 ? 1 : n - 1)) % n;
        mo_focus = mo_tab == TB_ROOMS && mo_nrooms ? 0 : 1; mo_btn = 0; mo_msg = -1;
        if (mo_tab == TB_ROOMS) net_online_refresh();
        if (mo_tab == TB_RANK) { mo_rkcur = mo_rktop = 0; net_online_ranking_refresh(mo_rkb); }
        mo_mv(c);
        return;
    }
    if (e & 0x1000) {
        if (mo_tab == TB_ROOMS) { net_online_refresh(); mo_refresh = 0; mo_mv(c); }
        else if (mo_tab == TB_RANK) { net_online_ranking_refresh(mo_rkb); mo_mv(c); }
        return;
    }
    if (e & 0x2000) {
        OnGroup g;
        mpm_sfx(c, 2, 0);
        if (mo_mm || mo_mm_wgrp) { if (mo_grp_in(&g) && !g.lead) mo_grp_leave("cancelado por el jugador"); else mo_mm_stop("cancelado por el jugador"); return; }
        mo_grp_leave("sale de En linea");
        net_online_stop(); mo_close_to(c, MA_TOSUB);
        return;
    }
    if (mo_tab == TB_RANKED) { mo_input_ranked(c, e); return; }
    if (mo_tab == TB_RANK) {
        if (e & 0x8000) { mo_rkb ^= 1; mo_rkcur = mo_rktop = 0; net_online_ranking_refresh(mo_rkb); mo_mv(c); return; }
        if (e & 0x10 && mo_rkcur > 0) { mo_rkcur--; mo_mv(c); }
        else if (e & 0x40 && mo_rkcur + 1 < mo_nrk) { mo_rkcur++; mo_mv(c); }
        if (mo_rkcur < mo_rktop) mo_rktop = mo_rkcur;
        if (mo_rkcur >= mo_rktop + mo_rk_vis()) mo_rktop = mo_rkcur - mo_rk_vis() + 1;
        return;
    }
    if (e & 0x4000) { mo_activate(c); return; }
    if (mo_focus == 0) {
        if (e & 0x10 && mo_row > 0) { mo_row--; mo_mv(c); }
        else if (e & 0x40 && mo_row + 1 < mo_nrooms) { mo_row++; mo_mv(c); }
        else if (e & 0x20) { mo_focus = 1; mo_mv(c); }
        if (mo_row < mo_top) mo_top = mo_row;
        if (mo_row >= mo_top + 6) mo_top = mo_row - 5;
    } else {
        if (e & 0x50) { mo_btn ^= 1; mo_mv(c); }
        else if (e & 0x80 && mo_nrooms) { mo_focus = 0; mo_mv(c); }
    }
}

static void mo_input_create(const Ctx& c, uint32_t e) {
    if (e & 0x2000) { mpm_sfx(c, 2, 0); mo_close_to(c, MA_TOONLINE); return; }
    if (e & 0x50) {
        static const int ord[6] = {0, 1, 2, 3, 4, 6};
        int p = 0, d = e & 0x40 ? 1 : 5;
        while (ord[p] != mo_csel) p++;
        mo_csel = ord[(p + d) % 6];
        mo_mv(c);
        return;
    }
    if (mo_csel == 2 && e & 0xA0) {
        int v = mo_cplayers + (e & 0x20 ? 1 : -1);
        if (mo_cmode) v = 2;
        if (v >= 2 && v <= 4) { mo_cplayers = v; mo_mv(c); } else mpm_sfx(c, 1, 3);
        return;
    }
    if ((mo_csel == 1 || mo_csel == 3) && e & 0x40A0) {
        int& v = mo_csel == 1 ? mo_cmode : mo_ccust;
        v ^= 1; mo_mv(c);
        if (mo_cmode) mo_cplayers = 2;
        return;
    }
    if (!(e & 0x4000)) return;
    if (mo_csel == 0) { mpm_sfx(c, 1, 6); mo_kopen(c, KB_ROOM_NAME, mo_cname); }
    else if (mo_csel == 4) { mpm_sfx(c, 1, 6); mo_kopen(c, KB_ROOM_PW, mo_cpw); }
    else if (mo_csel == 6) {
        mpm_sfx(c, 1, 6);
        mo_cpriv = !mo_cpw.empty();
        net_online_set_game_mode(mo_cmode, mo_ccust);
        net_online_host(mo_cname.c_str(), mo_cpw.c_str(), mo_cplayers, !mo_cpriv);
        mo_op = MOP_HOST; mo_op_t = 0;
        mo_say(MM_CREATING);
        printf("menu: en linea, crear sala \"%s\" (%s, %d jugadores, modo %d, personalizado %d)\n", mo_cname.c_str(), mo_cpriv ? "privada con contrasena" : "publica", mo_cplayers, mo_cmode, mo_ccust);
    }
}

static void mo_room_input(const Ctx& c, uint32_t e);

static void mo_input(const Ctx& c) {
    uint32_t e = psp_l32(0x08B4E990u + 0x14);
    if (mpm_scr == 4) { mo_room_input(c, e); return; }
    mo_tick(c);
    if (mo_kph == KP_NORMAL) { mo_ktype(c); if (mo_kph == KP_NORMAL) mo_kinput(c, e); return; }
    if (mo_kph != KP_OFF) return;
    if (mpm_scr >= 5) { ac_input(c, e); return; }
    if (mo_noauto && e & 0x4000) mo_noauto = false;
    mo_need_account(c);
    if (mo_kph != KP_OFF) return;
    if (mo_op != MOP_NONE) {
        if (e & 0x2000) { mpm_sfx(c, 2, 0); net_online_stop(); mo_op = MOP_NONE; mo_msg = -1; if (mo_mm_room) { mo_mm_room = false; mo_mm_stop("cancelado al conectar"); } printf("menu: en linea, espera cancelada\n"); }
        return;
    }
    if (mpm_scr == 2) mo_input_rooms(c, e); else mo_input_create(c, e);
}

static void mo_kanim(const Ctx& c) {
    if (mo_kph == KP_OPEN && mpm_ended(c, mpm_hld[K_ROOT])) { mpm_state(c, mpm_hld[K_ROOT], 1); mo_kph = KP_NORMAL; }
    else if (mo_kph == KP_CLOSE && mpm_ended(c, mpm_hld[K_ROOT])) { mo_kph = KP_OFF; mo_kdone(c); }
}

static std::string mo_cut(const char* s, int n) { std::string t = s; if ((int)t.size() > n) t = t.substr(0, n); return t; }

static const char* mo_mode_name(int m, uint32_t lang, bool rank = false) {
    static const char* MD[3][2] = {{"Free Battle", "Batalla libre"}, {"Battle Select", "Selecci\u00F3n de batalla"}, {"Survival", "Supervivencia"}};
    return m < 0 ? "-" : MD[(m & 1) && rank ? 2 : m & 1][mpm_li(lang)];
}
static const char* mo_yes_no(int v, uint32_t lang) {
    static const char* YN[2][2] = {{"No", "No"}, {"Yes", "S\u00ED"}};
    return v < 0 ? "-" : YN[v & 1][mpm_li(lang)];
}
static void mo_room_draw(const Ctx& c, uint32_t font, uint32_t lang);

// headers have no slots: list x of each column + 4 at y 76, moved with the root like head_name (30,76)
static void mo_rank_draw(const Ctx& c, uint32_t font, uint32_t lang) {
    uint32_t r = mpm_hld[O_ROOT], hs = mpm_slot(c, r, mo_sn[MS_HEAD]);
    int dx = hs ? (int)psp_l32(hs + 0xE4) - 30 : 0, dy = hs ? (int)psp_l32(hs + 0xE8) - 76 : 0;
    static const char* HK[2][5][2] = {{{"Pos.", "Pos."}, {"Name", "Nombre"}, {"Points", "Puntos"}, {"W", "V"}, {"L", "D"}},
                                      {{"Pos.", "Pos."}, {"Name", "Nombre"}, {"Points", "Puntos"}, {"Best", "R\u00E9cord"}, {"Runs", "Partidas"}}};
    static const int HX[5] = {28, 66, 166, 214, 262};
    for (int k = 0; k < 5; k++) mpm_call(c, 0x08832530u, font, (uint32_t)(HX[k] + dx), (uint32_t)(76 + dy), mpm_utf16(HK[mo_rkb][k][mpm_li(lang)], 93 + k), 0xFFFFFFFFu, 0xFFFFFFFFu);
    static const char* TT[2][2] = {{"Free Battle", "Batalla libre"}, {"Survival", "Supervivencia"}};
    uint32_t tp = mo_title_plate() ? mpm_slot(c, mpm_hld[O_RTITLE], mo_sn[MS_RTITLE]) : 0, bs = mpm_slot(c, r, mo_sn[MS_BTN1]);
    if (tp) mpm_font_at(c, font, tp, mpm_utf16(TT[mo_rkb][mpm_li(lang)], 171));
    else if (bs) mpm_call(c, 0x08832530u, font, psp_l32(bs + 0xE4) + 40, psp_l32(bs + 0xE8) + 5, mpm_utf16(TT[mo_rkb][mpm_li(lang)], 171), 0xFFFFFFFFu, 0xFFFFFFFFu);
    static const int SL[5] = {MS_RPOS, MS_NAME, MS_RELO, MS_RW, MS_RL};
    for (int i = 0; i < 6; i++) {
        if (mo_hide[O_KROW0 + i]) continue;
        int idx = mo_rktop + i;
        const OnRankRow& m = i < mo_rk_vis() && idx < mo_nrk ? mo_rk[idx] : mo_rkme;
        char v[5][24];
        if (m.pos > 0) snprintf(v[0], 24, "%d", m.pos); else snprintf(v[0], 24, "-");
        snprintf(v[1], 24, "%s", mo_cut(m.name, 12).c_str());
        snprintf(v[2], 24, "%d", m.elo); snprintf(v[3], 24, "%d", m.w); snprintf(v[4], 24, "%d", m.l);
        uint32_t h = mpm_hld[O_KROW0 + i];
        for (int j = 0; j < 5; j++) mo_font_dx(c, font, mpm_slot(c, h, mo_sn[SL[j]]), mpm_utf16(v[j], 140 + 5 * i + j), j == 0 && mpm_cur_state(h) == 31 ? 6 : 0);
    }
}

// text centred in the grey panel of AGS 125 (root rect (0, 72, 312, 166); head_name (30, 76) gives the root offset)
static void mo_panel_text(const Ctx& c, uint32_t font, const char* t) {
    uint32_t hs = mpm_slot(c, mpm_hld[O_ROOT], mo_sn[MS_HEAD]), str = mpm_utf16(t, 12);
    if (!hs) return;
    int dx = (int)psp_l32(hs + 0xE4) - 30, dy = (int)psp_l32(hs + 0xE8) - 76, w = (int)mpm_call(c, 0x08831F68u, font, str);
    mpm_call(c, 0x08832530u, font, (uint32_t)(dx + (312 - w) / 2), (uint32_t)(dy + 72 + (166 - 13) / 2), str, 0xFFFFFFFFu, 0xFFFFFFFFu);
}

static void mo_text_mid(const Ctx& c, uint32_t font, uint32_t slot, uint32_t str, int w) {
    if (!slot) return;
    int tw = (int)mpm_call(c, 0x08831F68u, font, str);
    mpm_call(c, 0x08832530u, font, (uint32_t)((int)psp_l32(slot + 0xE4) + (w - tw) / 2), psp_l32(slot + 0xE8), str, 0xFFFFFFFFu, 0xFFFFFFFFu);
}

static void mo_ranked_draw(const Ctx& c, uint32_t font, uint32_t lang) {
    int li = mpm_li(lang);
    static const char* LB[2][2] = {{"Mode", "Modo"}, {"Partner", "Compa\u00F1ero"}};
    static const char* MD[3][2] = {{"1 vs 1", "1 vs 1"}, {"2 vs 2", "2 vs 2"}, {"Survival", "Supervivencia"}};
    static const char* GR[3][2] = {{"Random", "Al azar"}, {"Create group", "Crear grupo"}, {"Join with code", "Unirse con c\u00F3digo"}};
    OnGroup g;
    bool in = mo_grp_in(&g);
    char gv[48];
    if (mo_mm_wgrp) snprintf(gv, sizeof(gv), "...");
    else if (in) {
        const char* me = net_online_account(), *o = g.n >= 2 ? (strcmp(g.names[0], me) ? g.names[0] : g.names[1]) : (li ? "(esperando)" : "(waiting)");
        snprintf(gv, sizeof(gv), "%s  %s", g.code, mo_cut(o, 12).c_str());
    } else snprintf(gv, sizeof(gv), "%s", GR[mo_ogrp][li]);
    const char* v[2] = {MD[mo_omode][li], gv};
    for (int i = 0; i < 2; i++) {
        if (mo_hide[O_OPT0 + i]) continue;
        uint32_t h = mpm_hld[O_OPT0 + i];
        mpm_font_at(c, font, mpm_slot(c, h, mo_sn[MS_TLABEL]), mpm_utf16(LB[i][li], 16 + 2 * i));
        mo_text_mid(c, font, mpm_slot(c, h, mo_sn[MS_TVALUE]), mpm_utf16(v[i], 17 + 2 * i), 142);
    }
}

static void mo_draw_text(const Ctx& c, uint32_t font, uint32_t lang) {
    if (mpm_scr == 4) { mo_room_draw(c, font, lang); return; }
    if (mpm_scr >= 5) { ac_draw(c, font, lang); return; }
    if (mpm_scr == 2) {
        uint32_t r = mpm_hld[O_ROOT];
        mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_ACCOUNT]), mpm_utf16(net_online_account(), 11));
        if (mo_tab == TB_RANKED) mo_ranked_draw(c, font, lang);
        std::string mmt = mo_mm_text(lang);
        if (!mmt.empty()) mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_TSTATUS]), mpm_utf16(mmt.c_str(), 22));
        if (mo_tab == TB_ROOMS) {
            static const char* HD[5][2] = {{"Name", "Nombre"}, {"Host", "Anfitri\u00F3n"}, {"ID", "ID"}, {"Mode", "Modo"}, {"Players", "En sala"}};
            for (int k = 0; k < 5; k++) if (k != 1) mo_font_dx(c, font, mpm_slot(c, r, mo_sn[MS_HEAD + k]), mpm_utf16(HD[k][mpm_li(lang)], 93 + k), k == 2 ? MO_DX_ID : k == 3 ? MO_DX_MODE : 0);
            mo_mode_c = (int)mpm_call(c, 0x08831F68u, font, mpm_utf16(HD[3][mpm_li(lang)], 96)) / 2;
        }
        if (mo_tab == TB_ROOMS) {
            for (int i = 0; i < 6 && mo_top + i < mo_nrooms; i++) {
                const OnRoomInfo& m = mo_rooms[mo_top + i];
                uint32_t h = mpm_hld[O_ROW0 + i];
                char pl[16];
                snprintf(pl, sizeof(pl), "%d/%d", m.n, m.max);
                mpm_font_at(c, font, mpm_slot(c, h, mo_sn[MS_NAME]), mpm_utf16(mo_cut(m.name, 12).c_str(), 16 + 4 * i));
                mo_font_dx(c, font, mpm_slot(c, h, mo_sn[MS_ID]), mpm_utf16(m.id, 18 + 4 * i), MO_DX_ID);
                mpm_font_at(c, font, mpm_slot(c, h, mo_sn[MS_PLAYERS]), mpm_utf16(pl, 19 + 4 * i));
                if (!mo_icons()) mo_font_dx(c, font, mpm_slot(c, h, mo_sn[MS_TMODE]), mpm_utf16(m.mode ? "SB" : "BL", 87 + i), MO_DX_MODE);
            }
        }
        if (mo_tab == TB_RANK) mo_rank_draw(c, font, lang);
        int empty = mo_tab == TB_RANKED ? -1 : mo_tab == TB_RANK ? (mo_nrk ? -1 : mo_rkst == 3 ? MM_SVC : mo_rkst == 2 ? MM_RK_EMPTY : MM_RK_LOADING)
                  : mo_nrooms ? -1 : mo_lstate == 3 ? MM_SVC : mo_lstate == 1 && !mo_nrooms ? MM_LOADING : MM_EMPTY;
        if (empty >= 0) mo_panel_text(c, font, mo_t(empty, lang));
        OnGroup g;
        int rk = mo_osel == 0 ? MM_K_MODE : mo_osel == 1 ? (mo_grp_in(&g) ? MM_K_LEAVE : MM_K_GROUP) : mo_grp_in(&g) && !g.lead ? MM_K_LEAVE : mo_mm || mo_mm_wgrp ? MM_K_CANCEL : MM_K_FIND;
        int box = mo_msg >= 0 ? mo_msg : mo_tab == TB_RANKED ? rk : mo_tab == TB_RANK ? MM_RANK_INFO : mo_focus == 0 ? MM_LIST : mo_btn == 0 ? MM_CREATE : MM_SEARCH_ID;
        mpm_font_lines(c, font, mpm_slot(c, mpm_hld[O_BOX], S_TEXT), mo_t(box, lang), 19, 8);
    } else if (mpm_scr == 3) {
        uint32_t r = mpm_hld[mpm_rootk()];
        char pl[8];
        snprintf(pl, sizeof(pl), "%d", mo_cplayers);
        mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_TRNAME]), mpm_utf16(mo_cname.c_str(), 82));
        mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_PLAYERS]), mpm_utf16(pl, 83));
        mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_TMODE]), mpm_utf16(mo_mode_name(mo_cmode, lang), 85));
        mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_TCUSTOM]), mpm_utf16(mo_yes_no(mo_ccust, lang), 86));
        mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_TPW]), mpm_utf16(mo_cpw.empty() ? "-" : std::string(mo_cpw.size(), '*').c_str(), 84));
        static const int HB[7] = {MM_C_NAME, MM_C_MODE, MM_C_PLAYERS, MM_C_CUSTOM, MM_C_PW, MM_C_CREATE, MM_C_CREATE};
        int box = mo_msg >= 0 ? mo_msg : HB[mo_csel];
        mpm_font_lines(c, font, mpm_slot(c, mpm_hld[C_BOX], S_TEXT), mo_t(box, lang), 60, 8);
    }
}

static void mo_draw_kb_text(const Ctx& c, uint32_t font, uint32_t lang) {
    if (mo_kph != KP_NORMAL) return;
    uint32_t r = mpm_hld[K_ROOT];
    mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_PROMPT]), mpm_utf16(mo_prompt(lang), 40));
    std::string t = mo_kpw() ? std::string(mo_ktext.size(), '*') : mo_ktext;
    if (mo_blink < 15 && (int)mo_ktext.size() < mo_kmax()) t += '_';
    mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_INPUT]), mpm_utf16(t.c_str(), 41));
    for (int i = 0; i < 40; i++) {
        uint32_t s = mpm_slot(c, r, mo_sn[MS_KEY + i]);
        if (!s) continue;
        char b[2] = {mo_kchar(i), 0};
        mpm_call(c, 0x08832530u, font, psp_l32(s + 0xE4) + 11, psp_l32(s + 0xE8), mpm_utf16(b, 42 + i), 0xFFFFFFFFu, 0xFFFFFFFFu);
    }
}

// room info (0x088118AC, 0x1D4 bytes, sent whole as the HELLO data): 4 members of 0x74 from +0 (name UTF-16 +0, MAC +0x40), +0x1D0 role,
// +0x1D1 count, +0x1D2 max; the original "Servidor" shows member 0's name, our room shows the online room name instead
static std::string mo_u16(uint32_t a, int n) { std::string s; for (int i = 0; i < n; i++) { uint16_t ch = psp_l16(a + 2 * i); if (!ch) break; s += ch < 0x80 ? (char)ch : '?'; } return s; }

// own online room (AGS 153): drives the game network layer like the room child of 56_38 (+0x304, sub-states 0x08ABCA00)
enum { RP_CONNECT, RP_JOINING, RP_ROOM, RP_START, RP_LEAVE, RP_GO, RP_FAIL };
enum { DL_NONE, DL_REQUEST, DL_START, DL_CLOSE, DL_WAIT, DL_INFO };
enum { RT_PREP, RT_ALONE, RT_READY, RT_GUEST, RT_STARTING, RT_NEED2, RT_LEAVING, RT_CONNECTING, RT_REQ, RT_REQ2, RT_QSTART, RT_QCLOSE,
       RT_QCLOSE2, RT_QLEAVE, RT_WAIT, RT_ACCEPT, RT_REJECT, RT_YES, RT_NO, RT_CANCEL, RT_HOST, RT_OK, RT_N };
static int mo_rph = RP_CONNECT, mo_rfail = -1, mo_dsel = 0, mo_rdelay = 0, mo_rleave_t = 0, mo_rcount = -1, mo_rmode = -1, mo_rcust = -1, mo_rmsg_t = 0;
static bool mo_rconn = false;
static uint32_t mo_rt0 = 0;
static std::string mo_rid, mo_rserver;

static const char* mo_rt(int id, uint32_t lang) {
    static const char* T[RT_N][2] = {
        {"Preparing the room...", "Preparando la sala..."},
        {"Waiting for other players.", "Esperando a otros jugadores."},
        {"Start the battle when everyone is here.", "Comienza la batalla cuando est\u00E9n todos."},
        {"Waiting for the host to start the battle.", "Esperando a que el anfitri\u00F3n comience la batalla."},
        {"Starting the battle...", "Comenzando la batalla..."},
        {"At least 2 players are needed.", "Se necesitan al menos 2 jugadores."},
        {"Leaving the room...", "Saliendo de la sala..."},
        {"Connecting to the room...", "Conectando con la sala..."},
        {"%s wants to join the room.", "%s quiere unirse a la sala."},
        {"Accept this player?", "\u00BFAceptar al jugador?"},
        {"Start the battle?", "\u00BFComenzar la batalla?"},
        {"Close the room?", "\u00BFCerrar la sala?"},
        {"Everyone inside will leave.", "Todos los jugadores saldr\u00E1n."},
        {"Leave the room?", "\u00BFSalir de la sala?"},
        {"Waiting for the host to accept...", "Esperando a que el anfitri\u00F3n acepte..."},
        {"Accept", "Aceptar"}, {"Reject", "Rechazar"}, {"Yes", "S\u00ED"}, {"No", "No"},
        {"Cancel", "Cancelar"}, {"Host", "Anfitri\u00F3n"}, {"OK", "Aceptar"}};
    return T[id][mpm_li(lang)];
}

static bool mo_host() { return mo_role == 1; }
static uint32_t mo_req() { return mpm_mem + 0xD000; }
static unsigned mo_rms() { return (unsigned)(((uint32_t)psp_time_us() - mo_rt0) / 1000); }
static uint32_t mo_rinfo(const Ctx& c) { uint32_t net = mpm_call(c, 0x08811A58u); return net ? mpm_call(c, 0x088118ACu, net) : 0; }
static int mo_rcnt(uint32_t ri) { return ri ? (int8_t)psp_l8(ri + 0x1D1) : 0; }
static int mo_rmax(uint32_t ri) { return ri ? (int8_t)psp_l8(ri + 0x1D2) : 0; }

static int mo_rown(uint32_t ri) {
    if (mo_host()) return 0;
    std::string me = mo_profile();
    for (int i = 0; i < mo_rcnt(ri) && i < 4; i++) if (mo_u16(ri + 0x74 * i, 32) == me) return i;
    return -1;
}

static void mo_room_begin(int role) {
    mo_role = role; mo_rph = RP_CONNECT; mo_dlg = DL_NONE; mo_dsel = 0; mo_rdelay = 0; mo_rcount = -1; mo_rconn = false; mo_rmsg_t = 0;
    mo_rt0 = (uint32_t)psp_time_us();
    mo_rid = role == 1 ? mo_room_id : mo_join_id;
    mo_rmode = role == 1 ? mo_cmode : -1;
    mo_rcust = role == 1 ? mo_ccust : -1;
    mo_rserver = role == 1 ? mo_cname : "";
    OnRoomInfo ji;
    if (role == 2 && net_online_joined(&ji)) { mo_rmode = ji.mode; mo_rcust = ji.cust; mo_rserver = ji.name; }
}

static void mo_room_enter(const Ctx& c) {
    printf("menu: sala, abierta como %s (sala %s, red %u, %u ms)\n", mo_host() ? "anfitrion" : "invitado", mo_rid.c_str(), mpm_net_state(c), mo_rms());
}

// back to scene 8 page 0 after an online battle with the game room still alive
static bool mo_room_resume(const Ctx& c) {
    if (!mo_role || !net_online_on()) return false;
    uint32_t st = mpm_net_state(c);
    if (st != 6 && st != 7) { printf("menu: sala, vuelta a la escena con red %u: no se reabre la sala\n", st); return false; }
    mo_rph = RP_ROOM; mo_dlg = DL_NONE; mo_rcount = -1;
    printf("menu: sala, se reabre tras la batalla (red %u)\n", st);
    return true;
}

static void mo_dlg_open(int d) { mo_dlg = d; mo_dsel = d == DL_CLOSE ? 1 : 0; }

static void mo_room_exit(const Ctx& c, int msg, const char* why) {
    if (mpm_next != MA_NONE) return;
    uint32_t net = mpm_call(c, 0x08811A58u);
    printf("menu: sala, vuelve a la pantalla en linea: %s (red %u, %u ms)\n", why, mpm_net_state(c), mo_rms());
    if (net) mpm_call(c, 0x08811568u, net);
    net_online_stop();
    if (net && mpm_reset_on()) mpm_net_reset = 1;
    mo_role = 0; mo_dlg = DL_NONE; mo_rph = RP_CONNECT;
    mo_back_msg = msg;
    if (mo_mm_room) { mo_mm_room = false; if (msg != -2) mo_mm_stop(why); else mo_back_msg = -1; }
    mo_close_to(c, MA_TOONLINE);
}

// a failure shows a popup in the room; confirming it returns to En linea
static void mo_room_fail(int msg, const char* why) {
    if (mo_rph == RP_FAIL) return;
    printf("menu: sala, aviso antes de volver: %s\n", why);
    mo_rph = RP_FAIL; mo_rfail = msg; mo_dlg = DL_INFO; mo_dsel = 0;
}

static void mo_room_log(int st, uint32_t ri) {
    static int last[5] = {-1, -1, -1, -1, -1};
    int v[5] = {mo_rph, st, mo_rcnt(ri), mo_rmax(ri), mo_dlg};
    if (!memcmp(v, last, sizeof(v))) return;
    memcpy(last, v, sizeof(v));
    static const char* PH[7] = {"conectando", "esperando aceptacion", "en sala", "inicio", "saliendo", "a batalla", "aviso"};
    printf("menu: sala, fase %s, red %d, miembros %d/%d, cuadro %d (%u ms)\n", PH[mo_rph], st, v[2], v[3], mo_dlg, mo_rms());
}

static void mo_room_tick(const Ctx& c) {
    if (mo_rmsg_t > 0) mo_rmsg_t--;
    if (mo_mm_room && mo_role && mo_rph != RP_START && mo_rph != RP_GO && mo_rph != RP_LEAVE) {
        OnMatch m;
        int ms = net_mm_state(&m);
        if (mo_rph == RP_FAIL || ms == MMS_WAIT || ms == MMS_IDLE) {
            bool back = ms != MMS_IDLE;
            if (back && ms != MMS_WAIT) mo_mm_re = 1;
            printf("menu: sala, emparejamiento: la partida no siguio (%s), %s\n", mo_rph == RP_FAIL ? "fallo de la sala" : ms == MMS_WAIT ? "el servicio devolvio la busqueda" : m.why, back ? "vuelve a la busqueda" : "fuera de la cola");
            mo_room_exit(c, back ? -2 : -1, back ? "emparejamiento: de vuelta a la busqueda" : "emparejamiento: fuera de la cola");
            return;
        }
    }
    if (mo_rph == RP_GO || mo_rph == RP_FAIL || !mo_role) return;
    uint32_t net = mpm_call(c, 0x08811A58u);
    if (!net) return;
    Ctx cp = c;
    int ev = psp_net_poll(cp);
    static int evlog = 0;
    if (ev && evlog++ < 20) printf("menu: sala, %d eventos de red entregados al juego (red %u, %u ms)\n", ev, mpm_net_state(c), mo_rms());
    int st = (int)mpm_net_state(c);
    uint32_t ri = mpm_call(c, 0x088118ACu, net);
    int cnt = mo_rcnt(ri);
    mo_room_log(st, ri);
    if (mpm_net_reset) return;
    switch (mo_rph) {
    case RP_CONNECT:
        // list child states 0..2 (0x08AADD48): connect while the game network is idle, then state 4
        if (st == 0 && !mo_rconn) { mo_rconn = true; mpm_call(c, 0x08811544u, net); printf("menu: sala, conecta la red del juego (%u ms)\n", mo_rms()); }
        else if (st == 4 && mo_host()) {
            // Start of Crear nueva sala (0x08AB2DB4)
            if (mpm_call(c, 0x088115F8u, net, mo_cmode ? 2u : 4u, mo_ccust ? 1u : 0u)) {
                mo_rph = RP_ROOM;
                printf("menu: sala, creada en el juego (modo %d, personalizado %d, %u ms)\n", mo_cmode, mo_ccust, mo_rms());
            }
        } else if (st == 4 && (int32_t)mpm_call(c, 0x088117C4u, net) > 0) {
            // list state 3 on a row (0x0881166C) and the accepted join box (0x088116A4)
            mpm_call(c, 0x0881166Cu, net, 0);
            mpm_call(c, 0x088116A4u, net);
            mo_rph = RP_JOINING; mo_dlg_open(DL_WAIT);
            printf("menu: sala, solicitud de union enviada (%u ms)\n", mo_rms());
        }
        if (mo_rph == RP_CONNECT && mo_rms() > 20000) mo_room_fail(mo_host() ? MM_R_PREP : MM_ICE, "tiempo agotado preparando la red del juego");
        break;
    case RP_JOINING:
        // list state 5 (0x08AAE840): 6 joined, 4 refused or failed (reason 0x08811880)
        if (st == 6) { mo_dlg = DL_NONE; mo_rph = RP_ROOM; printf("menu: sala, el anfitrion acepto (%u ms)\n", mo_rms()); }
        else if (st == 4) { printf("menu: sala, union rechazada o fallida (motivo %d)\n", (int)mpm_call(c, 0x08811880u, net)); mo_room_fail(MM_R_DENIED, "union no aceptada"); }
        break;
    case RP_ROOM:
        if (st >= 8 && st <= 14) { mo_rph = RP_START; mo_dlg = DL_NONE; printf("menu: sala, comienza la batalla (red %d)\n", st); break; }
        if (st == 4 || st < 4) {
            printf("menu: sala, la sala del juego termino (motivo %d)\n", (int)mpm_call(c, 0x08811880u, net));
            mo_room_fail(mo_host() ? MM_R_PREP : MM_R_CLOSED, mo_host() ? "sala del juego perdida" : "el anfitrion cerro la sala");
            break;
        }
        if (!mo_host() || st != 7) break;
        if (ri && mo_cplayers >= 2 && mo_cplayers < mo_rmax(ri)) {
            static int n = 0;
            if (n++ < 5) printf("menu: sala, maximo del juego %d -> %d jugadores (Crear sala)\n", mo_rmax(ri), mo_cplayers);
            psp_s8(ri + 0x1D2, (uint8_t)mo_cplayers);
        }
        if (cnt != mo_rcount) { mo_rcount = cnt; net_online_set_players(cnt); }
        if (mo_rdelay > 0) mo_rdelay--;
        if (mo_mm_room) {
            if (mo_rdelay <= 0 && mpm_call(c, 0x0881181Cu, net)) {
                uint32_t r = mpm_call(c, 0x088117F0u, net);
                if (r) { mpm_call(c, 0x08A6687Cu, mo_req(), r, 0x74); mpm_call(c, 0x088116D0u, net, mo_req() + 0x40); mo_rdelay = 30; printf("menu: sala, emparejamiento: \"%s\" aceptado solo\n", mo_u16(mo_req(), 32).c_str()); }
            }
            mo_mm_go = cnt >= mo_cplayers && mo_rdelay <= 0 ? mo_mm_go + 1 : 0;
            if (mo_mm_go >= 60) {
                uint32_t ok = mpm_call(c, 0x0881176Cu, net);
                mo_mm_go = 0;
                if (ok) mo_rph = RP_START;
                printf("menu: sala, emparejamiento: %d de %d en la sala, inicio %s\n", cnt, mo_cplayers, ok ? "aceptado por el juego" : "rechazado por el juego");
            }
            break;
        }
        if (mo_dlg == DL_START && cnt < 2) mo_dlg = DL_NONE;
        // room sub-state 4 (0x08AAB664): the request box closes when the request is no longer pending
        if (mo_dlg == DL_REQUEST && !mpm_call(c, 0x08811848u, net, mo_req() + 0x40)) { mo_dlg = DL_NONE; printf("menu: sala, la solicitud se retiro\n"); }
        // room sub-state 1, net state 7 (0x08AAB254): a pending request is copied (0x74 bytes) and asked in a box
        if (mo_dlg == DL_NONE && mo_rdelay <= 0 && mpm_call(c, 0x0881181Cu, net)) {
            uint32_t r = mpm_call(c, 0x088117F0u, net);
            if (r) { mpm_call(c, 0x08A6687Cu, mo_req(), r, 0x74); mo_dlg_open(DL_REQUEST); printf("menu: sala, solicitud de \"%s\"\n", mo_u16(mo_req(), 32).c_str()); }
        }
        break;
    case RP_START:
        // room sub-state 6 (0x08AAB8DC): net state 13 or 14 hands over to the battle setup
        if (st == 13 || st == 14) {
            mpm_call(c, 0x0881158Cu, net);
            uint32_t r = mpm_call(c, 0x08811930u, net);
            if (r) mpm_call(c, 0x089C03DCu, psp_l32(r + 8));
            mo_rph = RP_GO;
            if (mo_mm) { net_mm_cancel(); mo_mm = 0; }
            printf("menu: sala, red %d: pasa a la configuracion de batalla (%u ms)\n", st, mo_rms());
            mo_close_to(c, MA_ROOMGO);
        } else if (st == 6 || st == 7) mo_rph = RP_ROOM;
        else if (st <= 4) mo_room_fail(MM_R_CLOSED, "la sala termino durante el inicio");
        break;
    case RP_LEAVE:
        if (++mo_rleave_t > 60 || (st != 6 && st != 7)) mo_room_exit(c, -1, "salida confirmada");
        break;
    }
}

// parent state 6 (0x08AA9124): battle data from the room info, then the page result (2 players page 3, more page 1)
static int mo_room_handover(const Ctx& c) {
    uint32_t gd = mpm_call(c, 0x08A816E8u), ri = mo_rinfo(c);
    if (!gd || !ri) { printf("menu: sala, sin datos de partida (0x%08X, 0x%08X)\n", gd, ri); return 3; }
    bool changed = psp_l32(gd) != 0xB;
    int mx = mo_rmax(ri), cnt = mo_rcnt(ri);
    psp_s32(gd, 0xB);
    bool libre = mo_rmode >= 0 ? mo_rmode == 0 : mx >= 3;
    psp_s8(gd + 0x1958, libre);
    psp_s8(gd + 0x195A, 1);
    for (int i = 0; i < 4; i++) {
        uint32_t p = gd + 4 + 0x6C * i, s4 = gd + 0x1B4 + 0x18 * i, s2 = gd + 0x60 * i;
        int empty = i >= cnt;
        psp_s8(gd + 0x228 + 0x22 * i, 0);
        mpm_call(c, 0x08A6687Cu, p, ri + 0x74 * i, 0x40);
        psp_s32(p + 0x40, i / 2 != 0);
        psp_s32(p + 0x44, i < mx);
        psp_s32(p + 0x4C, empty);
        psp_s32(p + 0x54, !empty);
        psp_s32(p + 0x48, empty);
        psp_s32(p + 0x58, empty ? 0xFFFFFFFFu : (uint32_t)i);
        psp_s32(p + 0x50, 2);
        psp_s32(s4 + 0x14, empty);
        psp_s32(s2 + 0x2B0, i);
        psp_s32(s2 + 0x2C4, i);
        if (empty) psp_s32(s4, 0x63);
        if (changed) { psp_s32(s2 + 0x2D0, 2); psp_s32(s2 + 0x2D8, 1); psp_s32(s2 + 0x2DC, 1); }
    }
    psp_s32(gd + 0x214 + 8, psp_l32(gd + 0x2D0));
    psp_s32(gd + 0x214 + 0xC, psp_l32(gd + 0x2D8));
    psp_s32(gd + 0x214 + 0x10, psp_l32(gd + 0x2DC));
    bool surv = !libre && net_online_rank_mode() == 1;
    if (surv) psp_s8(gd + 0x1959, 0);
    printf("menu: sala, datos de partida: %d de %d jugadores, %s, modo de red %s\n", cnt, mx, libre ? "batalla libre (pagina 1)" : surv ? "supervivencia clasificatoria (pagina 5)" : "seleccion de batalla (pagina 3)", changed ? "nuevo" : "repetido");
    return libre ? 1 : surv ? 5 : 3;
}

static void mo_dlg_act(const Ctx& c, int b) {
    uint32_t net = mpm_call(c, 0x08811A58u);
    int d = mo_dlg;
    if (d == DL_WAIT) { mpm_sfx(c, 2, 0); mo_room_exit(c, -1, "espera cancelada"); return; }
    if (d == DL_INFO) { mpm_sfx(c, 1, 6); mo_room_exit(c, -1, "aviso confirmado"); return; }
    mo_dlg = DL_NONE;
    if (!net) return;
    if (d == DL_REQUEST) {
        mpm_sfx(c, b ? 2 : 1, b ? 0 : 6);
        mpm_call(c, b ? 0x08811708u : 0x088116D0u, net, mo_req() + 0x40);
        mo_rdelay = 30;
        printf("menu: sala, solicitud de \"%s\" %s\n", mo_u16(mo_req(), 32).c_str(), b ? "rechazada" : "aceptada");
    } else if (d == DL_START) {
        if (b) { mpm_sfx(c, 2, 0); return; }
        mpm_sfx(c, 1, 6);
        uint32_t ok = mpm_call(c, 0x0881176Cu, net);
        if (ok) mo_rph = RP_START;
        printf("menu: sala, inicio pedido (%s)\n", ok ? "aceptado por el juego" : "rechazado por el juego");
    } else if (d == DL_CLOSE) {
        if (b) { mpm_sfx(c, 2, 0); return; }
        mpm_sfx(c, 1, 6);
        mpm_call(c, mo_host() ? 0x08811640u : 0x08811740u, net);
        mo_rph = RP_LEAVE; mo_rleave_t = 0;
        printf("menu: sala, %s\n", mo_host() ? "el anfitrion cierra la sala" : "el invitado sale de la sala");
    }
}

static void mo_room_input(const Ctx& c, uint32_t e) {
    if (mo_rph == RP_GO || mo_rph == RP_LEAVE) return;
    if (mo_dlg) {
        bool one = mo_dlg == DL_WAIT || mo_dlg == DL_INFO;
        if (!one && e & 0xA0) { mo_dsel ^= 1; mo_mv(c); }
        else if (e & 0x4000) mo_dlg_act(c, one ? 1 : mo_dsel);
        else if (e & 0x2000) mo_dlg_act(c, 1);
        return;
    }
    if (mo_rph == RP_CONNECT) { if (e & 0x2000) { mpm_sfx(c, 2, 0); mo_room_exit(c, -1, "preparacion cancelada"); } return; }
    if (mo_rph != RP_ROOM) return;
    if (e & 0x2000) { mpm_sfx(c, 1, 0); mo_dlg_open(DL_CLOSE); return; }
    if (mo_host() && e & 0x4000) {
        if (mo_rcnt(mo_rinfo(c)) >= 2) { mpm_sfx(c, 1, 6); mo_dlg_open(DL_START); }
        else { mpm_sfx(c, 1, 3); mo_rmsg_t = 180; }
    }
}

static void mo_room_rows(const Ctx& c) {
    uint32_t ri = mo_rinfo(c);
    int cnt = mo_rcnt(ri), mx = mo_rmax(ri), own = mo_rown(ri);
    for (int i = 0; i < 4; i++) {
        bool show = ri && i < mx;
        mo_hide[R_ROW0 + i] = mo_hide[R_LABEL0 + i] = !show;
        if (!show) continue;
        mpm_set(c, R_ROW0 + i, i < cnt ? (i == own ? 5 : 4) : 6);
        mpm_set(c, R_LABEL0 + i, i);
    }
    for (int i = 0; i < 4; i++) {
        av_row[i] = ri && i < cnt && i < mx ? mo_u16(ri + 0x74 * i, 32) : std::string();
        mo_hide[R_AV0 + i] = av_row[i].empty() || !av_want() || !mpm_has_state(mpm_hld[R_AV0 + i], 35 + i);
        if (!mo_hide[R_AV0 + i]) mpm_set(c, R_AV0 + i, 35 + i);
    }
    if (ri && av_want()) net_avatar_room(cnt);
    mpm_set(c, R_BOX, 3);
    mo_hide[R_RICON] = net_online_rank_mode() < 0 || !mo_rank_ui();
    if (!mo_hide[R_RICON]) mpm_set(c, R_RICON, 32);
    mpm_set(c, R_DLG, 7);
    bool one = mo_dlg == DL_WAIT || mo_dlg == DL_INFO;
    mpm_set(c, R_DBTN0, one || mo_dsel == 0 ? 9 : 8);
    mpm_set(c, R_DBTN1, mo_dsel == 1 ? 9 : 8);
    mo_hide[R_DBTN1] = one;
}

static void mo_room_attach(const Ctx& c) {
    uint32_t r = mpm_hld[R_ROOT], d = mpm_hld[R_DLG];
    for (int i = 0; i < 4; i++) {
        mpm_hslot(c, R_ROW0 + i, r, S_PLAYER1 + i);
        mpm_hslot(c, R_LABEL0 + i, mpm_hld[R_ROW0 + i], S_LABEL);
        mpm_hslot(c, R_AV0 + i, mpm_hld[R_ROW0 + i], mo_sn[MS_AVATAR]);
    }
    mpm_hslot(c, R_BOX, r, S_SETSUMEI);
    mpm_hslot(c, R_RICON, r, mo_sn[MS_TMODE]);
    if (mpm_hld[R_RICON]) { psp_s32(mpm_hld[R_RICON] + 0x18, (uint32_t)(mo_rmode_w + 6 - 4)); psp_s32(mpm_hld[R_RICON] + 0x1C, (uint32_t)-2); }
    mpm_hslot(c, R_DLG, r, mo_sn[MS_RDLG]);
    mpm_hslot(c, R_DBTN0, d, mo_sn[mo_dlg == DL_WAIT || mo_dlg == DL_INFO ? MS_RDBC : MS_RDB1]);
    mpm_hslot(c, R_DBTN1, d, mo_sn[MS_RDB2]);
}

static void mo_room_draw(const Ctx& c, uint32_t font, uint32_t lang) {
    uint32_t r = mpm_hld[R_ROOT], ri = mo_rinfo(c);
    int cnt = mo_rcnt(ri), mx = mo_rmax(ri);
    mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_RSERVER]), mpm_utf16(mo_rserver.empty() ? "-" : mo_cut(mo_rserver.c_str(), 20).c_str(), 100));
    mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_ID]), mpm_utf16(mo_rid.empty() ? "-" : mo_rid.c_str(), 101));
    uint32_t ms = mpm_utf16(mo_mode_name(mo_rmode, lang, net_online_rank_mode() >= 0), 102);
    mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_TMODE]), ms);
    mo_rmode_w = (int)mpm_call(c, 0x08831F68u, font, ms);
    mpm_font_at(c, font, mpm_slot(c, r, mo_sn[MS_TCUSTOM]), mpm_utf16(mo_yes_no(mo_rcust, lang), 103));
    for (int i = 0; i < 4 && i < cnt && i < mx; i++) {
        uint32_t h = mpm_hld[R_ROW0 + i];
        mpm_font_at(c, font, mpm_slot(c, h, mo_sn[MS_NAME]), mpm_utf16(mo_u16(ri + 0x74 * i, 32).c_str(), 104 + i));
        if (i == 0) mpm_font_at(c, font, mpm_slot(c, h, mo_sn[MS_RROLE]), mpm_utf16(mo_rt(RT_HOST, lang), 108));
    }
    int t = mo_rmsg_t > 0 ? RT_NEED2 : mo_rph == RP_CONNECT ? (mo_host() ? RT_PREP : RT_CONNECTING) : mo_rph == RP_JOINING ? RT_CONNECTING
          : mo_rph == RP_START || mo_rph == RP_GO ? RT_STARTING : mo_rph == RP_FAIL ? RT_LEAVING : mo_rph == RP_LEAVE ? RT_LEAVING : !mo_host() ? RT_GUEST : cnt >= 2 ? RT_READY : RT_ALONE;
    mpm_font_lines(c, font, mpm_slot(c, mpm_hld[R_BOX], S_TEXT), mo_rt(t, lang), 60, 116);
}

// dialog colours (ABGR) of the game's own boxes
static const uint32_t MO_DLG_TEXT = 0xFFFFFFFFu, MO_BTN_TEXT = 0xFFFFFFFFu, MO_BTN_TEXT_ON = 0xFF0033EFu, MO_NAME_TEXT = 0xFF4040FFu;

// font style (0x088321EC copies 8 words; +8 colour ABGR), text centred on the slot origin (0x08831F68 widest line in px)
static void mo_text_at(const Ctx& c, uint32_t font, int x, int y, uint32_t str, uint32_t abgr) {
    uint32_t st = mpm_mem + 0xD100, cur = psp_l32(psp_l32(font) + 0x10000u - 0x76E8u);
    for (int k = 0; k < 8; k++) psp_s32(st + 4 * k, psp_l32(cur + 4 * k));
    uint32_t keep = psp_l32(st + 8);
    psp_s32(st + 8, abgr);
    mpm_call(c, 0x088321ECu, font, st);
    mpm_call(c, 0x08832530u, font, (uint32_t)x, (uint32_t)y, str, 0xFFFFFFFFu, 0xFFFFFFFFu);
    psp_s32(st + 8, keep);
    mpm_call(c, 0x088321ECu, font, st);
}
static void mo_text_c(const Ctx& c, uint32_t font, uint32_t slot, uint32_t str, uint32_t abgr) {
    if (!slot || !str) return;
    int w = (int)mpm_call(c, 0x08831F68u, font, str);
    mo_text_at(c, font, (int)psp_l32(slot + 0xE4) - w / 2, (int)psp_l32(slot + 0xE8), str, abgr);
}
// a centred "%s" line whose name part has its own colour
static void mo_text_name(const Ctx& c, uint32_t font, uint32_t slot, const char* fmt, const char* name, int buf, uint32_t abgr, uint32_t name_abgr) {
    if (!slot) return;
    const char* p = strstr(fmt, "%s");
    std::string a = p ? std::string(fmt, p - fmt) : fmt, z = p ? p + 2 : "";
    uint32_t sa = mpm_utf16(a.c_str(), buf), sn = mpm_utf16(name, buf + 1), sz = mpm_utf16(z.c_str(), buf + 2);
    int wa = a.empty() ? 0 : (int)mpm_call(c, 0x08831F68u, font, sa), wn = (int)mpm_call(c, 0x08831F68u, font, sn), wz = z.empty() ? 0 : (int)mpm_call(c, 0x08831F68u, font, sz);
    int x = (int)psp_l32(slot + 0xE4) - (wa + wn + wz) / 2, y = (int)psp_l32(slot + 0xE8);
    if (wa) mo_text_at(c, font, x, y, sa, abgr);
    mo_text_at(c, font, x + wa, y, sn, name_abgr);
    if (wz) mo_text_at(c, font, x + wa + wn, y, sz, abgr);
}

static void mo_draw_dlg_text(const Ctx& c, uint32_t font, uint32_t lang) {
    uint32_t d = mpm_hld[R_DLG];
    char b[96] = "";
    const char* l2 = "";
    int b0 = RT_YES, b1 = RT_NO;
    if (mo_dlg == DL_REQUEST) { l2 = mo_rt(RT_REQ2, lang); b0 = RT_ACCEPT; b1 = RT_REJECT; }
    else if (mo_dlg == DL_START) snprintf(b, sizeof(b), "%s", mo_rt(RT_QSTART, lang));
    else if (mo_dlg == DL_CLOSE) { snprintf(b, sizeof(b), "%s", mo_rt(mo_host() ? RT_QCLOSE : RT_QLEAVE, lang)); if (mo_host()) l2 = mo_rt(RT_QCLOSE2, lang); }
    else if (mo_dlg == DL_WAIT) { snprintf(b, sizeof(b), "%s", mo_rt(RT_WAIT, lang)); b0 = RT_CANCEL; }
    else if (mo_dlg == DL_INFO) { snprintf(b, sizeof(b), "%s", mo_t(mo_rfail >= 0 ? mo_rfail : MM_ICE, lang)); b0 = RT_OK; }
    bool one = mo_dlg == DL_WAIT || mo_dlg == DL_INFO;
    if (mo_dlg == DL_REQUEST) mo_text_name(c, font, mpm_slot(c, d, mo_sn[MS_RDL1]), mo_rt(RT_REQ, lang), mo_u16(mo_req(), 32).c_str(), 172, MO_DLG_TEXT, MO_NAME_TEXT);
    else mo_text_c(c, font, mpm_slot(c, d, mo_sn[MS_RDL1]), mpm_utf16(b, 112), MO_DLG_TEXT);
    if (*l2) mo_text_c(c, font, mpm_slot(c, d, mo_sn[MS_RDL2]), mpm_utf16(l2, 113), MO_DLG_TEXT);
    mo_text_c(c, font, mpm_slot(c, mpm_hld[R_DBTN0], mo_sn[MS_RBTXT]), mpm_utf16(mo_rt(b0, lang), 114), one || mo_dsel == 0 ? MO_BTN_TEXT_ON : MO_BTN_TEXT);
    if (!one) mo_text_c(c, font, mpm_slot(c, mpm_hld[R_DBTN1], mo_sn[MS_RBTXT]), mpm_utf16(mo_rt(b1, lang), 115), mo_dsel == 1 ? MO_BTN_TEXT_ON : MO_BTN_TEXT);
}

static int mo_pg_last = -1;
void rs_run_end(const char* why);
// pages 3/5 (battle selection modes, EBOOT_TTT 56.1): screen [page+0x10], mode row value [[scr+0x7C]+0x68], result [scr+0x70]
// ranked free battle: time limit 999 s and 2 senzu per team, fixed. Stage settings block of 56_38 S + 0x214 (S = 0x08B64078):
// +8 time selector 0..4 (4 = 999 s), +0xC / +0x10 senzu of team A / B; 56_38 reads them when page 1 ends (0x08AB9F78)
static const uint32_t MO_STG = 0x08B6428Cu;
static const int MO_FIX_OFF[3] = {8, 0xC, 0x10}, MO_FIX_VAL[3] = {4, 2, 2};
static bool mo_fixed() { return net_online_rank_mode() == 0; }

static void mo_fix_stage(const char* why) {
    bool chg = false;
    for (int k = 0; k < 3; k++) if ((int)psp_l32(MO_STG + MO_FIX_OFF[k]) != MO_FIX_VAL[k]) { psp_s32(MO_STG + MO_FIX_OFF[k], MO_FIX_VAL[k]); chg = true; }
    static int logs = 0;
    if (chg && logs++ < 20) printf("menu: clasificatoria, tiempo 999 y 2 semillas por equipo fijados (%s)\n", why);
}

// time/senzu panels over the stage select: set_active (vtable +0x88: 0x08A0F564 time, 0x08A0BE54 senzu) 1 = state 2 with the
// arrows, 0 = state 1 without; 56_38 activates them on its rows, training never does. Ranked keeps them idle.
// Panels at W+0x2CC / +0x2D4, W = the cursor's page callback [obj+0x12C]
static void mo_panels_idle(const Ctx& c, uint32_t obj) {
    uint32_t w = psp_l32(obj + 0x12C);
    static int logs = 0;
    if (w < 0x08800000u || w >= 0x0C000000u) { if (logs < 3) { logs++; printf("menu: clasificatoria, paneles sin pagina (0x%08X)\n", w); } return; }
    uint32_t tp = psp_l32(w + 0x2CC), sp = psp_l32(w + 0x2D4);
    bool ok_t = tp >= 0x08800000u && tp < 0x0C000000u && psp_l32(tp + 0x40) == 0x08A74710u;
    bool ok_s = sp >= 0x08800000u && sp < 0x0C000000u && psp_l32(sp + 0x40) == 0x08A74528u;
    if (!ok_t && !ok_s) { if (logs < 3) { logs++; printf("menu: clasificatoria, paneles no encontrados en 0x%08X (0x%08X 0x%08X)\n", w, tp, sp); } return; }
    for (int i = 0; i < 2; i++) {
        uint32_t p = i ? sp : tp;
        if (!(i ? ok_s : ok_t) || !psp_l32(p + 0x6C)) continue;
        mpm_call(c, i ? 0x08A0BE54u : 0x08A0F564u, p, 0);
        if (logs < 8) { logs++; printf("menu: clasificatoria, panel de %s sin flechas\n", i ? "semillas" : "tiempo"); }
    }
}

// option rows of the map select (cursor class 0x08A06368): 0x08A0758C time, 0x08A07AE0 senzu A, 0x08A07D18 senzu B.
// In ranked free battle left/right are masked for the call (confirm and back kept) and the row value is put back
static bool mo_opt_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    int k = dir == 0x08A0758Cu ? 0 : dir == 0x08A07AE0u ? 1 : dir == 0x08A07D18u ? 2 : dir == 0x08A07284u ? 3 : -1;
    if (k < 0) return false;
    uint32_t obj = c.r[4];
    if (!mo_fixed() || !psp_l32(obj + 0x68) || (int)psp_l32(obj + 0x80) < 0) { orig(c); return true; }
    if (k == 3) {   // stage row: up (bit 8 of +0x34) leads to the senzu row, masked as training never goes there
        uint32_t pe = 0x08B4E990u + 0x58u * psp_l32(obj + 0x80), w1 = psp_l32(pe + 0x34);
        psp_s32(pe + 0x34, w1 & ~8u);
        orig(c);
        psp_s32(pe + 0x34, w1);
        mo_panels_idle(c, obj);
        return true;
    }
    uint32_t pe = 0x08B4E990u + 0x58u * psp_l32(obj + 0x80), w1 = psp_l32(pe + 0x34);
    psp_s32(pe + 0x34, w1 & ~3u);
    orig(c);
    psp_s32(pe + 0x34, w1);
    uint32_t cur = obj + 0x8C + 4 * psp_l32(obj + 0x78);
    if ((int)psp_l32(cur) != MO_FIX_VAL[k]) psp_s32(cur, MO_FIX_VAL[k]);
    mo_fix_stage(k ? "fila de semillas" : "fila de tiempo");
    static int logs = 0;
    if (logs++ < 6) printf("menu: clasificatoria, fila %s bloqueada (estado %u)\n", k ? "semillas" : "tiempo", psp_l32(obj + 0x78));
    return true;
}

// rows of the stage select: 8 time, 10 / 11 senzu (top panel), 7 stage, 9 music. Ranked free battle keeps 7 and 9, as training
static bool mo_row_wrap(Ctx& c, void (*orig)(Ctx&)) {
    int to = (int)c.r[5];
    if (!mo_fixed() || (to != 8 && to != 10 && to != 11)) { orig(c); return true; }
    int cur = (int)psp_l32(c.r[4] + 0x78), nt = cur == 7 || cur == 9 ? cur : 7;
    static int logs = 0;
    if (logs++ < 10) printf("menu: clasificatoria, fila %d omitida: de %d pasa a %d\n", to, cur, nt);
    c.r[5] = (uint32_t)nt;
    orig(c);
    return true;
}

static void mo_page_tick(uint32_t pg, int i) {
    if (i == 1 && mo_fixed()) mo_fix_stage("pagina 1");
    static uint32_t lv = 0xFFFFFFFFu, lr = 0xFFFFFFFFu;
    static int logs = 0;
    if (i == 3 || i == 5) {
        uint32_t scr = psp_l32(pg + 0x10), rows = scr >= 0x08800000u && scr < 0x10000000u ? psp_l32(scr + 0x7C) : 0;
        uint32_t v = rows >= 0x08800000u && rows < 0x10000000u ? psp_l32(rows + 0x68) : 0xFFFFFFFFu, rr = scr ? psp_l32(scr + 0x70) : 0xFFFFFFFFu;
        if ((v != lv || rr != lr) && logs < 60) { logs++; printf("menu: pagina %d, fila de modo %d, resultado %d, modo de batalla %u\n", i, (int)v, (int)rr, psp_l32(0x08B48A28u)); }
        lv = v; lr = rr;
    }
    // ranked: the team step of page 1 (56_38 screen +0x31C: step, frames left) ends at once; the game's own timeout
    // path then keeps the room order teams (A rows 0-1, B rows 2-3) and moves to the character select
    if (i == 1 && net_online_ranked()) {
        uint32_t scr = psp_l32(pg + 8), step = scr >= 0x08800000u && scr < 0x10000000u ? scr + 0x31C : 0;
        if (step && psp_l32(step) == 1 && (int32_t)psp_l32(step + 4) > 0) { psp_s32(step + 4, 0); printf("menu: pagina 1, clasificatoria: pantalla de equipos omitida (equipos por orden de sala)\n"); }
        if (step && psp_l32(scr + 0x2F8) == 11 && (int32_t)psp_l32(scr + 0x314) < 0x3B) psp_s32(scr + 0x314, 0x3B);
    }
    if (i != 2) rs_run_end(i == mo_pg_last ? "pagina" : "pagina nueva");
    if (i != 2 && i != 6) mpm_surv_end = false;
    if (i == mo_pg_last) return;
    printf("menu: pagina %d activa (vtable 0x%08X, estado %u/%u), modo de batalla %u\n", i, mpm_pg_vt[i], psp_l32(pg + 8), psp_l32(pg + 0xC), psp_l32(0x08B48A28u));
    mo_pg_last = i; lv = lr = 0xFFFFFFFFu;
}

// Discord outside online play: menus, battles by mode, training, local and LAN; the tool windows (Modding) set it themselves
int psp_ui_activity();
static std::string dc_map_name(const Ctx& c, int li);
static int dc_bticks = 0;
static bool dc_gp(uint32_t a) { return guest_ptr(a); }

static int cv_owner();
static bool rm_active();
static std::string dc_team(const Ctx& c, int team, int li, bool brief = false) {
    uint32_t t = psp_l32(0x08A7DBC8u);
    if (!dc_gp(t)) return "";
    uint32_t n = psp_l32(t), b = psp_l32(t + 4);
    if (n < 1 || n > 8 || !dc_gp(b)) return "";
    std::string o, first;
    int cnt = 0;
    static int diag = 0;
    for (uint32_t k = 0; k < n; k++) {
        uint32_t f = b + 0x1A90u * k;
        if (!psp_l32(f + 0xC) || (int)(psp_l32(f + 4) & 1) != team) continue;
        bool pl;
        auto it = nm_seen.find(f);
        uint32_t s = it != nm_seen.end() ? it->second : nm_text(c, f, &pl);
        std::string nm = s ? sz_utf8(s) : "";
        static std::map<uint32_t, std::string> dl;
        if (nm.empty() && diag < 40 && dl[f] != nm + "|" + std::to_string(psp_l32(f + 0x2C))) { dl[f] = nm + "|" + std::to_string(psp_l32(f + 0x2C)); diag++; printf("discord: luchador %u de %u, equipo %d miembro %u ranura %u, nombre '%s' (%s), llamadas en combate %d\n", k, n, team, psp_l32(f + 8), psp_l32(f + 0x2C), nm.c_str(), it != nm_seen.end() ? "barras" : "llamada", dc_bticks); }
        if (nm.empty()) continue;
        if (!cnt++) first = nm;
        o += (o.empty() ? "" : li ? " y " : " & ") + nm;
    }
    if (brief && cnt > 1) return first + " +" + std::to_string(cnt - 1);
    return o.size() > 60 ? o.substr(0, 57) + "..." : o;
}

static const char* dc_mode(uint32_t md, int li) {
    switch (md) {
        case 0: case 11: return li ? "Batalla libre" : "Free Battle";
        case 3: case 12: return li ? "Supervivencia" : "Survival";
        case 4: return li ? "Batalla 100" : "Battle 100";
        default: return li ? "Combate" : "Battle";
    }
}

static void dc_offline(const Ctx& c) {
    if (psp_ui_activity()) return;
    int li = mpm_li(mpm_lang(c));
    uint32_t md = psp_l32(0x08B48A28u), mgr = psp_l32(0x08A7EA50u), sc = dc_gp(mgr) ? psp_l32(mgr + 0x10) : 0xFFFFFFFFu, pg = dc_gp(mgr) ? psp_l32(mgr + 0x14) : 0xFFFFFFFFu;
    // scenes: 4 main menu, 5 story, 6 free battle, 7 battle select, 10 training; the 3D select views use a battle scene
    bool view3d = cv_owner() != 0 || rm_active();
    bool bat = battle_active() && !view3d, lan = md >= 11 && md <= 13 && !net_online_on(), story = sc == 5, cpu = psp_l32(0x08B48A20u + 0x13C) == 2;
    int loc = bat && !lan ? lmp_n() : 1;
    std::string d, st;
    if (bat) {
        std::string a = dc_team(c, 0, li), b = dc_team(c, 1, li), map = dc_map_name(c, li);
        if (a.size() + b.size() > 44) { a = dc_team(c, 0, li, true); b = dc_team(c, 1, li, true); }
        std::string vs = a.empty() || b.empty() ? a + b : a + " vs " + b;
        if (!map.empty()) vs += (vs.empty() ? "" : " · ") + map;
        if (lan) { d = li ? "Partida LAN" : "LAN match"; st = dc_mode(md, li); }
        else if (loc >= 2) { d = li ? "Multijugador local" : "Local multiplayer"; st = std::to_string(loc) + (li ? " jugadores · " : " players · ") + dc_mode(md, li); }
        else if (story) { d = li ? "Modo historia" : "Story Mode"; st = vs; }
        else if (md == 7) { d = li ? "Entrenando" : "Training"; st = a + (map.empty() ? "" : (a.empty() ? "" : " · ") + map); }
        else if (cpu) { d = li ? "Viendo COM vs COM" : "Watching COM vs COM"; st = vs; }
        else { d = dc_mode(md, li); st = vs; }
    } else if (lan) d = li ? "En una sala LAN" : "In a LAN room";
    else if (story) d = li ? "Modo historia" : "Story Mode";
    else if (sc == 6 || sc == 7 || sc == 10) {
        d = li ? "Preparando un combate" : "Getting ready for a battle";
        st = sc == 10 ? (li ? "Entrenamiento" : "Training") : sc == 6 ? dc_mode(0, li) : md == 3 || md == 4 ? dc_mode(md, li) : (li ? "Selección de batalla" : "Battle Select");
    } else d = li ? "En los menús" : "In the menus";
    static std::string last;
    std::string key = d + "|" + st;
    static int logs = 0;
    if (key != last && logs < 60) { logs++; printf("discord: %s | %s (escena %d pagina %d, modo %u, combate %d, vista 3D %d, cpu %d, locales %d)\n", d.c_str(), st.c_str(), (int)sc, (int)pg, md, bat ? 1 : 0, view3d ? 1 : 0, cpu ? 1 : 0, loc); }
    last = key;
    net_discord_presence(d.c_str(), st.c_str(), "", 0, 0);
}

// Discord: presence only (invites removed)
static int mo_dc_t = 0, mo_dc_n = 0, mo_dc_max = 0;
static void mo_discord_presence(const Ctx& c) {
    if (++mo_dc_t < 30) return;
    mo_dc_t = 0;
    static const char* T[4][2] = {{"Looking for an online room", "Buscando sala en l\u00EDnea"},
                                  {"In an online room", "En una sala en l\u00EDnea"},
                                  {"Online battle", "Combate en l\u00EDnea"},
                                  {"Searching for a ranked match", "Buscando partida clasificatoria"}};
    static const char* MD[2][2] = {{"Free Battle", "Batalla libre"}, {"Battle Select", "Selecci\u00F3n de batalla"}};
    bool sub = mpm_mode == MP_SUB && mpm_w;
    int k = -1;
    if (mo_role && net_online_on()) k = mo_rph == RP_GO ? 2 : 1;
    else if (sub && (mpm_scr == 2 || mpm_scr == 3)) k = mo_mm ? 3 : 0;
    if (k < 0) { mo_dc_n = mo_dc_max = 0; dc_offline(c); return; }
    int li = mpm_li(mpm_lang(c));
    if (k == 1 && sub && mpm_scr == 4) { uint32_t ri = mo_rinfo(c); if (ri && mo_rmax(ri) > 0) { mo_dc_n = mo_rcnt(ri); mo_dc_max = mo_rmax(ri); } }
    const char* st = (k == 1 || k == 2) && mo_rmode >= 0 && mo_rmode < 2 ? MD[mo_rmode][li] : "";
    net_discord_presence(T[k][li], st, k == 1 || k == 2 ? mo_rid.c_str() : "", mo_dc_n, mo_dc_max);
}

static void mo_discord_frame(const Ctx& c) {
    mo_discord_presence(c);
}
static void dc_battle_frame(const Ctx& c) { dc_bticks++; mo_discord_presence(c); }
