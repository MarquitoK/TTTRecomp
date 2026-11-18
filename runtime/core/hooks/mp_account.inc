// Account screens (57_menu_multiplayer AGS 160, titles 167/174): start screen (screen 5) at the first launch, account settings (screen 6) from Options
enum { AE_NONE, AE_START, AE_SETTINGS };
enum { AF_TOP, AF_OFF, AF_SET };
enum { RK_ACT, RK_FIELD, RK_BTN };
enum { FD_NONE, FD_NAME };
enum { AO_NONE, AO_STEAM, AO_LOGOUT, AO_DELETE };
enum { AD_NONE, AD_LOGOUT, AD_DELETE, AD_STEAM };
enum { AT_STEAM, AT_NAME, AT_START, AT_CHNAME, AT_LOGOUT, AT_DELETE, AT_TAB_ACC, AT_TAB_OFF, AT_INFO_STEAM, AT_INFO_OFF, AT_INFO_EXP,
       AH_STEAM, AH_OFF, AH_NAME, AH_CHNAME, AH_SETSTEAM, AH_LOGOUT, AH_DELETE,
       AM_BUSY, AM_SVC, AM_EXPIRED, AM_DONE, AM_NAMEBAD, AM_STEAMBAD,
       AQ_LOGOUT, AQ_DELETE, AQ_DELETE2, AQ_STEAM, AQ_STEAM2, AQ_YES, AQ_NO, AQ_CANCEL, AT_N };
enum { AS_TEXT_TAB, AS_TEXT_LABEL, AS_TEXT_VALUE, AS_TEXT_INFO, AS_INFO, AS_N };

static int ac_entry = AE_NONE, ac_form = AF_TOP, ac_tab = 0, ac_row = 0, ac_op = AO_NONE, ac_msg = -1, ac_msg_t = 0, ac_dlg = AD_NONE, ac_dsel = 0;
static int ac_opt = 0, ac_back_opt = 0, ac_sync_log = 0;
static bool ac_cancel = false;
static int ac_boot = 0;
static std::string ac_name;
static int ac_sn[AS_N];
struct AcRow { int kind, label, field; };
static AcRow ac_rows_v[5];
static int ac_nrows = 0;

static void ac_names() {
    static const char* n[AS_N] = {"text_tab", "text_label", "text_value", "text_info", "info"};
    for (int k = 0; k < AS_N; k++) ac_sn[k] = mpm_sname(n[k]);
}

static const char* ac_t(int id, uint32_t lang) {
    static const char* T[AT_N][2] = {
        {"Sign in with Steam", "Entrar con Steam"},
        {"Name", "Nombre"},
        {"Start", "Comenzar"},
        {"Change name", "Cambiar nombre"},
        {"Sign out", "Cerrar sesi\u00F3n"},
        {"Delete account", "Borrar cuenta"},
        {"Account", "Cuenta"},
        {"Offline", "Sin conexi\u00F3n"},
        {"Steam: %s", "Steam: %s"},
        {"Offline: %s", "Sin conexi\u00F3n: %s"},
        {"Steam: %s (sign in again)", "Steam: %s (inicia sesi\u00F3n otra vez)"},
        {"Opens Steam or your browser to sign in. Your Steam name is used online.", "Abre Steam o el navegador para iniciar sesi\u00F3n. En l\u00EDnea se usa tu nombre de Steam."},
        {"Play without an account. You can sign in with Steam later from Options.", "Juega sin cuenta. Puedes iniciar sesi\u00F3n con Steam despu\u00E9s desde Opciones."},
        {"3 to 10 characters: letters, digits, hyphen, underscore or dot.", "De 3 a 10 caracteres: letras, n\u00FAmeros, guion, guion bajo o punto."},
        {"Name shown in the game.", "Nombre que se ve en el juego."},
        {"Sign in with Steam to play online. Your Steam name replaces this name.", "Inicia sesi\u00F3n con Steam para jugar en l\u00EDnea. Tu nombre de Steam reemplaza a este nombre."},
        {"Sign out on this device.", "Cerrar sesi\u00F3n en este dispositivo."},
        {"Deletes your Tag Team account. Your Steam account is not affected.", "Borra tu cuenta de Tag Team. No afecta a tu cuenta de Steam."},
        {"Please wait...", "Espera un momento..."},
        {"Online service unreachable.", "Sin conexi\u00F3n con el servicio."},
        {"Session expired. Sign in again.", "La sesi\u00F3n caduc\u00F3. Inicia sesi\u00F3n otra vez."},
        {"Done.", "Listo."},
        {"The name needs 3 to 10 characters: letters, digits, hyphen, underscore or dot.", "El nombre necesita de 3 a 10 caracteres: letras, n\u00FAmeros, guion, guion bajo o punto."},
        {"Steam sign-in did not finish. Try again.", "No se complet\u00F3 el inicio de sesi\u00F3n con Steam. Int\u00E9ntalo otra vez."},
        {"Sign out of this account?", "\u00BFCerrar la sesi\u00F3n de esta cuenta?"},
        {"Delete your account?", "\u00BFBorrar tu cuenta?"},
        {"This cannot be undone.", "No se puede deshacer."},
        {"Waiting for Steam...", "Esperando a Steam..."},
        {"Finish signing in in Steam or your browser.", "Termina de iniciar sesi\u00F3n en Steam o en el navegador."},
        {"Yes", "S\u00ED"},
        {"No", "No"},
        {"Cancel", "Cancelar"}};
    return T[id][mpm_li(lang)];
}

static bool ac_account() { return net_account_kind() == AK_ACCOUNT && *net_online_account(); }

static void ac_build() {
    int n = 0;
    auto add = [&](int k, int l, int f) { ac_rows_v[n++] = {k, l, f}; };
    switch (ac_form) {
    case AF_TOP: add(RK_ACT, AT_STEAM, 0); break;
    case AF_OFF: add(RK_FIELD, AT_NAME, FD_NAME); add(RK_BTN, AT_START, 0); break;
    case AF_SET:
        if (net_account_kind() == AK_ACCOUNT) {
            if (!ac_account()) add(RK_ACT, AT_STEAM, 0);
            add(RK_ACT, AT_LOGOUT, 0); add(RK_ACT, AT_DELETE, 0);
        } else { add(RK_ACT, AT_CHNAME, FD_NAME); add(RK_ACT, AT_STEAM, 0); }
        break;
    }
    ac_nrows = n;
    if (ac_row >= n) ac_row = n - 1;
}

// Back on the start screen (no profile yet): system_es 1_sfx entry 06 (TTT_ACCOUNT_SFX="bank,id" to try another)
static void ac_sfx_deny(const Ctx& c) {
    static int b = -1, id = 5;
    if (b < 0) { b = 1; if (const char* e = getenv("TTT_ACCOUNT_SFX")) sscanf(e, "%d,%d", &b, &id); printf("menu: cuenta, sonido de atras sin perfil (%d, %d)\n", b, id); }
    mpm_sfx(c, (uint32_t)b, (uint32_t)id);
}

static void ac_say(int m) { ac_msg = m; ac_msg_t = 300; }

static void ac_form_set(int f) {
    ac_form = f; ac_row = 0; ac_msg = -1;
    if (f == AF_OFF && ac_name.empty()) ac_name = mo_name_seed();
    ac_build();
}

static void ac_enter(int entry) {
    ac_entry = entry; ac_tab = 0; ac_op = AO_NONE; ac_dlg = AD_NONE; ac_name.clear();
    ac_form_set(entry == AE_SETTINGS ? AF_SET : AF_TOP);
    printf("menu: cuenta, pantalla %s (perfil %d, %s)\n", entry == AE_SETTINGS ? "ajustes de cuenta" : "inicio", net_account_kind(), net_account_name());
}

static void ac_leave(const Ctx& c) {
    if (ac_entry == AE_SETTINGS) ac_back_opt = 1;
    ac_entry = AE_NONE;
    printf("menu: cuenta, sale (perfil %d, %s)%s\n", net_account_kind(), net_account_name(), ac_back_opt ? ", vuelve a Opciones" : "");
    mo_close_to(c, MA_BACK);
}

static void ac_steam(const Ctx& c) { net_account_steam_login(); ac_op = AO_STEAM; ac_dlg = AD_STEAM; ac_dsel = 0; }

static void ac_result(const Ctx& c, int s) {
    int op = ac_op;
    ac_op = AO_NONE;
    printf("menu: cuenta, operacion %d -> estado %d (perfil %d, %s)\n", op, s, net_account_kind(), net_account_name());
    if (op == AO_STEAM) ac_dlg = AD_NONE;
    if (ac_cancel || s == OA_CANCEL) { ac_cancel = false; if (s != OA_OK) { ac_msg = -1; return; } }
    if (s == OA_OK) {
        mo_profile_set(net_account_name());
        if (op == AO_LOGOUT || op == AO_DELETE) { ac_tab = 0; ac_name.clear(); mo_close_to(c, MA_TOSTART); ac_form = AF_TOP; return; }
        if (mpm_scr == 6) { ac_form_set(AF_SET); ac_say(AM_DONE); return; }
        ac_leave(c);
        return;
    }
    ac_say(s == OA_EXPIRED ? AM_EXPIRED : op == AO_STEAM ? AM_STEAMBAD : AM_SVC);
    mpm_sfx(c, 1, 3);
}

static void ac_activate(const Ctx& c) {
    const AcRow& r = ac_rows_v[ac_row];
    if (r.kind == RK_FIELD) { mpm_sfx(c, 1, 6); mo_kopen(c, KB_A_NAME, ac_name); return; }
    if (r.kind == RK_BTN) {
        if (!mo_name_ok(ac_name)) { ac_say(AM_NAMEBAD); mpm_sfx(c, 1, 3); return; }
        mpm_sfx(c, 1, 6);
        net_account_offline(ac_name.c_str());
        mo_profile_set(net_account_name());
        printf("menu: cuenta, perfil sin conexion \"%s\"\n", ac_name.c_str());
        ac_leave(c);
        return;
    }
    mpm_sfx(c, 1, 6);
    switch (r.label) {
    case AT_STEAM: ac_steam(c); break;
    case AT_CHNAME: mo_kopen(c, KB_A_NAME, net_account_name()); break;
    case AT_LOGOUT: ac_dlg = AD_LOGOUT; ac_dsel = 1; break;
    case AT_DELETE: ac_dlg = AD_DELETE; ac_dsel = 1; break;
    }
}

static void ac_kdone(const Ctx& c, int kind, bool ok, const std::string& t) {
    if (!ok) return;
    if (ac_form == AF_SET) {
        if (!mo_name_ok(t)) { ac_say(AM_NAMEBAD); return; }
        if (t == net_account_name()) return;
        net_account_offline(t.c_str());
        mo_profile_set(t.c_str());
        ac_build(); ac_say(AM_DONE);
        return;
    }
    ac_name = t;
    if (ac_row + 1 < ac_nrows) ac_row++;
}

static void ac_dlg_input(const Ctx& c, uint32_t e) {
    if (ac_dlg == AD_STEAM) {
        if (e & 0x6000) { mpm_sfx(c, 2, 0); ac_dlg = AD_NONE; ac_cancel = true; net_account_steam_cancel(); ac_say(AM_BUSY); }
        return;
    }
    if (e & 0xA0) { ac_dsel ^= 1; mo_mv(c); return; }
    bool yes = (e & 0x4000) && ac_dsel == 0;
    if (!(e & 0x6000)) return;
    int d = ac_dlg;
    ac_dlg = AD_NONE;
    if (!yes) { mpm_sfx(c, 2, 0); return; }
    mpm_sfx(c, 1, 6);
    if (d == AD_LOGOUT) { net_account_logout(); ac_op = AO_LOGOUT; }
    else { net_account_delete(); ac_op = AO_DELETE; }
    ac_say(AM_BUSY);
}

static void ac_input(const Ctx& c, uint32_t e) {
    if (ac_msg_t > 0 && --ac_msg_t == 0 && ac_op == AO_NONE) ac_msg = -1;
    if (ac_op != AO_NONE) {
        int s = net_account_state();
        if (s != OA_BUSY) ac_result(c, s);
        else if (ac_dlg == AD_STEAM) ac_dlg_input(c, e);
        return;
    }
    if (ac_dlg != AD_NONE) { ac_dlg_input(c, e); return; }
    if (mpm_scr == 5 && (e & 0x300)) { ac_tab ^= 1; ac_form_set(ac_tab ? AF_OFF : AF_TOP); mo_mv(c); return; }
    if (e & 0x10 && ac_row > 0) { ac_row--; mo_mv(c); if (ac_msg != AM_DONE) ac_msg = -1; }
    else if (e & 0x40 && ac_row + 1 < ac_nrows) { ac_row++; mo_mv(c); if (ac_msg != AM_DONE) ac_msg = -1; }
    else if (e & 0x4000) ac_activate(c);
    else if (e & 0x2000) {
        if (mpm_scr == 6) { mpm_sfx(c, 2, 0); ac_leave(c); }
        else ac_sfx_deny(c);
    }
}

static void ac_rows(const Ctx& c) {
    ac_build();
    for (int k = 0; k < 2; k++) { mpm_set(c, A_TAB0 + k, 4 + (ac_tab == k)); mo_hide[A_TAB0 + k] = mpm_scr != 5; }
    for (int i = 0; i < 5; i++) {
        mo_hide[A_ROW0 + i] = i >= ac_nrows;
        if (i < ac_nrows) mpm_set(c, A_ROW0 + i, 8 + (ac_row == i && ac_dlg == AD_NONE));
    }
    mpm_set(c, A_BOX, 3);
    mo_hide[A_BOX] = ac_boot == 1;
    mpm_set(c, A_INFO, 13);
    mo_hide[A_INFO] = mpm_scr != 6;
    mpm_set(c, A_DLG, 10);
    bool one = ac_dlg == AD_STEAM;
    mpm_set(c, A_DBTN0, one || ac_dsel == 0 ? 12 : 11);
    mpm_set(c, A_DBTN1, ac_dsel == 1 ? 12 : 11);
    mo_hide[A_DBTN1] = one;
}

static void ac_attach(const Ctx& c) {
    uint32_t r = mpm_hld[A_ROOT], d = mpm_hld[A_DLG];
    for (int k = 0; k < 2; k++) mpm_hslot(c, A_TAB0 + k, r, mo_sn[MS_TAB1 + k]);
    for (int i = 0; i < 5; i++) mpm_hslot(c, A_ROW0 + i, r, mo_sn[MS_ROW1 + i]);
    mpm_hslot(c, A_BOX, r, S_SETSUMEI);
    mpm_hslot(c, A_INFO, r, ac_sn[AS_INFO]);
    mpm_hslot(c, A_DLG, r, mo_sn[MS_RDLG]);
    mpm_hslot(c, A_DBTN0, d, mo_sn[ac_dlg == AD_STEAM ? MS_RDBC : MS_RDB1]);
    mpm_hslot(c, A_DBTN1, d, mo_sn[MS_RDB2]);
}

static int ac_help() {
    if (ac_msg >= 0) return ac_msg;
    const AcRow& r = ac_rows_v[ac_row < ac_nrows ? ac_row : 0];
    if (ac_form == AF_TOP) return AH_STEAM;
    if (ac_form == AF_OFF) return r.kind == RK_FIELD ? AH_NAME : AH_OFF;
    switch (r.label) {
    case AT_CHNAME: return AH_CHNAME;
    case AT_LOGOUT: return AH_LOGOUT;
    case AT_DELETE: return AH_DELETE;
    default: return net_account_kind() == AK_ACCOUNT ? AH_STEAM : AH_SETSTEAM;
    }
}

static void ac_draw(const Ctx& c, uint32_t font, uint32_t lang) {
    uint32_t r = mpm_hld[A_ROOT];
    if (mpm_scr == 5) for (int k = 0; k < 2; k++) mo_text_c(c, font, mpm_slot(c, mpm_hld[A_TAB0 + k], ac_sn[AS_TEXT_TAB]), mpm_utf16(ac_t(k ? AT_TAB_OFF : AT_TAB_ACC, lang), 120 + k), MO_BTN_TEXT);
    else {
        char b[80];
        snprintf(b, sizeof(b), ac_t(net_account_kind() != AK_ACCOUNT ? AT_INFO_OFF : ac_account() ? AT_INFO_STEAM : AT_INFO_EXP, lang), net_account_name());
        uint32_t plate = mpm_has_state(mpm_hld[A_INFO], 13) ? mpm_slot(c, mpm_hld[A_INFO], ac_sn[AS_TEXT_INFO]) : 0;
        mo_text_c(c, font, plate ? plate : mpm_slot(c, r, ac_sn[AS_TEXT_INFO]), mpm_utf16(b, 122), MO_DLG_TEXT);
    }
    for (int i = 0; i < ac_nrows; i++) {
        const AcRow& w = ac_rows_v[i];
        uint32_t h = mpm_hld[A_ROW0 + i];
        bool on = ac_row == i && ac_dlg == AD_NONE;
        std::string t = ac_t(w.label, lang);
        if (w.kind == RK_FIELD) t += ": " + (ac_name.empty() ? std::string("-") : ac_name);
        mo_text_c(c, font, mpm_slot(c, h, mo_sn[MS_RBTXT]), mpm_utf16(t.c_str(), 123 + i), on ? MO_BTN_TEXT_ON : MO_BTN_TEXT);
    }
    if (ac_boot != 1) mpm_font_lines(c, font, mpm_slot(c, mpm_hld[A_BOX], S_TEXT), ac_t(ac_help(), lang), 60, 133);
}

static void ac_draw_dlg(const Ctx& c, uint32_t font, uint32_t lang) {
    uint32_t d = mpm_hld[A_DLG];
    int l1 = ac_dlg == AD_LOGOUT ? AQ_LOGOUT : ac_dlg == AD_DELETE ? AQ_DELETE : AQ_STEAM;
    int l2 = ac_dlg == AD_DELETE ? AQ_DELETE2 : ac_dlg == AD_STEAM ? AQ_STEAM2 : -1;
    bool one = ac_dlg == AD_STEAM;
    mo_text_c(c, font, mpm_slot(c, d, mo_sn[MS_RDL1]), mpm_utf16(ac_t(l1, lang), 136), MO_DLG_TEXT);
    if (l2 >= 0) mo_text_c(c, font, mpm_slot(c, d, mo_sn[MS_RDL2]), mpm_utf16(ac_t(l2, lang), 137), MO_DLG_TEXT);
    mo_text_c(c, font, mpm_slot(c, mpm_hld[A_DBTN0], mo_sn[MS_RBTXT]), mpm_utf16(ac_t(one ? AQ_CANCEL : AQ_YES, lang), 138), one || ac_dsel == 0 ? MO_BTN_TEXT_ON : MO_BTN_TEXT);
    if (!one) mo_text_c(c, font, mpm_slot(c, mpm_hld[A_DBTN1], mo_sn[MS_RBTXT]), mpm_utf16(ac_t(AQ_NO, lang), 139), ac_dsel == 1 ? MO_BTN_TEXT_ON : MO_BTN_TEXT);
}

// Options background (pak 57 copies of 22 entries 3/4 and 5/6): 181 Kame House, 188 turtle and Roshi drawn twice
static int ac_bg_ph = 0, ac_bg_t = 0, ac_bg_alt = 0;

static void ac_bg_start(const Ctx& c) {
    ac_bg_ph = 0; ac_bg_t = 0; ac_bg_alt = 0;
    if (!mpm_hld[B_OPT]) return;
    mpm_state(c, mpm_hld[B_OPT], 0);
    mpm_state(c, mpm_hld[B_TURTLE], 0);
    printf("menu: cuenta, fondo de Opciones (181 handle 0x%08X, 188 handle 0x%08X, estados %s)\n", mpm_hnd[B_OPT], mpm_hnd[B_TURTLE],
           mpm_has_state(mpm_hld[B_OPT], 1) && mpm_has_state(mpm_hld[B_TURTLE], 4) ? "completos" : "incompletos: pak sin aplicar");
}

// the account screens belong to Options: like Options (21_15 0x08A88570) they start its music 0x1129 when none is playing
static void ac_bgm(const Ctx& c) {
    uint32_t snd = psp_l32(0x08A78EE8u);
    if (!snd || mpm_call(c, 0x0883A7F4u, snd, 0)) return;
    Ctx c2 = c; c2.r[4] = snd; c2.r[5] = 0; c2.r[6] = 0; c2.r[7] = 0x1129; c2.r[8] = 0; c2.r[9] = 0; c2.f[12].f = 0.0f; c2.r[31] = 0;
    psp_call(c2, 0x0883A430u);
    printf("menu: cuenta, sin musica al entrar: suena la de Opciones (0x1129)\n");
}

static void ac_bg_anim(const Ctx& c) {
    if (mpm_scr >= 5 && ac_boot != 1 && mpm_phase != PH_LOAD) ac_bgm(c);
    if (!mpm_hld[B_OPT]) return;
    ac_bg_t++;
    if (ac_bg_ph == 0 && (ac_bg_t >= 20 || mpm_ended(c, mpm_hld[B_TURTLE]))) {
        ac_bg_ph = 1; ac_bg_t = 0;
        mpm_state(c, mpm_hld[B_OPT], 1); mpm_state(c, mpm_hld[B_TURTLE], 2); mpm_state(c, mpm_hld[B_ROSHI], 1);
    } else if (ac_bg_ph == 1 && ac_bg_t >= 240) {
        ac_bg_t = 0;
        int t = mpm_cur_state(mpm_hld[B_TURTLE]);
        mpm_state(c, mpm_hld[B_TURTLE], t == 2 ? (ac_bg_alt ^= 1) ? 3 : 4 : 2);
        mpm_state(c, mpm_hld[B_ROSHI], 1);
    }
    for (int k = B_OPT; k <= B_ROSHI; k++) {
        if (k == B_ROSHI && ac_bg_ph == 0) continue;
        uint32_t h = mpm_hld[k], inst = h ? psp_l32(h) : 0;
        mpm_advance(c, h);
        if (ac_bg_ph == 1 && k == B_OPT && inst && mpm_ended(c, h)) mpm_wf(inst + 0x2C, 0.0f);
    }
}

static void ac_bg_draw(const Ctx& c) {
    if (!mpm_hld[B_OPT]) return;
    mpm_draw_holder(c, mpm_hld[B_OPT]);
    mpm_draw_holder(c, mpm_hld[B_TURTLE]);
    if (ac_bg_ph == 1) mpm_draw_holder(c, mpm_hld[B_ROSHI]);
}

// first launch: the boot notices of overlay 7_7 (sub-state 3, Memory Stick check) wait on the savedata poll at 0x08A82280
// while the start screen runs as a widget on the root list that 7_7 updates and draws
static bool ac_boot_on() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_ACCOUNT_BOOT"); on = !(e && *e == '0'); }
    return on != 0;
}

static void ac_boot_open(Ctx& c) {
    mpm_setup_mem();
    mpm_act = mpm_next = MA_NONE; mpm_phase = PH_LOAD; mpm_scr = 5;
    ac_enter(AE_START);
    uint32_t w = mpm_alloc(c, 0x80);
    if (!w) { ac_boot = 2; printf("menu: cuenta, arranque: sin memoria para la pantalla de inicio\n"); return; }
    mpm_call(c, 0x089D1538u, w);
    psp_s32(w + 0x40, mpm_wvt());
    mpm_w = w;
    mpm_call(c, 0x089D0310u, mpm_root(), w, 0x80, 0);
    ac_boot = 1;
    printf("menu: cuenta, arranque sin perfil: pantalla de inicio antes de los creditos (widget 0x%08X, fuente 0x%08X)\n", w, psp_l32(0x08A78EECu));
}

bool ac_boot_wrap(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t ra = c.r[31];
    orig(c);
    if (ra != 0x08A82288u || !c.r[2] || ac_boot == 2) return true;
    int b = psp_bank_current();
    if (b < 0 || BANKS[b].indice != 6) return true;
    if (ac_boot == 0) {
        if (!patch_mp_menu.val || !ac_boot_on() || net_account_kind() != AK_NONE) { ac_boot = 2; return true; }
        ac_boot_open(c);
        if (ac_boot != 1) return true;
    }
    if (mpm_act == MA_NONE) { c.r[2] = 0; return true; }
    mpm_destroy(c);
    mpm_act = mpm_next = MA_NONE; mpm_scr = 0; mpm_phase = PH_LOAD; ac_boot = 2;
    printf("menu: cuenta, arranque: pantalla de inicio cerrada (perfil %d, %s), siguen los creditos\n", net_account_kind(), net_account_name());
    return true;
}

// Options (overlay 21_15) asked for the name keyboard: account settings instead
void psp_account_settings_request() {
    ac_opt = 1;
    printf("menu: cuenta, Opciones pidio el teclado del nombre: abre Ajustes de cuenta\n");
}

static bool mo_name_fit(const char* n) {
    size_t l = strlen(n);
    if (!l || l > 10) return false;
    for (size_t i = 0; i < l; i++) if ((unsigned char)n[i] < 0x20 || (unsigned char)n[i] > 0x7E) return false;
    return true;
}

bool psp_account_name(char* out, int n) {
    if (net_account_kind() == AK_NONE) return false;
    snprintf(out, n, "%s", net_account_name());
    return *out != 0;
}

void psp_gpu_blackout(bool on);
static bool ac_busy() { return ac_opt || ac_back_opt || ac_boot == 1; }

static void ac_frame(const Ctx& c) {
    if (net_account_kind() != AK_NONE) {
        uint32_t g = psp_l32(0x08A7F708u);
        std::string p = g ? mo_profile() : "";
        if (!p.empty() && p != net_account_name() && mo_name_fit(net_account_name())) {
            mo_profile_set(net_account_name());
            if (ac_sync_log++ < 4) printf("menu: cuenta, nombre del perfil del juego \"%s\" -> \"%s\"\n", p.c_str(), net_account_name());
        }
    }
    static int frames = 0;
    static bool hop = false;
    static int hop_f = 0;
    int b = psp_bank_current();
    int idx = b >= 0 ? BANKS[b].indice : -1;
    if (idx != 16) { hop = false; hop_f = 0; }
    else if ((ac_opt == 2 || ac_back_opt) && hop_f <= 180) hop = true;
    if (hop && ++hop_f > 180) hop = false;
    if (hop) psp_gpu_blackout(true);   // the main menu between Options and the account screen stays unseen
    if (ac_opt == 1) {
        if (idx != 20) { frames = 0; return; }
        if (++frames < 20) return;
        frames = 0; ac_opt = 2;
        printf("menu: cuenta, Opciones sale al menu principal\n");
        mpm_call(c, 0x089C34ACu, psp_l32(0x08A7EA50u), 1);
        return;
    }
    if (ac_opt != 2 && !ac_back_opt) { frames = 0; return; }
    if (idx != 16) { frames = 0; return; }
    if (++frames < 4) return;
    frames = 0;
    uint32_t mgr = psp_l32(0x08A7EA50u), sc = ac_opt == 2 ? 4u : 7u;
    if (sc == 7) ac_back_opt = 0; else { ac_opt = 0; ac_enter(AE_SETTINGS); }
    mpm_scene_prep(c, mgr, sc);
    printf("menu: cuenta, menu principal activo, pide la escena %u (%s)\n", sc, sc == 4 ? "ajustes de cuenta" : "Opciones");
    mpm_call(c, 0x089C34ACu, mgr, sc);
}
