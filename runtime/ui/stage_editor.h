#pragma once
// Maps tab of the mod creator (item 29.3): new map slots after the 10 originals (mods/<id>/Stages/stages.txt), their select
// images (big render, thumbnail, name per language) and their map and SFX files (Packfile/Maps/<ID>, Packfile/Maps/SFX/<ID>)
#include "disc/stages_def.h"

enum { IDS_NEW = 3500, IDS_DEL, IDS_TITLE, IDS_SUB, IDS_ID_L, IDS_ID, IDS_IMG_L, IDS_BIG, IDS_SMALL, IDS_NL0, IDS_NB0 = IDS_NL0 + 3,
       IDS_F_L = IDS_NB0 + 3, IDS_MAP, IDS_SFX, IDS_SFX_L, IDS_SFXOF, IDS_TOUR, IDS_INFO, IDS_GRID, IDS_SUM, IDS_FX0, IDS_FX1, IDS_FX2, IDS_LAST };
struct SeFx { int on[3] = {-1, -1, -1}; std::string src[2], bin; };   // map effects (Packfile/Maps/<map>.fx.txt): haze, bubbles, storm; src = BT3 pak or .fx.bin giving entry 17 / 13
struct SeMap { SeFx fx; std::string id, img[2], name[3], tname[2], map, sfx; bool tour = false; int sfxof = 0; };   // name: old name PNGs (kept), tname: name text en/es   // map/sfx: "ttt:" pak or "dir:" folder

static std::vector<SeMap> se_maps;
static SeFx se_ofx[stages::OLD];
std::string psp_orig_map_stem(int i);
static void se_fx_panel();
static bool se_bt3_stage(const std::string& f);
static std::string se_oname[stages::OLD + 1][2];   // names given to the originals and Random (empty = the game's)
static int se_cur = 0, se_scroll = 0;
static bool se_filling = false;
static HWND se_grid = nullptr;
static std::string se_render[stages::OLD + 1];
static std::map<int, HBITMAP> se_bmp;
static struct { int idx = -1; bool on = false; POINT p0{}; int marker = -1; } se_drag;
static const int SE_CW = 140, SE_CH = 96, SE_IW = 132, SE_IH = 67, SE_GAP = 6, SE_COLS = 4;
static const int SE_W[3] = {300, 64, 256}, SE_H[3] = {152, 44, 22};
static const wchar_t* SE_IMG_N[3] = {L"Big", L"Small", L"Name"};

static int se_count() { return stages::OLD + (int)se_maps.size() + 1; }
static bool se_is_new(int i) { return i >= stages::OLD && i < stages::OLD + (int)se_maps.size(); }
static SeMap* se_sel() { return se_is_new(se_cur) ? &se_maps[se_cur - stages::OLD] : nullptr; }
static int se_oi() { return se_cur < stages::OLD ? se_cur : se_is_new(se_cur) ? -1 : stages::OLD; }   // original or Random being shown
static int se_kind(int i) { return i < stages::OLD ? RK_ORIG : se_is_new(i) ? RK_NEW : RK_RANDOM; }
static std::string se_label(int i) {
    if (i < stages::OLD) return stages::NAMES[i];
    if (!se_is_new(i)) return "Random";
    const SeMap& m = se_maps[i - stages::OLD];
    return m.id.empty() ? "New map" : m.id;
}
static std::string se_image_src(int i) {
    if (i < stages::OLD) return se_render[i];
    if (!se_is_new(i)) return se_render[stages::OLD];
    const SeMap& m = se_maps[i - stages::OLD];
    return !m.img[0].empty() ? m.img[0] : m.img[1];
}
static void se_bitmaps_drop() { for (auto& b : se_bmp) DeleteObject(b.second); se_bmp.clear(); }
static HBITMAP se_bitmap(int i) {
    auto it = se_bmp.find(i);
    if (it != se_bmp.end()) return it->second;
    HBITMAP b = re_scaled(se_image_src(i), E(SE_IW), E(SE_IH), RE_DARK[se_kind(i)]);
    if (b) se_bmp[i] = b;
    return b;
}
static void se_renders_scan() {
    for (auto& r : se_render) r.clear();
    std::string dir = psp_assets_dir() + "/Shared/" + psp_shared_rel("Menus/map_select/map_renders_big.rhg");
    std::error_code ec;
    for (auto& p : std::filesystem::directory_iterator(std::filesystem::u8path(dir), ec)) {
        std::string n = p.path().filename().u8string();
        if (!isdigit((unsigned char)n[0]) || !re_ends(n, ".png")) continue;
        int k = atoi(n.c_str()) - 1;
        if (k >= 0 && k <= stages::OLD && (se_render[k].empty() || n.find('@') != std::string::npos)) se_render[k] = p.path().u8string();
    }
}

// ---------------- grid ----------------
static RECT se_cell_rect(int i) {
    RECT r; r.left = E(SE_GAP + (i % SE_COLS) * (SE_CW + SE_GAP)); r.top = E(SE_GAP + (i / SE_COLS) * (SE_CH + SE_GAP)) - se_scroll;
    r.right = r.left + E(SE_CW); r.bottom = r.top + E(SE_CH);
    return r;
}
static void se_scrollbar() {
    if (!se_grid) return;
    RECT rc; GetClientRect(se_grid, &rc);
    int rows = (se_count() + SE_COLS - 1) / SE_COLS, total = E(SE_GAP + rows * (SE_CH + SE_GAP));
    se_scroll = std::max(0, std::min(se_scroll, total - (int)rc.bottom));
    SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS; si.nMin = 0; si.nMax = std::max(0, total - 1); si.nPage = rc.bottom; si.nPos = se_scroll;
    SetScrollInfo(se_grid, SB_VERT, &si, TRUE);
}
static int se_hit(POINT p) { for (int i = 0; i < se_count(); i++) { RECT r = se_cell_rect(i); if (PtInRect(&r, p)) return i; } return -1; }
static void se_cell(HDC m, HDC im, int i, RECT r, bool sel, bool ghost) {
    int k = se_kind(i);
    HBRUSH b = CreateSolidBrush(RE_DARK[k]);
    HPEN pen = CreatePen(PS_SOLID, sel || ghost ? E(3) : E(2), ghost ? THEME_ACCENT : sel ? THEME_TEXT : RE_COL[k]);
    HGDIOBJ o1 = SelectObject(m, b), o2 = SelectObject(m, pen);
    RoundRect(m, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, E(12), E(12));
    SelectObject(m, o1); SelectObject(m, o2); DeleteObject(b); DeleteObject(pen);
    if (HBITMAP bmp = se_bitmap(i)) {
        HGDIOBJ ob = SelectObject(im, bmp);
        if (ghost) { BLENDFUNCTION bf = {AC_SRC_OVER, 0, 150, 0}; GdiAlphaBlend(m, r.left + E(4), r.top + E(4), E(SE_IW), E(SE_IH), im, 0, 0, E(SE_IW), E(SE_IH), bf); }
        else BitBlt(m, r.left + E(4), r.top + E(4), E(SE_IW), E(SE_IH), im, 0, 0, SRCCOPY);
        SelectObject(im, ob);
    }
    RECT tr = {r.left + E(4), r.top + E(SE_IH + 6), r.right - E(4), r.bottom - E(4)};
    std::wstring t = (i < se_count() - 1 ? std::to_wstring(i + 1) + L". " : L"") + mw_w(se_label(i));
    SetTextColor(m, ghost ? THEME_ACCENT : THEME_TEXT);
    HGDIOBJ of = SelectObject(m, theme_f_normal);
    DrawTextW(m, t.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(m, of);
}
static void se_paint() {
    PAINTSTRUCT ps; HDC dc = BeginPaint(se_grid, &ps);
    RECT rc; GetClientRect(se_grid, &rc);
    HDC m = CreateCompatibleDC(dc);
    HBITMAP bb = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(m, bb);
    FillRect(m, &rc, theme_b_field);
    HDC im = CreateCompatibleDC(dc);
    SetBkMode(m, TRANSPARENT);
    for (int i = 0; i < se_count(); i++) {
        RECT r = se_cell_rect(i);
        if (r.bottom < 0 || r.top > rc.bottom) continue;
        if (se_drag.on && i == se_drag.marker) se_cell(m, im, se_drag.idx, r, false, true);
        else se_cell(m, im, i, r, i == se_cur, false);
    }
    DeleteDC(im);
    BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
    SelectObject(m, ob); DeleteObject(bb); DeleteDC(m);
    EndPaint(se_grid, &ps);
}
static void se_rebuild();
static void se_select(int i) { se_cur = std::max(0, std::min(i, se_count() - 1)); se_rebuild(); }
static void se_remove() {
    if (!se_sel()) { cm_notify(L"Only new maps can be removed"); return; }
    se_maps.erase(se_maps.begin() + (se_cur - stages::OLD));
    se_bitmaps_drop();
    se_cur = std::min(se_cur, stages::OLD + (int)se_maps.size() - 1);
    if (se_cur < stages::OLD) se_cur = se_maps.empty() ? 0 : stages::OLD;
    cm_notify(L"");
    se_rebuild();
}
static LRESULT CALLBACK se_grid_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    switch (msg) {
        case WM_PAINT: se_paint(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: se_scrollbar(); return 0;
        case WM_GETDLGCODE: return DLGC_WANTARROWS | (l && ((MSG*)l)->message == WM_KEYDOWN && ((MSG*)l)->wParam == VK_DELETE ? DLGC_WANTMESSAGE : 0);
        case WM_MOUSEWHEEL: se_scroll -= GET_WHEEL_DELTA_WPARAM(w) * E(SE_CH + SE_GAP) / 240; se_scrollbar(); InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_VSCROLL: {
            SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_ALL; GetScrollInfo(h, SB_VERT, &si);
            int step = E(SE_CH + SE_GAP), v = se_scroll;
            switch (LOWORD(w)) {
                case SB_LINEUP: v -= step / 2; break; case SB_LINEDOWN: v += step / 2; break;
                case SB_PAGEUP: v -= (int)si.nPage; break; case SB_PAGEDOWN: v += (int)si.nPage; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: v = si.nTrackPos; break;
            }
            se_scroll = v; se_scrollbar(); InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(h);
            POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            int i = se_hit(p);
            if (i < 0) return 0;
            se_drag.idx = i; se_drag.on = false; se_drag.marker = -1; GetCursorPos(&se_drag.p0);
            SetCapture(h);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (GetCapture() != h || se_drag.idx < 0) return 0;
            POINT s; GetCursorPos(&s);
            if (!se_drag.on && (!se_is_new(se_drag.idx) || abs(s.x - se_drag.p0.x) + abs(s.y - se_drag.p0.y) < E(6))) return 0;
            se_drag.on = true;
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            POINT c = s; ScreenToClient(h, &c);
            RECT rc; GetClientRect(h, &rc);
            if (c.y < E(24)) { se_scroll -= E(12); se_scrollbar(); }
            else if (c.y > rc.bottom - E(24)) { se_scroll += E(12); se_scrollbar(); }
            int col = std::max(0, std::min(SE_COLS - 1, (int)(c.x / E(SE_CW + SE_GAP)))), row = std::max(0, (int)((c.y + se_scroll) / E(SE_CH + SE_GAP)));
            se_drag.marker = std::max(stages::OLD, std::min(row * SE_COLS + col, stages::OLD + (int)se_maps.size() - 1));
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONUP: case WM_CAPTURECHANGED: {
            if (se_drag.idx < 0) return 0;
            bool on = se_drag.on && msg == WM_LBUTTONUP;
            int idx = se_drag.idx, dst = se_drag.marker;
            se_drag.idx = -1; se_drag.on = false; se_drag.marker = -1;
            if (msg == WM_LBUTTONUP && GetCapture() == h) ReleaseCapture();
            if (msg == WM_LBUTTONUP && !on) se_select(idx);
            else if (on && dst >= stages::OLD && dst != idx) {   // the dragged map takes that place, the rest moves
                SeMap m = se_maps[idx - stages::OLD];
                se_maps.erase(se_maps.begin() + (idx - stages::OLD));
                se_maps.insert(se_maps.begin() + (dst - stages::OLD), m);
                se_bitmaps_drop();
                se_select(dst);
            } else InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_KEYDOWN:
            if (w == VK_DELETE) { se_remove(); return 0; }
            if (w == VK_LEFT || w == VK_RIGHT || w == VK_UP || w == VK_DOWN) {
                int d = w == VK_LEFT ? -1 : w == VK_RIGHT ? 1 : w == VK_UP ? -SE_COLS : SE_COLS;
                se_select(se_cur + d);
                RECT r = se_cell_rect(se_cur), rc; GetClientRect(h, &rc);
                if (r.top < 0) se_scroll += r.top - E(SE_GAP); else if (r.bottom > rc.bottom) se_scroll += r.bottom - rc.bottom + E(SE_GAP);
                se_scrollbar(); InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            break;
    }
    return DefWindowProcW(h, msg, w, l);
}

// ---------------- side panel ----------------
static void se_show(int id, bool on) { if (HWND c = GetDlgItem(cm, id)) ShowWindow(c, on && cm_tab == 2 ? SW_SHOW : SW_HIDE); }
static std::string se_file_name(const std::string& f) { std::string p = f.substr(4); size_t s = p.find_last_of("/\\"); return s == std::string::npos ? p : p.substr(s + 1); }
static void se_panel() {
    SeMap* m = se_sel();
    bool nw = m != nullptr;
    re_set_text(IDS_TITLE, se_label(se_cur));
    std::string sub = se_cur < stages::OLD ? "Original map " + std::to_string(se_cur + 1) : !nw ? "Random: picks any map" : "New map " + std::to_string(se_cur + 1) + "\nPackfile/Maps/" + (m->id.empty() ? std::string("<ID>") : m->id);
    re_set_text(IDS_SUB, sub);
    for (int id = IDS_ID_L; id < IDS_LAST; id++) if (id != IDS_GRID && id != IDS_SUM) se_show(id, nw);
    se_show(IDS_SFX_L, nw && m->sfx.empty());
    se_show(IDS_SFXOF, nw && m->sfx.empty());
    int oi = se_oi();
    for (int l = 0; l < 2; l++) { se_show(IDS_NL0 + l, true); se_show(IDS_NB0 + l, true); }
    se_fx_panel();
    se_filling = true;
    for (int l = 0; l < 2; l++) {
        HWND e = GetDlgItem(cm, IDS_NB0 + l);
        SetWindowTextW(e, mw_w(nw ? m->tname[l] : !se_oname[oi][l].empty() ? se_oname[oi][l] : stages::default_name(oi, l)).c_str());
        SendMessageW(e, EM_SETCUEBANNER, TRUE, (LPARAM)(nw ? mw_w(m->id).c_str() : L""));
    }
    se_filling = false;
    if (!nw) { InvalidateRect(cm, nullptr, TRUE); return; }
    se_filling = true;
    SetWindowTextW(GetDlgItem(cm, IDS_ID), mw_w(m->id).c_str());
    SendMessageW(GetDlgItem(cm, IDS_SFXOF), CB_SETCURSEL, m->sfxof, 0);
    se_filling = false;
    SetWindowTextW(GetDlgItem(cm, IDS_MAP), m->map.empty() ? L"Map" : (L"Map: " + mw_w(se_file_name(m->map))).c_str());
    SetWindowTextW(GetDlgItem(cm, IDS_SFX), m->sfx.empty() ? L"SFX" : (L"SFX: " + mw_w(se_file_name(m->sfx))).c_str());
    re_mark(IDS_MAP, !m->map.empty());
    re_mark(IDS_SFX, !m->sfx.empty());
    re_mark(IDS_TOUR, m->tour);
    std::string info = m->map.empty() ? "Missing: the map pak" : "";
    if (m->sfx.empty()) info += (info.empty() ? "" : "\n") + std::string("No SFX: uses those of ") + stages::NAMES[m->sfxof];
    re_set_text(IDS_INFO, info);
    for (int id : {(int)IDS_BIG, (int)IDS_SMALL, (int)IDS_NB0, IDS_NB0 + 1, IDS_NB0 + 2}) InvalidateRect(GetDlgItem(cm, id), nullptr, FALSE);
    InvalidateRect(cm, nullptr, TRUE);
}
static void se_rebuild() {
    if (se_cur >= se_count()) se_cur = se_count() - 1;
    se_scrollbar();
    if (se_grid) InvalidateRect(se_grid, nullptr, FALSE);
    int tour = 0; for (auto& m : se_maps) tour += m.tour;
    std::wstring s = std::to_wstring(stages::OLD + se_maps.size()) + L" maps   |   " + std::to_wstring(se_maps.size()) + L" new";
    if (tour) s += L"   |   " + std::to_wstring(tour) + L" with tournament ring";
    SetWindowTextW(GetDlgItem(cm, IDS_SUM), s.c_str());
    se_panel();
}

// ---------------- map effects ----------------
static std::vector<uint8_t> se_read_all(const std::string& f) {
    std::vector<uint8_t> d;
    if (FILE* fp = mods_fopen(f, "rb")) { fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET); if (n > 0 && n < (64 << 20)) { d.resize((size_t)n); d.resize(fread(d.data(), 1, d.size(), fp)); } fclose(fp); }
    return d;
}
static bool se_pck2(const std::vector<uint8_t>& d, int k, size_t* a, size_t* n) {   // little endian PCK2 entry k (BT3 stage or .fx.bin)
    auto u = [&](size_t o) { uint32_t v = 0; if (o + 4 <= d.size()) memcpy(&v, &d[o], 4); return v; };
    uint32_t c = u(0);
    if (d.size() < 16 || c < 8 || c > 64 || k >= (int)c) return false;
    uint32_t x = u(4 + 4 * k), y = u(8 + 4 * k);
    if (x > y || y > d.size()) return false;
    *a = x; *n = y - x;
    return true;
}
static int se_texset(const std::vector<uint8_t>& d, int k) {
    size_t a, n;
    if (!se_pck2(d, k, &a, &n) || n < 0x20) return 0;
    uint32_t c, t; memcpy(&c, &d[a], 4); memcpy(&t, &d[a + 4], 4);
    return t == 8 && c <= 32 ? (int)c : 0;
}
static std::map<std::string, int> se_fx_cache;
static bool se_fx_has(const std::string& f, int k) {   // k 1: bubbles (entry 17, 6 images or more), 2: storm (entry 13)
    std::string key = f + "#" + std::to_string(k);
    auto it = se_fx_cache.find(key);
    if (it != se_fx_cache.end()) return it->second != 0;
    auto d = se_read_all(f);
    bool ok = k == 1 ? se_texset(d, 17) >= 6 : se_texset(d, 13) > 0;
    se_fx_cache[key] = ok;
    return ok;
}
static int se_feat_file(const std::string& f) {   // what the map brings: 1 haze, 2 bubbles, 4 storm, 8 water
    auto it = se_fx_cache.find(f);
    if (it != se_fx_cache.end()) return it->second;
    auto d = se_read_all(f);
    int r = 0;
    auto le = [&](size_t o) { uint32_t v = 0; if (o + 4 <= d.size()) memcpy(&v, &d[o], 4); return v; };
    auto be = [&](size_t o) { return o + 4 <= d.size() ? (uint32_t)d[o] << 24 | (uint32_t)d[o + 1] << 16 | (uint32_t)d[o + 2] << 8 | d[o + 3] : 0u; };
    bool bt3 = se_bt3_stage(f);
    uint32_t m0 = bt3 ? le(8) : be(8), m1 = bt3 ? le(12) : be(12);
    if (m0 < m1 && m1 <= d.size() && le(m0) == 0x4450414Du) {
        auto sec = [&](uint32_t at, uint32_t len) -> size_t { uint32_t c = le(m0 + at), of = le(m0 + at + 4); return (int32_t)c > 0 && of && m0 + of + len <= m1 ? m0 + of : 0; };
        size_t inf = sec(0x10, 8), w9 = sec(0x90, 4), hz = sec(0xA0, 4);
        bool water = inf && (le(inf + 4) & 2) && (!bt3 || (w9 && (le(w9) & 1)));
        if (water) r |= 8;
        if (hz && (le(hz) & 1)) r |= 1;
    }
    if (bt3 && (r & 8) && se_texset(d, 17) >= 6) r |= 2;
    if (bt3 && se_texset(d, 13) > 0) r |= 4;
    se_fx_cache[f] = r;
    return r;
}
static SeFx* se_fx_cur() { SeMap* m = se_sel(); return m ? &m->fx : se_cur < stages::OLD ? &se_ofx[se_cur] : nullptr; }
static int se_feat(int i) {   // originals: TTT maps, no water and none of the BT3 effects
    if (!se_is_new(i)) return 0;
    const SeMap& m = se_maps[i - stages::OLD];
    if (m.map.compare(0, 4, "ttt:") == 0) return se_feat_file(m.map.substr(4));
    return m.map.empty() ? 0 : 8;   // opened folder: water unknown, bubbles allowed
}
static bool se_fx_eff(const SeFx& x, int f, int k) { return x.on[k] >= 0 ? x.on[k] != 0 : (f >> k & 1) != 0; }
static std::string se_fx_source(SeFx& x, int k) {   // k 1 bubbles, 2 storm: this map's source, else any map of the mod that already has one
    auto good = [&](const std::string& f) { return !f.empty() && se_fx_has(f, k); };
    if (good(x.src[k - 1])) return x.src[k - 1];
    if (good(x.bin)) return x.bin;
    std::vector<SeFx*> all;
    for (auto& o : se_ofx) all.push_back(&o);
    for (auto& m : se_maps) all.push_back(&m.fx);
    for (auto* o : all) { if (good(o->src[k - 1])) return x.src[k - 1] = o->src[k - 1]; if (good(o->bin)) return x.src[k - 1] = o->bin; }
    std::vector<std::string> paks;   // a BT3 stage of this mod that has the effect (Mount_Paozu / Ruined_Earth first)
    for (auto& m : se_maps) if (m.map.compare(0, 4, "ttt:") == 0) {
        std::string l = m.id; for (auto& ch : l) ch = (char)tolower((unsigned char)ch);
        bool pref = l == (k == 1 ? "mount_paozu" : "ruined_earth");
        paks.insert(pref ? paks.begin() : paks.end(), m.map.substr(4));
    }
    for (auto& f : paks) if (se_bt3_stage(f) && good(f)) return x.src[k - 1] = f;
    return "";
}
static void se_fx_panel() {
    SeFx* x = se_fx_cur();
    for (int k = 0; k < 3; k++) se_show(IDS_FX0 + k, x != nullptr);
    if (!x) return;
    int f = se_feat(se_cur);
    for (int k = 0; k < 3; k++) { re_mark(IDS_FX0 + k, se_fx_eff(*x, f, k)); InvalidateRect(GetDlgItem(cm, IDS_FX0 + k), nullptr, FALSE); }
    EnableWindow(GetDlgItem(cm, IDS_FX1), (f & 8) != 0);
}
static void se_fx_toggle(int k) {
    SeFx* x = se_fx_cur();
    if (!x) return;
    int f = se_feat(se_cur);
    bool want = !se_fx_eff(*x, f, k);
    if (k == 1 && !(f & 8)) { cm_notify(L"Bubbles need water: this map has none"); return; }
    if (want && k > 0 && !(f >> k & 1) && se_fx_source(*x, k).empty()) {
        auto p = re_open_files(k == 1 ? L"BT3 stage pak with bubbles (Mount_Paozu.pak)" : L"BT3 stage pak with the storm (Ruined_Earth.pak)", L"BT3 stage (*.pak;*.pck2)\0*.pak;*.pck2\0", false);
        if (p.empty()) return;
        if (!se_bt3_stage(p[0]) || !se_fx_has(p[0], k)) { cm_notify(k == 1 ? L"That pak has no bubbles: use a BT3 stage with them (Mount_Paozu.pak)" : L"That pak has no storm: use a BT3 stage with it (Ruined_Earth.pak)"); return; }
        x->src[k - 1] = p[0];
    }
    x->on[k] = want ? 1 : 0;
    static const wchar_t* N[3] = {L"Heat haze", L"Bubbles", L"Storm"};
    std::wstring from;
    if (want && k > 0 && !(f >> k & 1)) { const std::string& n = x->src[k - 1].empty() ? x->bin : x->src[k - 1]; size_t sl = n.find_last_of("/\\"); from = L", images from " + mw_w(sl == std::string::npos ? n : n.substr(sl + 1)); }
    cm_notify(std::wstring(N[k]) + (want ? L" on" : L" off") + from + L" (applies when saved)");
    se_fx_panel();
}
static void se_fx_load(const std::string& base, const std::string& rel, SeFx& x, std::vector<std::string>& taken) {
    std::error_code ec;
    auto isf = [&](const std::string& p) { return std::filesystem::is_regular_file(std::filesystem::u8path(p), ec); };
    if (isf(base + ".fx.bin")) { x.bin = base + ".fx.bin"; taken.push_back(rel + ".fx.bin"); }
    if (!isf(base + ".fx.txt")) return;
    taken.push_back(rel + ".fx.txt");
    auto d = se_read_all(base + ".fx.txt");
    std::string t(d.begin(), d.end());
    static const char* K[3] = {"haze", "bubbles", "storm"};
    size_t a = 0;
    while (a < t.size()) {
        size_t e = t.find('\n', a);
        std::string ln = t.substr(a, e == std::string::npos ? std::string::npos : e - a);
        a = e == std::string::npos ? t.size() : e + 1;
        size_t q = ln.find('=');
        if (q == std::string::npos) continue;
        std::string k = stages::trim(ln.substr(0, q));
        for (auto& ch : k) ch = (char)tolower((unsigned char)ch);
        for (int j = 0; j < 3; j++) if (k == K[j]) x.on[j] = atoi(ln.c_str() + q + 1) != 0 ? 1 : 0;
    }
}
static bool se_fx_export(const ReAdd& add, const std::string& rel, SeFx& x, int f, int& files, std::wstring& err) {
    bool e[3], need = false;
    for (int k = 0; k < 3; k++) { e[k] = se_fx_eff(x, f, k); need = need || (x.on[k] >= 0 && e[k] != ((f >> k & 1) != 0)); }
    if (x.on[0] >= 0 || x.on[1] >= 0 || x.on[2] >= 0) { printf("mods: efectos de mapa %s: calor %d burbujas %d tormenta %d (el mapa trae %d), %s\n", rel.c_str(), e[0], e[1], e[2], f, need ? "se escriben" : "sin cambios"); fflush(stdout); }
    if (!need) return true;
    std::string t = std::string("haze=") + (e[0] ? "1" : "0") + "\nbubbles=" + (e[1] ? "1" : "0") + "\nstorm=" + (e[2] ? "1" : "0") + "\n";
    if (!add("Packfile/" + rel + ".fx.txt", std::vector<uint8_t>(t.begin(), t.end()))) { err = L"Failed to write the map effects"; return false; }
    files++;
    std::vector<uint8_t> part[2];
    for (int k = 1; k <= 2; k++) {
        if (!e[k] || (f >> k & 1)) continue;
        std::string src = se_fx_source(x, k);
        size_t a, n;
        auto d = src.empty() ? std::vector<uint8_t>() : se_read_all(src);
        if (!se_pck2(d, k == 1 ? 17 : 13, &a, &n) || !n) { err = mw_w(rel) + (k == 1 ? L": bubbles need a BT3 stage pak with them" : L": the storm needs a BT3 stage pak with it"); return false; }
        part[k - 1].assign(d.begin() + a, d.begin() + a + n);
    }
    if (part[0].empty() && part[1].empty()) return true;
    std::vector<uint8_t> body, o(128, 0);
    uint32_t off[26];
    for (int i = 0; i < 25; i++) {
        off[i] = 128 + (uint32_t)body.size();
        const std::vector<uint8_t>* b = i == 17 ? &part[0] : i == 13 ? &part[1] : nullptr;
        if (b && !b->empty()) { body.insert(body.end(), b->begin(), b->end()); body.resize((body.size() + 63) & ~(size_t)63, 0); }
    }
    off[25] = 128 + (uint32_t)body.size();
    uint32_t c = 25; memcpy(&o[0], &c, 4); memcpy(&o[4], off, sizeof off);
    o.insert(o.end(), body.begin(), body.end());
    if (!add("Packfile/" + rel + ".fx.bin", o)) { err = L"Failed to write the map effects"; return false; }
    files++;
    printf("mods: efectos de mapa %s: .fx.bin de %zu bytes (burbujas %zu, tormenta %zu)\n", rel.c_str(), o.size(), part[0].size(), part[1].size()); fflush(stdout);
    return true;
}

// ---------------- images ----------------
static std::string& se_img_ref(SeMap* m, int box) { return box < 2 ? m->img[box] : m->name[box - 2]; }   // box: 0 big, 1 small, 2..3 name en/es
static int se_png_kind(const std::string& f, int* scale) {   // 0 big, 1 small, 2 name by size, -1 none
    std::vector<uint32_t> px; int w = 0, h = 0;
    if (!re_png(f, &w, &h, px)) return -2;
    for (int j = 0; j < 3; j++) { int x = w / SE_W[j]; if (x >= 1 && w % SE_W[j] == 0 && h == SE_H[j] * x) { if (scale) *scale = x; return j; } }
    return -1;
}
static bool se_set_image(SeMap* m, int box, const std::string& f) {
    int j = box < 2 ? box : 2, k = se_png_kind(f, nullptr);
    if (k == -2) { cm_notify(L"The file is not a valid PNG"); return false; }
    if (k != j) { cm_notify(std::wstring(SE_IMG_N[j]) + L" must be " + std::to_wstring(SE_W[j]) + L"x" + std::to_wstring(SE_H[j]) + L" or an exact multiple (2x, 3x...)"); return false; }
    se_img_ref(m, box) = f;
    return true;
}
static void se_images_changed() {
    se_bitmaps_drop();
    cm_notify(L"");
    se_rebuild();
}
static void se_image(int box, bool clear) {
    SeMap* m = se_sel();
    if (!m) return;
    if (clear) { if (se_img_ref(m, box).empty()) return; se_img_ref(m, box).clear(); se_images_changed(); return; }
    auto f = re_open_files((std::wstring(SE_IMG_N[box < 2 ? box : 2]) + L" image").c_str(), L"PNG image (*.png)\0*.png\0", false);
    if (!f.empty() && se_set_image(m, box, f[0])) se_images_changed();
}
static int se_box_of(int id) { return id == IDS_BIG ? 0 : id == IDS_SMALL ? 1 : -1; }
static LRESULT CALLBACK se_img_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    int box = se_box_of((int)GetWindowLongPtrW(h, GWLP_ID));
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, theme_b_bg);
        SeMap* m = se_sel();
        std::string src = m && box >= 0 ? se_img_ref(m, box) : "";
        HBRUSH b = CreateSolidBrush(THEME_FIELD);
        HPEN pen = CreatePen(src.empty() ? PS_DOT : PS_SOLID, 1, src.empty() ? THEME_BORDER : THEME_ACCENT);
        HGDIOBJ o1 = SelectObject(dc, b), o2 = SelectObject(dc, pen);
        RoundRect(dc, 0, 0, rc.right, rc.bottom, E(8), E(8));
        SelectObject(dc, o1); SelectObject(dc, o2); DeleteObject(b); DeleteObject(pen);
        if (!src.empty()) {
            int W = rc.right - E(6), H = rc.bottom - E(6);
            if (HBITMAP bmp = re_scaled(src, W, H, THEME_FIELD)) {
                HDC im = CreateCompatibleDC(dc);
                HGDIOBJ ob = SelectObject(im, bmp);
                BitBlt(dc, E(3), E(3), W, H, im, 0, 0, SRCCOPY);
                SelectObject(im, ob); DeleteDC(im); DeleteObject(bmp);
            }
        } else if (box >= 0) {
            int j = box < 2 ? box : 2;
            std::wstring t = std::wstring(SE_IMG_N[j]) + L" " + std::to_wstring(SE_W[j]) + L"x" + std::to_wstring(SE_H[j]);
            SetBkMode(dc, TRANSPARENT); SetTextColor(dc, THEME_TEXT2);
            HGDIOBJ of = SelectObject(dc, theme_f_normal);
            DrawTextW(dc, t.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(dc, of);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (msg == WM_LBUTTONDOWN) { SetCapture(h); return 0; }   // a press that starts here (not a dialog's double click below)
    if (msg == WM_LBUTTONUP) { bool mine = GetCapture() == h; if (mine) ReleaseCapture(); if (mine && box >= 0) se_image(box, false); return 0; }
    if (msg == WM_RBUTTONUP) { if (box >= 0) se_image(box, true); return 0; }
    if (msg == WM_SETCURSOR) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
    return DefWindowProcW(h, msg, w, l);
}

// ---------------- files ----------------
static bool se_bt3_stage(const std::string& f) {   // BT3 stage file (PCK2, little endian count, entry 0 = BMA3): the port reads it in place
    uint8_t h[0x200] = {0};
    size_t n = 0;
    if (FILE* fp = mods_fopen(f, "rb")) { n = fread(h, 1, sizeof h, fp); fclose(fp); }
    auto u = [&](size_t o) { uint32_t v = 0; if (o + 4 <= n) memcpy(&v, h + o, 4); return v; };
    uint32_t c = u(0), a = u(4);
    if (n < 16 || c < 20 || c > 64 || a + 16 > n) return false;
    return h[a] == 18 && !h[a + 1] && !h[a + 2] && !h[a + 3] && !u(a + 4) && !u(a + 8) && u(a + 12) == 0xFFFFFFFFu;
}
static bool se_pak_ok(const std::string& f, bool sfx) {   // a TTT map pak has 26 entries, its SFX pak 2 (first word, big endian); a map can also be a BT3 stage
    std::string l = f; for (auto& ch : l) ch = (char)tolower((unsigned char)ch);
    if (!re_ends(l, ".pak") && !re_ends(l, ".pck1") && !re_ends(l, ".pck2") && !re_ends(l, ".unk")) return false;
    return re_head_be(f) == (sfx ? 2u : 26u) || (!sfx && se_bt3_stage(f));
}
static bool se_dir_ok(const std::string& d, bool sfx) {
    std::error_code ec;
    for (auto& p : std::filesystem::directory_iterator(std::filesystem::u8path(d), ec)) {
        std::string n = p.path().filename().u8string(); for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
        if (sfx ? re_ends(n, ".pphd") : re_ends(n, ".bma7")) return true;
    }
    return false;
}
static void se_file(int j) {
    SeMap* m = se_sel();
    if (!m) return;
    auto f = re_open_files(j ? L"SFX pak of the map" : L"Map pak", L"Pak or BT3 stage (*.pak;*.pck1;*.pck2;*.unk)\0*.pak;*.pck1;*.pck2;*.unk\0", false);
    if (f.empty()) return;
    if (!se_pak_ok(f[0], j == 1)) { cm_notify(j ? L"SFX: a TTT map SFX pak (2 entries)" : L"Map: a TTT map pak (26 entries) or a BT3 stage file"); return; }
    (j ? m->sfx : m->map) = "ttt:" + f[0];
    cm_notify(L"");
    se_panel();
}
static void se_file_clear(int j) {
    SeMap* m = se_sel();
    if (!m || (j ? m->sfx : m->map).empty()) return;
    (j ? m->sfx : m->map).clear();
    cm_notify(j ? L"SFX removed: those of an original map are used" : L"Map removed");
    se_panel();
}
static void se_drop_files(HDROP hd) {
    POINT pt; DragQueryPoint(hd, &pt);
    UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::string> fs;
    for (UINT i = 0; i < n; i++) { wchar_t f[4096]; if (DragQueryFileW(hd, i, f, 4096)) fs.push_back(mw_u8(f)); }
    HWND at = ChildWindowFromPointEx(cm, pt, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT);
    int id = at ? GetDlgCtrlID(at) : 0, box = se_box_of(id);
    SeMap* m = se_sel();
    if (!m || fs.empty()) { cm_notify(L"Select a new map first"); return; }
    std::error_code ec;
    int done = 0;
    for (auto& f : fs) {
        std::string l = f; for (auto& ch : l) ch = (char)tolower((unsigned char)ch);
        if (std::filesystem::is_directory(std::filesystem::u8path(f), ec)) {
            bool sfx = id == IDS_SFX || (id != IDS_MAP && se_dir_ok(f, true));
            if (!se_dir_ok(f, sfx)) { cm_notify(L"Folder not recognized: a map folder has the .bma7, an SFX folder the .pphd"); continue; }
            (sfx ? m->sfx : m->map) = "dir:" + f; done++;
            continue;
        }
        if (re_ends(l, ".png")) {
            int k = box >= 0 && fs.size() == 1 ? box : se_png_kind(f, nullptr);
            if (k < 0) { cm_notify(L"PNG size not recognized: Big 300x152, Small 64x44, Name 256x22 (or exact multiples)"); continue; }
            if (k == 2 && box < 2) {   // a name dropped elsewhere: the languages without one
                if (se_png_kind(f, nullptr) != 2) continue;
                bool any = false;
                for (auto& x : m->name) if (x.empty()) { x = f; any = true; }
                if (!any) m->name[0] = f;
                done++;
                continue;
            }
            if (se_set_image(m, k, f)) done++;
            continue;
        }
        bool sfx = id == IDS_SFX || (id != IDS_MAP && se_pak_ok(f, true));
        if (se_pak_ok(f, sfx)) { (sfx ? m->sfx : m->map) = "ttt:" + f; done++; }
        else cm_notify(L"Only a TTT map pak (26 entries), a BT3 stage file, its SFX pak (2 entries), their opened folders or PNGs");
    }
    if (done) se_images_changed();
}

// ---------------- commands ----------------
static void se_command(int id, int code) {
    if (se_filling || !re_ready) return;
    SeMap* m = se_sel();
    if (code == BN_CLICKED) {
        if (id == IDS_NEW) {
            SeMap x;
            for (int k = 1; ; k++) { x.id = "Map_" + std::to_string(stages::OLD + (int)se_maps.size() + k); bool t = false; for (auto& o : se_maps) t = t || o.id == x.id; if (!t) break; }
            if (stages::OLD + (int)se_maps.size() >= stages::CAP) { cm_notify(L"The maximum is 126 maps"); return; }
            se_maps.push_back(x);
            se_bitmaps_drop();
            se_select(stages::OLD + (int)se_maps.size() - 1);
            SetFocus(GetDlgItem(cm, IDS_ID)); SendMessageW(GetDlgItem(cm, IDS_ID), EM_SETSEL, 0, -1);
            return;
        }
        if (id == IDS_DEL) { se_remove(); return; }
        if (id == IDS_MAP || id == IDS_SFX) { se_file(id == IDS_SFX); return; }
        if (id == IDS_TOUR && m) { m->tour = !m->tour; se_rebuild(); return; }
        if (id >= IDS_FX0 && id <= IDS_FX2) { se_fx_toggle(id - IDS_FX0); return; }
    }
    if (code == EN_CHANGE && id == IDS_ID && m) {
        HWND e = GetDlgItem(cm, IDS_ID);
        wchar_t t[256] = L""; GetWindowTextW(e, t, 256);
        std::string v = mw_u8(t), clean;
        for (char ch : v) if (isalnum((unsigned char)ch) || ch == '_' || ch == '-') clean += ch;
        if (clean.size() > 48) clean.resize(48);
        if (clean != v) {
            se_filling = true; SetWindowTextW(e, mw_w(clean).c_str()); SendMessageW(e, EM_SETSEL, clean.size(), clean.size()); se_filling = false;
            cm_notify(L"The ID is the map's folder name: letters, numbers, _ and - only, no spaces");
        }
        m->id = clean;
        re_set_text(IDS_TITLE, se_label(se_cur));
        re_set_text(IDS_SUB, "New map " + std::to_string(se_cur + 1) + "\nPackfile/Maps/" + (clean.empty() ? std::string("<ID>") : clean));
        InvalidateRect(se_grid, nullptr, FALSE);
        return;
    }
    if (code == EN_CHANGE && (id == IDS_NB0 || id == IDS_NB0 + 1)) {   // name text: a new map's, or an original's (the game's name = not stored)
        wchar_t t[256] = L""; GetWindowTextW(GetDlgItem(cm, id), t, 256);
        std::string v = stages::trim(mw_u8(t));
        int l = id - IDS_NB0, oi = se_oi();
        if (m) m->tname[l] = v;
        else if (oi >= 0) se_oname[oi][l] = v == stages::default_name(oi, l) ? "" : v;
        return;
    }
    if (code == CBN_SELCHANGE && id == IDS_SFXOF && m) {
        m->sfxof = std::max(0, (int)SendMessageW(GetDlgItem(cm, IDS_SFXOF), CB_GETCURSEL, 0, 0));
        se_panel();
    }
}

// ---------------- load, validate, export ----------------
static bool se_has_content() { if (!se_maps.empty()) return true; for (auto& o : se_ofx) for (int k = 0; k < 3; k++) if (o.on[k] >= 0) return true; for (auto& o : se_oname) for (auto& x : o) if (!x.empty()) return true; return false; }
static void se_reset() { for (auto& o : se_ofx) o = SeFx(); se_fx_cache.clear(); se_maps.clear(); for (auto& o : se_oname) for (auto& x : o) x.clear(); se_cur = 0; se_scroll = 0; se_bitmaps_drop(); }
// returns the mod's Packfile paths that belong to its maps (the Mod tab does not list them)
static std::vector<std::string> se_load_mod(const std::string& root, const std::string& mod) {
    se_reset();
    std::vector<std::string> taken;
    stages::Defs d;
    stages::read_file(root + "/" + mod + "/Stages/stages.txt", mod, d);
    std::error_code ec;
    auto isf = [&](const std::string& p) { return std::filesystem::is_regular_file(std::filesystem::u8path(p), ec); };
    auto isd = [&](const std::string& p) { return std::filesystem::is_directory(std::filesystem::u8path(p), ec); };
    std::string pk = root + "/" + mod + "/Packfile/Maps/";
    for (auto& x : d.maps) {
        SeMap m;
        m.id = x.id; m.tour = x.tour; m.sfxof = x.sfx; m.tname[0] = x.name[0]; m.tname[1] = x.name[1];
        std::string b = root + "/" + mod + "/Stages/" + x.id + "/";
        if (isf(b + "big.png")) m.img[0] = b + "big.png";
        if (isf(b + "small.png")) m.img[1] = b + "small.png";
        for (int l = 0; l < 2; l++) {
            std::string f = b + "name_" + RE_LANG_C[l] + ".png";
            m.name[l] = isf(f) ? f : isf(b + "name.png") ? b + "name.png" : "";
        }
        for (int j = 0; j < 2; j++) {
            std::string base = pk + (j ? "SFX/" : "") + x.id, rel = std::string("Maps/") + (j ? "SFX/" : "") + x.id;
            std::string& dst = j ? m.sfx : m.map;
            if (isf(base + ".pak")) { dst = "ttt:" + base + ".pak"; taken.push_back(rel + ".pak"); }
            else if (isd(base)) { dst = "dir:" + base; taken.push_back(rel); }
            else if (isd(base + ".pak")) { dst = "dir:" + base + ".pak"; taken.push_back(rel + ".pak"); }
        }
        se_fx_load(pk + x.id, "Maps/" + x.id, m.fx, taken);
        se_maps.push_back(m);
    }
    for (int i = 0; i < stages::OLD; i++) { std::string st = psp_orig_map_stem(i); if (!st.empty()) se_fx_load(pk + st, "Maps/" + st, se_ofx[i], taken); }
    for (int i = 0; i <= stages::OLD; i++) for (int l = 0; l < 2; l++) se_oname[i][l] = d.orig[i][l] == stages::default_name(i, l) ? "" : d.orig[i][l];
    se_cur = se_maps.empty() ? 0 : stages::OLD;
    return taken;
}
static bool se_validate(std::wstring& err) {
    std::set<std::string> ids;
    for (size_t i = 0; i < se_maps.size(); i++) {
        auto& m = se_maps[i];
        std::string low = m.id; for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        if (!stages::valid_id(m.id) || !ids.insert(low).second) { err = m.id.empty() ? L"A new map has no ID" : L"The map ID " + mw_w(m.id) + L" is repeated"; se_cur = stages::OLD + (int)i; return false; }
        if (m.map.empty()) { err = mw_w(m.id) + L" needs its map pak"; se_cur = stages::OLD + (int)i; return false; }
    }
    return true;
}
static bool se_export(const ReAdd& add, int& files, std::wstring& err) {
    if (!se_validate(err)) return false;
    std::string t = "# Maps made with the mod creator\n";
    for (auto& m : se_maps) {
        t += "\n[stage]\nid = " + m.id + "\n";
        if (m.tour) t += "tournament = 1\n";
        if (m.sfx.empty()) t += "sfx = " + std::to_string(m.sfxof + 1) + "\n";
        for (int l = 0; l < 2; l++) if (!m.tname[l].empty()) t += std::string("name_") + RE_LANG_C[l] + " = " + m.tname[l] + "\n";
    }
    for (int i = 0; i <= stages::OLD; i++) {
        if (se_oname[i][0].empty() && se_oname[i][1].empty()) continue;
        t += "\n[original]\nn = " + std::to_string(i + 1) + "\n";
        for (int l = 0; l < 2; l++) if (!se_oname[i][l].empty()) t += std::string("name_") + RE_LANG_C[l] + " = " + se_oname[i][l] + "\n";
    }
    if (!add("Stages/stages.txt", std::vector<uint8_t>(t.begin(), t.end()))) { err = L"Failed to write the mkpak"; return false; }
    files++;
    for (auto& m : se_maps) {
        std::string dir = "Stages/" + m.id + "/";
        static const char* IMG[2] = {"big.png", "small.png"};
        for (int j = 0; j < 2; j++) if (!m.img[j].empty() && !re_add_file(add, dir + IMG[j], m.img[j], files)) { err = L"Cannot read " + mw_w(m.img[j]); return false; }
        for (int l = 0; l < 2; l++) if (!m.name[l].empty() && !re_add_file(add, dir + "name_" + RE_LANG_C[l] + ".png", m.name[l], files)) { err = L"Cannot read " + mw_w(m.name[l]); return false; }
        for (int j = 0; j < 2; j++) {
            const std::string& f = j ? m.sfx : m.map;
            if (f.empty()) continue;
            std::string base = std::string("Packfile/Maps/") + (j ? "SFX/" : "") + m.id, kind = f.substr(0, 4), p = f.substr(4);
            bool ok = kind == "ttt:" ? re_add_file(add, base + ".pak", p, files) : re_add_dir(add, base, p, files);
            if (!ok) { err = L"Cannot read the files of " + mw_w(m.id); return false; }
        }
    }
    for (size_t i = 0; i < se_maps.size(); i++) if (!se_fx_export(add, "Maps/" + se_maps[i].id, se_maps[i].fx, se_feat(stages::OLD + (int)i), files, err)) { se_cur = stages::OLD + (int)i; return false; }
    for (int i = 0; i < stages::OLD; i++) {
        bool set = false; for (int k = 0; k < 3; k++) set = set || se_ofx[i].on[k] >= 0;
        if (!set) continue;
        std::string st = psp_orig_map_stem(i);
        if (st.empty()) { err = L"The original map " + std::to_wstring(i + 1) + L" was not found"; return false; }
        if (!se_fx_export(add, "Maps/" + st, se_ofx[i], 0, files, err)) { se_cur = i; return false; }
    }
    return true;
}

// ---------------- creation ----------------
static void se_register() {
    static bool reg = false;
    if (reg) return;
    WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = se_img_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTStageImg";
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    RegisterClassW(&wc);
    wc.lpfnWndProc = se_grid_proc; wc.lpszClassName = L"TTTStageGrid"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    reg = true;
}
static void se_controls() {   // called by the mod creator while it builds page 2
    se_register();
    cm_ctl(L"BUTTON", L"+ New map", BS_OWNERDRAW | WS_TABSTOP, 16, 88, 110, 28, IDS_NEW);
    cm_ctl(L"BUTTON", L"Remove", BS_OWNERDRAW | WS_TABSTOP, 132, 88, 96, 28, IDS_DEL);
    cm_ctl(L"STATIC", L"", SS_ENDELLIPSIS, 16, 130, 300, 24, IDS_TITLE, theme_f_bold);
    cm_ctl(L"STATIC", L"", 0, 16, 156, 300, 34, IDS_SUB);
    cm_ctl(L"STATIC", L"ID", 0, 16, 200, 80, 18, IDS_ID_L);
    cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 104, 196, 208, 24, IDS_ID);
    cm_ctl(L"STATIC", L"Select images (click, or drop the PNGs)", 0, 16, 236, 300, 18, IDS_IMG_L);
    cm_ctl(L"TTTStageImg", L"", 0, 16, 258, 160, 84, IDS_BIG);
    cm_ctl(L"TTTStageImg", L"", 0, 184, 258, 90, 64, IDS_SMALL);
    static const wchar_t* LN[2] = {L"EN", L"ES"};
    for (int l = 0; l < 2; l++) {
        cm_ctl(L"STATIC", LN[l], SS_CENTERIMAGE, 16, 352 + 34 * l, 28, 30, IDS_NL0 + l);
        cm_ctl(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 48, 355 + 34 * l, 268, 24, IDS_NB0 + l);
    }
    static const wchar_t* FXN[3] = {L"Heat haze", L"Bubbles", L"Storm"};
    for (int k = 0; k < 3; k++) cm_ctl(L"BUTTON", FXN[k], BS_OWNERDRAW | WS_TABSTOP, 16 + 102 * k, 424, 96, 28, IDS_FX0 + k);
    cm_ctl(L"STATIC", L"Files: click or drop, right click removes", 0, 16, 464, 300, 18, IDS_F_L);
    cm_ctl(L"BUTTON", L"Map", BS_OWNERDRAW | WS_TABSTOP, 16, 486, 146, 28, IDS_MAP);
    cm_ctl(L"BUTTON", L"SFX", BS_OWNERDRAW | WS_TABSTOP, 168, 486, 146, 28, IDS_SFX);
    cm_ctl(L"STATIC", L"SFX of", 0, 16, 528, 80, 18, IDS_SFX_L);
    HWND sc = cm_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 100, 524, 216, 320, IDS_SFXOF);
    theme_control(sc, L"DarkMode_CFD");
    for (int k = 0; k < stages::OLD; k++) SendMessageW(sc, CB_ADDSTRING, 0, (LPARAM)mw_w(stages::NAMES[k]).c_str());
    cm_ctl(L"BUTTON", L"Tournament ring", BS_OWNERDRAW | WS_TABSTOP, 16, 560, 146, 28, IDS_TOUR);
    cm_ctl(L"STATIC", L"", 0, 16, 598, 300, 36, IDS_INFO);
    se_grid = CreateWindowExW(0, L"TTTStageGrid", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP, E(332), E(130), E(612), E(470), cm, (HMENU)(INT_PTR)IDS_GRID, GetModuleHandle(nullptr), nullptr);
    theme_control(se_grid);
    cm_page[2].push_back(se_grid);
    cm_ctl(L"STATIC", L"", SS_ENDELLIPSIS, 332, 608, 612, 22, IDS_SUM);
    se_renders_scan();
    se_reset();
}
static void se_close() { se_bitmaps_drop(); se_grid = nullptr; for (int id : {(int)IDS_MAP, (int)IDS_SFX, (int)IDS_TOUR, (int)IDS_FX0, (int)IDS_FX1, (int)IDS_FX2}) theme_primary.erase(GetDlgItem(cm, id)); }
