#pragma once
// names as text (TTT_TEXT_NAMES): the name textures of the menus written with the app's fonts (games/ttt/fonts, disc/fonttext.h)
// from the names of the maps (games/ttt/names/maps.txt, stages.txt name_en/name_es); served at 4x over the game's atlases
namespace tn {
static const int Q = 4;
static bool on() {
    static int v = 0, n = 0;
    if ((n++ & 63) == 0) { const char* e = getenv("TTT_TEXT_NAMES"); v = e && *e ? atoi(e) : 0; }
    return v > 0;
}
static std::string latino(std::string s) {   // Spanish: "Saiyan" -> "Saiyajin"
    for (size_t p = 0; (p = s.find("Saiyan", p)) != std::string::npos; p += 8) s.replace(p, 6, "Saiyajin");
    return s;
}
static int lang_of(const std::string& f) {   // the container's language (menu_training_es/..., map_select_es.pak), else the game's
    if (f.find("_es/") != std::string::npos || f.find("_es.pak") != std::string::npos) return 1;
    if (f.find("_en/") != std::string::npos || f.find("_en.pak") != std::string::npos) return 0;
    return psp_mv_lang == 1 ? 1 : 0;
}
static void classify(uint32_t id) {   // tnk: -1 none, 1 map names atlas (tna -1 = the game's, else new atlas a), tnl language
    TexPng& t = tex_reg[id - 1];
    if (__atomic_load_n(&t.tnk, __ATOMIC_ACQUIRE) != -2) return;
    int tnk = -1, tna = 0, tnl = 0;
    for (const std::string& x0 : {t.fpath, link_real(t.fpath)}) {
        std::string f = lower(x0);
        for (auto& ch : f) if (ch == '\\') ch = '/';
        size_t p = f.find("map_names_bg.rhg/");
        if (p != std::string::npos && isdigit((unsigned char)f[p + 17])) {
            int nn = atoi(f.c_str() + p + 17), b = st_names_base >= 0 ? st_names_base : 2;
            if (nn == 2) tna = -1; else if (nn - 1 >= b) tna = nn - 1 - b; else continue;
            tnk = 1; tnl = lang_of(f);
            break;
        }
        p = f.find("char_names_");   // character names: char_names_<lang>.rhg/NN.png
        if (p != std::string::npos && f.compare(p + 13, 5, ".rhg/") == 0 && isdigit((unsigned char)f[p + 18])) {
            tnk = 2; tna = atoi(f.c_str() + p + 18) - 1; tnl = f.compare(p + 11, 2, "es") == 0 ? 1 : 0;
            break;
        }
        for (int k = 0; k < 2; k++) {   // form labels: 030 (left style, AGS 029), 032 (right style, AGS 031), or their cached copies
            p = f.find(k ? "032_char_forms" : "030_char_forms");
            if (p == std::string::npos) continue;
            tnk = 3; tna = k; tnl = f.find("/es_03") != std::string::npos ? 1 : f.find("/en_03") != std::string::npos ? 0 : lang_of(f);
            break;
        }
        if (tnk >= 0) break;
        if (f.find("music_selector.rhg/") != std::string::npos) {   // song names: the music selector texture tall enough for the 9 rows of 16
            static int sl = 0;
            if (sl++ < 16) { printf("nombres: textura del selector de musica %s (%dx%d)\n", t.fpath.c_str(), t.width0, t.height0); fflush(stdout); }
            if (t.width0 >= 128 && t.height0 >= 144) { tnk = 4; tna = 0; tnl = lang_of(f); break; }
        }
        p = f.find("/cache/stages/");
        if (p != std::string::npos && f.compare(p + 16, 7, "_names_") == 0) {
            tnk = 1; tna = atoi(f.c_str() + p + 23); tnl = f.compare(p + 14, 2, "es") == 0 ? 1 : 0;
            break;
        }
    }
    static int logs = 0;
    static const char* KN[5] = {"", "atlas de mapas", "nombres de personajes", "etiquetas de forma", "nombres de canciones"};
    if (tnk >= 0 && logs++ < 24) { printf("nombres: textura %s -> %s %d, idioma %s\n", t.fpath.c_str(), KN[tnk], tna, tnl ? "es" : "en"); fflush(stdout); }
    t.tna = tna; t.tnl = tnl;
    __atomic_store_n(&t.tnk, tnk, __ATOMIC_RELEASE);
}
static std::shared_ptr<gr::Img> map_atlas(int a, int l) {
    static std::mutex m;
    static std::map<int, std::shared_ptr<gr::Img>> done;
    { std::lock_guard<std::mutex> g(m); auto it = done.find(a * 2 + l); if (it != done.end()) return it->second; }
    static ft::Font fnt = ft::load("../../games/ttt/fonts/map_name");
    auto o = std::make_shared<gr::Img>();
    o->w = o->h = 256 * Q; o->px.assign((size_t)o->w * o->h, 0); o->uid = gr::img_uid++;
    int nn = (int)stages::defs().maps.size(), rows = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < 11; r++) {
        int i = a < 0 ? (r < stages::OLD ? r : stages::OLD + nn) : stages::OLD + a * 11 + r;
        if (a >= 0 && i >= stages::OLD + nn) break;
        ft::Img t = ft::compose(fnt, l ? latino(stages::name_of(i, l)) : stages::name_of(i, l), 256, 22, Q, -1, 4, 218);
        for (int y = 0; y < t.h; y++) memcpy(&o->px[(size_t)(22 * Q * r + y) * o->w], &t.px[(size_t)y * t.w], (size_t)t.w * 4);
        rows++;
    }
    printf("nombres: atlas de mapas %d (%s) con %d nombres en %.0f ms %s, fuente %s\n", a, l ? "es" : "en", rows, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(),
           gr::bg ? "en segundo plano" : "en el hilo del juego", fnt.ok ? "si" : "NO"); fflush(stdout);
    if (!fnt.ok) o.reset();
    std::lock_guard<std::mutex> g(m);
    return done.emplace(a * 2 + l, o).first->second;
}
// character names: read from the game's own text (system txt1 of each group) by the hooks, in the game's language
static std::mutex nm;
static std::string cname[roster::K_OLD];
static int clang = -1;
static std::atomic<uint32_t> cepoch{1};
static std::vector<std::array<std::string, 2>> rows_file(const char* name) {   // games/ttt/names/<name>: row | en | es
    std::vector<std::array<std::string, 2>> v;
    if (FILE* f = fopen((std::string("../../games/ttt/names/") + name).c_str(), "rb")) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            std::string s = line;
            if (s.empty() || s[0] == '#') continue;
            size_t a = s.find('|'), b = a == std::string::npos ? a : s.find('|', a + 1);
            if (b == std::string::npos) continue;
            int r = atoi(s.c_str());
            if (r < 0 || r > 255) continue;
            if ((int)v.size() <= r) v.resize(r + 1);
            v[r] = {stages::trim(s.substr(a + 1, b - a - 1)), stages::trim(s.substr(b + 1))};
        }
        fclose(f);
    }
    return v;
}
static std::shared_ptr<gr::Img> base_up(uint32_t id, int W, int H) {   // the game's texture at the served size
    TexPng* b = tex_load(id);
    auto m = std::make_shared<gr::Img>();
    if (!b) { m->w = W; m->h = H; m->px.assign((size_t)W * H, 0); return m; }
    gr::Img o; o.w = b->w; o.h = b->h; o.px = b->px;
    *m = gr::resample(o, 0, 0, (float)o.w, (float)o.h, W, H);
    return m;
}
static void paste(gr::Img& d, const ft::Img& t, int x0, int y0) {
    for (int y = 0; y < t.h; y++) for (int x = 0; x < t.w; x++) {
        int X = x0 + x, Y = y0 + y;
        if (X >= 0 && Y >= 0 && X < d.w && Y < d.h) d.px[(size_t)Y * d.w + X] = t.px[(size_t)y * t.w + x];
    }
}
static std::shared_ptr<gr::Img> forms(uint32_t id, int k, int l) {   // the form atlas, its text rows (forms.txt) written again
    static std::mutex m;
    static std::map<std::string, std::shared_ptr<gr::Img>> done;   // by real file, side and language: the same atlas in several paks is made once
    std::string key;
    { const TexPng& t = tex_reg[id - 1]; key = link_real(t.fpath) + "|" + std::to_string(t.width0) + "x" + std::to_string(t.height0) + "|" + std::to_string(k * 2 + l); }
    { std::lock_guard<std::mutex> g(m); auto it = done.find(key); if (it != done.end()) return it->second; }
    static ft::Font fnt[2] = {ft::load("../../games/ttt/fonts/char_form/left"), ft::load("../../games/ttt/fonts/char_form/right")};
    static const bool sized = (fnt[0].size = fnt[1].size = FORM_SIZE, true);
    (void)sized;
    auto t0 = std::chrono::steady_clock::now();
    auto& rows = roster::form_rows();
    const TexPng& t = tex_reg[id - 1];
    int W = std::max(1, t.width0) * Q, H = std::max(1, t.height0) * Q, n = 0;
    std::shared_ptr<gr::Img> o;
    if (fnt[k].ok && !rows.empty()) {
        o = base_up(id, W, H);
        for (size_t r = 0; r < rows.size(); r++) {
            if (rows[r][0].empty() && rows[r][1].empty()) continue;
            std::string s = roster::form_text((int)r, l);   // the roster's text for this row, else the game's
            int y0 = 14 * (int)r * Q;
            if (y0 + 14 * Q > H) break;
            for (int y = y0; y < y0 + 14 * Q; y++) memset(&o->px[(size_t)y * W], 0, (size_t)W * 4);
            paste(*o, ft::compose(fnt[k], s, t.width0, 14, Q, -1, k ? 3.0f : 2.0f), 0, y0);
            n++;
        }
        for (auto& e : tn_rects) {   // transformation labels given as text, in the cells the roster placed them
            if (e.kind != k + 1 || e.l != l) continue;
            int x0 = (int)e.r[0] * Q, y0 = (int)e.r[1] * Q, cw = (int)(e.r[2] - e.r[0]), ch = (int)(e.r[3] - e.r[1]);
            for (int y = y0; y < std::min(H, y0 + ch * Q); y++) for (int x = x0; x < std::min(W, x0 + cw * Q); x++) o->px[(size_t)y * W + x] = 0;
            paste(*o, ft::compose(fnt[k], e.text, cw, ch, Q, -1, k ? 3.0f : 2.0f), x0, y0);
            n++;
        }
        o->uid = gr::img_uid++;
    }
    printf("nombres: etiquetas de forma %s (%s), %d filas escritas, fuente %s, %.0f ms %s\n", k ? "derecha" : "izquierda", l ? "es" : "en", n, fnt[k].ok ? "si" : "NO",
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), gr::bg ? "en segundo plano" : "en el hilo del juego"); fflush(stdout);
    std::lock_guard<std::mutex> g(m);
    return done.emplace(key, o).first->second;
}
static std::shared_ptr<gr::Img> songs(uint32_t id, int l) {   // the song names texture, its rows (songs.txt) written again
    static std::mutex m;
    static std::map<uint32_t, std::shared_ptr<gr::Img>> done;
    std::lock_guard<std::mutex> g(m);
    auto it = done.find(id);
    if (it != done.end()) return it->second;
    static ft::Font fnt = ft::load("../../games/ttt/fonts/song_name");
    static auto rows = rows_file("songs.txt");
    const TexPng& t = tex_reg[id - 1];
    int W = std::max(1, t.width0) * Q, H = std::max(1, t.height0) * Q, n = 0;
    std::shared_ptr<gr::Img> o;
    if (fnt.ok && !rows.empty()) {
        o = base_up(id, W, H);
        for (size_t r = 0; r < rows.size(); r++) {
            const std::string& s = rows[r][l];
            int y0 = 16 * (int)r * Q;
            if (s.empty() || y0 + 16 * Q > H) continue;
            for (int y = y0; y < y0 + 16 * Q; y++) memset(&o->px[(size_t)y * W], 0, (size_t)W * 4);
            paste(*o, ft::compose(fnt, s, t.width0, 16, Q, -1, 0), 0, y0);
            n++;
        }
        o->uid = gr::img_uid++;
    }
    printf("nombres: canciones (%s), %d filas escritas en %s, fuente %s\n", l ? "es" : "en", n, t.fpath.c_str(), fnt.ok ? "si" : "NO"); fflush(stdout);
    done[id] = o;
    return o;
}
static std::shared_ptr<gr::Img> chars(uint32_t id, int tx, int l) {   // a character names texture: each original number's row from its state in AGS 025
    static std::mutex m;
    static std::map<uint64_t, std::shared_ptr<gr::Img>> done;
    std::string nms[roster::K_OLD];
    { std::lock_guard<std::mutex> g(nm); if (clang != l) return nullptr; for (int i = 0; i < roster::K_OLD; i++) nms[i] = cname[i]; }
    auto name_for = [&](int menu) -> std::string {   // an original: its name from the roster, else the game's; a new character: its roster name
        if (menu < roster::K_OLD) { const roster::Rename* rn = roster::renamed(roster::VANILLA[menu]); return rn && !rn->name[l].empty() ? rn->name[l] : nms[menu]; }
        for (auto& c : roster::defs().chars) if (roster::menu_of(c.sysid) == menu) return !c.name[l].empty() ? c.name[l] : c.name[0];
        return "";
    };
    const TexPng& t0r = tex_reg[id - 1];   // by page, size and language: the same page in several paks is made once
    uint64_t key = (uint64_t)(tx & 0xFF) << 56 | (uint64_t)l << 55 | (uint64_t)(t0r.width0 & 0xFFF) << 43 | (uint64_t)(t0r.height0 & 0xFFF) << 31 | (cepoch.load() & 0x7FFFFFFF);
    { std::lock_guard<std::mutex> g(m); auto it = done.find(key); if (it != done.end()) return it->second; }
    static ft::Font fnt = ft::load("../../games/ttt/fonts/char_name");
    static std::vector<uint8_t> d[2];
    static std::vector<ags::State> st[2];
    {
        static std::mutex im;
        std::lock_guard<std::mutex> g(im);
        if (d[l].empty()) {
            std::string lg = l ? "es" : "en", f = disc_dir + "/cache/roster/" + lg + "_025_char_names.ags";
            if (!is_file(f)) f = link_real(disc_dir + "/Shared/" + psp_shared_rel("Menus/char_select/char_select_" + lg + ".pak") + "/1_interface/025_char_names.ags");
            if (!read_file(f, d[l], true) || !ags::parse(d[l], st[l])) { d[l].clear(); printf("nombres: no se pudo leer %s\n", f.c_str()); return nullptr; }
        }
    }
    auto t0 = std::chrono::steady_clock::now();
    const TexPng& t = tex_reg[id - 1];
    int W = std::max(1, t.width0) * Q, H = std::max(1, t.height0) * Q, n = 0;
    auto o = std::make_shared<gr::Img>();
    o->w = W; o->h = H; o->px.assign((size_t)W * H, 0);
    int ns = (int)st[l].size();
    for (int i = 0; i < ns; i++) {
        int s = (i + roster::K_OLD) % ns;   // new numbers first: an original's own row is written last
        std::string name = name_for(s);
        int tt, tw, th; float r[4];
        if (name.empty() || !ags::rect(d[l], st[l][s], &tt, &tw, &th, r) || tt != tx || tw <= 0 || th <= 0) continue;
        float sx = (float)t.width0 / tw, sy = (float)t.height0 / th;
        int cw = (int)std::lround((r[2] - r[0]) * sx), ch = (int)std::lround((r[3] - r[1]) * sy);
        paste(*o, ft::compose(fnt, name, cw, ch, Q, -1, 5), (int)std::lround(r[0] * sx) * Q, (int)std::lround(r[1] * sy) * Q);
        n++;
    }
    o->uid = gr::img_uid++;
    printf("nombres: textura %d de nombres de personajes (%s), %d nombres, fuente %s, %.0f ms %s\n", tx, l ? "es" : "en", n, fnt.ok ? "si" : "NO",
           std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(), gr::bg ? "en segundo plano" : "en el hilo del juego"); fflush(stdout);
    if (!n || !fnt.ok) o.reset();
    std::lock_guard<std::mutex> g(m);
    return done.emplace(key, o).first->second;
}
static std::shared_ptr<gr::Img> psp_of(const std::shared_ptr<gr::Img>& m) {   // the PSP alpha copy, once per image
    static std::mutex pm;
    static std::map<uint64_t, std::shared_ptr<gr::Img>> pa;
    { std::lock_guard<std::mutex> g(pm); auto it = pa.find(m->uid); if (it != pa.end()) return it->second; }
    auto c = std::make_shared<gr::Img>(*m);
    gr::psp_alpha(*c);
    std::lock_guard<std::mutex> g(pm);
    return pa.emplace(m->uid, c).first->second;
}
}
int psp_tn_char_want() { if (!tn::on() || psp_mv_lang < 0) return 0; std::lock_guard<std::mutex> g(tn::nm); return tn::clang != (psp_mv_lang == 1 ? 1 : 0); }
void psp_tn_char_names(const std::vector<std::string>& v) {
    int l = psp_mv_lang == 1 ? 1 : 0, n = 0;
    { std::lock_guard<std::mutex> g(tn::nm); for (int i = 0; i < roster::K_OLD && i < (int)v.size(); i++) { tn::cname[i] = l ? tn::latino(v[i]) : v[i]; n += !v[i].empty(); } tn::clang = l; }
    tn::cepoch++;
    psp_sel_images_warm();
    printf("nombres: %d nombres de personajes del texto del juego (%s), ej. %s\n", n, l ? "es" : "en", tn::cname[0].c_str()); fflush(stdout);
}
static uint32_t tn_variant(uint32_t id) {
    if (!tn::on()) return 0;
    tn::classify(id);
    int k = tex_reg[id - 1].tnk;
    return k < 0 ? 0 : k == 2 ? 0x74E0A11Du ^ tn::cepoch.load() * 0x9E3779B1u : 0x74E0A11Du;
}
static bool tn_serve(uint32_t id, int* w, int* h, const uint32_t** px) {
    static thread_local std::shared_ptr<gr::Img> hold;
    if (!tn::on()) return false;
    tn::classify(id);
    const TexPng& t = tex_reg[id - 1];
    if (t.tnk < 1) return false;
    std::shared_ptr<gr::Img> m = t.tnk == 1 ? tn::map_atlas(t.tna, t.tnl) : t.tnk == 2 ? tn::chars(id, t.tna, t.tnl) : t.tnk == 3 ? tn::forms(id, t.tna, t.tnl) : tn::songs(id, t.tnl);
    if (!m) return false;
    if (tex_reg[id - 1].psp) m = tn::psp_of(m);
    hold = m;
    if (t.tnk == 2 || t.tnk == 3) psp_sel_images_warm();
    *w = m->w; *h = m->h; *px = m->px.data();
    return true;
}
int icons_pages_n();
std::string icons_page_core(int i, bool battle);
bool icons_compose_as(int page, uint32_t id, int w0, int h0, int w, int h, const uint32_t* px, bool psp, int held, int follow);
// the select's names pages, form labels and render mid/small pages built in a worker when the select opens (TTT_SEL_IMAGES), so the
// game thread only serves ready images
static std::atomic<uint64_t> sw_want{0};
void psp_sel_images_warm() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_SEL_IMAGES"); on = !(e && *e == '0'); }
    bool t = tn::on();
    int q = gr::on_q();
    int mq0 = gm::on_q();
    if (!on || (!t && !q && !mq0) || psp_mv_lang < 0) return;
    int l = psp_mv_lang == 1 ? 1 : 0;
    uint64_t w = (uint64_t)gr::epoch.load() << 32 | (uint64_t)(tn::cepoch.load() & 0xFFFFFF) << 8 | (uint64_t)q << 2 | (uint64_t)l << 1 | (t ? 1 : 0);
    w ^= ((uint64_t)gm::epoch.load() * 0x9E3779B1ull + (uint64_t)mq0) << 40;
    if (sw_want.exchange(w) == w) return;
    if (q) gr::prefetch(q);
    size_t n = tex_reg.size();
    std::thread([w, n, l, t, q] {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        gr::bg = true;
        auto t0 = std::chrono::steady_clock::now();
        std::vector<uint32_t> nm, fm, mid, sm, mp, ma, sg;
        int mq = gm::on_q();
        int big_w = 0, big_h = 0; bool big_psp = false;   // the big render's game size: every character's big fitted ahead
        for (size_t i = 0; i < n; i++) {
            if (sw_want.load() != w) return;
            const TexPng& x = tex_reg[i];
            if (x.pak.find("map_select") != std::string::npos || x.fpath.find("map_render") != std::string::npos || x.fpath.find("map_selector") != std::string::npos || x.fpath.find("music_selector") != std::string::npos) {
                uint32_t id = (uint32_t)i + 1;
                if (t) { tn::classify(id); int k = __atomic_load_n(&x.tnk, __ATOMIC_ACQUIRE); if ((k == 1 || k == 4) && x.tnl == l) (k == 1 ? ma : sg).push_back(id); }
                if (mq && gm::classify(id).kind >= 0) mp.push_back(id);
                continue;
            }
            if (x.pak.find("char_select") == std::string::npos && x.fpath.find("char_select") == std::string::npos && x.fpath.find("roster") == std::string::npos) continue;
            std::string pk = lower(x.pak);
            uint32_t id = (uint32_t)i + 1;
            if (t) {
                tn::classify(id);
                int k = __atomic_load_n(&x.tnk, __ATOMIC_ACQUIRE);
                if ((k == 2 || k == 3) && x.tnl == l) (k == 2 ? nm : fm).push_back(id);
            }
            if (q && tn::lang_of(pk) == l) {
                gr::Ov o = gr::classify(id);
                if (o.kind == 1) mid.push_back(id); else if (o.kind == 2) sm.push_back(id);
                else if (o.kind == 0 && !big_w) { big_w = std::max(1, x.width0) * q; big_h = std::max(1, x.height0) * q; big_psp = x.psp; }
            }
        }
        int a = 0, b = 0, c = 0;
        if (q && big_w) gr::prefit(q, big_psp, big_w, big_h);
        if (!mid.empty() || !sm.empty()) {
            for (int i = 0; i < 400 && !gr::pf_ready.load(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            for (auto* v : {&mid, &sm}) for (uint32_t id : *v) { if (sw_want.load() != w) return; c += gr::page_img(id, gr::classify(id), q) != nullptr; }
        }
        auto put = [&](uint32_t id, const std::shared_ptr<gr::Img>& m) { if (m && tex_reg[id - 1].psp) tn::psp_of(m); return m != nullptr; };
        for (uint32_t id : fm) { if (sw_want.load() != w) return; a += put(id, tn::forms(id, tex_reg[id - 1].tna, l)); }
        for (uint32_t id : nm) { if (sw_want.load() != w) return; b += put(id, tn::chars(id, tex_reg[id - 1].tna, l)); }
        int e = 0, f = 0;   // map select: names atlases, thumbnails atlas, big and small renders
        for (uint32_t id : ma) { if (sw_want.load() != w) return; e += put(id, tn::map_atlas(tex_reg[id - 1].tna, l)); }
        for (uint32_t id : sg) { if (sw_want.load() != w) return; e += put(id, tn::songs(id, l)); }
        for (uint32_t id : mp) {
            if (sw_want.load() != w) return;
            const TexPng& x = tex_reg[id - 1];
            gm::Mq o = gm::classify(id);
            int gw = std::max(1, x.width0) * mq, gh = std::max(1, x.height0) * mq;
            f += (o.kind == 1 ? gm::atlas(id, mq) : gm::img(mq, gm::key_of(o.map), o.kind ? "small" : "big", x.psp, gw, gh)) != nullptr;
        }
        int ic = 0;   // fixed menu button pages (navbar...) composed for the device in use
        if (!gr::bg_icons_done.exchange(true)) {
            std::vector<std::string> cores;
            for (int i = 0, np = icons_pages_n(); i < np; i++) { std::string c = lower(icons_page_core(i, false)); if (!c.empty()) cores.push_back(c); }
            for (size_t i = 0; i < n && !cores.empty(); i++) {
                if (sw_want.load() != w) return;
                std::string fp = lower(tex_reg[i].fpath);
                bool hit = false;
                for (auto& c : cores) if (fp.find(c) != std::string::npos) { hit = true; break; }
                if (!hit) continue;
                int page = tex_icon_page(tex_reg[i]);
                if (page < 0 || icons_page_core(page, false).empty()) continue;
                TexPng* b = tex_load((uint32_t)i + 1);
                int ow, oh; const uint32_t* opx;
                if (b && icons_compose(page, (uint32_t)i + 1, b->width0, b->height0, b->w, b->h, b->px.data(), b->psp, &ow, &oh, &opx)) ic++;
            }
        }
        if (nm.empty() && fm.empty() && mid.empty() && sm.empty() && ma.empty() && mp.empty() && sg.empty()) { uint64_t e = w; sw_want.compare_exchange_strong(e, 0); }
        static std::atomic<int> logs{0};
        if (logs++ < 8) printf("select: imagenes en segundo plano: %d texturas de etiquetas, %d texturas de nombres, %d texturas de renders (%d mid, %d small), mapas %d nombres y %d renders, %d paginas de botones en %.0f ms\n", a, b, c, (int)mid.size(), (int)sm.size(), e, f, ic,
               std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        fflush(stdout);
    }).detach();
}
// battle button pages composed while the loading screen reads the fighters (first state and the charge prompt), TTT_BATTLE_ICONS=0 off
void psp_battle_icons_ahead() {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_BATTLE_ICONS"); on = !(e && *e == '0'); }
    static std::atomic<bool> busy{false};
    if (!on || busy.exchange(true)) return;
    size_t n = tex_reg.size();
    std::thread([n] {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        auto t0 = std::chrono::steady_clock::now();
        static std::vector<std::pair<uint32_t, int>> found;   // texture, page: the scan runs once
        static bool scanned = false;
        if (!scanned) {
            scanned = true;
            std::vector<std::string> cores;
            for (int i = 0, np = icons_pages_n(); i < np; i++) { std::string c = lower(icons_page_core(i, true)); if (!c.empty()) cores.push_back(c); }
            for (size_t i = 0; i < n && !cores.empty(); i++) {
                std::string fp = lower(tex_reg[i].fpath);
                bool hit = false;
                for (auto& c : cores) if (fp.find(c) != std::string::npos) { hit = true; break; }
                if (!hit) continue;
                int page = tex_icon_page(tex_reg[i]);
                if (page >= 0 && !icons_page_core(page, true).empty()) found.push_back({(uint32_t)i + 1, page});
            }
        }
        int k = 0;
        for (auto& f : found) {
            TexPng* b = tex_load(f.first);
            if (!b) continue;
            for (int follow = 0; follow < 2; follow++) k += icons_compose_as(f.second, f.first, b->width0, b->height0, b->w, b->h, b->px.data(), b->psp, -1, follow);
        }
        static std::atomic<int> logs{0};
        if (logs++ < 4) printf("icons: paginas del combate preparadas en la carga: %d de %d texturas en %.0f ms\n", k, (int)found.size() * 2,
                               std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
        fflush(stdout);
        busy = false;
    }).detach();
}
