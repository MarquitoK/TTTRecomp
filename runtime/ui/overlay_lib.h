#pragma once
// settings menu, Mods > Library: the mod library drawn with the menu pieces (network, images and install from library_ui.h)
struct OvIn { uint32_t pr; bool click, rclick, moved, held; float mx, my; int wheel; };

static const int OL_X = OV_PX + 10, OL_TY = OV_SY + OV_SH + 6, OL_TH = 12, OL_IY = OL_TY + OL_TH + 3, OL_GY = OL_IY + 10, OL_GB = OV_BY - 4;
static const float OL_BX[4] = {OL_X, OL_X + 95, OL_X + 200, OL_X + 316}, OL_BW[4] = {90, 100, 111, 64};
static const int OL_NC = 5, OL_CW = 71, OL_CG = 3, OL_COV = 40, OL_CH = 68, OL_CS = 71, OL_GW = OL_NC * OL_CW + (OL_NC - 1) * OL_CG;
static const int OL_IW = 168, OL_IH = 94, OL_RX = OL_X + OL_IW + 8, OL_RW = OV_PX + OV_PW - 10 - OL_RX, OL_BY0 = OL_TY + 46, OL_DY0 = OL_TY + OL_IH + 23, OL_DL = 6;
static const float OL_S = 0.45f;
static const float OL_FX = 208, OL_FW = 222, OL_FTY = 141;
static const float OL_FR[5][4] = {{0, 0, 0, 0}, {OL_FX, 91, OL_FW, 13}, {OL_FX, 116, 60, 13}, {OL_FX + 64, 116, OL_FW - 64, 13}, {OL_FX, 183, OL_FW, 38}};

static bool ol_in = false, ol_typing = false, ol_eat = false;
static int ol_tgt = 0;   // text being typed: 0 search, 1 name, 2 version, 3 link, 4 description
static std::string ol_tagname(int t);
static void ol_pub_start();
static int ol_ff = 0;
static int ol_f = 10, ol_df = 0, ol_osk = -1, ol_pop = 0, ol_psel = 0, ol_pcols = 1, ol_lsel = 0, ol_hov = -2;
static float ol_scroll = 0, ol_ascroll = 0, ol_fscroll = 0;
static std::string ol_q, ol_ptitle;
static unsigned ol_q_due = 0;
static std::vector<std::pair<std::string, std::function<void()>>> ol_pitems;

static bool ov_lib_page() { return ov_tab == 4 && ov_sub == 1; }

struct OlName { const char* en; const char* es; };
static const OlName OL_CAT[14] = {{"All", "Todas"}, {"Ranked OK", "Clasificatorios"}, {"Textures", "Texturas"}, {"Audio", "Audio"}, {"Characters", "Personajes"}, {"Roster", "Roster"},
                                  {"Maps", "Mapas"}, {"Menus", "Menús"}, {"Attacks", "Ataques"}, {"Models", "Modelos"}, {"Story mode", "Modo historia"}, {"Logic", "Lógica"},
                                  {"Other", "Otros"}, {"Devs", "Devs"}};
static const OlName OL_SORT[3] = {{"Newest", "Más recientes"}, {"Most downloaded", "Más descargados"}, {"Most liked", "Más gustados"}};
static const OlName OL_TR[] = {
    {"This mod is no longer in the library.", "Este mod ya no está en la biblioteca."},
    {"Can't reach the mod library.", "No se pudo conectar con la biblioteca de mods."},
    {"Sign in again to do that.", "Inicia sesión de nuevo para hacer eso."},
    {"The mod library did not accept that.", "La biblioteca de mods no aceptó la acción."},
    {"Your heart could not be saved.", "No se pudo guardar tu me gusta."},
    {"Could not write the download to the disk.", "No se pudo guardar la descarga en el disco."},
    {"Download cancelled.", "Descarga cancelada."},
    {"The file of this mod is no longer on GitHub.", "El archivo de este mod ya no está en GitHub."},
    {"The download failed. Try again.", "La descarga falló. Inténtalo de nuevo."},
    {"The downloaded file is not the one that was published. Install cancelled.", "El archivo descargado no es el publicado. Instalación cancelada."},
    {"Thanks, the report was sent.", "Gracias, el reporte fue enviado."},
    {"The mod is visible again.", "El mod vuelve a ser visible."},
    {"The mod is hidden.", "El mod quedó oculto."},
    {"The mod was removed from the library.", "El mod fue quitado de la biblioteca."},
    {"The author was banned.", "El autor fue bloqueado."},
    {"Your mod was removed from the library.", "Tu mod fue quitado de la biblioteca."},
    {"GitHub did not answer as expected. Try again in a moment.", "GitHub no respondió como se esperaba. Inténtalo en un momento."},
    {"Can't reach GitHub. Check your internet connection.", "No se pudo conectar con GitHub. Revisa tu conexión a internet."},
    {"The sign in was cancelled on GitHub.", "El inicio de sesión se canceló en GitHub."},
    {"The code expired. Try again.", "El código expiró. Inténtalo de nuevo."},
    {"The mod library refused the sign in.", "La biblioteca de mods rechazó el inicio de sesión."},
    {"Type the mod name.", "Escribe el nombre del mod."},
    {"Only the author can publish this mod.", "Solo el autor puede publicar este mod."},
    {"Type a version with letters, numbers and dots (for example 1.0).", "Escribe una versión con letras, números y puntos (por ejemplo 1.0)."},
    {"The link must start with http:// or https://", "El enlace debe empezar con http:// o https://"},
    {"Still checking the mod, try again in a moment.", "Todavía se está revisando el mod, inténtalo en un momento."},
    {"Sign in with GitHub again to publish.", "Vuelve a iniciar sesión con GitHub para publicar."},
    {"Sign in with GitHub to publish.", "Inicia sesión con GitHub para publicar."},
    {"The mod is too big (2 GB or 65535 files at most).", "El mod es demasiado grande (2 GB o 65535 archivos como máximo)."},
    {"The mod is too big (2 GB at most).", "El mod es demasiado grande (2 GB como máximo)."},
    {"The mod folder is empty.", "La carpeta del mod está vacía."},
    {"Could not pack the mod.", "No se pudo empaquetar el mod."},
    {"GitHub closed the session. Sign out and sign in again.", "GitHub cerró la sesión. Cierra sesión y vuelve a entrar."},
    {"Can't reach GitHub.", "No se pudo conectar con GitHub."},
    {"Could not create the repository tttrecomp-mods on your GitHub.", "No se pudo crear el repositorio tttrecomp-mods en tu GitHub."},
    {"Could not create the release on GitHub.", "No se pudo crear el release en GitHub."},
    {"The upload to GitHub failed. Try again.", "La subida a GitHub falló. Inténtalo de nuevo."},
    {"Your account can't publish in the library.", "Tu cuenta no puede publicar en la biblioteca."},
    {"Too many publications today. Try again tomorrow.", "Demasiadas publicaciones hoy. Inténtalo mañana."},
    {"The mod library did not accept the mod.", "La biblioteca de mods no aceptó el mod."},
    {"Saved.", "Guardado."},
    {"Can't reach the mod library. Close this form and try again.", "No se pudo conectar con la biblioteca de mods. Cierra el formulario e inténtalo de nuevo."},
    {"Only .mkpak files can be published.", "Solo se pueden publicar archivos .mkpak."},
    {"This file is not a valid .mkpak.", "Este archivo no es un .mkpak válido."},
    {"This .mkpak has too many files.", "Este .mkpak tiene demasiados archivos."},
    {"This .mkpak contains program files and can't be published.", "Este .mkpak contiene archivos de programa y no se puede publicar."},
    {"This .mkpak is damaged or uses an unsupported format.", "Este .mkpak está dañado o usa un formato no compatible."},
    {"This .mkpak is damaged.", "Este .mkpak está dañado."},
    {"This .mkpak has no mod.txt.", "Este .mkpak no tiene mod.txt."},
};
static std::string ol_tr(const std::string& s) {
    if (!ov_es()) return s;
    for (const OlName& t : OL_TR) if (s == t.en) return t.es;
    if (!s.compare(0, 13, "Signed in as ")) { std::string n = s.substr(13); if (!n.empty() && n.back() == '.') n.pop_back(); return "Sesión iniciada como " + n + "."; }
    if (!s.compare(0, 13, "Link copied: ")) return "Enlace copiado: " + s.substr(13);
    if (!s.compare(0, 11, "Published \"")) return "Publicado \"" + s.substr(11);
    if (!s.compare(0, 46, "Change the version to publish an update (now v")) return "Cambia la versión para publicar una actualización (ahora v" + s.substr(46);
    if (!s.compare(0, 55, "This mod was installed from the library and belongs to ")) return "Este mod se instaló desde la biblioteca y pertenece a " + s.substr(55);
    return s;
}

static std::string ol_fit(std::string s, float size, float maxw) {
    if (psp_ui_text_w(0, size, s) <= maxw) return s;
    while (!s.empty() && psp_ui_text_w(0, size, s + "...") > maxw) {
        while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
        if (!s.empty()) s.pop_back();
    }
    return s + "...";
}
static std::string ol_count(long long n) {
    char b[32];
    if (n >= 1000000) snprintf(b, sizeof(b), "%.1fM", n / 1000000.0);
    else if (n >= 10000) snprintf(b, sizeof(b), "%lldk", n / 1000);
    else if (n >= 1000) snprintf(b, sizeof(b), "%.1fk", n / 1000.0);
    else snprintf(b, sizeof(b), "%lld", n);
    return b;
}
static std::string ol_size(long long b) { return mw_u8(lb_size(b).c_str()); }
static int ol_cat() { return lb_ranked ? 1 : lb_tag + 2 < 2 ? 0 : lb_tag + 2; }
static void ol_reload() { ol_scroll = 0; if (ol_f >= 10) ol_f = 10; lb_view = 0; lb_load(false); }
static void ol_cat_set(int c) {
    c = (c % 14 + 14) % 14;
    lb_ranked = c == 1; lb_tag = c >= 2 ? c - 2 : -1;
    printf("ajustes: biblioteca: categoria %s\n", OL_CAT[c].en);
    ol_reload();
}
static void ol_sort_set(int s) { lb_sort = (s % 3 + 3) % 3; printf("ajustes: biblioteca: orden %s\n", OL_SORT[lb_sort].en); ol_reload(); }
static void ol_col_set(int c) { if (lb_col == c) return; lb_col = c; ol_reload(); }

// small white glyphs drawn tinted: 0 heart, 1 download, 2 search, 3 check, 4 person, 5 link, 6 flag, 7 pencil, 8 eye, 9 trash, 10 ban, 11 globe
static float ol_seg(float px, float py, float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay, t = std::max(0.0f, std::min(1.0f, ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)));
    return hypotf(px - ax - t * dx, py - ay - t * dy);
}
static bool ol_ink(int k, float x, float y) {
    if (k == 0) { float X = (x - 0.5f) * 2.6f, Y = (0.42f - y) * 2.6f, a = X * X + Y * Y - 1; return a * a * a - X * X * Y * Y * Y <= 0; }
    if (k == 1) return (fabsf(x - 0.5f) < 0.09f && y > 0.08f && y < 0.55f) || (y >= 0.42f && y < 0.76f && fabsf(x - 0.5f) < (0.76f - y) * 1.05f) || (y > 0.84f && y < 0.95f && x > 0.14f && x < 0.86f);
    if (k == 2) { float d = hypotf(x - 0.42f, y - 0.42f); return (d > 0.2f && d < 0.31f) || ol_seg(x, y, 0.64f, 0.64f, 0.88f, 0.88f) < 0.08f; }
    if (k == 3) return ol_seg(x, y, 0.14f, 0.52f, 0.4f, 0.78f) < 0.09f || ol_seg(x, y, 0.4f, 0.78f, 0.88f, 0.24f) < 0.09f;
    if (k == 5) { float a = ol_seg(x, y, 0.22f, 0.78f, 0.36f, 0.64f), b = ol_seg(x, y, 0.64f, 0.36f, 0.78f, 0.22f); return fabsf(a - 0.15f) < 0.055f || fabsf(b - 0.15f) < 0.055f || ol_seg(x, y, 0.4f, 0.6f, 0.6f, 0.4f) < 0.055f; }
    if (k == 6) return (x > 0.14f && x < 0.27f && y > 0.06f && y < 0.95f) || (x >= 0.26f && x < 0.86f && y > 0.1f && y < 0.56f && !(x > 0.66f && fabsf(y - 0.33f) < (x - 0.66f) * 0.9f));
    if (k == 7) return ol_seg(x, y, 0.3f, 0.7f, 0.82f, 0.18f) < 0.11f || ol_seg(x, y, 0.13f, 0.87f, 0.27f, 0.73f) < 0.06f;
    if (k == 8) { float u = (x - 0.5f) / 0.46f, v = (y - 0.5f) / 0.3f, o = u * u + v * v, iu = (x - 0.5f) / 0.34f, iv = (y - 0.5f) / 0.19f; return (o < 1 && iu * iu + iv * iv >= 1) || hypotf(x - 0.5f, y - 0.5f) < 0.13f; }
    if (k == 9) return (y > 0.14f && y < 0.23f && x > 0.12f && x < 0.88f) || (y > 0.06f && y < 0.15f && x > 0.36f && x < 0.64f) || (y > 0.27f && y < 0.93f && x > 0.22f && x < 0.78f && !(y > 0.38f && y < 0.84f && ((x > 0.36f && x < 0.43f) || (x > 0.57f && x < 0.64f))));
    if (k == 10) { float d = hypotf(x - 0.5f, y - 0.5f); return (d > 0.32f && d < 0.44f) || (d < 0.34f && ol_seg(x, y, 0.27f, 0.27f, 0.73f, 0.73f) < 0.07f); }
    if (k == 11) { float d = hypotf(x - 0.5f, y - 0.5f), u = (x - 0.5f) / 0.18f, v = (y - 0.5f) / 0.42f, e = sqrtf(u * u + v * v); return (d > 0.36f && d < 0.45f) || (d < 0.4f && (fabsf(e - 1) < 0.22f || fabsf(y - 0.5f) < 0.035f || fabsf(x - 0.5f) < 0.035f)); }
    float bx = (x - 0.5f) / 0.37f, by = (y - 0.98f) / 0.4f;
    return hypotf(x - 0.5f, y - 0.3f) < 0.19f || (bx * bx + by * by < 1 && y < 0.94f);
}
static GLuint ol_glyph(int k) {
    std::string key = "g|" + std::to_string(k);
    auto it = ov_cache.find(key);
    if (it != ov_cache.end()) return it->second.t;
    OvImg im; im.w = im.h = 32; im.px.resize(32 * 32);
    for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
        int c = 0;
        for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) c += ol_ink(k, (x + (i + 0.5f) / 4) / 32, (y + (j + 0.5f) / 4) / 32);
        im.px[y * 32 + x] = 0xFFFFFFu | (uint32_t)(c * 255 / 16) << 24;
    }
    return (ov_cache[key] = ov_upload(im)).t;
}
static void ol_box(float x, float y, float w, float h, uint32_t fill, float a, uint32_t border) {
    ov_quad(ov_white, x, y, w, h, fill, a);
    ov_quad(ov_white, x, y, w, 1, border, 1); ov_quad(ov_white, x, y + h - 1, w, 1, border, 1);
    ov_quad(ov_white, x, y, 1, h, border, 1); ov_quad(ov_white, x + w - 1, y, 1, h, border, 1);
}

// library images as menu textures: cards shrunk, the detail image whole, avatars round
static const OvTex* ol_img(const std::string& url, int mode) {
    if (url.empty()) return nullptr;
    std::string k = (mode == 0 ? "lc|" : mode == 1 ? "lf|" : "la|") + url;
    auto it = ov_cache.find(k);
    if (it != ov_cache.end()) return it->second.t ? &it->second : nullptr;
    lb_img_want(url);
    std::vector<uint8_t> px;
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> g(lb_im);
        auto f = lb_imgs.find(url);
        if (f == lb_imgs.end() || f->second.st < 2) return nullptr;
        if (f->second.st == 3) { ov_cache[k] = OvTex(); return nullptr; }
        px = f->second.px; w = f->second.w; h = f->second.h;
    }
    if (mode == 0) lb_shrink(px, w, h, 320);
    OvImg im;
    if (mode == 2) {
        int d = std::min(w, h), ox = (w - d) / 2, oy = (h - d) / 2;
        im.w = im.h = d; im.px.resize((size_t)d * d);
        float r = d * 0.5f;
        for (int y = 0; y < d; y++) for (int x = 0; x < d; x++) {
            const uint8_t* p = &px[((size_t)(y + oy) * w + x + ox) * 4];
            float e = std::max(0.0f, std::min(1.0f, r - hypotf(x + 0.5f - r, y + 0.5f - r)));
            im.px[(size_t)y * d + x] = p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)(p[3] * e) << 24;
        }
    } else { im.w = w; im.h = h; im.px.resize((size_t)w * h); memcpy(im.px.data(), px.data(), im.px.size() * 4); }
    return &(ov_cache[k] = ov_upload(im));
}
static void ol_fitimg(const OvTex* t, float x, float y, float w, float h, float a = 1.0f) {
    float k = std::min(w / t->w, h / t->h), dw = t->w * k, dh = t->h * k;
    ov_quad(t->t, x + (w - dw) * 0.5f, y + (h - dh) * 0.5f, dw, dh, 0xFFFFFF, a);
}

static std::string ol_desc_text() {
    std::string t;
    if (lb_me.admin && !lb_cur.reports_list.empty()) { t = ov_s("Reports: ", "Reportes: "); for (size_t i = 0; i < lb_cur.reports_list.size(); i++) t += (i ? ", " : "") + lb_cur.reports_list[i]; t += ".\n"; }
    t += lb_cur.descr.empty() ? (lb_cur.full ? ov_s("No description.", "Sin descripción.") : ov_s("Loading...", "Cargando...")) : lb_cur.descr;
    return t;
}
static std::vector<std::string> ol_paragraphs(const std::string& s, float size, float w) {
    std::vector<std::string> out;
    size_t p = 0;
    while (p <= s.size()) {
        size_t e = s.find('\n', p);
        std::string l = s.substr(p, e == std::string::npos ? std::string::npos : e - p);
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        if (l.empty()) { if (!out.empty() && !out.back().empty()) out.push_back(""); }
        else for (auto& x : ov_wrap(l, size, w, 999)) out.push_back(x);
        if (e == std::string::npos) break;
        p = e + 1;
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}
static bool ol_desc_cut() { return (int)ol_paragraphs(ol_desc_text(), 0.4f, OL_IW - 8.0f).size() > OL_DL; }

static void ol_enter() {
    if (ol_in) return;
    ol_in = true;
    if (mods_root_global.empty()) { const char* e = getenv("TTT_ASSETS"); mods_root_global = mods_root_of(e && *e && strcmp(e, "0") ? e : "../../assets"); }
    mw_root = mods_root_global;
    mw_mods = mods_list(mw_root);
    lb_notice_fn = [](const std::wstring& t) { ov_notice(ol_tr(mw_u8(t.c_str()))); };
    lb_done_fn = [](const std::string& id) {
        ov_mods_changed = true; ov_mods_read = false;
        mw_mods = mods_list(mw_root);
        std::string n = lb_cur.name;
        for (auto& m : mw_mods) if (m.id == id) n = mw_name(m);
        ov_notice(ov_s("Installed \"", "Instalado \"") + n + ov_s("\". It applies after restarting.", "\". Se aplica al reiniciar."));
        printf("ajustes: biblioteca: %s instalado como %s\n", lb_cur.id.c_str(), id.c_str());
    };
    if (!ov_net) ui_activity(1);
    if (!lb_started) { lb_started = true; lb_me_check(); lb_load(false); }
    ol_q = lb_text;
}
static void ol_leave() { if (ol_in) ui_activity(0); ol_in = false; ol_typing = false; ol_osk = -1; ol_pop = 0; }
static void ol_open_id(const std::string& id) {
    ov_sub_set(1);
    ol_enter();
    LbMod m; m.id = id;
    lb_open_detail(m);
    ol_df = 0; ol_ascroll = 0;
    printf("ajustes: biblioteca: enlace a %s\n", id.c_str());
}

static void ol_search_set(const std::string& q) { ol_q = q; ol_q_due = GetTickCount() + 450; }
static void ol_tick() {
    lb_poll();
    if (ol_q_due && GetTickCount() >= ol_q_due) {
        ol_q_due = 0;
        std::string s = ol_q;
        while (!s.empty() && s.back() == ' ') s.pop_back();
        if (s != lb_text) { lb_text = s; printf("ajustes: biblioteca: busqueda \"%s\"\n", s.c_str()); ol_reload(); }
    }
}
static void ol_type_end() { ol_typing = false; if (ol_q_due) ol_q_due = GetTickCount(); }
static bool ol_wants_chars() { return ov_open && ov_lib_page() && (ol_typing || ol_osk >= 0); }
static std::string& ol_tref() { return ol_tgt ? lb_ftxt[ol_tgt - 1] : ol_q; }
static void ol_tset(std::string q) {
    static const size_t M[5] = {64, 64, 16, 300, 4000};
    while (q.size() > M[ol_tgt]) { q.pop_back(); while (!q.empty() && ((unsigned char)q.back() & 0xC0) == 0x80) q.pop_back(); if (!q.empty() && ((unsigned char)q.back() & 0xC0) == 0xC0) q.pop_back(); }
    if (ol_tgt == 0) ol_search_set(q); else lb_ftxt[ol_tgt - 1] = q;
}
static void ol_char(wchar_t c) {
    if (c == 27 || (c == 13 && ol_tgt != 4)) { if (ol_osk >= 0) ol_osk = -1; ol_type_end(); ol_eat = true; return; }
    std::string q = ol_tref();
    if (c == 13) q += "\n";
    else if (c == 8) { while (!q.empty() && ((unsigned char)q.back() & 0xC0) == 0x80) q.pop_back(); if (!q.empty()) q.pop_back(); }
    else if (c == 22) {
        if (!OpenClipboard(ventana)) return;
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        const wchar_t* w = h ? (const wchar_t*)GlobalLock(h) : nullptr;
        std::string t = w ? mw_u8(w) : "";
        if (w) GlobalUnlock(h);
        CloseClipboard();
        for (char ch : t) if (ch == '\n' && ol_tgt == 4) q += ch; else if (ch == '\n' || ch == '\t') q += ' '; else if (ch != '\r') q += ch;
    }
    else if (c >= 32 && (c < 0xD800 || c > 0xDFFF)) { wchar_t w[2] = {c, 0}; q += mw_u8(w); }
    else return;
    ol_tset(q);
}

// on-screen keyboard for the controller
static const char* OL_KEYS[4] = {"1234567890", "qwertyuiop", "asdfghjklñ", "zxcvbnm-_."};
static const float OL_KX = 120, OL_KY = 70, OL_KW = 240, OL_KH = 128;
static std::string ol_key(int i) {
    const char* s = OL_KEYS[i / 10];
    int n = i % 10;
    const unsigned char* p = (const unsigned char*)s;
    while (n > 0 && *p) { p++; while ((*p & 0xC0) == 0x80) p++; n--; }
    const unsigned char* e = p + 1;
    while ((*e & 0xC0) == 0x80) e++;
    return std::string((const char*)p, (const char*)e);
}
static void ol_key_rect(int i, float* x, float* y, float* w) {
    if (i < 40) { *x = OL_KX + 10 + (i % 10) * 22; *y = OL_KY + 40 + (i / 10) * 16; *w = 20; }
    else { *x = OL_KX + 10 + (i - 40) * 74; *y = OL_KY + 40 + 4 * 16 + 2; *w = 70; }
}
static void ol_key_press(int i) {
    if (i < 40) ol_tset(ol_tref() + ol_key(i));
    else if (i == 40) ol_tset(ol_tref() + " ");
    else if (i == 41) ol_char(8);
    else { ol_osk = -1; ol_type_end(); }
}

// list popups: categories, sort, account, report reasons, full description (kind 5)
static void ol_popup(int kind, const std::string& title, std::vector<std::pair<std::string, std::function<void()>>> items, int cols, int sel) {
    ol_pop = kind; ol_ptitle = title; ol_pitems = std::move(items); ol_pcols = cols; ol_psel = std::max(0, std::min(sel, (int)ol_pitems.size() - 1)); ol_fscroll = 0;
}
static void ol_pop_rect(float* x, float* y, float* w, float* h) {
    if (ol_pop == 5) { *x = 70; *y = 30; *w = 340; *h = 200; return; }
    int rows = ((int)ol_pitems.size() + ol_pcols - 1) / ol_pcols;
    *w = ol_pcols * 124.0f + 20; *h = 26.0f + rows * 17 + 6; *x = 240 - *w * 0.5f; *y = std::max(30.0f, 136 - *h * 0.5f);
}
static int ol_pop_at(float mx, float my) {
    float x, y, w, h; ol_pop_rect(&x, &y, &w, &h);
    int rows = ((int)ol_pitems.size() + ol_pcols - 1) / ol_pcols;
    for (size_t i = 0; i < ol_pitems.size(); i++) {
        float bx = x + 10 + (i / rows) * 124, by = y + 24 + (i % rows) * 17;
        if (mx >= bx && mx < bx + 120 && my >= by && my < by + 15) return (int)i;
    }
    return -1;
}
static void ol_cat_popup() {
    std::vector<std::pair<std::string, std::function<void()>>> it;
    for (int i = 0; i < 14; i++) it.push_back({ov_s(OL_CAT[i].en, OL_CAT[i].es), [i] { ol_cat_set(i); }});
    ol_popup(1, ov_s("Category", "Categoría"), it, 2, ol_cat());
}
static void ol_sort_popup() {
    std::vector<std::pair<std::string, std::function<void()>>> it;
    for (int i = 0; i < 3; i++) it.push_back({ov_s(OL_SORT[i].en, OL_SORT[i].es), [i] { ol_sort_set(i); }});
    ol_popup(2, ov_s("Sort by", "Ordenar por"), it, 1, lb_sort);
}
static void ol_account() {
    if (lb_me_checking) return;
    if (!lb_me.in) { lb_login_start(); ol_lsel = 0; return; }
    std::vector<std::pair<std::string, std::function<void()>>> it;
    if (lb_col) it.push_back({ov_s("All mods", "Todos los mods"), [] { ol_col_set(0); }});
    if (lb_col != 2) it.push_back({ov_s("My mods", "Mis mods"), [] { ol_col_set(2); }});
    if (lb_col != 1) it.push_back({ov_s("Liked", "Me gustan"), [] { ol_col_set(1); }});
    if (lb_me.admin && lb_col != 3) it.push_back({ov_s("Review", "Revisión"), [] { ol_col_set(3); }});
    it.push_back({ov_s("Publish a mod", "Publicar un mod"), [] { ol_pub_start(); }});
    it.push_back({ov_s("My repository", "Mi repositorio"), [] { lb_act(LA_REPO, 0); }});
    it.push_back({ov_s("Sign out", "Cerrar sesión"), [] { lb_signout(); printf("ajustes: biblioteca: sesion cerrada\n"); }});
    ol_popup(3, lb_me.login, it, 1, 0);
}

// detail buttons: install, the general row, the web link, the description, then the admin or own-mod row
struct OlBtn { float x, y, w, h; std::string t, tag; std::function<void()> fn, alt; bool on = true; int glyph = -1, kind = 0, prog = -1, arg = 0; uint32_t ic = 0x95F1FF; };
static const int OL_KMAIN = 1, OL_KSMALL = 2, OL_KLINK = 3, OL_KDESC = 4, OL_KADMIN = 5, OL_KREST = 6;
static std::vector<OlBtn> ol_btns() {
    std::vector<OlBtn> a;
    const LbMod& m = lb_cur;
    bool busy = lb_dl.st != 0, here = busy && lb_dl.id == m.id;
    int ins = lb_installed(m);
    float x = (float)OL_RX, w = (float)OL_RW, y = (float)OL_BY0;
    OlBtn i; i.x = x; i.y = y; i.w = w; i.h = 16; i.kind = OL_KMAIN; i.glyph = 1; i.ic = 0xFFFFFF;
    if (here && lb_dl.st == 1) {
        uint64_t d = lb_dl_done, t = lb_dl_total;
        i.prog = t ? (int)std::min<uint64_t>(100, d * 100 / t) : 0;
        i.t = ov_s("Downloading ", "Descargando ") + std::to_string(i.prog) + " %"; i.tag = ov_s("Cancel", "Cancelar");
        i.fn = [] { lb_dl_cancel = true; };
    } else if (here) { i.t = ov_s("Installing...", "Instalando..."); i.on = false; }
    else if (m.hidden == 3) { i.t = ov_s("File not available", "Archivo no disponible"); i.on = false; }
    else if (ins == 2) { i.t = ov_s("Installed", "Instalado"); i.on = false; i.glyph = 3; i.ic = 0x4CD486; }
    else {
        i.t = ins == 1 ? ov_s("Update to v", "Actualizar a v") + m.ver : ov_s("Install", "Instalar");
        if (m.size) i.tag = ol_size(m.size);
        i.on = !busy;
        i.fn = [] { printf("ajustes: biblioteca: instalando %s v%s\n", lb_cur.id.c_str(), lb_cur.ver.c_str()); lb_install(); };
    }
    a.push_back(i);
    std::vector<OlBtn> row;
    { OlBtn b; b.t = ol_count(m.likes); b.glyph = 0; b.ic = m.liked ? 0xEC5464 : 0x95F1FF; b.fn = [] { if (!lb_me.in) ol_lsel = 0; lb_like(); }; row.push_back(b); }
    { OlBtn b; b.t = ov_s("Link", "Enlace"); b.glyph = 5; b.fn = [] { lb_act(LA_SHARE, 0); }; row.push_back(b); }
    if (!m.mine) { OlBtn b; b.t = ov_s("Report", "Reportar"); b.glyph = 6; b.fn = [] {
        if (!lb_me.in) { lb_login_start(); ol_lsel = 0; return; }
        static const OlName W[4] = {{"Broken content", "Contenido roto"}, {"Someone else's content", "Contenido ajeno"}, {"Offensive", "Ofensivo"}, {"Other", "Otro"}};
        std::vector<std::pair<std::string, std::function<void()>>> it;
        for (int k = 0; k < 4; k++) it.push_back({ov_s(W[k].en, W[k].es), [k] { printf("ajustes: biblioteca: reporte de %s\n", lb_cur.id.c_str()); lb_report(k); }});
        ol_popup(4, ov_s("Report", "Reportar"), it, 1, 0);
    }; row.push_back(b); }
    if (!m.author.empty()) { OlBtn b; b.t = ov_s("Author", "Autor"); b.glyph = 4; b.fn = [] { lb_act(LA_AUTHOR, 0); ol_q = lb_text; ol_f = 10; ol_scroll = 0; }; row.push_back(b); }
    y += 19;
    float bw = floorf((w - 3 * 4) / 4);
    for (size_t k = 0; k < row.size(); k++) { row[k].x = x + k * (bw + 4); row[k].y = y; row[k].w = bw; row[k].h = 14; row[k].kind = OL_KSMALL; a.push_back(row[k]); }
    y += 17;
    if (!m.link.compare(0, 7, "http://") || !m.link.compare(0, 8, "https://")) {
        OlBtn b; b.x = x; b.y = y; b.w = w; b.h = 10; b.kind = OL_KLINK; b.glyph = 11; b.t = mw_u8(lb_host(m.link).c_str()); b.fn = [] { lb_act(LA_LINK, 0); };
        a.push_back(b);
    }
    y += 14;
    if (ol_desc_cut()) { OlBtn b; b.x = (float)OL_X; b.y = (float)OL_DY0; b.w = (float)OL_IW; b.h = (float)(OL_GB - OL_DY0); b.kind = OL_KDESC; b.fn = [] { ol_popup(5, lb_cur.name, {}, 1, 0); }; a.push_back(b); }
    std::vector<OlBtn> adm;
    if (lb_me.admin) {
        std::string id = m.id;
        { OlBtn b; b.t = ov_s("Edit", "Editar"); b.glyph = 7; b.fn = [] { lb_edit_open(); ol_ff = 0; }; adm.push_back(b); }
        { OlBtn b; b.t = m.hidden ? ov_s("Show", "Mostrar") : ov_s("Hide", "Ocultar"); b.glyph = 8; b.fn = [] { lb_act(LA_HIDE, 0); }; adm.push_back(b); }
        { OlBtn b; b.t = ov_s("Remove", "Quitar"); b.glyph = 9; b.ic = 0xFF8A70; b.fn = [] { ov_confirm = 7; ov_yes = 0; }; adm.push_back(b); }
        if (m.author != lb_me.login) { OlBtn b; b.t = ov_s("Ban", "Bloquear"); b.glyph = 10; b.ic = 0xFF8A70; b.fn = [] { ov_confirm = 8; ov_yes = 0; }; adm.push_back(b); }
    } else if (m.mine) { OlBtn b; b.t = ov_s("Remove from the library", "Quitar de la biblioteca"); b.glyph = 9; b.ic = 0xFF8A70; b.fn = [] { ov_confirm = 9; ov_yes = 0; }; adm.push_back(b); }
    y += 10;
    for (size_t k = 0; k < adm.size(); k++) { adm[k].x = x + k * (bw + 4); adm[k].y = y; adm[k].w = adm.size() == 1 ? w : bw; adm[k].h = 14; adm[k].kind = OL_KADMIN; a.push_back(adm[k]); }
    OlBtn r; r.x = OV_BX; r.y = OV_BY; r.w = OV_BW; r.h = OV_BH; r.kind = OL_KREST; r.fn = [] { ov_restart(); };
    a.push_back(r);
    return a;
}
static float ol_sect_y() { return OL_BY0 + 19 + 17 + 14.0f; }
static int ol_nav(const std::vector<OlBtn>& b, int cur, int dx, int dy) {
    if (cur < 0 || cur >= (int)b.size()) return 0;
    float cx = b[cur].x + b[cur].w * 0.5f, cy = b[cur].y + b[cur].h * 0.5f, best = 1e9f;
    int r = cur;
    for (int k = 0; k < (int)b.size(); k++) {
        if (k == cur) continue;
        float ex = b[k].x + b[k].w * 0.5f, ey = b[k].y + b[k].h * 0.5f, ddx = ex - cx, ddy = ey - cy;
        float along = dx ? ddx * dx : ddy * dy, side = dx ? fabsf(ddy) : std::max(0.0f, std::max(b[k].x - (b[cur].x + b[cur].w), b[cur].x - (b[k].x + b[k].w)));
        if (along <= 1) continue;
        float s = along + side * 3;
        if (s < best) { best = s; r = k; }
    }
    return r;
}

static void ol_confirm(int c) {
    if (c == 7) lb_action("POST", "admin/remove/" + lb_cur.id, "{}", L"The mod was removed from the library.", true);
    else if (c == 8) lb_action("POST", "admin/ban/" + lb_cur.author, "{\"on\":1}", L"The author was banned.", true);
    else if (c == 9) lb_action("DELETE", "mod/" + lb_cur.id, "", L"Your mod was removed from the library.", true);
    printf("ajustes: biblioteca: accion %d sobre %s\n", c, lb_cur.id.c_str());
}
static void ol_confirm_text(int c, std::string& q, std::string& sub, std::string& yes) {
    std::string n = lb_cur.name;
    if (c == 8) { q = ol_fit(ov_s("Ban ", "¿Bloquear a ") + lb_cur.author, OV_TS, 170) + "?"; sub = ov_s("All their mods are hidden and they can't publish.", "Se ocultan todos sus mods y ya no puede publicar."); yes = ov_s("Ban", "Bloquear"); return; }
    q = ol_fit(ov_s("Remove \"", "¿Quitar \"") + n, OV_TS, 165) + "\"?";
    sub = c == 9 ? ov_s("The file stays on your GitHub.", "El archivo sigue en tu GitHub.") : ov_s("The file stays on the author's GitHub.", "El archivo sigue en el GitHub del autor.");
    yes = ov_s("Remove", "Quitar");
}

// ---- input
static int ol_card_at(float mx, float my) {
    if (mx < OL_X || mx >= OL_X + OL_GW || my < OL_GY || my >= OL_GB) return -1;
    int c = (int)((mx - OL_X) / (OL_CW + OL_CG)), r = (int)((my - OL_GY + ol_scroll) / OL_CS);
    float cx = OL_X + c * (OL_CW + OL_CG), cy = OL_GY + r * OL_CS - ol_scroll;
    int i = r * OL_NC + c;
    return c < OL_NC && mx < cx + OL_CW && my < cy + OL_CH && i < (int)lb_mods.size() ? i : -1;
}
static bool ol_restart_at(float mx, float my) { return mx >= OV_BX && mx < OV_BX + OV_BW && my >= OV_BY && my < OV_BY + OV_BH; }
static int ol_grid_hit(float mx, float my) {
    if (ol_restart_at(mx, my)) return -1;
    if (my >= OL_TY && my < OL_TY + OL_TH) for (int i = 0; i < 4; i++) if (mx >= OL_BX[i] && mx < OL_BX[i] + OL_BW[i]) return i;
    int c = ol_card_at(mx, my);
    if (c >= 0) return 10 + c;
    if (lb_mods.empty() && lb_err && my >= OL_GY + 40 && my < OL_GY + 55 && mx >= 190 && mx < 290) return 9;
    return -2;
}
static void ol_activate(int f, float mx) {
    if (f == 0) { ol_tgt = 0; if (ov_pad()) ol_osk = 0; else ol_typing = true; }
    else if (f == 1 || f == 2) {
        float x = OL_BX[f], w = OL_BW[f];
        int d = mx < 0 ? 0 : mx < x + 15 ? -1 : mx >= x + w - 15 ? 1 : 0;
        if (f == 1) { if (d) ol_cat_set(ol_cat() + d); else ol_cat_popup(); }
        else { if (d) ol_sort_set(lb_sort + d); else ol_sort_popup(); }
    } else if (f == 3) ol_account();
    else if (f == 9) lb_load(false);
    else if (f >= 10 && f - 10 < (int)lb_mods.size()) { lb_open_detail(lb_mods[f - 10]); ol_df = 0; ol_ascroll = 0; printf("ajustes: biblioteca: ficha %s\n", lb_mods[f - 10].id.c_str()); }
    else if (f == -1) ov_restart();
}

// dialogs that take the whole input: sign in, popups, on-screen keyboard, typing
static bool ol_modal_step(const OvIn& in) {
    ol_enter();
    ol_tick();
    if (ov_net) return false;
    if (ol_eat) { ol_eat = false; return true; }
    uint32_t pr = in.pr;
    if (lb_overlay == 1) {
        int nb = lb_login_st == 2 || lb_login_st == 4 ? 2 : 1, hov = -1;
        for (int k = 0; k < nb; k++) { float bx = nb == 2 ? (k ? 246.0f : 128.0f) : 187.0f; if (in.mx >= bx && in.mx < bx + 106 && in.my >= 168 && in.my < 183) hov = k; }
        if (in.moved && hov >= 0) ol_lsel = hov;
        if (pr & (PB_LEFT | PB_RIGHT)) ol_lsel = nb == 2 ? !ol_lsel : 0;
        int b = (pr & PB_CRO) ? ol_lsel : in.click && hov >= 0 ? hov : -1;
        if ((pr & PB_CIR) || in.rclick || (b >= 0 && (nb == 1 || b == 1))) { lb_act(LA_CANCEL, 0); return true; }
        if (b == 0 && lb_login_st == 2) { lb_act(LA_COPYOPEN, 0); ov_notice(ov_s("Code copied, paste it on GitHub", "Código copiado, pégalo en GitHub")); }
        else if (b == 0 && lb_login_st == 4) lb_login_start();
        return true;
    }
    if (ol_pop) {
        float x, y, w, h; ol_pop_rect(&x, &y, &w, &h);
        if (ol_pop == 5) {
            if (in.wheel) ol_fscroll -= in.wheel * 24.0f;
            if (pr & PB_UP) ol_fscroll -= 12; if (pr & PB_DOWN) ol_fscroll += 12;
            if ((pr & (PB_CRO | PB_CIR)) || in.rclick || (in.click && !(in.mx >= x && in.mx < x + w && in.my >= y && in.my < y + h))) ol_pop = 0;
            return true;
        }
        int n = (int)ol_pitems.size(), rows = (n + ol_pcols - 1) / ol_pcols, hov = ol_pop_at(in.mx, in.my);
        if (in.moved && hov >= 0) ol_psel = hov;
        if (pr & PB_UP) ol_psel = ol_psel % rows ? ol_psel - 1 : std::min(n - 1, ol_psel + rows - 1);
        if (pr & PB_DOWN) ol_psel = (ol_psel % rows == rows - 1 || ol_psel + 1 >= n) ? ol_psel - ol_psel % rows : ol_psel + 1;
        if ((pr & (PB_LEFT | PB_RIGHT)) && ol_pcols > 1) { int t = ol_psel + ((pr & PB_RIGHT) ? rows : -rows); if (t >= 0 && t < n) ol_psel = t; }
        if ((pr & PB_CRO) || (in.click && hov >= 0)) { auto f = ol_pitems[(in.click && hov >= 0) ? hov : ol_psel].second; ol_pop = 0; f(); }
        else if ((pr & PB_CIR) || in.rclick || in.click) ol_pop = 0;
        return true;
    }
    if (ol_osk >= 0) {
        int hov = -1;
        for (int i = 0; i < 43; i++) { float x, y, w; ol_key_rect(i, &x, &y, &w); if (in.mx >= x && in.mx < x + w && in.my >= y && in.my < y + 14) hov = i; }
        if (in.moved && hov >= 0) ol_osk = hov;
        int k = ol_osk;
        if (pr & PB_LEFT) k = k < 40 ? (k % 10 ? k - 1 : k + 9) : (k > 40 ? k - 1 : 42);
        if (pr & PB_RIGHT) k = k < 40 ? (k % 10 < 9 ? k + 1 : k - 9) : (k < 42 ? k + 1 : 40);
        if (pr & PB_UP) k = k < 10 ? 40 + std::min(2, k * 3 / 10) : k < 40 ? k - 10 : 30 + (k - 40) * 4;
        if (pr & PB_DOWN) k = k < 30 ? k + 10 : k < 40 ? 40 + std::min(2, (k - 30) * 3 / 10) : (k - 40) * 4;
        ol_osk = k;
        if ((pr & PB_CRO) || (in.click && hov >= 0)) ol_key_press(in.click && hov >= 0 ? hov : ol_osk);
        else if (pr & PB_SQU) ol_char(8);
        else if (pr & PB_TRI) ol_key_press(40);
        else if ((pr & PB_CIR) || in.rclick || (in.click && hov < 0)) { ol_osk = -1; ol_type_end(); }
        return true;
    }
    if (ol_typing) {
        ov_kprev = ov_keys();
        float rx = ol_tgt ? OL_FR[ol_tgt][0] : OL_BX[0], ry = ol_tgt ? OL_FR[ol_tgt][1] : OL_TY, rw = ol_tgt ? OL_FR[ol_tgt][2] : OL_BW[0], rh = ol_tgt ? OL_FR[ol_tgt][3] : OL_TH;
        if (in.rclick || (in.click && !(in.mx >= rx && in.mx < rx + rw && in.my >= ry && in.my < ry + rh))) ol_type_end();
        return true;
    }
    return false;
}

static void ol_detail_step(const OvIn& in) {
    std::vector<OlBtn> b = ol_btns();
    int nb = (int)b.size(), ng = (int)lb_gallery().size();
    uint32_t pr = in.pr;
    int hov = -2;
    for (int k = 0; k < nb; k++) if (in.mx >= b[k].x && in.mx < b[k].x + b[k].w && in.my >= b[k].y && in.my < b[k].y + b[k].h) hov = k;
    ol_hov = hov;
    if (in.moved && hov >= 0) ol_df = hov;
    if (ol_df < 0 || ol_df >= nb) ol_df = 0;
    if (ng > 1 && (pr & PB_SQU)) lb_shot = (lb_shot + ng - 1) % ng;
    if (ng > 1 && (pr & PB_TRI)) lb_shot = (lb_shot + 1) % ng;
    if (pr & PB_LEFT) ol_df = ol_nav(b, ol_df, -1, 0);
    if (pr & PB_RIGHT) ol_df = ol_nav(b, ol_df, 1, 0);
    if (pr & PB_UP) ol_df = ol_nav(b, ol_df, 0, -1);
    if (pr & PB_DOWN) ol_df = ol_nav(b, ol_df, 0, 1);
    if (pr & (PB_UP | PB_DOWN | PB_LEFT | PB_RIGHT)) ov_mouse = false;
    bool rest = b[ol_df].kind == OL_KREST;
    ov_row = rest ? 0 : -5; ov_hover = hov >= 0 && b[hov].kind == OL_KREST ? 0 : -2;
    if ((pr & PB_CIR) || in.rclick) { lb_view = 0; lb_dgen++; return; }
    if (pr & PB_CRO) { if (b[ol_df].on && b[ol_df].fn) b[ol_df].fn(); return; }
    if (!in.click) return;
    if (hov >= 0) { if (b[hov].on && b[hov].fn) b[hov].fn(); return; }
    if (in.mx >= OL_X && in.mx < OL_X + OL_IW && in.my >= OL_TY && in.my < OL_TY + OL_IH && ng > 1) { lb_shot = (lb_shot + (in.mx < OL_X + OL_IW / 2 ? ng - 1 : 1)) % ng; return; }
    for (int j = 0; j < std::min(ng, 5); j++) { float tx = OL_X + j * 34.0f, ty = OL_TY + OL_IH + 3.0f; if (in.mx >= tx && in.mx < tx + 30 && in.my >= ty && in.my < ty + 17) lb_shot = j; }
}

// publish: the mod picker (lb_overlay 3) and the form (lb_overlay 4), the same steps as the mods window
static int ol_pk = 0;
static float ol_pks = 0;
static const int OL_KCOVER = 10, OL_KSHOT = 11, OL_KFIELD = 12, OL_KTAG = 13, OL_KBTN = 14, OL_KRANK = 15;
static void ol_pub_start() {
    if (!lb_me.in) { lb_login_start(); ol_lsel = 0; return; }
    lb_act(LA_PUBLISH, 0);
    ol_pk = 0; ol_pks = 0;
}
static void ol_publish(const std::string& folder) {
    ov_sub_set(1);
    ol_enter();
    ol_ff = 0;
    lb_publish_from(folder);
    if (!lb_me.in) ol_lsel = 0;
}
static std::vector<OlBtn> ol_form_btns() {
    std::vector<OlBtn> a;
    const LbPub& p = lb_pub;
    bool edit = p.mode == 2, busy = p.st == 1;
    auto add = [&](float x, float y, float w, float h, int kind, int arg, std::function<void()> fn, std::function<void()> alt = nullptr) {
        OlBtn b; b.x = x; b.y = y; b.w = w; b.h = h; b.kind = kind; b.arg = arg; b.fn = fn; b.alt = alt; b.on = !busy; a.push_back(b);
    };
    if (!edit) {
        add((float)OL_X, 83, 150, 84, OL_KCOVER, 0, [] { lb_act(LA_COVER, 0); }, [] { lb_act(LA_COVER_DEL, 0); });
        int ns = std::min(4, (int)p.shots.size() + 1);
        for (int k = 0; k < ns; k++) add(OL_X + k * 38.0f, 171, 34, 19, OL_KSHOT, k, [k] { if (k >= (int)lb_pub.shots.size()) lb_act(LA_SHOT_ADD, 0); }, [k] { lb_act(LA_SHOT_DEL, k); });
    } else add((float)OL_X, 194, 150, 11, OL_KRANK, 0, [] { lb_act(LA_PRANKED, 0); });
    for (int f = 1; f <= 4; f++) {
        if (edit && (f == 2 || f == 3)) continue;
        add(OL_FR[f][0], OL_FR[f][1], OL_FR[f][2], OL_FR[f][3], OL_KFIELD, f, [f] { ol_tgt = f; if (ov_pad()) ol_osk = 0; else ol_typing = true; });
    }
    float x = OL_FX, y = OL_FTY;
    for (int t = 0; t < 12; t++) {
        if (t == 11 && !lb_me.admin) continue;
        float w = ceilf(psp_ui_text_w(0, 0.36f, ol_tagname(t))) + 7;
        if (x + w > OL_FX + OL_FW) { x = OL_FX; y += 11; }
        add(x, y, w, 9, OL_KTAG, t, [t] { lb_act(LA_PTAG, t); });
        x += w + 3;
    }
    add(OL_FX + OL_FW - 160, 225, 78, 13, OL_KBTN, 0, [] { lb_act(LA_CANCEL, 0); });
    add(OL_FX + OL_FW - 78, 225, 78, 13, OL_KBTN, 1, [] { lb_pub_go(); });
    a.back().on = !busy && !p.blocked && !p.checking;
    OlBtn r; r.x = OV_BX; r.y = OV_BY; r.w = OV_BW; r.h = OV_BH; r.kind = OL_KREST; r.fn = [] { ov_restart(); };
    a.push_back(r);
    return a;
}
static void ol_form_step(const OvIn& in) {
    std::vector<OlBtn> b = ol_form_btns();
    int nb = (int)b.size(), hov = -2;
    uint32_t pr = in.pr;
    for (int k = 0; k < nb; k++) if (in.mx >= b[k].x && in.mx < b[k].x + b[k].w && in.my >= b[k].y && in.my < b[k].y + b[k].h) hov = k;
    ol_hov = hov;
    if (in.moved && hov >= 0) ol_ff = hov;
    if (ol_ff < 0 || ol_ff >= nb) ol_ff = 0;
    ov_row = b[ol_ff].kind == OL_KREST ? 0 : -5; ov_hover = hov >= 0 && b[hov].kind == OL_KREST ? 0 : -2;
    if (lb_pub.st == 1) return;
    if (pr & PB_LEFT) ol_ff = ol_nav(b, ol_ff, -1, 0);
    if (pr & PB_RIGHT) ol_ff = ol_nav(b, ol_ff, 1, 0);
    if (pr & PB_UP) ol_ff = ol_nav(b, ol_ff, 0, -1);
    if (pr & PB_DOWN) ol_ff = ol_nav(b, ol_ff, 0, 1);
    if (pr & (PB_UP | PB_DOWN | PB_LEFT | PB_RIGHT)) ov_mouse = false;
    if ((pr & PB_CIR) || in.rclick) { lb_act(LA_CANCEL, 0); return; }
    if ((pr & PB_SQU) && b[ol_ff].alt) { b[ol_ff].alt(); return; }
    if (pr & PB_CRO) { if (b[ol_ff].on && b[ol_ff].fn) b[ol_ff].fn(); return; }
    if (in.click && hov >= 0 && b[hov].on && b[hov].fn) b[hov].fn();
}
static void ol_pick_step(const OvIn& in) {
    int n = (int)mw_mods.size() + 1;
    uint32_t pr = in.pr;
    float top = 94, area = OL_GB - top;
    int hov = -2;
    if (ol_restart_at(in.mx, in.my)) hov = -1;
    else if (in.mx >= OL_X && in.mx < OL_X + OL_GW && in.my >= top && in.my < OL_GB) { int k = (int)((in.my - top + ol_pks) / 13); if (k < n) hov = k; }
    ol_hov = hov;
    if (in.moved && hov != -2) ol_pk = hov;
    if (in.wheel) ol_pks -= in.wheel * 26.0f;
    if (pr & PB_UP) ol_pk = ol_pk == -1 ? n - 1 : std::max(0, ol_pk - 1);
    if (pr & PB_DOWN) ol_pk = ol_pk == -1 ? -1 : ol_pk + 1 >= n ? -1 : ol_pk + 1;
    if (pr & (PB_UP | PB_DOWN)) ov_mouse = false;
    if (!ov_mouse && ol_pk >= 0) { float y = ol_pk * 13.0f; if (y < ol_pks) ol_pks = y; if (y + 13 > ol_pks + area) ol_pks = y + 13 - area; }
    ol_pks = std::max(0.0f, std::min(ol_pks, std::max(0.0f, n * 13.0f - area)));
    ov_row = ol_pk == -1 ? 0 : -5; ov_hover = hov == -1 ? 0 : -2;
    if ((pr & PB_CIR) || in.rclick) { lb_act(LA_CANCEL, 0); return; }
    int k = (pr & PB_CRO) ? ol_pk : in.click ? hov : -2;
    if (k == -1) ov_restart();
    else if (k == 0) { ol_ff = 0; lb_act(LA_PICK_FILE, 0); }
    else if (k > 0) { ol_ff = 0; printf("ajustes: biblioteca: publicar %s\n", mw_mods[k - 1].id.c_str()); lb_act(LA_PICK, k - 1); }
}

static void ol_step(const OvIn& in) {
    if (ov_net) { ov_row = 0; if ((in.pr & PB_CIR) || in.rclick) ov_close(); return; }
    if (lb_overlay == 3) { ol_pick_step(in); return; }
    if (lb_overlay == 4) { ol_form_step(in); return; }
    if (lb_view == 1) { ol_detail_step(in); return; }
    int n = (int)lb_mods.size();
    uint32_t pr = in.pr;
    if (n && ol_f >= 10 && ol_f - 10 >= n) ol_f = 10 + n - 1;
    if (!n && lb_err && !lb_loading && ol_f >= 10) ol_f = 9;
    if (ol_f == 9 && !(n == 0 && lb_err)) ol_f = 10;
    int h = ol_grid_hit(in.mx, in.my);
    ol_hov = h;
    if (in.moved && h != -2) ol_f = h;
    float content = (float)((n + OL_NC - 1) / OL_NC * OL_CS - (OL_CS - OL_CH)), area = (float)(OL_GB - OL_GY);
    if (in.wheel) ol_scroll -= in.wheel * OL_CS * 0.5f;
    int down_to = n ? -3 : lb_err ? 9 : -1;
    if (pr & PB_LEFT) { if (ol_f >= 1 && ol_f <= 3) ol_f--; else if (ol_f >= 10 && (ol_f - 10) % OL_NC) ol_f--; }
    if (pr & PB_RIGHT) { if (ol_f >= 0 && ol_f <= 2) ol_f++; else if (ol_f >= 10 && (ol_f - 10) % OL_NC < OL_NC - 1 && ol_f - 9 < n) ol_f++; }
    if (pr & PB_UP) {
        if (ol_f == -1) ol_f = n ? 10 + n - 1 : lb_err ? 9 : 3;
        else if (ol_f == 9) ol_f = 0;
        else if (ol_f >= 10 + OL_NC) ol_f -= OL_NC;
        else if (ol_f >= 10) { static const int UP[5] = {0, 1, 1, 2, 3}; ol_f = UP[(ol_f - 10) % OL_NC]; }
    } else if (pr & PB_DOWN) {
        if (ol_f >= 0 && ol_f <= 3) { static const int DN[4] = {0, 1, 3, 4}; ol_f = down_to == -3 ? 10 + std::min(DN[ol_f], n - 1) : down_to; }
        else if (ol_f == 9) ol_f = -1;
        else if (ol_f >= 10) { int i = ol_f - 10; ol_f = i + OL_NC < n ? ol_f + OL_NC : i / OL_NC < (n - 1) / OL_NC ? 10 + n - 1 : -1; }
    }
    if (pr & (PB_UP | PB_DOWN | PB_LEFT | PB_RIGHT)) ov_mouse = false;
    if (ol_f >= 10 && ol_f - 10 >= n - OL_NC && n < lb_total) lb_load(true);
    if (in.wheel && ol_scroll > content - area - OL_CS && n < lb_total) lb_load(true);
    if (!ov_mouse && ol_f >= 10) { float y = (float)((ol_f - 10) / OL_NC * OL_CS); if (y < ol_scroll) ol_scroll = y; if (y + OL_CH > ol_scroll + area) ol_scroll = y + OL_CH - area; }
    if (!ov_mouse && ol_f >= 0 && ol_f <= 3) ol_scroll = 0;
    ol_scroll = std::max(0.0f, std::min(ol_scroll, std::max(0.0f, content - area)));
    ov_row = ol_f == -1 ? 0 : -5; ov_hover = h == -1 ? 0 : -2;
    if ((pr & PB_SQU) && ov_pad()) { ol_f = 0; ol_tgt = 0; ol_osk = 0; return; }
    if ((pr & PB_CIR) || in.rclick) { ov_close(); return; }
    if (pr & PB_CRO) { ol_activate(ol_f, -1); return; }
    if (in.click && h != -2) { ol_f = h; ol_activate(h, in.mx); }
}

// ---- drawing
static const uint32_t OL_TAGC[12] = {0x2F7FD0, 0x8A4FD0, 0xD06A2F, 0xB08A1E, 0x2E9E5A, 0x2BA3A8, 0xCE4248, 0x5C6FD0, 0xB8508F, 0x5A8F3A, 0x6A7280, 0x9A6A2A};
static float ol_pill(float x, float y, const std::string& t, uint32_t col, float size, float h, float maxx) {
    float w = ceilf(psp_ui_text_w(0, size, t)) + 5;
    if (x + w > maxx) return 0;
    ov_quad(ov_white, x, y, w, h, col, 0.92f);
    ov_txt(0, size, t, x, y, (int)w, (int)h, 1, 0xFFFFFF);
    return w;
}
static std::string ol_tagname(int t) { return ov_s(OL_CAT[t + 2].en, OL_CAT[t + 2].es); }
static void ol_tags(const LbMod& m, float x, float y, float maxx, float size, float h, bool ranked) {
    std::vector<std::pair<std::string, uint32_t>> p;
    if (ranked && m.ranked) p.push_back({ov_s("Ranked OK", "Clasificatorio"), 0x2E9E5A});
    for (int t : m.tags) if (t >= 0 && t < 12) p.push_back({ol_tagname(t), OL_TAGC[t]});
    for (size_t k = 0; k < p.size(); k++) {
        std::string more = "+" + std::to_string(p.size() - k);
        float rest = k + 1 < p.size() ? ceilf(psp_ui_text_w(0, size, more)) + 7 : 0;
        float w = ol_pill(x, y, p[k].first, p[k].second, size, h, maxx - rest);
        if (!w) { ol_pill(x, y, more, 0x3D6A9A, size, h, maxx); return; }
        x += w + 2;
    }
}
static void ol_value(int i, const std::string& t, bool on) {
    float x = OL_BX[i], w = OL_BW[i];
    ov_spr(on ? OP_ROW_ON : OP_ROW_OFF, x, (float)OL_TY, (int)w, OL_TH);
    if (on) ov_spr(OP_CURSOR, x, OL_TY + 1.0f, (int)w, OL_TH - 2, 0.55f + 0.25f * sinf((GetTickCount() - ov_t0) * 0.006f));
    ov_spr(OP_ARROW_L, x + 2, OL_TY + 1.5f, 9, 9);
    ov_spr(OP_ARROW_R, x + w - 11, OL_TY + 1.5f, 9, 9);
    ov_txt(0, OL_S, ol_fit(t, OL_S, w - 24), x + 11, (float)OL_TY, (int)w - 22, OL_TH, 1, on ? 0xFFF379 : 0x95F1FF);
}
static void ol_draw_top(bool foc) {
    bool on0 = foc && ol_f == 0, typing = ol_typing || ol_osk >= 0;
    float x = OL_BX[0], w = OL_BW[0];
    ol_box(x, (float)OL_TY, w, (float)OL_TH, 0x0E2547, 0.92f, on0 || typing ? 0xFFA300 : 0x3D6A9A);
    ov_quad(ol_glyph(2), x + 3, OL_TY + 2.0f, 8, 8, 0x9DB4D6, 1);
    std::string q = ol_q;
    if (q.empty() && !typing) ov_txt(0, OL_S, ov_s("Search...", "Buscar..."), x + 13, (float)OL_TY, (int)w - 15, OL_TH, 0, 0x9DB4D6);
    else {
        while (!q.empty() && psp_ui_text_w(0, OL_S, q) > w - 19) { size_t k = 1; while (k < q.size() && ((unsigned char)q[k] & 0xC0) == 0x80) k++; q.erase(0, k); }
        float tw = psp_ui_text_w(0, OL_S, q);
        ov_txt(0, OL_S, q, x + 13, (float)OL_TY, (int)ceilf(tw) + 3, OL_TH, 0, 0xFFFFFF);
        if (typing && (GetTickCount() / 500) % 2) ov_quad(ov_white, x + 13 + tw + 1, OL_TY + 2.5f, 1, OL_TH - 5.0f, 0xFFF379, 1);
    }
    ol_value(1, ov_s(OL_CAT[ol_cat()].en, OL_CAT[ol_cat()].es), foc && ol_f == 1);
    ol_value(2, ov_s(OL_SORT[lb_sort].en, OL_SORT[lb_sort].es), foc && ol_f == 2);
    bool on3 = foc && ol_f == 3;
    x = OL_BX[3]; w = OL_BW[3];
    ov_spr(on3 ? OP_ROW_ON : OP_ROW_OFF, x, (float)OL_TY, (int)w, OL_TH);
    if (on3) ov_spr(OP_CURSOR, x, OL_TY + 1.0f, (int)w, OL_TH - 2, 0.55f + 0.25f * sinf((GetTickCount() - ov_t0) * 0.006f));
    if (lb_me.in) {
        const OvTex* av = ol_img(lb_me.av, 2);
        if (av) ov_quad(av->t, x + 4, OL_TY + 1.5f, 9, 9, 0xFFFFFF, 1); else ov_quad(ol_glyph(4), x + 4, OL_TY + 1.5f, 9, 9, 0x95F1FF, 1);
        ov_txt(0, 0.42f, ol_fit(lb_me.login, 0.42f, w - 19), x + 15, (float)OL_TY, (int)w - 17, OL_TH, 0, on3 ? 0xFFF379 : 0xFFFFFF);
    } else ov_txt(0, OL_S, lb_me_checking ? "..." : ov_s("Sign in", "Entrar"), x, (float)OL_TY, (int)w, OL_TH, 1, on3 ? 0xFFF379 : 0xFFFFFF);
}
static void ol_draw_card(int i, float x, float y, bool on, float pulse) {
    const LbMod& m = lb_mods[i];
    ol_box(x, y, (float)OL_CW, (float)OL_CH, 0x16365F, 0.9f, on ? 0xFFA300 : 0x3D6A9A);
    if (const OvTex* t = ol_img(m.cover, 0)) ol_fitimg(t, x + 1, y + 1, OL_CW - 2.0f, OL_COV - 1.0f);
    else if (m.cover.empty()) ov_txt(0, 0.38f, ov_s("No image", "Sin imagen"), x, y, OL_CW, OL_COV, 1, 0x9DB4D6);
    ov_quad(ov_white, x + 1, y + OL_COV, OL_CW - 2.0f, 1, on ? 0xFFA300 : 0x3D6A9A, 1);
    if (m.ranked) { ov_quad(ov_white, x + 2, y + 2, 8, 8, 0x2E9E5A, 0.95f); ov_txt(0, 0.38f, "C", x + 2, y + 2, 8, 8, 1, 0xFFFFFF); }
    if (m.hidden) ol_pill(x + 2, y + OL_COV - 9, m.hidden == 3 ? ov_s("BROKEN LINK", "ENLACE ROTO") : m.hidden == 1 ? ov_s("REPORTED", "REPORTADO") : ov_s("HIDDEN", "OCULTO"), 0xCE4248, 0.32f, 7, x + OL_CW - 2);
    if (on) ov_spr(OP_CURSOR, x + 1, y + OL_COV + 1.0f, OL_CW - 2, 9, pulse * 0.6f);
    ov_txt(0, 0.4f, ol_fit(m.name, 0.4f, OL_CW - 6.0f), x + 3, y + OL_COV + 1.0f, OL_CW - 4, 9, 0, on ? 0xFFF379 : 0xFFFFFF);
    ol_tags(m, x + 3, y + OL_COV + 11.0f, x + OL_CW - 3, 0.3f, 7, false);
    float sy = y + OL_COV + 19, sx = x + 3;
    int ins = lb_installed(m);
    if (ins) {
        ov_quad(ol_glyph(ins == 2 ? 3 : 1), sx, sy + 1.5f, 6, 6, ins == 2 ? 0x4CD486 : 0xFFA300, 1);
        ov_txt(0, 0.36f, ins == 2 ? ov_s("Installed", "Instalado") : ov_s("Update", "Actualizar"), sx + 8, sy, 50, 9, 0, ins == 2 ? 0x4CD486 : 0xFFA300);
        return;
    }
    std::string a = ol_count(m.dl), b = ol_count(m.likes);
    float aw = ceilf(psp_ui_text_w(0, 0.36f, a)) + 2, bw = ceilf(psp_ui_text_w(0, 0.36f, b)) + 2;
    ov_quad(ol_glyph(1), sx, sy + 1.5f, 6, 6, 0x95F1FF, 1);
    ov_txt(0, 0.36f, a, sx + 8, sy, (int)aw + 2, 9, 0, 0x95F1FF);
    sx += 8 + aw + 5;
    ov_quad(ol_glyph(0), sx, sy + 1.5f, 6, 6, m.liked ? 0xEC5464 : 0x95F1FF, 1);
    ov_txt(0, 0.36f, b, sx + 8, sy, (int)bw + 2, 9, 0, 0x95F1FF);
}
static void ol_draw_grid(bool foc, float pulse) {
    int n = (int)lb_mods.size();
    std::string info;
    if (n || lb_total) info = std::to_string(lb_total) + (lb_total == 1 ? " mod" : " mods");
    static const OlName COL[4] = {{"", ""}, {"Liked", "Me gustan"}, {"My mods", "Mis mods"}, {"Review", "Revisión"}};
    if (lb_col) info += (info.empty() ? "" : " · ") + std::string(ov_s(COL[lb_col].en, COL[lb_col].es));
    if (!lb_text.empty()) info += (info.empty() ? "" : " · ") + ("\"" + lb_text + "\"");
    if (lb_loading && n) info += (info.empty() ? "" : " · ") + std::string(ov_s("Loading...", "Cargando..."));
    std::string fm;
    if (ol_f >= 10 && ol_f - 10 < n) {
        const LbMod& m = lb_mods[ol_f - 10];
        fm = m.author_name.empty() ? m.author : m.author_name;
        if (!m.ver.empty()) fm += " · v" + m.ver;
        if (m.size) fm += " · " + ol_size(m.size);
    }
    float fw = fm.empty() ? 0 : std::min((float)OL_GW * 0.55f, ceilf(psp_ui_text_w(0, 0.4f, fm)) + 2);
    if (!info.empty()) ov_txt(0, 0.4f, ol_fit(info, 0.4f, OL_GW - fw - 10.0f), OL_X + 2.0f, (float)OL_IY, (int)(OL_GW - fw - 6), 9, 0, 0x9DB4D6);
    if (fw) ov_txt(0, 0.4f, ol_fit(fm, 0.4f, fw), OL_X + OL_GW - fw, (float)OL_IY, (int)fw, 9, 2, 0x95F1FF);
    if (!n) {
        const char* t = lb_loading ? ov_s("Loading the mod library...", "Cargando la biblioteca de mods...") : lb_err ? ov_s("Can't reach the mod library.", "No se pudo conectar con la biblioteca de mods.") : ov_s("No mods found.", "No se encontraron mods.");
        ov_txt(0, 0.5f, t, OL_X, OL_GY + 20.0f, OL_GW, 12, 1, 0xD8E4F4);
        if (lb_err && !lb_loading) {
            bool on = foc && ol_f == 9;
            ov_spr(OP_BUTTON, 190, OL_GY + 40.0f, 100, 15, on ? 1.0f : 0.5f);
            ov_txt(0, 0.5f, ov_s("Retry", "Reintentar"), 190, OL_GY + 40.0f, 100, 15, 1, on ? 0xEF3300 : 0xFFFFFF);
        }
        return;
    }
    float content = (float)((n + OL_NC - 1) / OL_NC * OL_CS - (OL_CS - OL_CH)), area = (float)(OL_GB - OL_GY);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)(ov_fx + OL_X * ov_fs), (int)(ov_ch - (ov_fy + OL_GB * ov_fs)), (int)((OL_GW + 1) * ov_fs + 1), (int)(area * ov_fs + 1));
    for (int i = 0; i < n; i++) {
        float x = OL_X + (i % OL_NC) * (float)(OL_CW + OL_CG), y = OL_GY + (i / OL_NC) * (float)OL_CS - ol_scroll;
        if (y + OL_CH < OL_GY || y > OL_GB) continue;
        ol_draw_card(i, x, y, foc && ol_f == 10 + i, pulse);
    }
    glDisable(GL_SCISSOR_TEST);
    if (content > area) {
        float sx = OL_X + OL_GW + 4.0f, kh = 24;
        ov_spr(OP_SB_TRACK, sx + 1, (float)OL_GY, 4, (int)area, 0.8f);
        ov_spr(OP_SB_KNOB, sx - 1, OL_GY + (area - kh) * (ol_scroll / (content - area)), 8, (int)kh);
    }
}
static void ol_draw_detail(bool foc, float pulse) {
    const LbMod& m = lb_cur;
    std::vector<std::string> g = lb_gallery();
    int ng = (int)g.size();
    if (lb_shot >= ng) lb_shot = 0;
    std::vector<OlBtn> b = ol_btns();
    auto focused = [&](int k) { return foc && (ov_mouse ? ol_hov == k : ol_df == k); };
    ol_box((float)OL_X, (float)OL_TY, (float)OL_IW, (float)OL_IH, 0x16365F, 0.9f, 0x3D6A9A);
    if (ng) { if (const OvTex* t = ol_img(g[lb_shot], 1)) ol_fitimg(t, OL_X + 1.0f, OL_TY + 1.0f, OL_IW - 2.0f, OL_IH - 2.0f); }
    else ov_txt(0, 0.45f, ov_s("No image", "Sin imagen"), (float)OL_X, (float)OL_TY, OL_IW, OL_IH, 1, 0x9DB4D6);
    if (ng > 1) {
        ov_spr(OP_ARROW_L, OL_X + 3.0f, OL_TY + OL_IH * 0.5f - 5, 10, 10);
        ov_spr(OP_ARROW_R, OL_X + OL_IW - 13.0f, OL_TY + OL_IH * 0.5f - 5, 10, 10);
        for (int j = 0; j < std::min(ng, 5); j++) {
            float tx = OL_X + j * 34.0f, ty = OL_TY + OL_IH + 3.0f;
            ol_box(tx, ty, 30, 17, 0x16365F, 0.9f, j == lb_shot ? 0xFFA300 : 0x3D6A9A);
            if (const OvTex* t = ol_img(g[j], 0)) ol_fitimg(t, tx + 1, ty + 1, 28, 15, j == lb_shot ? 1.0f : 0.7f);
        }
    }
    int dk = -1;
    for (int k = 0; k < (int)b.size(); k++) if (b[k].kind == OL_KDESC) dk = k;
    float dy = (float)OL_DY0;
    ol_box((float)OL_X, dy - 2, (float)OL_IW, OL_GB - dy + 2, 0x0E2547, 0.6f, dk >= 0 && focused(dk) ? 0xFFA300 : 0x3D6A9A);
    std::vector<std::string> p = ol_paragraphs(ol_desc_text(), 0.4f, OL_IW - 8.0f);
    for (int i = 0; i < (int)p.size() && i < OL_DL; i++) {
        std::string l = p[i];
        if (i == OL_DL - 1 && (int)p.size() > OL_DL) l = ol_fit(l + "...", 0.4f, OL_IW - 8.0f);
        ov_txt(0, 0.4f, l, OL_X + 4.0f, dy + i * 8.5f, OL_IW - 6, 9, 0, 0xD8E4F4);
    }
    float x = (float)OL_RX, w = (float)OL_RW, y = (float)OL_TY;
    ov_txt(0, 0.55f, ol_fit(m.name, 0.55f, w - 2), x, y, (int)w, 12, 0, 0xFFF379);
    y += 13;
    std::string meta = m.ver.empty() ? "" : "v" + m.ver;
    std::string au = m.author_name.empty() ? m.author : m.author_name;
    if (!au.empty()) meta += (meta.empty() ? "" : " · ") + au;
    float mx0 = x;
    if (const OvTex* av = ol_img(m.author_av, 2)) { ov_quad(av->t, x, y + 0.5f, 8, 8, 0xFFFFFF, 1); mx0 += 10; }
    ov_txt(0, 0.42f, ol_fit(meta, 0.42f, w - (mx0 - x)), mx0, y, (int)(w - (mx0 - x)), 9, 0, 0x95F1FF);
    y += 10;
    std::string a = ol_count(m.dl), lk = ol_count(m.likes), sz = m.size ? ol_size(m.size) : "";
    float sx = x;
    ov_quad(ol_glyph(1), sx, y + 1.5f, 6, 6, 0x95F1FF, 1);
    float aw = ceilf(psp_ui_text_w(0, 0.42f, a)) + 2; ov_txt(0, 0.42f, a, sx + 8, y, (int)aw + 2, 9, 0, 0x95F1FF); sx += 8 + aw + 6;
    ov_quad(ol_glyph(0), sx, y + 1.5f, 6, 6, m.liked ? 0xEC5464 : 0x95F1FF, 1);
    float bw = ceilf(psp_ui_text_w(0, 0.42f, lk)) + 2; ov_txt(0, 0.42f, lk, sx + 8, y, (int)bw + 2, 9, 0, 0x95F1FF); sx += 8 + bw + 6;
    if (!sz.empty()) ov_txt(0, 0.42f, sz, sx, y, (int)(x + w - sx), 9, 0, 0x9DB4D6);
    y += 10;
    ol_tags(m, x, y, x + w, 0.36f, 8, true);
    bool sect = false;
    for (int k = 0; k < (int)b.size(); k++) {
        const OlBtn& t = b[k];
        bool on = focused(k);
        float al = t.on ? 1.0f : 0.55f;
        if (t.kind == OL_KMAIN) {
            ov_spr(on ? OP_ROW_ON : OP_ROW_OFF, t.x, t.y, (int)t.w, (int)t.h, al);
            if (t.prog >= 0) ov_quad(ov_white, t.x + 3, t.y + 3, (t.w - 6) * t.prog / 100.0f, t.h - 6, 0xFFA300, 0.35f);
            if (on) ov_spr(OP_CURSOR, t.x, t.y + 1, (int)t.w, (int)t.h - 2, pulse * 0.6f);
            ov_quad(ol_glyph(t.glyph), t.x + 9, t.y + 4, 8, 8, t.ic, al);
            float tw = t.tag.empty() ? 0 : ceilf(psp_ui_text_w(0, 0.42f, t.tag)) + 4;
            ov_txt(0, 0.5f, ol_fit(t.t, 0.5f, t.w - 30 - tw), t.x + 21, t.y, (int)(t.w - 26 - tw), (int)t.h, 0, on ? 0xFFF379 : t.glyph == 3 ? 0x4CD486 : 0xFFFFFF, al);
            if (tw) ov_txt(0, 0.42f, t.tag, t.x + t.w - 6 - tw, t.y, (int)tw, (int)t.h, 2, 0x95F1FF, al);
        } else if (t.kind == OL_KSMALL || t.kind == OL_KADMIN) {
            bool ad = t.kind == OL_KADMIN;
            if (ad && !sect) {
                sect = true;
                const char* h = lb_me.admin ? "ADMIN" : ov_s("YOUR MOD", "TU MOD");
                float hy = ol_sect_y(), hw = ceilf(psp_ui_text_w(0, 0.36f, h)) + 3;
                ov_txt(0, 0.36f, h, x, hy, (int)hw + 2, 8, 0, 0xFFA300);
                ov_quad(ov_white, x + hw + 3, hy + 4, w - hw - 3, 1, 0xFFA300, 0.4f);
            }
            ol_box(t.x, t.y, t.w, t.h, on ? (ad ? 0x5A3420 : 0x24508A) : (ad ? 0x3A2418 : 0x16365F), 0.9f, on ? 0xFFA300 : ad ? 0x9A5A30 : 0x3D6A9A);
            float tw = ceilf(psp_ui_text_w(0, 0.38f, t.t)), cw = 7 + 3 + tw, cx = t.x + (t.w - std::min(cw, t.w - 4)) * 0.5f;
            ov_quad(ol_glyph(t.glyph), cx, t.y + 3.5f, 7, 7, t.ic, al);
            ov_txt(0, 0.38f, ol_fit(t.t, 0.38f, t.w - 14), cx + 10, t.y, (int)(t.w - (cx + 10 - t.x) - 1), (int)t.h, 0, on ? 0xFFF379 : 0xFFFFFF, al);
        } else if (t.kind == OL_KLINK) {
            ov_quad(ol_glyph(t.glyph), t.x, t.y + 1.5f, 7, 7, on ? 0xFFF379 : 0x95F1FF, 1);
            float tw = std::min(t.w - 12, ceilf(psp_ui_text_w(0, 0.42f, t.t)) + 2);
            ov_txt(0, 0.42f, ol_fit(t.t, 0.42f, tw), t.x + 10, t.y, (int)tw + 2, (int)t.h, 0, on ? 0xFFF379 : 0x95F1FF);
            if (on) ov_quad(ov_white, t.x + 10, t.y + t.h - 1, tw, 1, 0xFFF379, 1);
        }
    }
}

static void ol_draw_pick(bool foc, float pulse) {
    ov_txt(0, 0.5f, ov_s("Choose the mod to publish", "Elige el mod que quieres publicar"), OL_X, (float)OL_TY, OL_GW, 12, 0, 0xFFF379);
    ov_txt(0, 0.38f, ov_s("One of your installed mods, or a .mkpak from any folder.", "Uno de tus mods instalados o un .mkpak de cualquier carpeta."), OL_X, OL_TY + 12.0f, OL_GW, 9, 0, 0x9DB4D6);
    int n = (int)mw_mods.size() + 1;
    float top = 94, area = OL_GB - top, lw = OL_GW - (n * 13 > area ? 10.0f : 0);
    glEnable(GL_SCISSOR_TEST);
    glScissor((int)(ov_fx + OL_X * ov_fs), (int)(ov_ch - (ov_fy + OL_GB * ov_fs)), (int)(lw * ov_fs + 1), (int)(area * ov_fs + 1));
    for (int k = 0; k < n; k++) {
        float y = top + k * 13.0f - ol_pks;
        if (y + 13 < top || y > OL_GB) continue;
        bool on = foc && (ov_mouse ? ol_hov == k : ol_pk == k);
        ov_spr(on ? OP_ROW_ON : OP_ROW_OFF, (float)OL_X, y, (int)lw, 12);
        if (on) ov_spr(OP_CURSOR, (float)OL_X, y + 1, (int)lw, 10, pulse * 0.6f);
        if (!k) {
            ov_quad(ol_glyph(2), OL_X + 8.0f, y + 2.5f, 7, 7, 0x95F1FF, 1);
            ov_txt(0, 0.42f, ov_s("A .mkpak from another folder...", "Un .mkpak de otra carpeta..."), OL_X + 18.0f, y, (int)lw - 24, 12, 0, on ? 0xFFF379 : 0x95F1FF);
            continue;
        }
        const ModInfo& m = mw_mods[k - 1];
        float rx = OL_X + lw - 6;
        std::string v = mw_field(m, "version");
        if (!mw_field(m, "biblioteca").empty()) { const char* t = ov_s("IN LIBRARY", "EN BIBLIOTECA"); float w = ceilf(psp_ui_text_w(0, 0.32f, t)) + 5; rx -= w; ol_pill(rx, y + 2.5f, t, 0x2F7FD0, 0.32f, 7, rx + w); rx -= 5; }
        if (!v.empty()) { float w = ceilf(psp_ui_text_w(0, 0.4f, "v" + v)) + 2; rx -= w; ov_txt(0, 0.4f, "v" + v, rx, y, (int)w + 1, 12, 0, 0x9DB4D6); rx -= 6; }
        ov_txt(0, 0.42f, ol_fit(mw_name(m), 0.42f, rx - OL_X - 12), OL_X + 8.0f, y, (int)(rx - OL_X - 9), 12, 0, on ? 0xFFF379 : 0xFFFFFF);
    }
    glDisable(GL_SCISSOR_TEST);
    if (n * 13 > area) {
        float sx = OL_X + lw + 4, kh = 24;
        ov_spr(OP_SB_TRACK, sx + 1, top, 4, (int)area, 0.8f);
        ov_spr(OP_SB_KNOB, sx - 1, top + (area - kh) * (ol_pks / std::max(1.0f, n * 13.0f - area)), 8, (int)kh);
    }
}
static std::string ol_step_tr(const std::wstring& s) {
    static const OlName T[] = {{"Packing the mod...", "Empaquetando el mod..."}, {"Preparing your GitHub repository...", "Preparando tu repositorio de GitHub..."}, {"Creating the release...", "Creando el release..."},
                               {"Uploading the mod...", "Subiendo el mod..."}, {"Uploading the images...", "Subiendo las imágenes..."}, {"Publishing in the library...", "Publicando en la biblioteca..."}};
    std::string u = mw_u8(s.c_str());
    if (ov_es()) for (const OlName& t : T) if (u == t.en) return t.es;
    return u;
}
static void ol_draw_form(bool foc, float pulse) {
    const LbPub& p = lb_pub;
    bool edit = p.mode == 2, busy = p.st == 1;
    std::string title = edit ? ov_s("Edit mod (admin)", "Editar mod (admin)") : p.mode == 1 ? ov_s("Publish an update", "Publicar una actualización") : ov_s("Publish a mod", "Publicar un mod");
    ov_txt(0, 0.5f, title, OL_X, (float)OL_TY, 200, 12, 0, 0xFFF379);
    std::string st = p.checking ? ov_s("Checking the mod...", "Revisando el mod...") : p.blocked ? ov_s("Only the author can publish it", "Solo el autor puede publicarlo") : p.mode == 1 ? ov_s("Now in the library: v", "En la biblioteca: v") + p.old_ver : "";
    if (!st.empty()) ov_txt(0, 0.4f, ol_fit(st, 0.4f, 170), OL_X + OL_GW - 172.0f, OL_TY + 1.0f, 172, 10, 2, p.blocked ? 0xFF8A70 : 0x95F1FF);
    std::vector<OlBtn> b = ol_form_btns();
    for (int k = 0; k < (int)b.size(); k++) {
        const OlBtn& t = b[k];
        bool on = foc && (ov_mouse ? ol_hov == k : ol_ff == k);
        uint32_t bc = on ? 0xFFA300 : 0x3D6A9A;
        if (t.kind == OL_KCOVER || t.kind == OL_KSHOT) {
            std::string u = t.kind == OL_KCOVER ? p.cover : t.arg < (int)p.shots.size() ? p.shots[t.arg] : "";
            ol_box(t.x, t.y, t.w, t.h, 0x16365F, 0.9f, bc);
            if (const OvTex* tx = ol_img(u, t.kind == OL_KCOVER ? 1 : 0)) ol_fitimg(tx, t.x + 1, t.y + 1, t.w - 2, t.h - 2);
            else if (u.empty()) ov_txt(0, t.kind == OL_KCOVER ? 0.45f : 0.6f, t.kind == OL_KCOVER ? ov_s("+ Cover", "+ Portada") : "+", t.x, t.y, (int)t.w, (int)t.h, 1, on ? 0xFFF379 : 0x9DB4D6);
            if (t.kind == OL_KCOVER && !u.empty()) { const char* l = ov_s("COVER", "PORTADA"); ol_pill(t.x + 2, t.y + 2, l, 0x16365F, 0.3f, 7, t.x + t.w); }
        } else if (t.kind == OL_KRANK) {
            std::string l = ov_s("Ranked OK: ", "Clasificatorio: ") + std::string(p.ranked ? ov_s("yes", "sí") : ov_s("no", "no"));
            ol_box(t.x, t.y, t.w, t.h, p.ranked ? 0x1E5A38 : 0x16365F, 0.9f, bc);
            ov_txt(0, 0.38f, l, t.x, t.y, (int)t.w, (int)t.h, 1, on ? 0xFFF379 : 0xFFFFFF);
        } else if (t.kind == OL_KFIELD) {
            static const OlName L[5] = {{"", ""}, {"Name", "Nombre"}, {"Version", "Versión"}, {"Link (optional)", "Enlace (opcional)"}, {"Description", "Descripción"}};
            ov_txt(0, 0.36f, ov_s(L[t.arg].en, L[t.arg].es), t.x + 1, t.y - 8, (int)t.w, 8, 0, 0x9DB4D6);
            bool ty = (ol_typing || ol_osk >= 0) && ol_tgt == t.arg;
            ol_box(t.x, t.y, t.w, t.h, 0x0E2547, 0.92f, ty ? 0xFFA300 : bc);
            const std::string& v = lb_ftxt[t.arg - 1];
            if (t.arg == 4) {
                std::vector<std::string> ls = ol_paragraphs(v, 0.4f, t.w - 8);
                int nl = (int)((t.h - 2) / 8.5f), s0 = ty ? std::max(0, (int)ls.size() - nl) : 0;
                for (int i = 0; i < nl && s0 + i < (int)ls.size(); i++) ov_txt(0, 0.4f, ls[s0 + i], t.x + 4, t.y + 1 + i * 8.5f, (int)t.w - 6, 9, 0, 0xFFFFFF);
                if (ty && (GetTickCount() / 500) % 2) {
                    int li = std::max(0, std::min((int)ls.size() - 1 - s0, nl - 1));
                    float cx = ls.empty() || (!v.empty() && v.back() == '\n') ? 0 : psp_ui_text_w(0, 0.4f, ls.back());
                    if (!v.empty() && v.back() == '\n') li = std::min(li + 1, nl - 1);
                    ov_quad(ov_white, t.x + 4 + cx + 1, t.y + 2 + li * 8.5f, 1, 7, 0xFFF379, 1);
                }
            } else {
                std::string q = v;
                if (ty) while (!q.empty() && psp_ui_text_w(0, 0.42f, q) > t.w - 9) { size_t k2 = 1; while (k2 < q.size() && ((unsigned char)q[k2] & 0xC0) == 0x80) k2++; q.erase(0, k2); }
                else q = ol_fit(q, 0.42f, t.w - 8);
                float tw = psp_ui_text_w(0, 0.42f, q);
                ov_txt(0, 0.42f, q, t.x + 4, t.y, (int)ceilf(tw) + 3, (int)t.h, 0, 0xFFFFFF);
                if (ty && (GetTickCount() / 500) % 2) ov_quad(ov_white, t.x + 4 + tw + 1, t.y + 3, 1, t.h - 6, 0xFFF379, 1);
            }
        } else if (t.kind == OL_KTAG) {
            bool sel = std::find(p.tags.begin(), p.tags.end(), t.arg) != p.tags.end();
            if (t.arg == 0) ov_txt(0, 0.36f, ov_s("Tags", "Etiquetas"), OL_FX + 1, OL_FTY - 8, 100, 8, 0, 0x9DB4D6);
            ov_quad(ov_white, t.x, t.y, t.w, t.h, sel ? OL_TAGC[t.arg] : 0x16365F, sel ? 0.95f : 0.8f);
            if (on) { ov_quad(ov_white, t.x, t.y, t.w, 1, 0xFFA300, 1); ov_quad(ov_white, t.x, t.y + t.h - 1, t.w, 1, 0xFFA300, 1); ov_quad(ov_white, t.x, t.y, 1, t.h, 0xFFA300, 1); ov_quad(ov_white, t.x + t.w - 1, t.y, 1, t.h, 0xFFA300, 1); }
            ov_txt(0, 0.36f, ol_tagname(t.arg), t.x, t.y, (int)t.w, (int)t.h, 1, sel ? 0xFFFFFF : 0x9DB4D6);
        } else if (t.kind == OL_KBTN) {
            const char* l = t.arg ? (edit ? ov_s("Save", "Guardar") : p.mode == 1 ? ov_s("Update", "Actualizar") : ov_s("Publish", "Publicar")) : ov_s("Cancel", "Cancelar");
            ov_spr(OP_BUTTON, t.x, t.y, (int)t.w, (int)t.h, on ? 1.0f : t.on ? 0.55f : 0.25f);
            ov_txt(0, 0.45f, l, t.x, t.y, (int)t.w, (int)t.h, 1, on ? 0xEF3300 : t.on ? 0xFFFFFF : 0x9DB4D6);
        }
    }
    if (!edit) {
        float y = 194;
        if (p.ranked < 0) ov_txt(0, 0.38f, ov_s("Checking if it is ranked OK...", "Revisando si es clasificatorio..."), OL_X, y, 150, 9, 0, 0x9DB4D6);
        else ol_pill((float)OL_X, y + 1, p.ranked ? ov_s("Ranked OK", "Clasificatorio") : ov_s("Not ranked", "No clasificatorio"), p.ranked ? 0x2E9E5A : 0x6A7280, 0.36f, 8, OL_X + 150.0f);
        if (p.files) ov_txt(0, 0.38f, std::to_string(p.files) + ov_s(" files · ", " archivos · ") + ol_size((long long)p.bytes), OL_X, y + 11, 150, 9, 0, 0x9DB4D6);
        ov_txt(0, 0.34f, ol_fit(ov_s("The file is kept on your GitHub.", "El archivo se guarda en tu GitHub."), 0.34f, 148), OL_X, y + 22, 150, 8, 0, 0x7F93B4);
    }
    if (busy) {
        ov_spr(OP_DIALOG, 140, 110, 200, 52);
        ov_txt(0, 0.5f, ol_step_tr(p.step), 150, 117, 180, 12, 1, 0xFFF379);
        uint64_t d = lb_up_done, t = lb_up_total;
        float f = t ? (float)std::min<uint64_t>(d, t) / t : 0;
        ol_box(155, 136, 170, 9, 0x0E2547, 0.92f, 0x3D6A9A);
        if (t) ov_quad(ov_white, 156, 137, 168 * f, 7, 0xFFA300, 0.9f);
        if (t) ov_txt(0, 0.36f, ol_size((long long)d) + " / " + ol_size((long long)t), 150, 147, 180, 8, 1, 0x95F1FF);
    }
}

// page body, notices and prompts (the library uses the whole height, no description area)
static void ol_draw(bool pad) {
    ol_enter();
    float pulse = 0.55f + 0.45f * sinf((GetTickCount() - ov_t0) * 0.006f);
    bool foc = !ov_confirm && !ol_pop && lb_overlay != 1 && ol_osk < 0;
    if (ov_net) { ov_txt(0, 0.5f, ov_s("Not available during a network match.", "No disponible durante una partida en red."), OV_PX, OL_TY + 40.0f, OV_PW, 12, 1, 0xD8E4F4); return; }
    if (lb_overlay == 3) ol_draw_pick(foc, pulse);
    else if (lb_overlay == 4) ol_draw_form(foc, pulse);
    else if (lb_view == 1) ol_draw_detail(foc, pulse);
    else { ol_draw_top(foc); ol_draw_grid(foc, pulse); }
    float x = OV_BX - 10;
    if (!ov_confirm) {
        if (ol_osk >= 0) { x = ov_prompt(ov_bind(A_M_RANDOM, true), ov_s("Space", "Espacio"), x, OV_BY + 1.5f); x = ov_prompt(ov_bind(A_M_SWITCH, true), ov_s("Delete", "Borrar"), x, OV_BY + 1.5f); }
        else if (ol_typing) x = ov_prompt(C_KEY(ol_tgt == 4 ? VK_ESCAPE : VK_RETURN), ov_s("Done", "Listo"), x, OV_BY + 1.5f);
        else if (lb_overlay == 4 && !lb_pub.st && !ol_pop) {
            std::vector<OlBtn> b = ol_form_btns();
            if (ol_ff >= 0 && ol_ff < (int)b.size() && b[ol_ff].alt && (b[ol_ff].kind == OL_KCOVER ? !lb_pub.cover.empty() : b[ol_ff].arg < (int)lb_pub.shots.size()))
                x = ov_prompt(ov_bind(A_M_SWITCH, pad), ov_s("Remove", "Quitar"), x, OV_BY + 1.5f);
        }
        else if (lb_overlay == 0 && lb_view == 0 && pad && !ol_pop) x = ov_prompt(ov_bind(A_M_SWITCH, true), ov_s("Search", "Buscar"), x, OV_BY + 1.5f);
        else if (lb_overlay == 0 && lb_view == 1 && lb_gallery().size() > 1 && !ol_pop) {
            uint32_t pv = ov_bind(A_M_SWITCH, pad), nx = ov_bind(A_M_RANDOM, pad);
            float r = ov_prompt(nx, ov_s("Images", "Imágenes"), x, OV_BY + 1.5f), iw = ov_ico_w(pv, 11);
            if (iw > 0) { ov_ico(pv, r + 8 - iw, OV_BY + 2.0f, 11); x = r - iw - 2; } else x = r;
        }
    }
    if (GetTickCount() < ov_note_t) {
        float nw = x - (OV_PX + 12) + 4;
        std::vector<std::string> ls = ov_wrap(ov_note, 0.42f, nw, 2);
        float ny = OV_BY + (ls.size() > 1 ? -0.5f : 3.5f);
        for (size_t i = 0; i < ls.size(); i++) ov_txt(0, 0.42f, ol_fit(ls[i], 0.42f, nw), OV_PX + 12, ny + i * 8.5f, (int)nw, 9, 0, 0x95F1FF);
    }
}

// dialogs over everything
static void ol_draw_modal() {
    if (ov_net) return;
    if (lb_overlay == 1) {
        ov_spr(OP_DIALOG, 110, 92, 260, 98);
        ov_txt(0, OV_TS, ov_s("Sign in with GitHub", "Iniciar sesión con GitHub"), 120, 98, 240, 15, 1, 0xFFF379);
        if (lb_login_st == 2) {
            ov_txt(0, 0.5f, ov_s("Open github.com/login/device and type this code:", "Abre github.com/login/device y escribe este código:"), 120, 116, 240, 11, 1, 0xD8E4F4);
            ov_txt(1, 0.9f, lb_code, 120, 129, 240, 20, 1, 0xFFFFFF);
            ov_txt(0, 0.45f, ov_s("Waiting for GitHub...", "Esperando a GitHub..."), 120, 151, 240, 10, 1, 0x9DB4D6);
        } else {
            std::string t = lb_login_st == 4 ? ol_tr(lb_login_err) : lb_login_st == 3 ? ov_s("Signing in...", "Iniciando sesión...") : ov_s("Connecting to GitHub...", "Conectando con GitHub...");
            std::vector<std::string> ls = ov_wrap(t, 0.55f, 230);
            for (size_t i = 0; i < ls.size(); i++) ov_txt(0, 0.55f, ls[i], 120, 126 + i * 12.0f, 240, 12, 1, lb_login_st == 4 ? 0xFF8A70 : 0xD8E4F4);
        }
        int nb = lb_login_st == 2 || lb_login_st == 4 ? 2 : 1;
        for (int k = 0; k < nb; k++) {
            float bx = nb == 2 ? (k ? 246.0f : 128.0f) : 187.0f;
            bool on = ol_lsel == k || nb == 1;
            const char* t = nb == 1 || k == 1 ? ov_s("Cancel", "Cancelar") : lb_login_st == 2 ? ov_s("Copy and open GitHub", "Copiar y abrir GitHub") : ov_s("Retry", "Reintentar");
            ov_spr(OP_BUTTON, bx, 168, 106, 15, on ? 1.0f : 0.35f);
            ov_txt(0, 0.5f, t, bx, 168, 106, 15, 1, on ? 0xEF3300 : 0xFFFFFF);
        }
        return;
    }
    if (ol_pop == 5) {
        float x, y, w, h; ol_pop_rect(&x, &y, &w, &h);
        ov_spr(OP_DIALOG, x, y, (int)w, (int)h);
        ov_txt(0, OV_TS, ol_fit(lb_cur.name, OV_TS, w - 20), x + 10, y + 6, (int)w - 20, 15, 1, 0xFFF379);
        std::vector<std::string> ls = ol_paragraphs(ol_desc_text(), OV_DS, w - 30);
        float top = y + 24, area = h - 32, content = ls.size() * 12.0f;
        ol_fscroll = std::max(0.0f, std::min(ol_fscroll, std::max(0.0f, content - area)));
        glEnable(GL_SCISSOR_TEST);
        glScissor((int)(ov_fx + x * ov_fs), (int)(ov_ch - (ov_fy + (top + area) * ov_fs)), (int)(w * ov_fs), (int)(area * ov_fs + 1));
        for (size_t i = 0; i < ls.size(); i++) { float ly = top + i * 12.0f - ol_fscroll; if (ly + 12 >= top && ly <= top + area) ov_txt(0, OV_DS, ls[i], x + 14, ly, (int)w - 28, 12, 0, 0xD8E4F4); }
        glDisable(GL_SCISSOR_TEST);
        if (content > area) {
            float kh = 24;
            ov_spr(OP_SB_TRACK, x + w - 11, top, 4, (int)area, 0.8f);
            ov_spr(OP_SB_KNOB, x + w - 13, top + (area - kh) * (ol_fscroll / (content - area)), 8, (int)kh);
        }
        return;
    }
    if (ol_pop) {
        float x, y, w, h; ol_pop_rect(&x, &y, &w, &h);
        int n = (int)ol_pitems.size(), rows = (n + ol_pcols - 1) / ol_pcols;
        ov_spr(OP_DIALOG, x, y, (int)w, (int)h);
        ov_txt(0, OV_TS, ol_fit(ol_ptitle, OV_TS, w - 20), x + 10, y + 6, (int)w - 20, 15, 1, 0xFFF379);
        for (int i = 0; i < n; i++) {
            bool on = i == ol_psel;
            float bx = x + 10 + (i / rows) * 124.0f, by = y + 24 + (i % rows) * 17.0f;
            ov_spr(OP_BUTTON, bx, by, 120, 15, on ? 1.0f : 0.35f);
            ov_txt(0, OL_S, ol_fit(ol_pitems[i].first, OL_S, 112), bx, by, 120, 15, 1, on ? 0xEF3300 : 0xFFFFFF);
        }
        return;
    }
    if (ol_osk >= 0) {
        ov_spr(OP_DIALOG, OL_KX, OL_KY, (int)OL_KW, (int)OL_KH);
        static const OlName KT[5] = {{"Search", "Buscar"}, {"Name", "Nombre"}, {"Version", "Versión"}, {"Link", "Enlace"}, {"Description", "Descripción"}};
        ov_txt(0, OV_TS, ov_s(KT[ol_tgt].en, KT[ol_tgt].es), OL_KX + 10, OL_KY + 5, (int)OL_KW - 20, 15, 1, 0xFFF379);
        ol_box(OL_KX + 10, OL_KY + 21, OL_KW - 20, 15, 0x0E2547, 0.92f, 0xFFA300);
        std::string q = ol_tref();
        for (char& ch : q) if (ch == '\n') ch = ' ';
        while (!q.empty() && psp_ui_text_w(0, OL_S, q) > OL_KW - 32) { size_t k = 1; while (k < q.size() && ((unsigned char)q[k] & 0xC0) == 0x80) k++; q.erase(0, k); }
        float tw = psp_ui_text_w(0, OL_S, q);
        ov_txt(0, OL_S, q, OL_KX + 14, OL_KY + 21, (int)ceilf(tw) + 3, 15, 0, 0xFFFFFF);
        if ((GetTickCount() / 500) % 2) ov_quad(ov_white, OL_KX + 15 + tw, OL_KY + 24, 1, 9, 0xFFF379, 1);
        static const OlName B[3] = {{"Space", "Espacio"}, {"Delete", "Borrar"}, {"Done", "Listo"}};
        for (int i = 0; i < 43; i++) {
            float x, y, w; ol_key_rect(i, &x, &y, &w);
            bool on = i == ol_osk;
            ov_spr(OP_BUTTON, x, y, (int)w, 14, on ? 1.0f : 0.4f);
            ov_txt(0, i < 40 ? OV_TS : OL_S, i < 40 ? ol_key(i) : ov_s(B[i - 40].en, B[i - 40].es), x, y, (int)w, 14, 1, on ? 0xEF3300 : 0xFFFFFF);
        }
    }
}
