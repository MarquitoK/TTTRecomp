#pragma once
// Roster tab of the mod creator (item 29.3 stage 2, his ttt_casillas.pyw editor): cells moved by dragging, a cell dropped
// on the right panel becomes a transformation; new characters (ID = folder in Packfile/Characters, BT3 id, base character,
// names, select images, colour/SFX/voice files) and own names for the originals. Written into the .mkpak as Roster/ and
// Packfile/Characters/<ID>/
#include "disc/roster_def.h"
#include <array>
#include <functional>
#include <windowsx.h>
void psp_roster_vanilla(std::vector<std::vector<int>>& cells, std::vector<uint32_t>& name_ids, std::vector<int>& sys_of_menu);
std::string psp_assets_dir();
std::string psp_shared_rel(const std::string& legacy);
std::string psp_system_text(int lang, uint32_t id);
bool psp_roster_label(int lang, int kind, int menu, std::vector<uint32_t>& px, int* w, int* h);
std::string psp_colour_folder(int k, int j);
std::string psp_original_path(int k, int kind);
int psp_voice_packs(std::vector<std::string>* names, std::vector<std::string>* ids, int lang);
int psp_voice_codes(std::vector<std::string>* codes);
bool psp_original_exists(int k, int kind);

enum { IDR_NEW = 3200, IDR_COPY, IDR_FORM, IDR_DEL, IDR_MOD_L, IDR_MOD, IDR_RESET, IDR_TITLE, IDR_SUB, IDR_ID_L, IDR_ID, IDR_SYS_L, IDR_SYS,
       IDR_BASE_L, IDR_BASE, IDR_N_L0, IDR_N_L1, IDR_N_L2, IDR_N0, IDR_N1, IDR_N2, IDR_IMG_L, IDR_IMG0, IDR_IMG1, IDR_IMG2, IDR_IMG3,
       IDR_F_L, IDR_F0, IDR_F1, IDR_F2, IDR_F3, IDR_F4, IDR_F5, IDR_F6, IDR_F7, IDR_D_L, IDR_D0, IDR_D1, IDR_D2, IDR_D3, IDR_T0, IDR_T1, IDR_T2, IDR_T3, IDR_CPREV, IDR_CNAME, IDR_CNEXT, IDR_LANG0, IDR_LANG1, IDR_LANG2, IDR_LANG3, IDR_INFO, IDR_GRID, IDR_FORMS_L, IDR_FORMS, IDR_SUM,
       IDR_PG0, IDR_PG1, IDR_LT0, IDR_LT1, IDR_LT2, IDR_LN0, IDR_LB0 = IDR_LN0 + 9, IDR_LHELP = IDR_LB0 + 9, IDR_FTL, IDR_FTC, IDR_FTN0, IDR_FTN1, IDR_FT0, IDR_FT1, IDR_PCM, IDR_LAST };   // label boxes: kind * 3 + lang (name, form right, form left; en, es)

struct ReChar { bool pcm = false; int sysid = -1, donor = 3; std::string id, name[3], img[4], file[14], bt3[10], dmg[10], tex1[10]; std::map<std::string, std::string> lang; };   // tex1: level 1 damage textures per colour ("on" = made on export, "mod:" = on disk)   // dmg: damaged costume per colour ("ttt:", "bt3:" or "dir:")   // file: "ttt:" pak or "dir:" folder; bt3: BT3 paks over a folder (\n)
struct ReOrig { std::string name[3]; int newid = -1; };   // an original character: own names, own BT3 id
struct ReItem { int sys, kind; };
struct ReGrid { HWND h = nullptr; int mode = 0, cols = 4, scroll = 0, sel = -1, marker = -1; std::vector<ReItem> items; int locked = -1; int cw = 118, ch = 98, iw = 110, ih = 62; };   // mode 2 picker, 3 label picker
// select screen labels per cell: [kind][lang] kind 0 name, 1 form right, 2 form left; lang 0 en, 1 es, 2 unused, 3 all
struct ReLab { int ref[3][4]; std::string img[3][4]; ReLab() { for (auto& a : ref) for (int& v : a) v = -2; } };

static const int RE_CW = 118, RE_CH = 98, RE_IW = 110, RE_IH = 62, RE_GAP = 6, RE_NEWITEM = -2;
static const COLORREF RE_COL[4] = {RGB(0xE0, 0x8A, 0x2B), RGB(0x8E, 0x5B, 0xD6), RGB(0x3B, 0x8E, 0xE0), RGB(0x4C, 0xC3, 0x8A)};
static const COLORREF RE_DARK[4] = {RGB(0x5A, 0x38, 0x12), RGB(0x3A, 0x25, 0x60), RGB(0x17, 0x3A, 0x5C), RGB(0x17, 0x42, 0x2F)};
enum { RK_ORIG, RK_RANDOM, RK_NEW, RK_COPY };
static const char* RE_IMG[4] = {"small", "mid", "name", "big"};
static const int RE_IMG_W[4] = {46, 68, 256, 264}, RE_IMG_H[4] = {30, 48, 22, 172};
static const wchar_t* RE_IMG_N[4] = {L"Small", L"Mid", L"Name", L"Big"};
// file index: 0..9 colours 1..10, 10 SFX, 11 voices, 12 phase 2 charge aura, 13 phase 1 charge animations (in + loop); one colour is edited at a time (re_col, the selector)
static const int RE_COLS = 10, RF_SFX = 10, RF_VOICES = 11, RF_AURA = 12, RF_CHG1 = 13, RF_N = 14;
static int re_col = 0;
static bool re_lang_shown = false;
static std::wstring RE_FILE_N(int j) { return j < RE_COLS ? L"Color " + std::to_wstring(j + 1) : j == RF_SFX ? L"SFX" : j == RF_VOICES ? L"Voices" : j == RF_AURA ? L"Charge aura" : L"Charge anims"; }
static const char* RE_CHG_IN = "phase1_in.tanm", *RE_CHG_LOOP = "phase1.tanm", *RE_CHG_AURA = "phase2_aura.pak";
static bool re_chg_on() { const char* e = getenv("TTT_CHARGE2"); return e && *e == '1'; }
static std::wstring RE_DMG_N(int j) { return L"Color " + std::to_wstring(j + 1) + L" damaged"; }
static std::wstring RE_TEX1_N(int j) { return L"Color " + std::to_wstring(j + 1) + L" damaged textures"; }
static int re_disc_kind(int j) { return j < RE_COLS ? j : j - 6; }   // psp_original_path kinds: 4 SFX, 5 voices
std::string psp_colour_folder(int k, int j);
bool psp_colour_exists(int k, int j);
static std::string re_chg_dir(int k) { std::string f = psp_original_path(k, 4); size_t s = f.find_last_of('/'); return s == std::string::npos ? "" : f.substr(0, s) + "/charge"; }
static std::string re_orig_path(int k, int j) {
    if (j == RF_AURA || j == RF_CHG1) { std::string d = re_chg_dir(k); return d.empty() ? d : d + "/" + (j == RF_AURA ? RE_CHG_AURA : RE_CHG_IN); }
    return j < RE_COLS ? psp_colour_folder(k, j) : psp_original_path(k, re_disc_kind(j));
}
static bool re_orig_exists(int k, int j) { return j >= RF_AURA ? false : j < RE_COLS ? psp_colour_exists(k, j) : psp_original_exists(k, re_disc_kind(j)); }
static const char* RE_DMG_TTT[11] = {"002_model.pmdl", "003_face_1.pmdl", "004_face_2.pmdl", "005_face_3.pmdl", "006_face_4.pmdl", "007_face_5.pmdl",
                                     "008_face_6.pmdl", "009_face_7.pmdl", "010_face_8.pmdl", "011_textures.atex", "012_face_blink.atex"};

static ReGrid re_main, re_forms, re_pick;
static std::vector<std::vector<int>> re_cells, re_vanilla;
static std::vector<ReChar> re_chars;
static std::map<int, ReOrig> re_orig;
static std::map<int, std::array<std::string, 2>> re_ftab;   // transformation texts (names as text): id < 100 a game row given another text, >= 100 new
static std::map<int, int> re_fref;   // character -> transformation text id it was given (-1 none)
int psp_form_row(int menu);
static bool re_text_names() { const char* e = getenv("TTT_TEXT_NAMES"); return e && atoi(e) > 0; }
static std::vector<uint32_t> re_name_ids;
static std::vector<std::string> re_bt3;
static std::map<int, std::string> re_render;
static std::map<long long, HBITMAP> re_bmp;
static std::set<int> re_vbase;
static std::map<int, ReLab> re_lab;
static std::map<int, ReChar> re_ochar;   // files of original characters (colours, SFX, voices, damaged); "mod:" = already in the edited mod
static std::set<std::string> re_gone;   // files of the edited mod cleared with right click: left out when it is saved
static std::string re_norm(std::string s) { for (auto& ch : s) ch = ch == '\\' ? '/' : (char)tolower((unsigned char)ch); while (!s.empty() && s.back() == '/') s.pop_back(); return s; }
static std::set<std::string> re_kept;   // "mod:" paths still used: a source under one stays even inside a gone folder
static std::map<int, ReChar> re_ochar_load;   // originals' slots as loaded from the edited mod
static bool re_dropped(const std::string& src) {
    if (re_gone.empty()) return false;
    std::string l = re_norm(src);
    auto under = [&](const std::set<std::string>& set) { size_t best = 0; bool hit = false; for (auto& d : set) if (l == d || (l.size() > d.size() && !l.compare(0, d.size(), d) && l[d.size()] == '/')) { hit = true; best = std::max(best, d.size() + 1); } return hit ? best : 0; };
    size_t g = under(re_gone);
    return g && g > under(re_kept);
}
static void re_drop_mod(const std::string& v) {
    for (size_t a = 4; a < v.size();) { size_t b = v.find('\n', a); std::string p = v.substr(a, b == std::string::npos ? std::string::npos : b - a); re_gone.insert(re_norm(p)); re_gone.insert(re_norm(p + ".pak")); a = b == std::string::npos ? v.size() : b + 1; }
}
static int re_page = 0, re_pick_k = 0, re_pick_l = 0;
static const int RE_L_IMPORT = -20, RE_L_DEFAULT = -21, RE_L_NONE = -22;
static int re_cur = -1, re_form = -1, re_pick_choice = -3;
static bool re_layout_set = false, re_filling = false, re_ready = false, re_pick_open = false;
static std::string re_mod_src, re_mod_root;
static HWND re_pick_w = nullptr;
static struct { ReGrid* src = nullptr; int idx = -1; bool on = false; POINT p0{}; ReGrid* tgt = nullptr; } re_drag;

static bool re_vanilla_id(int s) { return roster::vanilla(s); }
static ReChar* re_find(int s) { for (auto& c : re_chars) if (c.sysid == s) return &c; return nullptr; }
static std::string re_bt3_name(int s) {
    if (s < 0 || s >= (int)re_bt3.size()) return "BT3 " + std::to_string(s);
    std::string n = re_bt3[s];
    size_t u = n.find('_');
    if (u != std::string::npos && u <= 3) n = n.substr(u + 1);
    for (auto& ch : n) if (ch == '_') ch = ' ';
    return n;
}
static int re_parent(int s) {   // base of the cell where s is a transformation, -1 when s is a base
    for (auto& c : re_cells) for (size_t j = 1; j < c.size(); j++) if (c[j] == s) return c[0];
    return -1;
}
static std::string re_inherit(int s, int l);
static std::string re_label(int s) {
    if (s == -1) return "Random";
    if (s == RE_NEWITEM) return "+ New character";
    if (ReChar* c = re_find(s)) return !c->name[0].empty() ? c->name[0] : re_parent(s) >= 0 ? re_inherit(s, 0) : c->id;
    auto it = re_orig.find(s);
    if (it != re_orig.end() && !it->second.name[0].empty()) return it->second.name[0];
    return re_bt3_name(s);
}
static std::vector<int> re_sys_of_menu;   // original menu number -> BT3 id
static int re_menu(int s) { for (size_t m = 0; m < re_sys_of_menu.size(); m++) if (re_sys_of_menu[m] == s) return (int)m; return -1; }

static bool re_ends(const std::string& s, const char* f) { size_t n = strlen(f); return s.size() >= n && s.compare(s.size() - n, n, f) == 0; }

// ---------------- images ----------------
static bool re_png(const std::string& f, int* w, int* h, std::vector<uint32_t>& px) {
    std::vector<uint8_t> d; std::string err;
    FILE* fp = mods_fopen(f, "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    d.resize(n > 0 ? n : 0);
    bool ok = n > 0 && fread(d.data(), 1, n, fp) == (size_t)n;
    fclose(fp);
    return ok && png_decode(d, w, h, px, &err);
}
static void re_renders_scan() {
    re_render.clear();
    std::string dir = psp_assets_dir() + "/Shared/" + psp_shared_rel("Menus/char_select/renders");
    std::error_code ec;
    for (auto& p : std::filesystem::directory_iterator(std::filesystem::u8path(dir), ec)) {
        std::string n = p.path().filename().u8string();
        if (!isdigit((unsigned char)n[0])) continue;
        int num = atoi(n.c_str());
        std::string f;
        if (p.is_directory(ec)) {
            std::vector<std::string> pngs;
            for (auto& q : std::filesystem::directory_iterator(p.path(), ec)) { std::string m = q.path().filename().u8string(); if (re_ends(m, ".png")) pngs.push_back(q.path().u8string()); }
            std::sort(pngs.begin(), pngs.end());
            if (!pngs.empty()) f = pngs[0];
        } else if (re_ends(n, ".png")) f = p.path().u8string();
        if (!f.empty() && !re_render.count(num)) re_render[num] = f;
    }
}
static std::string re_image_src(int s) {
    if (s == RE_NEWITEM) return "";
    if (ReChar* c = re_find(s)) { for (int k : {3, 1, 0}) if (!c->img[k].empty()) return c->img[k]; return ""; }
    int m = s == -1 ? 0x46 : re_menu(s);
    auto it = m < 0 ? re_render.end() : re_render.find(184 + m);
    return it == re_render.end() ? "" : it->second;
}
static HBITMAP re_scaled_px(const std::vector<uint32_t>& px, int w, int h, int W, int H, COLORREF bg);
static HBITMAP re_scaled(const std::string& src, int W, int H, COLORREF bg) {   // the image fitted in WxH over bg
    std::vector<uint32_t> px; int w = 0, h = 0;
    if (src.empty() || !re_png(src, &w, &h, px)) { px.clear(); w = h = 0; }
    return re_scaled_px(px, w, h, W, H, bg);
}
static HBITMAP re_scaled_px(const std::vector<uint32_t>& px, int w, int h, int W, int H, COLORREF bg) {
    BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth = W; bi.bmiHeader.biHeight = -H; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp) return nullptr;
    uint32_t* d = (uint32_t*)bits;
    uint32_t fr = GetRValue(bg), fg = GetGValue(bg), fb = GetBValue(bg);
    for (int i = 0; i < W * H; i++) d[i] = (fr << 16) | (fg << 8) | fb;
    if (w > 0 && h > 0 && px.size() >= (size_t)w * h) {
        double sc = std::min((double)W / w, (double)H / h);
        int dw = std::max(1, (int)(w * sc)), dh = std::max(1, (int)(h * sc)), x0 = (W - dw) / 2, y0 = (H - dh) / 2;
        for (int y = 0; y < dh; y++) {
            int sy0 = y * h / dh, sy1 = std::max(sy0 + 1, (y + 1) * h / dh);
            for (int x = 0; x < dw; x++) {
                int sx0 = x * w / dw, sx1 = std::max(sx0 + 1, (x + 1) * w / dw);
                uint64_t r = 0, g = 0, b = 0, a = 0, n = 0;
                for (int sy = sy0; sy < sy1; sy++) for (int sx = sx0; sx < sx1; sx++) {
                    uint32_t p = px[(size_t)sy * w + sx], pa = p >> 24;
                    r += (p & 255) * pa; g += ((p >> 8) & 255) * pa; b += ((p >> 16) & 255) * pa; a += pa; n++;
                }
                uint32_t A = (uint32_t)(a / n), R = a ? (uint32_t)(r / a) : 0, G = a ? (uint32_t)(g / a) : 0, B = a ? (uint32_t)(b / a) : 0;
                R = (R * A + fr * (255 - A)) / 255; G = (G * A + fg * (255 - A)) / 255; B = (B * A + fb * (255 - A)) / 255;
                d[(y0 + y) * W + x0 + x] = (R << 16) | (G << 8) | B;
            }
        }
    }
    return bmp;
}
static HBITMAP re_bitmap(int s, int kind) {
    long long key = ((long long)(s + 8) << 8) | kind;
    auto it = re_bmp.find(key);
    if (it != re_bmp.end()) return it->second;
    HBITMAP bmp = re_scaled(re_image_src(s), E(RE_IW), E(RE_IH), RE_DARK[kind]);
    if (bmp) re_bmp[key] = bmp;
    return bmp;
}
static void re_bitmaps_drop(int s) {
    for (auto it = re_bmp.begin(); it != re_bmp.end();) {
        if (s == -100 || (it->first >> 8) == s + 8) { DeleteObject(it->second); it = re_bmp.erase(it); }
        else ++it;
    }
}

// ---------------- select labels ----------------
static int re_selected();
static const char* RE_LANG_C[3] = {"en", "es", ""};   // slot 2 (the game's French) is never read or written
static const char* RE_LAB_FILE[3] = {"name", "form_right", "form_left"};
static const char* RE_LAB_KEY[3] = {"name_image", "form_right", "form_left"};
static std::string re_limg(int s, int k, int l) {   // a new character's all-languages name image is its Name select image
    ReChar* c = re_find(s);
    if (c && k == 0 && l == 3) return c->img[2];
    auto it = re_lab.find(s);
    return it == re_lab.end() ? "" : it->second.img[k][l];
}
static void re_limg_set(int s, int k, int l, const std::string& f) {
    ReChar* c = re_find(s);
    if (c && k == 0 && l == 3) c->img[2] = f; else re_lab[s].img[k][l] = f;
}
static bool re_lab_any(int s) {
    auto it = re_lab.find(s);
    if (it == re_lab.end()) return false;
    for (int k = 0; k < 3; k++) for (int l = 0; l < 4; l++) if (it->second.ref[k][l] != -2 || !it->second.img[k][l].empty()) return true;
    return false;
}
struct ReLabView { std::string file; int menu = -1; bool none = false, own = false; std::string how; };
static ReLabView re_lab_view(int s, int k, int l, int depth = 0) {   // l 3 = what "All" sets
    ReLabView v;
    if (s < 0 || depth > 8) return v;
    ReChar* c = re_find(s);
    for (int q : {l, 3}) {
        std::string f = re_limg(s, k, q);
        if (!f.empty()) { v.file = f; v.own = depth == 0; v.how = q == 3 && l != 3 ? "Own image (All)" : "Own image"; return v; }
    }
    auto it = re_lab.find(s);
    int ref = it == re_lab.end() ? -2 : it->second.ref[k][l] != -2 ? it->second.ref[k][l] : it->second.ref[k][3];
    if (ref == -1) { v.none = true; v.own = depth == 0; v.how = "Empty"; return v; }
    int from = ref >= 0 ? ref : k == 0 && c ? re_parent(s) : -1;
    if (from >= 0) {
        ReLabView t = re_lab_view(from, k, l, depth + 1);
        if (t.file.empty() && t.menu < 0 && !t.none && !re_find(from)) t.menu = re_menu(from);
        t.own = ref >= 0 && depth == 0;
        t.how = (ref >= 0 ? "Same as " : "Inherited from ") + re_label(from);
        return t;
    }
    v.menu = c ? 0 : re_menu(s);   // a new number without labels copies number 0 (Goku)
    v.how = c ? "Goku's" : "Game";
    return v;
}
static bool re_lab_px(int s, int k, int l, std::vector<uint32_t>& px, int* w, int* h, ReLabView* out = nullptr) {
    ReLabView v = re_lab_view(s, k, l);
    if (out) *out = v;
    if (!v.file.empty()) return re_png(v.file, w, h, px);
    if (v.menu >= 0) return psp_roster_label(l == 3 ? 0 : l, k, v.menu, px, w, h);
    return false;
}
static HBITMAP re_lab_bitmap(int s, int k, int l, int W, int H, COLORREF bg) {
    long long key = ((long long)(s + 8) << 8) | (16 + k * 4 + l);
    auto it = re_bmp.find(key);
    if (it != re_bmp.end()) return it->second;
    std::vector<uint32_t> px; int w = 0, h = 0;
    if (!re_lab_px(s, k, l, px, &w, &h)) { px.clear(); w = h = 0; }
    HBITMAP bmp = re_scaled_px(px, w, h, E(W), E(H), bg);
    if (bmp) re_bmp[key] = bmp;
    return bmp;
}
static void re_lab_bitmaps_drop() {
    for (auto it = re_bmp.begin(); it != re_bmp.end();) {
        if ((it->first & 0xFF) >= 16) { DeleteObject(it->second); it = re_bmp.erase(it); }
        else ++it;
    }
}
static std::wstring re_lab_special(int v) {
    if (v == RE_L_IMPORT) return L"Import PNG...";
    if (v == RE_L_NONE) return L"Empty";
    int s = re_selected();
    if (re_pick_k == 0 && re_find(s) && re_parent(s) >= 0) return L"Inherit from " + mw_w(re_label(re_parent(s)));
    return re_find(s) ? L"Default" : L"Game's own";
}

// ---------------- model ----------------
static int re_kind(int s, bool main) {
    if (s == -1) return RK_RANDOM;
    if (s == RE_NEWITEM || re_find(s)) return RK_NEW;
    if (!main) return RK_ORIG;
    int n = 0;
    for (auto& c : re_cells) n += c[0] == s;
    return !re_vbase.count(s) || n > 1 ? RK_COPY : RK_ORIG;
}
static int re_selected() {
    if (re_cur < 0 || re_cur >= (int)re_cells.size()) return -3;
    auto& c = re_cells[re_cur];
    return re_form >= 0 && re_form + 1 < (int)c.size() ? c[re_form + 1] : c[0];
}
static bool re_used(int s) { for (auto& c : re_cells) for (int v : c) if (v == s) return true; return false; }
static int re_final(int s) { auto it = re_orig.find(s); return it != re_orig.end() && it->second.newid >= 0 ? it->second.newid : s; }   // BT3 id written to the file
static bool re_id_taken(int i, int self) {   // a BT3 id held by an original, its own id or a new character
    if (i == self) return false;
    if (roster::vanilla(i)) return true;
    if (re_find(i)) return true;
    for (auto& o : re_orig) if (o.second.newid == i && o.first != self) return true;
    return false;
}
static int re_free_id(int skip = -1) {
    for (int i = 0; i < roster::SYS; i++) if (!re_id_taken(i, skip)) return i;   // in order: the first free BT3 id
    return -1;
}
static void re_default_cells() {
    re_cells = re_vanilla;
    for (auto& c : re_chars) if (!re_used(c.sysid)) re_cells.insert(re_cells.end() - 1, std::vector<int>{c.sysid});
}
static bool re_lab_any(int s);
static bool re_has_content() {
    bool lab = false, own = false;
    for (auto& x : re_lab) lab = lab || re_lab_any(x.first);
    for (auto& o : re_ochar) {   // files of originals (colours, SFX, voices, damaged, level 1 textures)
        own = own || o.second.pcm;
        for (auto& v : o.second.file) own = own || !v.empty();
        for (int j = 0; j < RE_COLS; j++) own = own || !o.second.dmg[j].empty() || !o.second.tex1[j].empty();
        for (auto& l : o.second.lang) own = own || !l.second.empty();
    }
    return !re_chars.empty() || !re_orig.empty() || re_layout_set || lab || own;
}
static std::string re_form_keys(int s) { auto f = re_fref.find(s); return f == re_fref.end() ? "" : "form = " + (f->second < 0 ? std::string("none") : std::to_string(f->second)) + "\n"; }
static int re_form_eff(int s, int depth = 0) {   // the text id a character shows: given, an original's own row, a transformation's base's
    auto f = re_fref.find(s);
    if (f != re_fref.end()) return f->second;
    if (roster::vanilla(s)) return psp_form_row(roster::rank_vanilla(s));
    int p = depth < 8 ? re_parent(s) : -1;
    return p >= 0 ? re_form_eff(p, depth + 1) : -2;
}
static std::string re_form_txt(int id, int l) {
    auto it = re_ftab.find(id);
    if (it != re_ftab.end() && !it->second[l].empty()) return it->second[l];
    auto& r = roster::form_rows();
    return id >= 0 && id < (int)r.size() ? r[id][l] : "";
}
static void re_form_fill(int s) {   // the list of texts (the game's rows and the new ones) and the chosen one's EN / ES
    HWND cb = GetDlgItem(cm, IDR_FTC);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    int cur = s >= 0 ? re_form_eff(s) : -2, sel = -1;
    auto add = [&](int id, const std::wstring& t) { int i = (int)SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)t.c_str()); SendMessageW(cb, CB_SETITEMDATA, i, (LPARAM)id); if (id == cur) sel = i; };
    add(-1, L"None");
    auto& r = roster::form_rows();
    for (int i = 0; i < (int)r.size(); i++) if (!r[i][0].empty() || !r[i][1].empty()) add(i, mw_w(re_form_txt(i, 0) + "  /  " + re_form_txt(i, 1)));
    for (auto& e : re_ftab) if (e.first >= roster::FORM_NEW) add(e.first, mw_w((e.second[0].empty() ? std::string("(new)") : e.second[0]) + "  /  " + e.second[1]));
    add(-9, L"+ New text");
    SendMessageW(cb, CB_SETCURSEL, sel, 0);
    for (int l = 0; l < 2; l++) { SetWindowTextW(GetDlgItem(cm, IDR_FT0 + l), cur >= 0 ? mw_w(re_form_txt(cur, l)).c_str() : L""); EnableWindow(GetDlgItem(cm, IDR_FT0 + l), cur >= 0); }
}
static bool re_has_roster() {   // something only roster.txt can say (originals' files alone travel as Packfile content)
    bool lab = false;
    for (auto& x : re_lab) lab = lab || re_lab_any(x.first);
    return !re_chars.empty() || !re_orig.empty() || re_layout_set || lab || !re_ftab.empty() || !re_fref.empty();
}
static void re_drop_unused() {   // a new character without any cell is gone (the game would add it again before Random)
    for (size_t i = 0; i < re_chars.size();) {
        if (re_used(re_chars[i].sysid)) { i++; continue; }
        cm_notify(L"Removed " + mw_w(re_chars[i].id));
        re_bitmaps_drop(re_chars[i].sysid);
        re_lab.erase(re_chars[i].sysid);
        re_chars.erase(re_chars.begin() + i);
    }
}
static void re_rebuild();
static void re_panel();
static void re_lang_panel(int s, bool on);
static std::string* re_tex1_of(int s, int j);
static std::set<std::string> re_tex1_new;   // level 1 textures made or dropped in this session
static void re_tex1_sync(int s);

// ---------------- grid control ----------------
static int re_rows(const ReGrid& g) { return ((int)g.items.size() + g.cols - 1) / g.cols; }
static RECT re_cell_rect(const ReGrid& g, int i) {
    int col = i % g.cols, row = i / g.cols;
    RECT r; r.left = E(RE_GAP + col * (g.cw + RE_GAP)); r.top = E(RE_GAP + row * (g.ch + RE_GAP)) - g.scroll;
    r.right = r.left + E(g.cw); r.bottom = r.top + E(g.ch);
    return r;
}
static void re_scrollbar(ReGrid& g) {
    RECT rc; GetClientRect(g.h, &rc);
    int total = E(RE_GAP + re_rows(g) * (g.ch + RE_GAP)), page = rc.bottom;
    g.scroll = std::max(0, std::min(g.scroll, total - page));
    SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS; si.nMin = 0; si.nMax = std::max(0, total - 1); si.nPage = page; si.nPos = g.scroll;
    SetScrollInfo(g.h, SB_VERT, &si, TRUE);
}
static void re_scroll_to(ReGrid& g, int i) {
    if (i < 0) return;
    RECT rc; GetClientRect(g.h, &rc);
    RECT r = re_cell_rect(g, i);
    if (r.top < 0) g.scroll += r.top - E(RE_GAP);
    else if (r.bottom > rc.bottom) g.scroll += r.bottom - rc.bottom + E(RE_GAP);
    re_scrollbar(g);
}
static int re_hit(const ReGrid& g, POINT p) {
    for (int i = 0; i < (int)g.items.size(); i++) { RECT r = re_cell_rect(g, i); if (PtInRect(&r, p)) return i; }
    return -1;
}
static int re_slot(const ReGrid& g, POINT p) {   // the cell under the point takes the dragged one; past the last cell = the end
    int col = std::max(0, std::min(g.cols - 1, (int)(p.x / E(g.cw + RE_GAP))));
    int row = std::max(0, (int)((p.y + g.scroll) / E(g.ch + RE_GAP)));
    return std::max(0, std::min(row * g.cols + col, (int)g.items.size()));
}
static void re_paint(ReGrid& g) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(g.h, &ps);
    RECT rc; GetClientRect(g.h, &rc);
    HDC m = CreateCompatibleDC(dc);
    HBITMAP bb = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
    HGDIOBJ ob = SelectObject(m, bb);
    FillRect(m, &rc, theme_b_field);
    HDC im = CreateCompatibleDC(dc);
    SetBkMode(m, TRANSPARENT);
    for (int i = 0; i < (int)g.items.size(); i++) {
        RECT r = re_cell_rect(g, i);
        if (r.bottom < 0 || r.top > rc.bottom) continue;
        const ReItem& it = g.items[i];
        bool sel = i == g.sel && g.mode < 2;
        HBRUSH b = CreateSolidBrush(RE_DARK[it.kind]);
        HPEN pen = CreatePen(PS_SOLID, sel ? E(3) : E(2), sel ? THEME_TEXT : RE_COL[it.kind]);
        HGDIOBJ o1 = SelectObject(m, b), o2 = SelectObject(m, pen);
        RoundRect(m, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, E(12), E(12));
        SelectObject(m, o1); SelectObject(m, o2); DeleteObject(b); DeleteObject(pen);
        HBITMAP bmp = it.sys == RE_NEWITEM || it.sys <= RE_L_IMPORT ? nullptr : g.mode == 3 ? re_lab_bitmap(it.sys, re_pick_k, re_pick_l, g.iw, g.ih, RE_DARK[it.kind]) : re_bitmap(it.sys, it.kind);
        RECT tr = {r.left + E(4), r.top + E(g.ih + 6), r.right - E(4), r.bottom - E(4)};
        if (bmp) {
            HGDIOBJ ob2 = SelectObject(im, bmp);
            BitBlt(m, r.left + E(4), r.top + E(4), E(g.iw), E(g.ih), im, 0, 0, SRCCOPY);
            SelectObject(im, ob2);
        } else tr.top = r.top;
        std::wstring t = it.sys <= RE_L_IMPORT ? re_lab_special(it.sys) : g.mode == 3 && re_pick_k ? L"" : mw_w(re_label(it.sys));
        if (g.mode == 0 && i < (int)re_cells.size() && re_cells[i].size() > 1) t += L"  +" + std::to_wstring(re_cells[i].size() - 1);
        SetTextColor(m, it.sys == RE_NEWITEM ? RE_COL[RK_NEW] : THEME_TEXT);
        HGDIOBJ of = SelectObject(m, theme_f_normal);
        DrawTextW(m, t.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(m, of);
    }
    if (re_drag.on && re_drag.tgt == &g && g.marker >= 0 && re_drag.src && re_drag.idx < (int)re_drag.src->items.size()) {   // ghost over the whole target cell
        RECT r = re_cell_rect(g, g.marker);
        const ReItem& it = re_drag.src->items[re_drag.idx];
        HBRUSH b = CreateSolidBrush(RE_DARK[it.kind]);
        HPEN pen = CreatePen(PS_SOLID, E(3), THEME_ACCENT);
        HGDIOBJ o1 = SelectObject(m, b), o2 = SelectObject(m, pen);
        RoundRect(m, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1, E(12), E(12));
        SelectObject(m, o1); SelectObject(m, o2); DeleteObject(b); DeleteObject(pen);
        if (HBITMAP bmp = re_bitmap(it.sys, it.kind)) {
            HGDIOBJ ob2 = SelectObject(im, bmp);
            BLENDFUNCTION bf = {AC_SRC_OVER, 0, 150, 0};
            GdiAlphaBlend(m, r.left + E(4), r.top + E(4), E(RE_IW), E(RE_IH), im, 0, 0, E(RE_IW), E(RE_IH), bf);
            SelectObject(im, ob2);
        }
        RECT tr = {r.left + E(4), r.top + E(RE_IH + 6), r.right - E(4), r.bottom - E(4)};
        SetTextColor(m, THEME_ACCENT);
        HGDIOBJ of = SelectObject(m, theme_f_normal);
        DrawTextW(m, mw_w(re_label(it.sys)).c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(m, of);
    }
    DeleteDC(im);
    BitBlt(dc, 0, 0, rc.right, rc.bottom, m, 0, 0, SRCCOPY);
    SelectObject(m, ob); DeleteObject(bb); DeleteDC(m);
    EndPaint(g.h, &ps);
}
static ReGrid* re_grid_at(POINT screen) {
    for (ReGrid* g : {&re_main, &re_forms}) {
        if (!g->h || !IsWindowVisible(g->h)) continue;
        RECT r; GetWindowRect(g->h, &r);
        if (PtInRect(&r, screen)) return g;
    }
    return nullptr;
}
static void re_select(ReGrid& g, int i);
static void re_drop(ReGrid* src, int idx, ReGrid* tgt, int dst);
static void re_remove();

static LRESULT CALLBACK re_grid_proc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    ReGrid* g = (ReGrid*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (msg == WM_NCCREATE) { SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams); return DefWindowProcW(h, msg, w, l); }
    if (!g) return DefWindowProcW(h, msg, w, l);
    switch (msg) {
        case WM_PAINT: re_paint(*g); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_SIZE: re_scrollbar(*g); return 0;
        case WM_GETDLGCODE: return DLGC_WANTARROWS | (l && ((MSG*)l)->message == WM_KEYDOWN && ((MSG*)l)->wParam == VK_DELETE ? DLGC_WANTMESSAGE : 0);
        case WM_MOUSEWHEEL: g->scroll -= GET_WHEEL_DELTA_WPARAM(w) * E(g->ch + RE_GAP) / 240; re_scrollbar(*g); InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_VSCROLL: {
            SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_ALL; GetScrollInfo(h, SB_VERT, &si);
            int step = E(g->ch + RE_GAP), v = g->scroll;
            switch (LOWORD(w)) {
                case SB_LINEUP: v -= step / 2; break; case SB_LINEDOWN: v += step / 2; break;
                case SB_PAGEUP: v -= (int)si.nPage; break; case SB_PAGEDOWN: v += (int)si.nPage; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: v = si.nTrackPos; break;
            }
            g->scroll = v; re_scrollbar(*g); InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            SetFocus(h);
            POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            int i = re_hit(*g, p);
            if (g->mode >= 2) { if (i >= 0) { re_pick_choice = g->items[i].sys; re_pick_open = false; } return 0; }
            if (i < 0) return 0;
            re_drag.src = g; re_drag.idx = i; re_drag.on = false; GetCursorPos(&re_drag.p0); re_drag.tgt = nullptr;
            SetCapture(h);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (GetCapture() != h || re_drag.src != g) return 0;
            POINT s; GetCursorPos(&s);
            if (!re_drag.on && (re_drag.idx == g->locked || abs(s.x - re_drag.p0.x) + abs(s.y - re_drag.p0.y) < E(6))) return 0;
            re_drag.on = true;
            SetCursor(LoadCursor(nullptr, IDC_SIZEALL));
            ReGrid* t = re_grid_at(s);
            if (re_drag.tgt && re_drag.tgt != t) { re_drag.tgt->marker = -1; InvalidateRect(re_drag.tgt->h, nullptr, FALSE); }
            re_drag.tgt = t;
            if (t) {
                POINT c = s; ScreenToClient(t->h, &c);
                RECT rc; GetClientRect(t->h, &rc);
                if (c.y < E(24)) { t->scroll -= E(12); re_scrollbar(*t); }
                else if (c.y > rc.bottom - E(24)) { t->scroll += E(12); re_scrollbar(*t); }
                int slot = re_slot(*t, c);
                if (t->locked >= 0) slot = std::min(slot, t->locked);
                t->marker = slot;
                InvalidateRect(t->h, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: case WM_CAPTURECHANGED: {
            if (re_drag.src != g) return 0;
            bool on = re_drag.on && msg == WM_LBUTTONUP;
            ReGrid* t = re_drag.tgt; int idx = re_drag.idx, dst = t ? t->marker : -1;
            re_drag.src = nullptr; re_drag.on = false; re_drag.tgt = nullptr;
            if (t) { t->marker = -1; InvalidateRect(t->h, nullptr, FALSE); }
            bool click = msg == WM_LBUTTONUP && !on;
            if (msg == WM_LBUTTONUP && GetCapture() == h) ReleaseCapture();
            if (click && idx >= 0 && idx < (int)g->items.size()) re_select(*g, idx);
            else if (on && t && dst >= 0) re_drop(g, idx, t, dst);
            return 0;
        }
        case WM_KEYDOWN:
            if (w == VK_DELETE && g->mode < 2) { re_remove(); return 0; }
            if ((w == VK_LEFT || w == VK_RIGHT || w == VK_UP || w == VK_DOWN) && g->mode < 2 && !g->items.empty()) {
                int d = w == VK_LEFT ? -1 : w == VK_RIGHT ? 1 : w == VK_UP ? -g->cols : g->cols;
                int i = std::max(0, std::min((int)g->items.size() - 1, (g->sel < 0 ? 0 : g->sel + d)));
                re_select(*g, i); re_scroll_to(*g, i); InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            break;
    }
    return DefWindowProcW(h, msg, w, l);
}
static HWND re_grid_create(ReGrid& g, HWND parent, int x, int y, int w, int h, int id) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = re_grid_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRosterGrid";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = nullptr;
        RegisterClassW(&wc);
        reg = true;
    }
    g.h = CreateWindowExW(0, L"TTTRosterGrid", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP, E(x), E(y), E(w), E(h), parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), &g);
    theme_control(g.h);
    return g.h;
}

// ---------------- actions ----------------
static void re_rebuild() {
    re_main.items.clear();
    for (auto& c : re_cells) re_main.items.push_back({c[0], re_kind(c[0], true)});
    re_main.locked = (int)re_cells.size() - 1;
    if (re_cur >= (int)re_cells.size()) re_cur = (int)re_cells.size() - 1;
    re_main.sel = re_cur;
    re_forms.items.clear();
    if (re_cur >= 0) for (size_t k = 1; k < re_cells[re_cur].size(); k++) re_forms.items.push_back({re_cells[re_cur][k], re_kind(re_cells[re_cur][k], false)});
    if (re_form >= (int)re_forms.items.size()) re_form = -1;
    re_forms.sel = re_form;
    for (ReGrid* g : {&re_main, &re_forms}) if (g->h) { re_scrollbar(*g); InvalidateRect(g->h, nullptr, FALSE); }
    int nnew = (int)re_chars.size(), lost = 0;
    for (auto& c : re_vanilla) for (int v : c) if (v >= 0 && !re_used(v)) lost++;
    std::wstring s = std::to_wstring(re_cells.size() - 1) + L" cells   |   " + std::to_wstring(nnew) + L" new";
    if (lost) s += L"   |   " + std::to_wstring(lost) + L" originals out of the menu";
    if (!re_orig.empty()) s += L"   |   " + std::to_wstring(re_orig.size()) + L" originals edited";
    SetWindowTextW(GetDlgItem(cm, IDR_SUM), s.c_str());
    if (re_cur >= 0) SetWindowTextW(GetDlgItem(cm, IDR_FORMS_L), (L"Transformations of " + mw_w(re_label(re_cells[re_cur][0]))).c_str());
    else SetWindowTextW(GetDlgItem(cm, IDR_FORMS_L), L"Transformations");
    re_panel();
}
static void re_select(ReGrid& g, int i) {
    if (&g == &re_main) { re_cur = i; re_form = -1; }
    else re_form = i;
    re_rebuild();
}
static void re_drop(ReGrid* src, int idx, ReGrid* tgt, int dst) {   // his tool: the dragged cell takes the target's place, the rest moves right
    int last = (int)re_cells.size() - 1;
    if (src == &re_main && tgt == &re_main) {
        if (idx >= last) return;
        auto c = re_cells[idx];
        re_cells.erase(re_cells.begin() + idx);
        int to = std::min(dst, last - 1);
        re_cells.insert(re_cells.begin() + to, c);
        re_cur = to; re_form = -1;
    } else if (src == &re_main && tgt == &re_forms) {
        if (re_cur < 0 || idx == re_cur || idx >= last || re_cur >= last) return;
        if (re_cells[idx].size() > 1) { cm_notify(L"That cell has transformations: move them out first"); return; }
        if (re_cells[re_cur].size() >= 5) { cm_notify(L"A cell holds at most 4 transformations"); return; }
        re_cells[re_cur].push_back(re_cells[idx][0]);
        re_cells.erase(re_cells.begin() + idx);
        if (idx < re_cur) re_cur--;
        re_form = (int)re_cells[re_cur].size() - 2;
    } else if (src == &re_forms && tgt == &re_forms) {
        auto& t = re_cells[re_cur];
        if (idx + 1 >= (int)t.size()) return;
        int v = t[idx + 1];
        t.erase(t.begin() + idx + 1);
        int to = std::min(dst, (int)t.size() - 1);
        t.insert(t.begin() + 1 + to, v);
        re_form = to;
    } else if (src == &re_forms && tgt == &re_main) {
        auto& t = re_cells[re_cur];
        if (idx + 1 >= (int)t.size()) return;
        int v = t[idx + 1];
        t.erase(t.begin() + idx + 1);
        int to = std::min(dst, last);
        re_cells.insert(re_cells.begin() + to, std::vector<int>{v});
        re_cur = to; re_form = -1;
    } else return;
    re_layout_set = true;
    cm_notify(L"");
    re_rebuild();
}
static void re_remove() {
    if (re_cur < 0 || re_cur >= (int)re_cells.size() - 1) return;
    if (re_form >= 0) { re_cells[re_cur].erase(re_cells[re_cur].begin() + 1 + re_form); re_form = -1; }
    else { re_cells.erase(re_cells.begin() + re_cur); re_cur = std::min(re_cur, (int)re_cells.size() - 2); }
    re_layout_set = true;
    re_drop_unused();
    re_rebuild();
}
static int re_new_char() {
    if ((int)re_chars.size() >= roster::NEW) { cm_notify(L"No free BT3 ids left"); return -1; }
    ReChar c;
    c.sysid = re_free_id();
    if (c.sysid < 0) return -1;
    std::string base = re_bt3_name(c.sysid), id;
    for (char ch : base) id += isalnum((unsigned char)ch) ? ch : '_';
    if (!roster::valid_id(id)) id = "Character_" + std::to_string(c.sysid);
    auto dup = [&](const std::string& x) { for (auto& o : re_chars) if (o.id == x) return true; return false; };
    std::string cand = id;
    for (int k = 2; dup(cand); k++) cand = id + "_" + std::to_string(k);
    id = cand;
    c.id = id;
    re_chars.push_back(c);
    return c.sysid;
}
static int re_picker(const std::wstring& title, const std::vector<int>& items, int mode = 2);
static void re_take(int v, int& target) {   // cells of v leave the menu; their transformations become cells of their own
    std::vector<std::vector<int>> out;
    int t = -1;
    for (int i = 0; i < (int)re_cells.size(); i++) {
        if (i == target) t = (int)out.size();
        if (re_cells[i][0] != v || i == target || i == (int)re_cells.size() - 1) { out.push_back(re_cells[i]); continue; }
        for (size_t k = 1; k < re_cells[i].size(); k++) out.push_back(std::vector<int>{re_cells[i][k]});
    }
    re_cells = out;
    target = t;
}
static void re_action(int id) {
    int last = (int)re_cells.size() - 1;
    if (id == IDR_NEW) {
        if (last >= 0xA6) { cm_notify(L"The menu is full"); return; }
        int s = re_new_char();
        if (s < 0) return;
        re_cells.insert(re_cells.begin() + last, std::vector<int>{s});
        re_cur = last; re_form = -1;
        re_rebuild(); re_scroll_to(re_main, re_cur);
        SetFocus(GetDlgItem(cm, IDR_ID));
    } else if (id == IDR_COPY) {
        std::vector<int> v;
        for (auto& c : re_vanilla) if (c[0] >= 0) v.push_back(c[0]);
        for (auto& c : re_chars) v.push_back(c.sysid);
        int s = re_picker(L"Character for the new cell", v);
        if (s < -1) return;
        re_cells.insert(re_cells.begin() + last, std::vector<int>{s});
        re_cur = last; re_form = -1; re_layout_set = true;
        re_rebuild(); re_scroll_to(re_main, re_cur);
    } else if (id == IDR_FORM) {
        if (re_cur < 0 || re_cur >= last) { cm_notify(L"Select a cell first"); return; }
        if (re_cells[re_cur].size() >= 5) { cm_notify(L"A cell holds at most 4 transformations"); return; }
        std::vector<int> v = {RE_NEWITEM};
        for (auto& c : re_chars) if (c.sysid != re_cells[re_cur][0]) v.push_back(c.sysid);
        for (int s : roster::VANILLA) if (s != re_cells[re_cur][0]) v.push_back(s);
        int s = re_picker(L"Transformation for " + mw_w(re_label(re_cells[re_cur][0])), v);
        if (s < -2) return;
        int t = re_cur;
        if (s == RE_NEWITEM) { s = re_new_char(); if (s < 0) return; }
        else re_take(s, t);
        re_cells[t].push_back(s);
        re_cur = t; re_form = (int)re_cells[t].size() - 2; re_layout_set = true;
        re_drop_unused();
        re_rebuild(); re_scroll_to(re_main, re_cur);
    } else if (id == IDR_DEL) re_remove();
    else if (id == IDR_RESET) {
        re_default_cells(); re_layout_set = false; re_cur = 0; re_form = -1;
        cm_notify(L"Original layout, new characters before Random");
        re_rebuild();
    }
}

// ---------------- side panel ----------------
#ifndef EM_SETCUEBANNER
#define EM_SETCUEBANNER 0x1501
#endif
static int re_page_of(int id) { return id >= IDR_SUB && id <= IDR_INFO ? 0 : id >= IDR_LT0 && id <= IDR_FT1 ? 1 : -1; }
static void re_show(int id, bool on) { HWND c = GetDlgItem(cm, id); int pg = re_page_of(id); if (c) ShowWindow(c, on && cm_tab == 1 && (pg < 0 || pg == re_page) ? SW_SHOW : SW_HIDE); }
static void re_set_text(int id, const std::string& t) {
    HWND c = GetDlgItem(cm, id);
    wchar_t cur[1024] = L""; GetWindowTextW(c, cur, 1024);
    std::wstring w = mw_w(t);
    if (w != cur) SetWindowTextW(c, w.c_str());
}
static void re_switch(int id, bool on) {   // Tex switches: drawn as a toggle, not as a file button
    HWND c = GetDlgItem(cm, id);
    theme_switch.insert(c);
    if (on) theme_primary.insert(c); else theme_primary.erase(c);
    InvalidateRect(c, nullptr, TRUE);
}
// a slot value that comes from the mod being edited (not replaced in this session)
static bool re_is_mod(const std::string& v) {
    if (v.size() <= 4) return false;
    if (!v.compare(0, 4, "mod:")) return true;
    if (re_mod_src.empty()) return false;
    std::string r = re_norm(re_mod_root + "/" + re_mod_src) + "/", p = re_norm(v.substr(4, v.find('\n') == std::string::npos ? std::string::npos : v.find('\n') - 4));
    return !p.compare(0, r.size(), r);
}
static void re_mark(int id, bool on, bool exists = false, bool modded = false) {
    HWND c = GetDlgItem(cm, id);
    if (on) theme_primary.insert(c); else theme_primary.erase(c);
    if (on && modded) theme_modded.insert(c); else theme_modded.erase(c);
    if (exists && !on) theme_exists.insert(c); else theme_exists.erase(c);
    InvalidateRect(c, nullptr, TRUE);
}
static std::string re_file_info_unused(const std::string& f) {
    if (f.empty()) return "";
    std::string kind = f.substr(0, 4), p = f.substr(4);
    if (kind == "bt3:") { int n = 1; for (char ch : p) n += ch == '\n'; return "BT3, " + std::to_string(n) + (n == 1 ? " file" : " files"); }
    size_t sl = p.find_last_of("/\\");
    return (kind == "ttt:" ? "TTT pak " : "folder ") + (sl == std::string::npos ? p : p.substr(sl + 1));
}
static std::string re_game_name(int s, int l) {   // an original character's name in the game, per language
    int k = roster::group_vanilla(s);
    std::string t = k >= 0 && k < (int)re_name_ids.size() ? psp_system_text(l, re_name_ids[k]) : "";
    return t.empty() ? re_bt3_name(s) : t;
}
static std::string re_inherit(int s, int l) {   // name a transformation takes from its base when it has none of its own
    int p = re_parent(s);
    if (p < 0) return "";
    if (ReChar* b = re_find(p)) return !b->name[l].empty() ? b->name[l] : !b->name[0].empty() ? b->name[0] : b->id;
    auto it = re_orig.find(p);
    return it != re_orig.end() && !it->second.name[l].empty() ? it->second.name[l] : re_game_name(p, l);
}
static ReChar* re_fchar(int s) {   // the files of a new character, or of an original one
    if (s < 0) return nullptr;
    if (ReChar* c = re_find(s)) return c;
    if (roster::group_vanilla(s) < 0) return nullptr;
    ReChar& o = re_ochar[s];
    o.sysid = s;
    return &o;
}
static std::string* re_dmg_of(int s, int j) { ReChar* c = j >= 0 && j < RE_COLS ? re_fchar(s) : nullptr; return c ? &c->dmg[j] : nullptr; }
static void re_panel() {
    if (!re_ready) return;
    re_filling = true;
    int s = re_selected();
    ReChar* c = s >= 0 ? re_find(s) : nullptr;
    bool any = s >= -1, nw = c != nullptr, orig = s >= 0 && !c;
    bool base = nw && (c->file[RF_SFX].empty() || c->file[RF_VOICES].empty());
    re_show(IDR_TITLE, true); re_show(IDR_SUB, any);
    re_show(IDR_IMG2, false);
    for (int id : {IDR_ID_L, IDR_ID, IDR_IMG_L, IDR_IMG0, IDR_IMG1, IDR_IMG3}) re_show(id, nw);
    for (int id : {IDR_F_L, IDR_F0, IDR_F4, IDR_F5, IDR_INFO, IDR_CPREV, IDR_CNEXT, IDR_CNAME, IDR_D0, IDR_T0, IDR_PCM}) re_show(id, nw || orig);
    for (int id : {IDR_F1, IDR_F2, IDR_F3, IDR_D1, IDR_D2, IDR_D3, IDR_T1, IDR_T2, IDR_T3, IDR_D_L}) re_show(id, false);
    for (int id : {IDR_BASE_L, IDR_BASE}) re_show(id, base);
    re_set_text(IDR_CNAME, "Color " + std::to_string(re_col + 1));
    { std::string* v = re_dmg_of(s, re_col); re_mark(IDR_D0, v && !v->empty(), false, v && re_is_mod(*v)); }
    re_lang_panel(s, nw || orig);
    {   // charge row after the voices: below the voice pack buttons when they are shown
        bool chg = (nw || orig) && re_chg_on();
        int y = re_lang_shown ? 588 : 561;
        for (int k = 0; k < 2; k++) { re_show(IDR_F6 + k, chg); SetWindowPos(GetDlgItem(cm, IDR_F6 + k), nullptr, E(16 + 152 * k), E(y), E(146), E(26), SWP_NOZORDER | SWP_NOACTIVATE); }
        SetWindowPos(GetDlgItem(cm, IDR_INFO), nullptr, E(16), E(chg ? y + 30 : 591), E(300), E(18), SWP_NOZORDER | SWP_NOACTIVATE);
    }
    if (nw || orig) re_tex1_sync(s);
    {   // gray: none, orange: made or dropped now, violet: already in the mod
        std::string* v = re_tex1_of(s, re_col);
        theme_switch.erase(GetDlgItem(cm, IDR_T0));
        bool on = v && !v->empty();
        re_mark(IDR_T0, on, false, on && !v->compare(0, 4, "mod:") && !re_tex1_new.count(*v));
    }
    if (orig) {
        ReChar* o = re_fchar(s);
        int gk = roster::group_vanilla(s);
        re_mark(IDR_F0, o && !o->file[re_col].empty(), re_orig_exists(gk, re_col), o && re_is_mod(o->file[re_col]));
        re_mark(IDR_F4, o && !o->file[RF_SFX].empty(), re_orig_exists(gk, RF_SFX), o && re_is_mod(o->file[RF_SFX]));
        re_mark(IDR_F5, o && !o->file[RF_VOICES].empty(), re_orig_exists(gk, RF_VOICES), o && re_is_mod(o->file[RF_VOICES]));
        re_mark(IDR_F6, o && !o->file[RF_AURA].empty(), false, o && re_is_mod(o->file[RF_AURA]));
        re_mark(IDR_F7, o && !o->file[RF_CHG1].empty(), false, o && re_is_mod(o->file[RF_CHG1]));
        re_set_text(IDR_INFO, re_mod_src.empty() ? "Blue: in the game.  Orange: replaced or on." : "Blue: in the game.  Violet: in this mod.  Orange: replaced now.");
    }
    for (int id : {IDR_SYS_L, IDR_SYS, IDR_N_L0, IDR_N_L1, IDR_N_L2, IDR_N0, IDR_N1, IDR_N2}) re_show(id, nw || orig);
    EnableWindow(GetDlgItem(cm, IDR_FORM), re_cur >= 0 && re_cur < (int)re_cells.size() - 1);
    EnableWindow(GetDlgItem(cm, IDR_DEL), re_cur >= 0 && re_cur < (int)re_cells.size() - 1);
    if (!any || s == -1) for (int id = IDR_LT0; id <= IDR_FT1; id++) re_show(id, false);
    if (!any) { re_set_text(IDR_TITLE, "Select a cell"); re_filling = false; InvalidateRect(cm, nullptr, TRUE); return; }
    re_set_text(IDR_TITLE, re_label(s));
    if (s == -1) re_set_text(IDR_SUB, "Random cell, always the last one");
    else if (nw) re_set_text(IDR_SUB, "New character\nPackfile/Characters/" + (c->id.empty() ? std::string("<ID>") : c->id));
    else re_set_text(IDR_SUB, "Original character" + std::string(re_final(s) != s ? ", originally BT3 id " + std::to_string(s) : ""));
    HWND sys = GetDlgItem(cm, IDR_SYS);
    SendMessageW(sys, CB_RESETCONTENT, 0, 0);
    if (s >= 0) {
        int cur = re_final(s);
        for (int i = 0; i < roster::SYS; i++) {
            if (i != cur && i != s && re_id_taken(i, nw ? s : -1)) continue;
            if (orig && i != s && roster::vanilla(i)) continue;
            int k = (int)SendMessageW(sys, CB_ADDSTRING, 0, (LPARAM)(std::to_wstring(i) + L"   " + mw_w(re_bt3_name(i))).c_str());
            SendMessageW(sys, CB_SETITEMDATA, k, i);
            if (i == cur) SendMessageW(sys, CB_SETCURSEL, k, 0);
        }
    }
    if (nw) {
        re_set_text(IDR_ID, c->id);
        HWND b = GetDlgItem(cm, IDR_BASE);
        if (SendMessageW(b, CB_GETCOUNT, 0, 0) == 0)
            for (int v : roster::VANILLA) { int k = (int)SendMessageW(b, CB_ADDSTRING, 0, (LPARAM)mw_w(re_bt3_name(v)).c_str()); SendMessageW(b, CB_SETITEMDATA, k, v); }
        if (c->donor < 0) c->donor = 3;
        for (int k = 0; k < roster::K_OLD; k++) if (roster::VANILLA[k] == c->donor) SendMessageW(b, CB_SETCURSEL, k, 0);
        re_mark(IDR_F0, !c->file[re_col].empty(), false, re_is_mod(c->file[re_col])); re_mark(IDR_F4, !c->file[RF_SFX].empty(), false, re_is_mod(c->file[RF_SFX])); re_mark(IDR_F5, !c->file[RF_VOICES].empty(), false, re_is_mod(c->file[RF_VOICES]));
        re_mark(IDR_F6, !c->file[RF_AURA].empty(), false, re_is_mod(c->file[RF_AURA])); re_mark(IDR_F7, !c->file[RF_CHG1].empty(), false, re_is_mod(c->file[RF_CHG1]));
        auto fv = [&](int j) { std::string k = c->file[j].substr(0, 4); return k == "ttt:" ? std::string("TTT pak") : k == "dir:" ? std::string("folder") : k == "bt3:" ? std::string("BT3") : "from " + re_bt3_name(c->donor); };
        std::string info = c->file[0].empty() ? "Color 1 missing.  " : "";
        info += c->file[RF_SFX].empty() && c->file[RF_VOICES].empty() ? "SFX and Voices " + fv(RF_SFX) : "SFX: " + fv(RF_SFX) + ",  Voices: " + fv(RF_VOICES);
        re_set_text(IDR_INFO, info);
    }
    for (int k = 0; k < 2; k++) re_mark(IDR_PG0 + k, re_page == k);
    bool tx = re_text_names();   // names as text: the transformation is one text per language instead of the label images
    for (int id = IDR_LT0; id <= IDR_LHELP; id++) {
        bool form = id == IDR_LT1 || id == IDR_LT2 || (id >= IDR_LN0 + 3 && id < IDR_LN0 + 9) || (id >= IDR_LB0 + 3 && id < IDR_LB0 + 9);
        re_show(id, s >= 0 && !(tx && form));
    }
    for (int id = IDR_FTL; id <= IDR_FT1; id++) re_show(id, s >= 0 && tx);
    if (s >= 0 && tx) re_form_fill(s);
    for (int b = 0; b < 9; b++) InvalidateRect(GetDlgItem(cm, IDR_LB0 + b), nullptr, FALSE);
    for (int l = 0; l < 3; l++) {
        std::string v;
        if (nw) v = !c->name[l].empty() ? c->name[l] : re_inherit(s, l);
        else if (orig) {
            auto it = re_orig.find(s);
            v = it != re_orig.end() && !it->second.name[l].empty() ? it->second.name[l] : re_game_name(s, l);
        }
        re_set_text(IDR_N0 + l, v);
        SendMessageW(GetDlgItem(cm, IDR_N0 + l), EM_SETCUEBANNER, TRUE, (LPARAM)(nw ? mw_w(c->id).c_str() : L""));
    }
    re_filling = false;
    InvalidateRect(cm, nullptr, TRUE);
}

// ---------------- file choosers ----------------
static std::vector<std::string> re_open_files(const wchar_t* title, const wchar_t* filter, bool multi) {
    std::vector<wchar_t> buf(65536, 0);
    OPENFILENAMEW of; memset(&of, 0, sizeof(of));
    of.lStructSize = sizeof(of); of.hwndOwner = cm; of.lpstrFilter = filter; of.lpstrFile = buf.data(); of.nMaxFile = (DWORD)buf.size(); of.lpstrTitle = title;
    of.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER | (multi ? OFN_ALLOWMULTISELECT : 0);
    std::vector<std::string> out;
    if (!GetOpenFileNameW(&of)) return out;
    std::wstring dir = buf.data();
    const wchar_t* p = buf.data() + dir.size() + 1;
    if (!*p) { out.push_back(mw_u8(dir.c_str())); return out; }
    while (*p) { std::wstring f = p; out.push_back(mw_u8((dir + L"\\" + f).c_str())); p += f.size() + 1; }
    return out;
}
static std::string re_open_folder(const wchar_t* title) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IFileOpenDialog* d = nullptr;
    std::string r;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&d))) return r;
    DWORD op = 0; d->GetOptions(&op);
    d->SetOptions(op | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    d->SetTitle(title);
    IShellItem* it = nullptr;
    if (SUCCEEDED(d->Show(cm)) && SUCCEEDED(d->GetResult(&it))) {
        PWSTR p = nullptr;
        if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { r = mw_u8(p); CoTaskMemFree(p); }
        it->Release();
    }
    d->Release();
    return r;
}
static int re_menu_pick(const std::vector<std::wstring>& items) {
    HMENU m = CreatePopupMenu();
    for (size_t i = 0; i < items.size(); i++) AppendMenuW(m, MF_STRING, i + 1, items[i].c_str());
    POINT p; GetCursorPos(&p);
    int r = TrackPopupMenu(m, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN, p.x, p.y, 0, cm, nullptr);
    DestroyMenu(m);
    return r - 1;
}
static uint32_t re_head_be(const std::string& f) {
    uint8_t b[4] = {0, 0, 0, 0};
    if (FILE* fp = mods_fopen(f, "rb")) { if (fread(b, 1, 4, fp) != 4) memset(b, 0, 4); fclose(fp); }
    return (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
}
static bool re_set_image(ReChar* c, int j, const std::string& f) {
    std::vector<uint32_t> px; int w = 0, h = 0;
    if (!re_png(f, &w, &h, px)) { cm_notify(L"The file is not a valid PNG"); return false; }
    int x = w / RE_IMG_W[j];
    if (w % RE_IMG_W[j] || h != RE_IMG_H[j] * x || x < 1) {
        cm_notify(std::wstring(RE_IMG_N[j]) + L" must be " + std::to_wstring(RE_IMG_W[j]) + L"x" + std::to_wstring(RE_IMG_H[j]) + L" or an exact multiple (2x, 3x...)");
        return false;
    }
    c->img[j] = f;
    return true;
}
static int re_image_kind(const std::string& f) {   // which select image a PNG is, by its size
    std::vector<uint32_t> px; int w = 0, h = 0;
    if (!re_png(f, &w, &h, px)) return -1;
    for (int j = 0; j < 4; j++) { int x = w / RE_IMG_W[j]; if (j != 2 && x >= 1 && w % RE_IMG_W[j] == 0 && h == RE_IMG_H[j] * x) return j; }
    return -1;
}
static void re_images_changed(ReChar* c) {
    re_bitmaps_drop(c->sysid);
    for (int j = 0; j < 4; j++) InvalidateRect(GetDlgItem(cm, IDR_IMG0 + j), nullptr, FALSE);
    cm_notify(L"");
    re_rebuild();
}
static void re_image(int j, bool clear) {
    ReChar* c = re_find(re_selected());
    if (!c) return;
    if (clear) { if (c->img[j].empty()) return; c->img[j].clear(); re_images_changed(c); return; }
    auto f = re_open_files((std::wstring(RE_IMG_N[j]) + L" image").c_str(), L"PNG image (*.png)\0*.png\0", false);
    if (!f.empty() && re_set_image(c, j, f[0])) re_images_changed(c);
}
static LRESULT CALLBACK re_img_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int j = (int)GetWindowLongPtrW(h, GWLP_ID) - IDR_IMG0;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, theme_b_bg);
        ReChar* c = re_find(re_selected());
        std::string src = c ? c->img[j] : "";
        HBRUSH b = CreateSolidBrush(THEME_FIELD);
        HPEN pen = CreatePen(src.empty() ? PS_DOT : PS_SOLID, 1, src.empty() ? THEME_BORDER : THEME_ACCENT);
        HGDIOBJ o1 = SelectObject(dc, b), o2 = SelectObject(dc, pen);
        RoundRect(dc, 0, 0, rc.right, rc.bottom, E(8), E(8));
        SelectObject(dc, o1); SelectObject(dc, o2); DeleteObject(b); DeleteObject(pen);
        if (!src.empty()) {
            int W = rc.right - E(8), H = rc.bottom - E(8);
            if (HBITMAP bmp = re_scaled(src, W, H, THEME_FIELD)) {
                HDC im = CreateCompatibleDC(dc);
                HGDIOBJ ob = SelectObject(im, bmp);
                BitBlt(dc, E(4), E(4), W, H, im, 0, 0, SRCCOPY);
                SelectObject(im, ob); DeleteDC(im); DeleteObject(bmp);
            }
        } else {
            std::wstring t = std::wstring(RE_IMG_N[j]) + L" " + std::to_wstring(RE_IMG_W[j]) + L"x" + std::to_wstring(RE_IMG_H[j]);
            SetBkMode(dc, TRANSPARENT); SetTextColor(dc, THEME_TEXT2);
            HGDIOBJ of = SelectObject(dc, theme_f_normal);
            DrawTextW(dc, t.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(dc, of);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_LBUTTONUP) { re_image(j, false); return 0; }
    if (m == WM_RBUTTONUP) { re_image(j, true); return 0; }
    if (m == WM_SETCURSOR) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
    return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK re_lbl_proc(HWND h, UINT m, WPARAM w, LPARAM l);
static void re_register() {
    static bool reg = false;
    if (reg) return;
    WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = re_img_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRosterImg";
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    RegisterClassW(&wc);
    wc.lpfnWndProc = re_lbl_proc; wc.lpszClassName = L"TTTRosterLbl";
    RegisterClassW(&wc);
    reg = true;
}
static uint32_t re_magic(const std::string& f) {   // first word as stored (bytes 0..3 big endian)
    return re_head_be(f);
}
// a colour is one TTT pak (00 00 01 F1) or the three BT3 paks p (FC 00 00 00, 252 entries), anm (B8 01 00 00, 440) and eff
// (06 00 00 00, 6); colours 2 to 4 also take the BT3 p pak alone (the rest comes from colour 1). Anything else is refused
static bool re_colour_paks_main(ReChar* c, int j, std::vector<std::string> fs);
static std::string re_lowname(const std::string& f);
static std::wstring re_col_note;   // notice left by the last colour import
// a BT3 p pak named like the colour's p pak plus "_dmg" goes to the damaged costume; dropped alone it is a normal colour pak
static bool re_colour_paks(ReChar* c, int j, std::vector<std::string> fs) {
    auto stem = [](const std::string& f) { std::string l = re_lowname(f); size_t d = l.find_last_of('.'); return d == std::string::npos ? l : l.substr(0, d); };
    std::string dmg;
    re_col_note.clear();
    if (fs.size() >= 2) for (size_t i = 0; i < fs.size() && dmg.empty(); i++) {
        std::string a = stem(fs[i]);
        if (a.size() <= 4 || a.compare(a.size() - 4, 4, "_dmg") || re_magic(fs[i]) != 0xFC000000u) continue;
        for (size_t k = 0; k < fs.size(); k++) if (k != i && stem(fs[k]) == a.substr(0, a.size() - 4) && re_magic(fs[k]) == 0xFC000000u) { dmg = fs[i]; fs.erase(fs.begin() + i); break; }
    }
    if (!re_colour_paks_main(c, j, fs)) return false;
    if (!dmg.empty()) { c->dmg[j] = "bt3:" + dmg; re_col_note = RE_FILE_N(j) + L" and its damaged costume (_dmg) imported"; }
    return true;
}
static bool re_colour_paks_main(ReChar* c, int j, std::vector<std::string> fs) {
    for (auto& f : fs) {
        std::string l = f; for (auto& ch : l) ch = (char)tolower((unsigned char)ch);
        if (!re_ends(l, ".pak") && !re_ends(l, ".pck1") && !re_ends(l, ".pck2")) { cm_notify(L"Only .pak, .pck1 or .pck2 files"); return false; }
    }
    const uint32_t TTT = 0x000001F1u, P = 0xFC000000u, ANM = 0xB8010000u, EFF = 0x06000000u;
    if (fs.size() == 1 && re_magic(fs[0]) == TTT) { c->file[j] = "ttt:" + fs[0]; c->bt3[j].clear(); return true; }
    if (fs.size() == 1 && (j > 0 || !re_find(c->sysid)) && re_magic(fs[0]) == P) { c->file[j] = "bt3:" + fs[0]; c->bt3[j].clear(); return true; }
    if (fs.size() == 3) {
        std::string p, a, e;
        for (auto& f : fs) { uint32_t m = re_magic(f); if (m == P && p.empty()) p = f; else if (m == ANM && a.empty()) a = f; else if (m == EFF && e.empty()) e = f; }
        if (!p.empty() && !a.empty() && !e.empty()) { c->file[j] = "bt3:" + p + "\n" + a + "\n" + e; c->bt3[j].clear(); return true; }
    }
    cm_notify(j ? L"Import one TTT pak, the three BT3 paks (p, anm, eff) or only the BT3 p pak" : L"Color 1 takes one TTT pak or the three BT3 paks (p, anm, eff)");
    return false;
}
// charge files (TTT_CHARGE2): aura = one pak; anims = two .tanm, the one whose name has "in" is the charge in, else the first by name
static bool re_chg_set(ReChar* c, int j, std::vector<std::string> fs) {
    auto low = [](std::string f) { for (auto& ch : f) ch = (char)tolower((unsigned char)ch); return f; };
    auto name = [&](const std::string& f) { size_t s = f.find_last_of("/\\"); return low(f.substr(s == std::string::npos ? 0 : s + 1)); };
    if (j == RF_AURA) {
        if (fs.size() != 1 || !re_ends(low(fs[0]), ".pak")) { cm_notify(L"Charge aura: drop one pak (01_charge_aura)"); return false; }
        c->file[j] = "ttt:" + fs[0];
        return true;
    }
    if (fs.size() != 2 || !re_ends(low(fs[0]), ".tanm") || !re_ends(low(fs[1]), ".tanm")) { cm_notify(L"Charge anims: drop the two .tanm files (charge in and loop)"); return false; }
    auto has_in = [&](const std::string& f) { std::string n = name(f); return n.find("_in") != std::string::npos || n.find("in.") != std::string::npos; };
    bool sw = has_in(fs[1]) && !has_in(fs[0]);
    if (!has_in(fs[0]) && !has_in(fs[1]) && name(fs[1]) < name(fs[0])) sw = true;
    if (sw) std::swap(fs[0], fs[1]);
    c->file[j] = "two:" + fs[0] + "\n" + fs[1];
    cm_notify((L"Charge in: " + mw_w(name(fs[0])) + L",  loop: " + mw_w(name(fs[1]))).c_str());
    return true;
}
// click: straight to the file dialog (colours and SFX: paks; voices: a folder); right click clears (re_file_clear)
static void re_file(int j) {
    ReChar* c = re_fchar(re_selected());
    if (!c) return;
    if (j < RE_COLS) {
        auto f = re_open_files((RE_FILE_N(j) + L": one TTT pak or the BT3 paks").c_str(), L"Pak (*.pak;*.pck1;*.pck2)\0*.pak;*.pck1;*.pck2\0", true);
        if (f.empty() || !re_colour_paks(c, j, f)) return;
        cm_notify(re_col_note); re_panel();
        return;
    } else if (j == RF_SFX) {
        auto f = re_open_files(L"SFX pak", L"Pak (*.pak;*.pck1;*.pck2)\0*.pak;*.pck1;*.pck2\0", false);
        if (f.empty()) return;
        c->file[j] = "ttt:" + f[0];
    } else if (j == RF_AURA) {
        auto f = re_open_files(L"Phase 2 charge aura: the 01_charge_aura pak", L"Pak (*.pak)\0*.pak\0", false);
        if (f.empty() || !re_chg_set(c, j, f)) return;
    } else if (j == RF_CHG1) {
        auto f = re_open_files(L"Phase 1 charge: the two animations (charge in and loop)", L"Animation (*.tanm)\0*.tanm\0", true);
        if (f.empty() || !re_chg_set(c, j, f)) return;
    } else {
        std::string d = re_open_folder(L"Folder with the voices (.wav)");
        if (d.empty()) return;
        c->file[j] = "dir:" + d;
    }
    if (j != RF_CHG1) cm_notify(L"");
    re_panel();
}
// voices in a voice pack's language (one button for every installed pack): Lang/<pack>/Packfile/<character>/Voices
static std::vector<std::string> re_pk_ids, re_pk_names, re_pk_codes;
static int re_packs() {
    re_pk_ids.clear(); re_pk_names.clear(); re_pk_codes.clear();
    psp_voice_packs(&re_pk_names, &re_pk_ids, 0);
    psp_voice_codes(&re_pk_codes);
    return (int)re_pk_ids.size();
}
static const int RE_LANG_N = 4;
static void re_lang_panel(int s, bool on) {
    int n = std::min(re_packs(), RE_LANG_N);
    re_lang_shown = on && n > 0;
    ReChar* c = re_find(s);
    if (!c && s >= 0) { auto it = re_ochar.find(s); c = it == re_ochar.end() ? nullptr : &it->second; }
    int w = n ? (300 - 4 * (n - 1)) / n : 0;
    for (int k = 0; k < RE_LANG_N; k++) {
        re_show(IDR_LANG0 + k, on && k < n);
        if (k >= n) continue;
        HWND b = GetDlgItem(cm, IDR_LANG0 + k);
        SetWindowPos(b, nullptr, E(16 + (w + 4) * k), E(561), E(w), E(26), SWP_NOZORDER | SWP_NOACTIVATE);
        std::wstring t = (w >= 90 ? L"Voices " : L"") + mw_w(re_pk_codes[k]);
        wchar_t cur[64] = L""; GetWindowTextW(b, cur, 64);
        if (t != cur) SetWindowTextW(b, t.c_str());
        auto it = c ? c->lang.find(re_pk_ids[k]) : std::map<std::string, std::string>::iterator();
        re_mark(IDR_LANG0 + k, c && it != c->lang.end() && !it->second.empty(), false, c && it != c->lang.end() && re_is_mod(it->second));
    }
}
static void re_lang(int k, const std::string* dropped = nullptr) {
    ReChar* c = re_fchar(re_selected());
    if (!c || k < 0 || k >= re_packs()) return;
    std::string d = dropped ? *dropped : re_open_folder((L"Folder with the " + mw_w(re_pk_names[k]) + L" voices (.wav)").c_str());
    if (d.empty()) return;
    c->lang[re_pk_ids[k]] = "dir:" + d;
    cm_notify(mw_w(re_pk_names[k]) + L" voices: " + mw_w(d));
    re_panel();
}
static void re_lang_clear(int k) {
    ReChar* c = re_fchar(re_selected());
    if (!c || k < 0 || k >= re_packs() || c->lang[re_pk_ids[k]].empty()) return;
    if (!c->lang[re_pk_ids[k]].compare(0, 4, "mod:")) re_drop_mod(c->lang[re_pk_ids[k]]);
    c->lang.erase(re_pk_ids[k]);
    cm_notify(mw_w(re_pk_names[k]) + L" voices removed");
    re_panel();
}
static void re_file_clear(int j) {
    ReChar* c = re_fchar(re_selected());
    if (!c || c->file[j].empty()) return;
    bool in_mod = !c->file[j].compare(0, 4, "mod:");
    if (in_mod) re_drop_mod(c->file[j]);
    c->file[j].clear();
    if (j < RE_COLS) c->bt3[j].clear();
    cm_notify(RE_FILE_N(j) + (in_mod ? L" removed from the mod (applies when saved)" : j < RE_COLS || j >= RF_AURA || !re_find(c->sysid) ? L" removed" : L" removed: the base character's is used"));
    re_panel();
}

// damaged costume of a colour: one TTT colour pak (00 00 01 F1) or one BT3 p pak (FC 00 00 00); only model, faces and textures
// travel (002..012 of TTT, 002..016 of BT3), into color_N/1_p_dmg
static bool re_dmg_set(int j, const std::vector<std::string>& fs) {
    std::string* v = re_dmg_of(re_selected(), j);
    if (!v) { cm_notify(L"Select a character first"); return false; }
    uint32_t m = fs.size() == 1 ? re_magic(fs[0]) : 0;
    if (m != 0x000001F1u && m != 0xFC000000u) { cm_notify(L"Damaged costume: one TTT color pak or one BT3 p pak"); return false; }
    ReChar* c = re_find(re_selected());
    if (m == 0xFC000000u && c && !c->file[j].compare(0, 4, "ttt:")) { cm_notify(L"A BT3 damaged costume needs a BT3 color (or an opened folder)"); return false; }
    *v = (m == 0xFC000000u ? "bt3:" : "ttt:") + fs[0];
    cm_notify(L""); re_panel();
    return true;
}
static void re_dmg(int j) {
    if (!re_dmg_of(re_selected(), j)) return;
    auto f = re_open_files((RE_DMG_N(j) + L": one TTT pak or the BT3 p pak").c_str(), L"Pak (*.pak)\0*.pak\0", false);
    if (!f.empty()) re_dmg_set(j, f);
}
static void re_dmg_clear(int j) {
    std::string* v = re_dmg_of(re_selected(), j);
    if (!v || v->empty()) return;
    bool in_mod = !v->compare(0, 4, "mod:");
    if (in_mod) re_drop_mod(*v);
    v->clear();
    cm_notify(RE_DMG_N(j) + (in_mod ? L" removed from the mod (applies when saved)" : L" removed"));
    re_panel();
}

// level 1 damage textures (switch per colour): 1_p/011_textures.atex|.dbt copied as 011_textures_dmg.*; TTT_DMG1_FLAT=1 makes them flat red (test)
static bool tx_write_png(const std::string& path, const std::vector<uint8_t>& rgba, int w, int h);
static std::string re_lowname(const std::string& f) { std::string l = f.substr(f.find_last_of("/\\") + 1); for (auto& ch : l) ch = (char)tolower((unsigned char)ch); return l; }
static std::string re_tex1_in(const std::string& dir1p, bool dmg) {   // the colour's 011 textures (or their level 1 copy) in a 1_p folder
    std::error_code ec;
    std::string best;
    for (auto& e : std::filesystem::directory_iterator(std::filesystem::u8path(dir1p), ec)) {
        std::string n = e.path().filename().u8string(), l = re_lowname(n);
        bool d = e.is_directory(ec);
        if (dmg) { if (l.compare(0, 17, "011_textures_dmg.") == 0) return n; continue; }
        if (d && l == "011_textures.dbt") return n;
        if ((d && l == "011_textures.atex") || (!d && l.compare(0, 17, "011_textures.atex") == 0 && l.size() >= 21 && l.compare(l.size() - 4, 4, ".png") == 0)) best = n;
    }
    return best;
}
static std::string* re_tex1_of(int s, int j) { ReChar* c = j >= 0 && j < RE_COLS ? re_fchar(s) : nullptr; return c ? &c->tex1[j] : nullptr; }
static std::string re_tex1_here(int s, int j, bool make = false) {   // the 1_p folder of the colour on disk (edited mod, then the colour's folder), "" when none
    ReChar* n = re_find(s);
    ReChar* c = re_fchar(s);
    int k = roster::group_vanilla(s);
    std::error_code ec;
    auto isd = [&](const std::string& p) { return !p.empty() && std::filesystem::is_directory(std::filesystem::u8path(p), ec); };
    std::string col = n ? (n->id.empty() ? "" : "Characters/" + n->id + "/color_" + std::to_string(j + 1)) : psp_colour_folder(k, j);
    std::string mc = re_mod_src.empty() || col.empty() ? "" : re_mod_root + "/" + re_mod_src + "/Packfile/" + col;
    if (isd(mc + "/1_p")) return mc + "/1_p";
    if (!mc.empty() && make && isd(re_mod_root + "/" + re_mod_src)) { std::filesystem::create_directories(std::filesystem::u8path(mc + "/1_p"), ec); if (isd(mc + "/1_p")) return mc + "/1_p"; }
    if (c && (!c->file[j].compare(0, 4, "dir:") || !c->file[j].compare(0, 4, "mod:")) && isd(c->file[j].substr(4) + "/1_p")) return c->file[j].substr(4) + "/1_p";
    return "";
}
static std::string re_tex1_src(int s, int j) {   // originals: the assets colour; new characters: their own colour folder (mod or source)
    int k = roster::group_vanilla(s);
    if (!re_find(s)) {
        std::string a = k >= 0 ? psp_assets_dir() + "/Packfile/" + psp_colour_folder(k, j) + "/1_p" : "";
        return !a.empty() && !re_tex1_in(a, false).empty() ? a : "";
    }
    std::string h = re_tex1_here(s, j);
    return !h.empty() && !re_tex1_in(h, false).empty() ? h : "";
}
static bool re_tex1_png(const std::string& src, const std::string& dst) {
    const char* e = getenv("TTT_DMG1_FLAT");
    std::error_code ec;
    if (!(e && atoi(e) > 0)) return std::filesystem::copy_file(std::filesystem::u8path(src), std::filesystem::u8path(dst), std::filesystem::copy_options::overwrite_existing, ec);
    int w = 0, h = 0; std::vector<uint32_t> px;
    if (!re_png(src, &w, &h, px)) return false;
    std::vector<uint8_t> rgba((size_t)w * h * 4);
    for (size_t i = 0; i < px.size(); i++) { rgba[i * 4] = 230; rgba[i * 4 + 1] = 30; rgba[i * 4 + 2] = 30; rgba[i * 4 + 3] = (uint8_t)(px[i] >> 24); }
    return tx_write_png(dst, rgba, w, h);
}
static bool re_tex1_make(const std::string& src, const std::string& dst) {   // a file or a folder of PNGs
    std::error_code ec;
    auto sp = std::filesystem::u8path(src);
    if (!std::filesystem::is_directory(sp, ec)) return re_tex1_png(src, dst);
    std::filesystem::create_directories(std::filesystem::u8path(dst), ec);
    int n = 0;
    for (auto& e : std::filesystem::directory_iterator(sp, ec)) {
        std::string f = e.path().filename().u8string();
        if (e.is_directory(ec) || re_lowname(f).size() < 5 || re_lowname(f).compare(re_lowname(f).size() - 4, 4, ".png")) continue;
        if (!re_tex1_png(e.path().u8string(), dst + "/" + f)) return false;
        n++;
    }
    return n > 0;
}
static std::string re_tex1_name(const std::string& src) { return "011_textures_dmg" + src.substr(12); }
static void re_tex1_sync(int s) {   // a copy already on disk turns the switch on
    for (int j = 0; j < RE_COLS; j++) {
        std::string* v = re_tex1_of(s, j);
        if (!v || !v->empty()) continue;
        std::string h = re_tex1_here(s, j), d = h.empty() ? "" : re_tex1_in(h, true);
        if (!d.empty()) *v = "mod:" + h + "/" + d;
    }
}
static void re_tex1_drop(int j, const std::vector<std::string>& fs) {   // a texture (or a folder of PNGs) dropped on the button becomes the level 1 copy
    int s = re_selected();
    std::string* v = re_tex1_of(s, j);
    if (!v) { cm_notify(L"Select a character first"); return; }
    std::error_code ec;
    std::string src = re_tex1_src(s, j);
    if (src.empty()) { cm_notify(L"Level 1 damage needs the color as a folder with 1_p/011_textures"); return; }
    std::string sn = re_tex1_in(src, false), dn = re_tex1_name(sn);
    bool want_dir = std::filesystem::is_directory(std::filesystem::u8path(src + "/" + sn), ec);
    if (fs.size() != 1 || std::filesystem::is_directory(std::filesystem::u8path(fs[0]), ec) != want_dir) { cm_notify(want_dir ? L"Damaged textures: drop one folder of PNGs like " + mw_w(sn) : L"Damaged textures: drop one PNG like " + mw_w(sn)); return; }
    if (!want_dir && re_lowname(fs[0]).size() < 5) return;
    std::string here = re_tex1_here(s, j, true);
    if (here.empty()) { *v = "drop:" + fs[0]; re_tex1_new.insert(*v); cm_notify(RE_TEX1_N(j) + L": " + mw_w(dn) + L" is made with the mod from the dropped file"); re_panel(); return; }
    std::filesystem::remove_all(std::filesystem::u8path(here + "/" + dn), ec);
    bool ok = want_dir ? (std::filesystem::copy(std::filesystem::u8path(fs[0]), std::filesystem::u8path(here + "/" + dn), std::filesystem::copy_options::recursive, ec), !ec)
                       : std::filesystem::copy_file(std::filesystem::u8path(fs[0]), std::filesystem::u8path(here + "/" + dn), std::filesystem::copy_options::overwrite_existing, ec);
    if (!ok) { cm_notify(L"Could not copy " + mw_w(fs[0])); return; }
    printf("mods: Tex recibido %s/%s\n", here.c_str(), dn.c_str());
    *v = "mod:" + here + "/" + dn;
    re_tex1_new.insert(*v);
    cm_notify(RE_TEX1_N(j) + L": " + mw_w(dn) + L" placed in " + mw_w(here));
    re_panel();
}
// Pre CMAN: <colour>/1_p/pre_cman with the six pre-animation cameras (empty) in every colour, always as mod content (never the game's assets):
// the edited mod's colour folder (an original gets a mod-only 1_p), a new character's own colour folder, or written on export for an
// original in a new mod; BT3 colours without 1_p get <colour>/pre_cman; existing files are kept
static const char* const RE_PCM_N[6] = {"attack_1.cma", "attack_2.cma", "attack_3.cma", "attack_3_start.cma", "skill_1.cma", "skill_2.cma"};
static void re_pre_cman() {
    int s = re_selected();
    ReChar* c = re_fchar(s);
    if (!c) { cm_notify(L"Select a character first"); return; }
    ReChar* n = re_find(s);
    int k = roster::group_vanilla(s), made = 0, cols = 0, later = 0;
    std::error_code ec;
    auto isd = [&](const std::string& p) { return !p.empty() && std::filesystem::is_directory(std::filesystem::u8path(p), ec); };
    auto isf = [&](const std::string& p) { return !p.empty() && std::filesystem::is_regular_file(std::filesystem::u8path(p), ec); };
    for (int j = 0; j < RE_COLS; j++) {
        std::string col = n ? (n->id.empty() ? "" : "Characters/" + n->id + "/color_" + std::to_string(j + 1)) : psp_colour_folder(k, j);
        if (col.empty()) continue;
        std::string mc = re_mod_src.empty() ? "" : re_mod_root + "/" + re_mod_src + "/Packfile/" + col, d;
        if (isd(mc)) d = isd(mc + "/1_p") ? mc + "/1_p" : mc;
        else if (!mc.empty() && !n && !isf(mc + ".pak") && !isd(mc + ".pak")) d = mc + "/1_p";
        else if (n && !c->file[j].compare(0, 4, "dir:") && isd(c->file[j].substr(4))) { std::string b = c->file[j].substr(4); d = isd(b + "/1_p") ? b + "/1_p" : b; }
        else if (!n && mc.empty()) { later++; continue; }
        if (d.empty()) continue;
        d += "/pre_cman";
        std::filesystem::create_directories(std::filesystem::u8path(d), ec);
        if (!isd(d)) continue;
        cols++;
        for (const char* nm : RE_PCM_N) {
            auto f = std::filesystem::u8path(d + "/" + nm);
            if (std::filesystem::exists(f, ec)) continue;
            if (FILE* fp = _wfopen(f.wstring().c_str(), L"wb")) { fclose(fp); made++; }
        }
        printf("mods: pre_cman en %s\n", d.c_str());
    }
    if (later) { c->pcm = true; cm_notify(L"Pre CMAN: " + std::to_wstring(later) + L" colors get the empty files when the mod is created"); return; }
    cm_notify(cols ? L"Pre CMAN: " + std::to_wstring(cols) + L" colors, " + std::to_wstring(made) + L" new empty files" : std::wstring(L"Pre CMAN: no color folder for this character in the mod"));
}
static void re_tex1(int j) {
    int s = re_selected();
    std::string* v = re_tex1_of(s, j);
    if (!v) { cm_notify(L"Select a character first"); return; }
    std::error_code ec;
    if (!v->empty()) {
        if (!v->compare(0, 4, "mod:")) {
            std::string p = v->substr(4);
            if (re_lowname(p).compare(0, 17, "011_textures_dmg.") == 0) std::filesystem::remove_all(std::filesystem::u8path(p), ec);
        }
        v->clear();
        cm_notify(RE_TEX1_N(j) + L": level 1 damage textures removed");
        re_panel();
        return;
    }
    std::string src = re_tex1_src(s, j), here = src.empty() ? "" : re_tex1_here(s, j, true);
    if (src.empty()) { cm_notify(L"Level 1 damage needs the color as a folder with 1_p/011_textures"); return; }
    std::string sn = re_tex1_in(src, false), dn = re_tex1_name(sn);
    if (here.empty()) { *v = "on"; cm_notify(RE_TEX1_N(j) + L": " + mw_w(dn) + L" is made with the mod"); re_panel(); return; }
    if (!re_tex1_make(src + "/" + sn, here + "/" + dn)) { printf("mods: Tex no se pudo copiar %s\n", sn.c_str()); cm_notify(L"Could not copy " + mw_w(sn)); return; }
    printf("mods: Tex creado %s/%s\n", here.c_str(), dn.c_str());
    *v = "mod:" + here + "/" + dn;
    re_tex1_new.insert(*v);
    cm_notify(RE_TEX1_N(j) + L": " + mw_w(dn) + L" created in " + mw_w(here));
    re_panel();
}
// files dropped on the Roster tab: PNGs on an image box (or anywhere, placed by size), paks or folders on a file button
static bool re_lab_file(int k, const std::string& f);
static void re_lab_changed();
static void re_drop_files(HDROP hd) {
    POINT pt; DragQueryPoint(hd, &pt);
    UINT n = DragQueryFileW(hd, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::string> fs;
    for (UINT i = 0; i < n; i++) { wchar_t f[4096]; if (DragQueryFileW(hd, i, f, 4096)) fs.push_back(mw_u8(f)); }
    HWND at = ChildWindowFromPointEx(cm, pt, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT);
    int id = at ? GetDlgCtrlID(at) : 0;
    if (id >= IDR_LB0 && id < IDR_LB0 + 9 && fs.size() == 1 && re_selected() >= 0) { if (re_lab_file(id - IDR_LB0, fs[0])) re_lab_changed(); return; }
    if (id == IDR_D0) { re_dmg_set(re_col, fs); return; }
    if (id == IDR_T0) { re_tex1_drop(re_col, fs); return; }
    ReChar* c = re_fchar(re_selected());
    if (!c || fs.empty()) { cm_notify(L"Select a character first"); return; }
    std::error_code ec;
    auto isdir = [&](const std::string& f) { return std::filesystem::is_directory(std::filesystem::u8path(f), ec); };
    auto lowext = [](const std::string& f) { std::string l = f; for (auto& ch : l) ch = (char)tolower((unsigned char)ch); return l; };
    if (id >= IDR_LANG0 && id <= IDR_LANG3) {
        if (fs.size() == 1 && isdir(fs[0])) re_lang(id - IDR_LANG0, &fs[0]); else cm_notify(L"Voices: drop the folder with the .wav files");
        return;
    }
    if (id == IDR_F6 || id == IDR_F7) {
        if (re_chg_set(c, id == IDR_F6 ? RF_AURA : RF_CHG1, fs)) { if (id == IDR_F6) cm_notify(L""); re_panel(); }
        return;
    }
    if (id == IDR_F0 || id == IDR_F4 || id == IDR_F5) {
        int j = id == IDR_F0 ? re_col : id == IDR_F4 ? RF_SFX : RF_VOICES;
        if (j < RE_COLS) { if (!re_colour_paks(c, j, fs)) return; }
        else if (fs.size() == 1 && isdir(fs[0])) c->file[j] = "dir:" + fs[0];
        else if (j == RF_SFX && fs.size() == 1 && (re_ends(lowext(fs[0]), ".pak") || re_ends(lowext(fs[0]), ".pck1") || re_ends(lowext(fs[0]), ".pck2"))) c->file[j] = "ttt:" + fs[0];
        else { cm_notify(j == RF_VOICES ? L"Voices: drop the folder with the .wav files" : L"SFX: drop one pak or its opened folder"); return; }
        cm_notify(j < RE_COLS ? re_col_note : L""); re_panel();
        return;
    }
    if (!re_find(c->sysid)) { cm_notify(L"Original characters: drop files on the Color, SFX, Voices, Charge or Dmg buttons; names go in Labels"); return; }
    int done = 0;
    for (auto& f : fs) {
        if (!re_ends(lowext(f), ".png")) continue;
        int j = id >= IDR_IMG0 && id <= IDR_IMG3 && fs.size() == 1 ? id - IDR_IMG0 : re_image_kind(f);
        if (j < 0) { cm_notify(L"PNG size not recognized: Small 46x30, Mid 68x48, Big 264x172 (or exact multiples). Names go in Labels"); continue; }
        if (re_set_image(c, j, f)) done++;
    }
    if (done) re_images_changed(c);
}

// ---------------- picker ----------------
static std::string re_pick_file;   // a PNG dropped on the label picker
static LRESULT CALLBACK re_pick_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CLOSE) { re_pick_open = false; return 0; }
    if (m == WM_DROPFILES) {
        wchar_t f[4096];
        if (DragQueryFileW((HDROP)w, 0, f, 4096)) { re_pick_file = mw_u8(f); re_pick_choice = RE_L_IMPORT; re_pick_open = false; }
        DragFinish((HDROP)w);
        return 0;
    }
    { LRESULT r; if (theme_color(m, w, l, &r)) return r; }
    return DefWindowProcW(h, m, w, l);
}
static int re_picker(const std::wstring& title, const std::vector<int>& items, int mode) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = re_pick_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRosterPick";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
        RegisterClassW(&wc);
        reg = true;
    }
    ReGrid proto; proto.mode = mode;
    if (mode == 3 && re_pick_k) { proto.cols = 4; proto.cw = 200; proto.ch = 40; proto.iw = 192; proto.ih = 32; }
    else if (mode == 3) { proto.cols = 3; proto.cw = 268; proto.ch = 58; proto.iw = 260; proto.ih = 28; } else proto.cols = 6;
    int cols = proto.cols, gw = cols * (proto.cw + RE_GAP) + RE_GAP + 20, gh = (mode == 3 ? 6 : 3) * (proto.ch + RE_GAP) + RE_GAP + proto.ch / 2;
    HWND old = ventana; ventana = cm;
    re_pick_w = theme_create(L"TTTRosterPick", title.c_str(), gw + 32, gh + 64, WS_EX_DLGMODALFRAME);
    ventana = old;
    if (!re_pick_w) return -3;
    HWND t = CreateWindowExW(0, L"STATIC", (title + L"   (click one, Esc cancels)").c_str(), WS_CHILD | WS_VISIBLE, E(16), E(12), E(gw), E(22), re_pick_w, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessageW(t, WM_SETFONT, (WPARAM)theme_f_bold, TRUE);
    re_pick = proto;
    for (int s : items) re_pick.items.push_back({s, mode == 3 ? (int)RK_ORIG : re_kind(s, false)});
    re_grid_create(re_pick, re_pick_w, 16, 44, gw, gh, 1);
    re_scrollbar(re_pick);
    re_pick_choice = -3; re_pick_open = true; re_pick_file.clear();
    if (mode == 3) DragAcceptFiles(re_pick_w, TRUE);
    EnableWindow(cm, FALSE);
    ShowWindow(re_pick_w, SW_SHOW);
    SetForegroundWindow(re_pick_w);
    SetFocus(re_pick.h);
    while (re_pick_open && abierta && IsWindow(re_pick_w)) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { abierta = false; break; }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { re_pick_open = false; break; }
            TranslateMessage(&msg); DispatchMessageW(&msg);
        }
        Sleep(8);
    }
    EnableWindow(cm, TRUE);
    DestroyWindow(re_pick_w); re_pick_w = nullptr; re_pick.h = nullptr;
    SetForegroundWindow(cm);
    return re_pick_choice;
}

// ---------------- label boxes ----------------
static void re_lab_changed() {
    re_lab_bitmaps_drop();
    int s = re_selected();
    if (re_find(s)) re_bitmaps_drop(s);
    for (int b = 0; b < 9; b++) InvalidateRect(GetDlgItem(cm, IDR_LB0 + b), nullptr, FALSE);
    InvalidateRect(GetDlgItem(cm, IDR_IMG2), nullptr, FALSE);
    re_panel();
}
static void re_lab_put(int s, int b, int ref, const std::string& f) {   // box b = kind * 3 + language
    re_limg_set(s, b / 3, b % 3, f);
    re_lab[s].ref[b / 3][b % 3] = ref;
}
// a label chosen in one box is chosen in the other languages too (and on the other side for transformations) when that
// character has one there
static void re_lab_pair(int s, int b, int r) {
    for (int o = b < 3 ? 0 : 3; o < (b < 3 ? 3 : 9); o++) {
        if (o == b) continue;
        std::vector<uint32_t> px; int w = 0, h = 0;
        if (r >= 0 && !re_lab_px(r, o / 3, o % 3, px, &w, &h)) continue;
        re_lab_put(s, o, r == RE_L_DEFAULT ? -2 : r == RE_L_NONE ? -1 : r, "");
    }
}
static bool re_lab_file(int b, const std::string& f) {
    int s = re_selected();
    std::vector<uint32_t> px; int w = 0, h = 0;
    if (s < 0) return false;
    if (!re_png(f, &w, &h, px)) { cm_notify(L"The file is not a valid PNG"); return false; }
    if (b < 3 && (w % 256 || h != 22 * (w / 256) || w < 256)) { cm_notify(L"Name must be 256x22 or an exact multiple (2x, 3x...)"); return false; }
    re_lab_put(s, b, -2, f);
    if (b >= 3) {   // the other side takes the same image while it has nothing of its own
        int o = (b / 3 == 1 ? 6 : 3) + b % 3;
        if (re_limg(s, o / 3, o % 3).empty() && re_lab[s].ref[o / 3][o % 3] == -2) re_lab_put(s, o, -2, f);
    }
    cm_notify(L"");
    return true;
}
static void re_lab_clear(int b) {
    int s = re_selected();
    if (s < 0) return;
    re_lab_put(s, b, -2, "");
    re_lab_changed();
}
static void re_lab_pick(int b) {
    int s = re_selected();
    if (s < 0) return;
    std::vector<int> items = {RE_L_IMPORT, RE_L_DEFAULT};
    if (b >= 3) items.push_back(RE_L_NONE);
    re_pick_k = b / 3; re_pick_l = b % 3;
    std::set<int> seen;
    std::set<uint64_t> looks;   // transformation labels: each different image once
    for (auto& c : re_cells) for (int v : c) {
        if (v < 0 || v == s || !seen.insert(v).second) continue;
        if (b >= 3) {
            std::vector<uint32_t> px; int w = 0, h = 0;
            if (!re_lab_px(v, re_pick_k, re_pick_l, px, &w, &h)) continue;
            uint64_t hs = 1469598103934665603ull ^ ((uint64_t)w << 32 | (uint32_t)h);
            for (uint32_t q : px) { hs ^= q; hs *= 1099511628211ull; }
            if (!looks.insert(hs).second) continue;
        }
        items.push_back(v);
    }
    static const wchar_t* LN[3] = {L"EN", L"ES", L""}, *KN[3] = {L"Name ", L"Transformation, right ", L"Transformation, left "};
    int r = re_picker(std::wstring(KN[b / 3]) + LN[b % 3] + L" of " + mw_w(re_label(s)), items, 3);
    if (r == -3) return;
    if (r == RE_L_IMPORT) {
        std::string f = re_pick_file;
        if (f.empty()) { auto fs = re_open_files(L"Label image", L"PNG image (*.png)\0*.png\0", false); if (!fs.empty()) f = fs[0]; }
        if (f.empty() || !re_lab_file(b, f)) return;
    } else {
        re_lab_put(s, b, r == RE_L_DEFAULT ? -2 : r == RE_L_NONE ? -1 : r, "");
        re_lab_pair(s, b, r);
    }
    re_lab_changed();
}
static LRESULT CALLBACK re_lbl_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int bx = (int)GetWindowLongPtrW(h, GWLP_ID) - IDR_LB0, k = bx / 3, lg = bx % 3;
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, theme_b_bg);
        int s = re_selected();
        ReLabView v;
        std::vector<uint32_t> px; int pw = 0, ph = 0;
        bool img = s >= 0 && re_lab_px(s, k, lg, px, &pw, &ph, &v);
        HBRUSH b = CreateSolidBrush(THEME_FIELD);
        HPEN pen = CreatePen(v.own ? PS_SOLID : PS_DOT, 1, v.own ? THEME_ACCENT : THEME_BORDER);
        HGDIOBJ o1 = SelectObject(dc, b), o2 = SelectObject(dc, pen);
        RoundRect(dc, 0, 0, rc.right, rc.bottom, E(8), E(8));
        SelectObject(dc, o1); SelectObject(dc, o2); DeleteObject(b); DeleteObject(pen);
        if (img) {
            int W = rc.right - E(8), H = rc.bottom - E(8);
            if (HBITMAP bmp = re_scaled_px(px, pw, ph, W, H, THEME_FIELD)) {
                HDC im = CreateCompatibleDC(dc);
                HGDIOBJ ob = SelectObject(im, bmp);
                BitBlt(dc, E(4), E(4), W, H, im, 0, 0, SRCCOPY);
                SelectObject(im, ob); DeleteDC(im); DeleteObject(bmp);
            }
        } else {
            SetBkMode(dc, TRANSPARENT); SetTextColor(dc, THEME_TEXT2);
            HGDIOBJ of = SelectObject(dc, theme_f_normal);
            DrawTextW(dc, v.none ? L"Empty" : L"No image", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dc, of);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_CREATE) { DragAcceptFiles(h, TRUE); return 0; }
    if (m == WM_DROPFILES) {
        wchar_t f[4096];
        if (DragQueryFileW((HDROP)w, 0, f, 4096) && re_selected() >= 0 && re_lab_file(bx, mw_u8(f))) re_lab_changed();
        DragFinish((HDROP)w);
        return 0;
    }
    static HWND down = nullptr;   // a click only counts when it started on this box (not the release of a dialog's double click)
    if (m == WM_LBUTTONDOWN || m == WM_RBUTTONDOWN) { down = h; return 0; }
    if (m == WM_LBUTTONUP) { if (down == h) { down = nullptr; re_lab_pick(bx); } return 0; }
    if (m == WM_RBUTTONUP) { if (down == h) { down = nullptr; re_lab_clear(bx); } return 0; }
    if (m == WM_SETCURSOR) { SetCursor(LoadCursor(nullptr, IDC_HAND)); return TRUE; }
    return DefWindowProcW(h, m, w, l);
}

// ---------------- commands ----------------
static void re_command(int id, int code) {
    if (re_filling || !re_ready) return;
    int s = re_selected();
    ReChar* c = s >= 0 ? re_find(s) : nullptr;
    if (code == BN_CLICKED) {
        if (id == IDR_PG0 || id == IDR_PG1) { re_page = id - IDR_PG0; re_panel(); return; }
    }
    if (code == CBN_SELCHANGE && id == IDR_FTC && s >= 0) {   // the character takes that text (shared with every character that has it)
        HWND cb = GetDlgItem(cm, IDR_FTC);
        int i = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0), v = i < 0 ? -2 : (int)SendMessageW(cb, CB_GETITEMDATA, i, 0);
        if (v == -9) { v = roster::FORM_NEW; for (auto& e : re_ftab) v = std::max(v, e.first + 1); re_ftab[v] = {"", ""}; }
        if (v == -2) return;
        re_fref.erase(s);
        if (re_form_eff(s) != v) re_fref[s] = v;
        re_filling = true; re_form_fill(s); re_filling = false;
        if (v >= roster::FORM_NEW) SetFocus(GetDlgItem(cm, IDR_FT0));
        return;
    }
    if (code == EN_CHANGE && (id == IDR_FT0 || id == IDR_FT1) && s >= 0) {   // editing the text changes it for every character that shows it
        int f = re_form_eff(s), l = id - IDR_FT0;
        if (f < 0) return;
        wchar_t t[256] = L""; GetWindowTextW(GetDlgItem(cm, id), t, 256);
        std::string v = mw_u8(t);
        while (!v.empty() && isspace((unsigned char)v.back())) v.pop_back();
        auto& r = roster::form_rows();
        bool game = f < roster::FORM_NEW && f < (int)r.size() && v == r[f][l];
        if (game) { auto it = re_ftab.find(f); if (it != re_ftab.end()) { it->second[l].clear(); if (it->second[0].empty() && it->second[1].empty()) re_ftab.erase(it); } }
        else re_ftab[f][l] = v;
        HWND cb = GetDlgItem(cm, IDR_FTC);
        int i = (int)SendMessageW(cb, CB_GETCURSEL, 0, 0);
        if (i >= 0) {   // its line in the list follows the text
            std::wstring nt = mw_w((f >= roster::FORM_NEW && re_form_txt(f, 0).empty() ? std::string("(new)") : re_form_txt(f, 0)) + "  /  " + re_form_txt(f, 1));
            re_filling = true; SendMessageW(cb, CB_DELETESTRING, i, 0); SendMessageW(cb, CB_INSERTSTRING, i, (LPARAM)nt.c_str()); SendMessageW(cb, CB_SETITEMDATA, i, (LPARAM)f); SendMessageW(cb, CB_SETCURSEL, i, 0); re_filling = false;
        }
        return;
    }
    if (code == BN_CLICKED) {
        if (id >= IDR_NEW && id <= IDR_RESET && id != IDR_MOD && id != IDR_MOD_L) { re_action(id); return; }
        if (id == IDR_F0) { re_file(re_col); return; }
        if (id == IDR_F4) { re_file(RF_SFX); return; }
        if (id == IDR_F5) { re_file(RF_VOICES); return; }
        if (id == IDR_F6) { re_file(RF_AURA); return; }
        if (id == IDR_F7) { re_file(RF_CHG1); return; }
        if (id == IDR_D0) { re_dmg(re_col); return; }
        if (id == IDR_T0) { re_tex1(re_col); return; }
        if (id == IDR_PCM) { re_pre_cman(); return; }
        if (id == IDR_CPREV || id == IDR_CNEXT) { re_col = (re_col + (id == IDR_CNEXT ? 1 : RE_COLS - 1)) % RE_COLS; cm_notify(L""); re_panel(); return; }
        if (id >= IDR_LANG0 && id <= IDR_LANG3) { re_lang(id - IDR_LANG0); return; }
    }
    if (code == EN_CHANGE && id == IDR_ID && c) {
        HWND e = GetDlgItem(cm, IDR_ID);
        wchar_t t[256] = L""; GetWindowTextW(e, t, 256);
        std::string v = mw_u8(t), clean;
        for (char ch : v) if (isalnum((unsigned char)ch) || ch == '_' || ch == '-') clean += ch;
        if (clean.size() > 48) clean.resize(48);
        if (clean != v) {
            re_filling = true; SetWindowTextW(e, mw_w(clean).c_str()); SendMessageW(e, EM_SETSEL, clean.size(), clean.size()); re_filling = false;
            cm_notify(L"The ID is the character's folder name: letters, numbers, _ and - only, no spaces");
        }
        c->id = clean;
        re_set_text(IDR_SUB, "New character, BT3 id " + std::to_string(s) + "\nPackfile/Characters/" + (clean.empty() ? std::string("<ID>") : clean));
        for (int l = 0; l < 3; l++) SendMessageW(GetDlgItem(cm, IDR_N0 + l), EM_SETCUEBANNER, TRUE, (LPARAM)mw_w(clean).c_str());
        if (c->name[0].empty()) re_set_text(IDR_TITLE, re_label(s));
        InvalidateRect(re_main.h, nullptr, FALSE); InvalidateRect(re_forms.h, nullptr, FALSE);
        return;
    }
    if (code == EN_CHANGE && id >= IDR_N0 && id <= IDR_N2 && s >= 0) {
        int l = id - IDR_N0;
        wchar_t t[512] = L""; GetWindowTextW(GetDlgItem(cm, id), t, 512);
        std::string v = mw_u8(t);
        if (c) c->name[l] = re_parent(s) >= 0 && v == re_inherit(s, l) ? "" : v;
        else {
            auto& o = re_orig[s];
            o.name[l] = v == re_game_name(s, l) ? "" : v;
            if (o.name[0].empty() && o.name[1].empty() && o.name[2].empty() && o.newid < 0) re_orig.erase(s);
        }
        if (l == 0) re_set_text(IDR_TITLE, re_label(s));
        InvalidateRect(re_main.h, nullptr, FALSE); InvalidateRect(re_forms.h, nullptr, FALSE);
        return;
    }
    if (code == CBN_SELCHANGE && id == IDR_SYS && s >= 0) {
        HWND b = GetDlgItem(cm, IDR_SYS);
        int k = (int)SendMessageW(b, CB_GETCURSEL, 0, 0), v = (int)SendMessageW(b, CB_GETITEMDATA, k, 0);
        if (c) {
            if (v == s || re_id_taken(v, s) || roster::vanilla(v)) return;
            re_bitmaps_drop(s);
            c->sysid = v;
            for (auto& cell : re_cells) for (auto& x : cell) if (x == s) x = v;
        } else {
            auto& o = re_orig[s];
            o.newid = v == s ? -1 : v;
            if (o.name[0].empty() && o.name[1].empty() && o.name[2].empty() && o.newid < 0) re_orig.erase(s);
            if (v != s) cm_notify(L"Its transformations in battle use the BT3 id: keep them in step with the character's files");
        }
        re_rebuild();
        return;
    }
    if (code == CBN_SELCHANGE && id == IDR_BASE && c) {
        HWND b = GetDlgItem(cm, IDR_BASE);
        int k = (int)SendMessageW(b, CB_GETCURSEL, 0, 0);
        c->donor = (int)SendMessageW(b, CB_GETITEMDATA, k, 0);
        re_panel();
        return;
    }
}

// ---------------- load (an installed mod) ----------------
static void re_reset_all() {
    re_chars.clear(); re_orig.clear(); re_lab.clear(); re_ftab.clear(); re_fref.clear(); re_ochar.clear(); re_gone.clear(); re_kept.clear(); re_ochar_load.clear(); re_tex1_new.clear(); re_layout_set = false; re_mod_src.clear();
    re_packs();
    re_bitmaps_drop(-100);
    re_default_cells();
    re_cur = 0; re_form = -1;
}
// returns the mod's character folders (Packfile-relative) so the caller does not add them twice
static std::vector<std::string> re_load_mod(const std::string& root, const std::string& mod) {
    re_reset_all();
    re_mod_src = mod; re_mod_root = root;
    std::vector<std::string> folders;
    roster::Defs d;
    roster::read_file(root + "/" + mod + "/Roster/roster.txt", mod, true, d);
    std::error_code ec;
    auto isf = [&](const std::string& p) { return std::filesystem::is_regular_file(std::filesystem::u8path(p), ec); };
    auto isd = [&](const std::string& p) { return std::filesystem::is_directory(std::filesystem::u8path(p), ec); };
    for (auto& x : d.chars) {
        if (x.sysid < 0) continue;   // keyed characters: not in the editor yet
        ReChar c;
        c.sysid = x.sysid; c.donor = x.donor; c.id = roster::valid_id(x.id) ? x.id : "";
        for (int l = 0; l < 3; l++) c.name[l] = x.name[l];
        for (const std::string& sub : {c.id, std::to_string(x.sysid)}) {
            if (sub.empty()) continue;
            std::string b = root + "/" + mod + "/Roster/" + sub + "/";
            for (int j = 0; j < 4; j++) if (c.img[j].empty() && isf(b + RE_IMG[j] + ".png")) c.img[j] = b + RE_IMG[j] + ".png";
            for (int k = 0; k < 3; k++) for (int l = 0; l < 4; l++) {
                if (l == 2) continue;
                std::string f = b + RE_LAB_FILE[k] + (l < 3 ? std::string("_") + RE_LANG_C[l] : std::string()) + ".png";
                if ((k || l < 3) && re_lab[x.sysid].img[k][l].empty() && isf(f)) re_lab[x.sysid].img[k][l] = f;
            }
        }
        for (int k = 0; k < 3; k++) for (int l = 0; l < 4; l++) re_lab[x.sysid].ref[k][l] = x.lab.ref[k][l];
        std::string folder = !x.folder.empty() ? x.folder : c.id.empty() ? "" : "Characters/" + c.id;
        if (!folder.empty() && folder.compare(0, 11, "Characters/") == 0 && folder.find('/', 11) == std::string::npos) {
            std::string b = root + "/" + mod + "/Packfile/" + folder + "/";
            folders.push_back(folder);
            for (int j = 0; j <= RF_SFX; j++) {
                std::string n = j < RE_COLS ? "color_" + std::to_string(j + 1) : "sfx";
                if (isf(b + n + ".pak")) c.file[j] = "ttt:" + b + n + ".pak";
                else if (isd(b + n)) c.file[j] = "dir:" + b + n;
                else if (isd(b + n + ".pak")) c.file[j] = "dir:" + b + n + ".pak";
            }
            if (isd(b + "Voices")) c.file[RF_VOICES] = "dir:" + b + "Voices";
            if (isf(b + "charge/" + RE_CHG_AURA)) c.file[RF_AURA] = "ttt:" + b + "charge/" + RE_CHG_AURA;
            if (isf(b + "charge/" + RE_CHG_IN) && isf(b + "charge/" + RE_CHG_LOOP)) c.file[RF_CHG1] = "two:" + b + "charge/" + RE_CHG_IN + "\n" + b + "charge/" + RE_CHG_LOOP;
            for (auto& pk : re_pk_ids) { std::string lv = root + "/" + mod + "/Lang/" + pk + "/Packfile/" + folder + "/Voices"; if (isd(lv)) c.lang[pk] = "dir:" + lv; }
            for (int j = 0; j < RE_COLS; j++) { std::string dd = b + "color_" + std::to_string(j + 1) + "/1_p_dmg"; if (isd(dd)) c.dmg[j] = "dir:" + dd; }
        }
        re_chars.push_back(c);
    }
    for (int k = 0; k < roster::K_OLD; k++)   // files of originals already in this mod (they travel with its Packfile content)
        for (int j = 0; j < RF_N; j++) {
            std::string f = re_orig_path(k, j), a = root + "/" + mod + "/Packfile/" + f;
            if (j == RF_CHG1) {
                std::string l = a.substr(0, a.find_last_of('/') + 1) + RE_CHG_LOOP;
                if (!f.empty() && isf(a) && isf(l)) { ReChar& o = re_ochar[roster::VANILLA[k]]; o.sysid = roster::VANILLA[k]; o.file[j] = "mod:" + a + "\n" + l; }
                continue;
            }
            if (!f.empty() && (isf(a) || isd(a) || (j < RE_COLS && (isf(a + ".pak") || isd(a + ".pak"))))) { ReChar& o = re_ochar[roster::VANILLA[k]]; o.sysid = roster::VANILLA[k]; o.file[j] = "mod:" + a; }
        }
    for (int k = 0; k < roster::K_OLD; k++)   // voices of originals in a voice pack's language in this mod
        for (auto& pk : re_pk_ids) {
            std::string f = psp_original_path(k, 5), a = root + "/" + mod + "/Lang/" + pk + "/Packfile/" + f;
            if (!f.empty() && isd(a)) { ReChar& o = re_ochar[roster::VANILLA[k]]; o.sysid = roster::VANILLA[k]; o.lang[pk] = "mod:" + a; }
        }
    for (int k = 0; k < roster::K_OLD; k++)   // damaged costumes of originals in this mod
        for (int j = 0; j < RE_COLS; j++) {
            std::string f = psp_colour_folder(k, j), dd = root + "/" + mod + "/Packfile/" + f + "/1_p_dmg";
            if (!f.empty() && isd(dd)) re_ochar[roster::VANILLA[k]].dmg[j] = "mod:" + dd;
        }
    re_ftab = d.ftext;
    for (auto& x : d.chars) if (x.form >= -1) re_fref[x.sysid] = x.form;
    for (auto& x : d.renames) if (x.form >= -1) re_fref[x.sysid] = x.form;
    for (auto& r : d.renames) {
        ReOrig o; for (int l = 0; l < 3; l++) o.name[l] = r.name[l]; o.newid = r.newid >= 0 && r.newid != r.sysid ? r.newid : -1;
        if (!o.name[0].empty() || !o.name[1].empty() || !o.name[2].empty() || o.newid >= 0) re_orig[r.sysid] = o;
        ReLab& lb = re_lab[r.sysid];
        std::string b = root + "/" + mod + "/Roster/" + std::to_string(r.sysid) + "/";
        for (int k = 0; k < 3; k++) for (int l = 0; l < 4; l++) {
            lb.ref[k][l] = r.lab.ref[k][l];
            if (l == 2) continue;
            std::string f = b + RE_LAB_FILE[k] + (l < 3 ? std::string("_") + RE_LANG_C[l] : std::string()) + ".png";
            if (isf(f)) lb.img[k][l] = f;
        }
    }
    if (!d.layout.empty()) {
        std::vector<std::vector<int>> cells;
        std::vector<int> rnd;
        for (auto& l : d.layout) {
            std::vector<int> c;
            for (int v : l) {
                if (v < -1) continue;
                for (auto& o : re_orig) if (v >= 0 && o.second.newid == v) v = o.first;   // the file holds an original's own BT3 id
                if (v < 0 || roster::vanilla(v) || re_find(v)) c.push_back(v);
            }
            if (c.empty()) continue;
            if (c[0] == -1) rnd = {-1}; else cells.push_back(c);
        }
        cells.push_back({-1});
        re_cells = cells;
        for (auto& c : re_chars) if (!re_used(c.sysid)) re_cells.insert(re_cells.end() - 1, std::vector<int>{c.sysid});
        re_layout_set = true;
        for (auto& c : re_chars) if (re_parent(c.sysid) >= 0) for (int l = 0; l < 3; l++) if (c.name[l] == re_inherit(c.sysid, l)) c.name[l].clear();
    } else re_default_cells();
    re_cur = 0; re_form = -1;
    re_ochar_load = re_ochar;
    return folders;
}

// ---------------- export ----------------
static std::string re_pak_name(std::string n) {   // .pck1/.pck2 travel as .pak (what the installer opens)
    std::string l = n; for (auto& ch : l) ch = (char)tolower((unsigned char)ch);
    if (re_ends(l, ".pck1") || re_ends(l, ".pck2")) n = n.substr(0, n.size() - 5) + ".pak";
    return n;
}
typedef std::function<bool(const std::string&, const std::vector<uint8_t>&)> ReAdd;
static std::function<int(const std::string&, const std::string&)> re_copy;   // saving over a mod: a file goes straight to disk (1 done, -1 failed, 0 read it)
static bool re_add_file(const ReAdd& add, const std::string& name, const std::string& src, int& files) {
    if (re_dropped(src)) return true;
    if (re_copy) { int r = re_copy(name, src); if (r) { files++; return r > 0; } }
    std::vector<uint8_t> d;
    if (!cm_read(src, d)) return false;
    files++;
    return add(name, d);
}
// path below root as text: std::filesystem::relative canonicalizes both sides (several system calls per file on Windows)
static std::string fs_rel(const std::filesystem::path& p, const std::filesystem::path& root) {
    std::string a = p.generic_u8string(), r = root.generic_u8string();
    while (!r.empty() && r.back() == '/') r.pop_back();
    if (a.size() > r.size() && !a.compare(0, r.size(), r) && a[r.size()] == '/') return a.substr(r.size() + 1);
    std::error_code ec;
    return std::filesystem::relative(p, root, ec).generic_u8string();
}
// every file below dir (rel with '/', full path); the callback returns false to stop. Win32 directly: no per-file stat or path objects
static bool fs_walk(const std::string& dir, const std::function<bool(const std::string&, const std::string&)>& cb) {
#ifdef _WIN32
    std::string base = dir;
    while (!base.empty() && (base.back() == '/' || base.back() == '\\')) base.pop_back();
    std::vector<std::string> todo{""};
    while (!todo.empty()) {
        std::string rel = todo.back(); todo.pop_back();
        std::wstring pat = mw_w(base + (rel.empty() ? "" : "/" + rel) + "/*");
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW(pat.c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            const wchar_t* n = fd.cFileName;
            if (n[0] == L'.' && (!n[1] || (n[1] == L'.' && !n[2]))) continue;
            std::string r = (rel.empty() ? "" : rel + "/") + mw_u8(n);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) todo.push_back(r); continue; }
            if (!cb(r, base + "/" + r)) { FindClose(h); return false; }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    return true;
#else
    std::error_code ec;
    std::filesystem::path root = std::filesystem::u8path(dir);
    for (auto it = std::filesystem::recursive_directory_iterator(root, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        if (it->is_regular_file(ec) && !cb(fs_rel(it->path(), root), it->path().u8string())) return false;
    return true;
#endif
}
static bool re_add_dir(const ReAdd& add, const std::string& prefix, const std::string& dir, int& files) {
    return fs_walk(dir, [&](const std::string& sub, const std::string& full) {
        std::string low = sub;
        for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        if (sub.empty() || sub[0] == '.' || low == "thumbs.db" || low == "desktop.ini") return true;
        return re_add_file(add, prefix + "/" + sub, full, files);
    });
}
static bool re_tex1_export(const ReAdd& add, const std::string& prefix, int s, int j, int& files) {   // "on": made from the colour, "drop:": the dropped file, "mod:": kept from the mod folder
    std::string* v = re_tex1_of(s, j);
    if (!v || v->empty()) return true;
    std::error_code ec0;
    if (!v->compare(0, 4, "mod:") || !v->compare(0, 5, "drop:")) {
        std::string p = v->substr(v->find(':') + 1), dn = std::filesystem::u8path(p).filename().u8string();
        if (!v->compare(0, 5, "drop:")) { std::string src = re_tex1_src(s, j); if (src.empty()) return false; dn = re_tex1_name(re_tex1_in(src, false)); }
        if (std::filesystem::is_directory(std::filesystem::u8path(p), ec0)) return re_add_dir(add, prefix + "/" + dn, p, files);
        return re_add_file(add, prefix + "/" + dn, p, files);
    }
    if (*v != "on") return true;
    std::string src = re_tex1_src(s, j);
    if (src.empty()) return false;
    std::string sn = re_tex1_in(src, false), tmp = re_mod_root.empty() ? std::string("tex1_tmp") : re_mod_root + "/.tex1_tmp";
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::u8path(tmp), ec);
    std::filesystem::create_directories(std::filesystem::u8path(tmp), ec);
    std::string dn = re_tex1_name(sn);
    bool ok = re_tex1_make(src + "/" + sn, tmp + "/" + dn);
    if (ok) {
        if (std::filesystem::is_directory(std::filesystem::u8path(tmp + "/" + dn), ec)) ok = re_add_dir(add, prefix + "/" + dn, tmp + "/" + dn, files);
        else ok = re_add_file(add, prefix + "/" + dn, tmp + "/" + dn, files);
    }
    std::filesystem::remove_all(std::filesystem::u8path(tmp), ec);
    return ok;
}

static bool re_dmg_export(const ReAdd& add, const std::string& prefix, const std::string& v, int& files) {
    if (v.empty()) return true;
    std::string kind = v.substr(0, 4), p = v.substr(4);
    if (kind == "dir:") return re_add_dir(add, prefix, p, files);
    size_t sl = p.find_last_of("/\\");
    if (kind == "bt3:") return re_add_file(add, prefix + "/" + re_pak_name(p.substr(sl + 1)), p, files);
    std::vector<uint8_t> d;
    if (!cm_read(p, d) || d.size() < 0x40) return false;
    auto be = [&](size_t o) { return o + 4 <= d.size() ? (uint32_t)d[o] << 24 | (uint32_t)d[o + 1] << 16 | (uint32_t)d[o + 2] << 8 | d[o + 3] : 0u; };
    uint32_t n = be(0);
    if (n < 13 || 4 + 4 * (size_t)(n + 1) > d.size()) return false;
    for (uint32_t i = 2; i <= 12; i++) {
        uint32_t a = be(4 + 4 * i), b = be(8 + 4 * i);
        if (b < a || b > d.size()) return false;
        if (!add(prefix + "/" + RE_DMG_TTT[i - 2], std::vector<uint8_t>(d.begin() + a, d.begin() + b))) return false;
        files++;
    }
    return true;
}
static bool re_validate(std::wstring& err) {
    std::set<std::string> ids;
    for (size_t i = 0; i < re_chars.size(); i++) {
        auto& c = re_chars[i];
        std::string low = c.id;
        for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
        if (!roster::valid_id(c.id) || !ids.insert(low).second) {
            err = c.id.empty() ? L"A new character has no ID" : L"The ID " + mw_w(c.id) + L" is repeated";
            for (size_t k = 0; k < re_cells.size(); k++) for (size_t j = 0; j < re_cells[k].size(); j++)
                if (re_cells[k][j] == c.sysid) { re_cur = (int)k; re_form = j ? (int)j - 1 : -1; }
            return false;
        }
    }
    for (auto& c : re_chars)
        for (int j = 1; j < RE_COLS; j++) {
            std::string f = c.file[j];
            if (!f.compare(0, 4, "bt3:") && f.find('\n') == std::string::npos && !c.file[0].compare(0, 4, "ttt:")) {
                err = mw_w(c.id) + L": Color " + std::to_wstring(j + 1) + L" has only the BT3 p pak, so Color 1 must be BT3 too (or an opened folder)";
                return false;
            }
        }
    for (auto& c : re_chars)
        if (c.file[0].empty()) {
            err = mw_w(c.id) + L" needs its Color 1";
            for (size_t k = 0; k < re_cells.size(); k++) for (size_t j = 0; j < re_cells[k].size(); j++)
                if (re_cells[k][j] == c.sysid) { re_cur = (int)k; re_form = j ? (int)j - 1 : -1; }
            return false;
        }
    if (re_cells.size() > 0xA7) { err = L"Too many cells"; return false; }
    return true;
}
static std::string re_lab_keys(int s) {
    auto it = re_lab.find(s);
    if (it == re_lab.end()) return "";
    std::string t;
    for (int k = 0; k < 3; k++) for (int l = 0; l < 4; l++) {
        int v = it->second.ref[k][l];
        if (v == -2 || l == 2) continue;
        t += std::string(RE_LAB_KEY[k]) + (l < 3 ? std::string("_") + RE_LANG_C[l] : std::string()) + " = " + (v < 0 ? std::string("none") : std::to_string(v)) + "\n";
    }
    return t;
}
// a slot of an original loaded from the mod and changed or cleared: its old files leave the mod (no residue), other slots
// still loaded from the mod (a damaged costume inside a replaced colour folder) stay
static void re_sync_gone() {
    auto cmp = [&](const std::string& was, const std::string& now) { if (!was.compare(0, 4, "mod:") && now != was) re_drop_mod(was); };
    for (auto& o : re_ochar_load) {
        auto it = re_ochar.find(o.first);
        ReChar none;
        const ReChar& n = it == re_ochar.end() ? none : it->second;
        for (int j = 0; j < RF_N; j++) cmp(o.second.file[j], n.file[j]);
        for (int j = 0; j < RE_COLS; j++) cmp(o.second.dmg[j], n.dmg[j]);
        for (auto& l : o.second.lang) { auto x = n.lang.find(l.first); cmp(l.second, x == n.lang.end() ? std::string() : x->second); }
    }
    re_kept.clear();
    auto keep = [&](const std::string& v) { if (v.compare(0, 4, "mod:")) return; for (size_t a = 4; a < v.size();) { size_t b = v.find('\n', a); re_kept.insert(re_norm(v.substr(a, b == std::string::npos ? std::string::npos : b - a))); a = b == std::string::npos ? v.size() : b + 1; } };
    for (auto& o : re_ochar) {
        for (int j = 0; j < RF_N; j++) keep(o.second.file[j]);
        for (int j = 0; j < RE_COLS; j++) keep(o.second.dmg[j]);
        for (auto& l : o.second.lang) keep(l.second);
    }
}
static bool re_export(const ReAdd& add, int& files, std::wstring& err) {
    if (!re_validate(err)) return false;
    re_sync_gone();
    std::string t = "# Roster made with the mod creator\n";
    if (re_layout_set) {
        t += "[layout]\n";
        for (auto& c : re_cells) {
            std::string line;
            for (int v : c) line += (line.empty() ? "" : " ") + (v < 0 ? std::string("random") : std::to_string(re_final(v)));
            t += line + "\n";
        }
    }
    {   // transformation texts: the game's rows given another text and the new ones in use
        std::set<int> used;
        for (auto& x : re_fref) used.insert(x.second);
        std::string ft;
        for (auto& e : re_ftab) if ((e.first < roster::FORM_NEW || used.count(e.first)) && (!e.second[0].empty() || !e.second[1].empty())) ft += std::to_string(e.first) + " | " + e.second[0] + " | " + e.second[1] + "\n";
        if (!ft.empty()) t += "\n[forms]\n" + ft;
    }
    static const char* L[2] = {"name_en", "name_es"};
    for (auto& c : re_chars) {
        t += "\n[character " + std::to_string(c.sysid) + "]\nid = " + c.id + "\n";
        for (int l = 0; l < 2; l++) if (!c.name[l].empty()) t += std::string(L[l]) + " = " + c.name[l] + "\n";
        t += "base = " + (c.file[RF_SFX].empty() || c.file[RF_VOICES].empty() ? std::to_string(c.donor < 0 ? 3 : c.donor) : std::string("none")) + "\n";
        t += re_form_keys(c.sysid);
        t += re_lab_keys(c.sysid);
    }
    std::set<int> origs;
    for (auto& r : re_orig) origs.insert(r.first);
    for (auto& x : re_lab) if (!re_find(x.first) && re_lab_any(x.first)) origs.insert(x.first);
    for (auto& x : re_fref) if (!re_find(x.first)) origs.insert(x.first);
    for (int o : origs) {
        t += "\n[original " + std::to_string(o) + "]\n";
        auto r = re_orig.find(o);
        if (r != re_orig.end()) {
            for (int l = 0; l < 2; l++) if (!r->second.name[l].empty()) t += std::string(L[l]) + " = " + r->second.name[l] + "\n";
            if (r->second.newid >= 0) t += "bt3_id = " + std::to_string(r->second.newid) + "\n";
        }
        t += re_form_keys(o);
        t += re_lab_keys(o);
    }
    if (re_has_roster()) {
        if (!add("Roster/roster.txt", std::vector<uint8_t>(t.begin(), t.end()))) { err = L"Failed to write the mkpak"; return false; }
        files++;
    }
    for (auto& x : re_lab) {   // label images (a new character's all-languages name travels as its Name image)
        ReChar* c = re_find(x.first);
        std::string dir = "Roster/" + (c ? c->id : std::to_string(x.first)) + "/";
        for (int k = 0; k < 3; k++) for (int l = 0; l < 4; l++) {
            const std::string& f = x.second.img[k][l];
            if (f.empty() || l == 2 || (c && k == 0 && l == 3)) continue;
            std::string n = dir + RE_LAB_FILE[k] + (l < 3 ? std::string("_") + RE_LANG_C[l] : std::string()) + ".png";
            if (!re_add_file(add, n, f, files)) { err = L"Cannot read " + mw_w(f); return false; }
        }
    }
    for (auto& c : re_chars) {
        std::string pf = "Packfile/Characters/" + c.id;
        for (int j = 0; j < 4; j++) if (!c.img[j].empty() && !re_add_file(add, "Roster/" + c.id + "/" + RE_IMG[j] + ".png", c.img[j], files)) { err = L"Cannot read " + mw_w(c.img[j]); return false; }
        for (int j = 0; j < RF_N; j++) {
            if (c.file[j].empty()) continue;
            std::string kind = c.file[j].substr(0, 4), p = c.file[j].substr(4);
            std::string n = j < RE_COLS ? "color_" + std::to_string(j + 1) : j == RF_SFX ? "sfx" : j == RF_VOICES ? "Voices" : j == RF_AURA ? std::string("charge/phase2_aura") : std::string("charge");
            bool ok = true;
            if (kind == "two:") { size_t b = p.find('\n'); ok = b != std::string::npos && re_add_file(add, pf + "/charge/" + RE_CHG_IN, p.substr(0, b), files) && re_add_file(add, pf + "/charge/" + RE_CHG_LOOP, p.substr(b + 1), files); }
            else if (kind == "ttt:") ok = re_add_file(add, pf + "/" + n + ".pak", p, files);
            else if (kind == "dir:") ok = re_add_dir(add, pf + "/" + n, p, files);
            else if (kind == "bt3:") {
                size_t a = 0;
                while (ok && a < p.size()) {
                    size_t b = p.find('\n', a);
                    std::string f = p.substr(a, b == std::string::npos ? std::string::npos : b - a);
                    size_t sl = f.find_last_of("/\\");
                    ok = re_add_file(add, pf + "/" + n + "/" + re_pak_name(f.substr(sl + 1)), f, files);
                    a = b == std::string::npos ? p.size() : b + 1;
                }
            }
            if (ok && j < RE_COLS && !c.bt3[j].empty()) {
                const std::string& p2 = c.bt3[j];
                size_t a = 0;
                while (ok && a < p2.size()) {
                    size_t b = p2.find('\n', a);
                    std::string f = p2.substr(a, b == std::string::npos ? std::string::npos : b - a);
                    size_t sl = f.find_last_of("/\\");
                    ok = re_add_file(add, pf + "/" + n + "/" + re_pak_name(f.substr(sl + 1)), f, files);
                    a = b == std::string::npos ? p2.size() : b + 1;
                }
            }
            if (!ok) { err = L"Cannot read the files of " + mw_w(c.id) + L" " + RE_FILE_N(j); return false; }
        }
        for (auto& l : c.lang)
            if (l.second.size() > 4 && !re_add_dir(add, "Lang/" + l.first + "/Packfile/Characters/" + c.id + "/Voices", l.second.substr(4), files)) { err = L"Cannot read the " + mw_w(l.first) + L" voices of " + mw_w(c.id); return false; }
        for (int j = 0; j < RE_COLS; j++)
            if (!re_dmg_export(add, pf + "/color_" + std::to_string(j + 1) + "/1_p_dmg", c.dmg[j], files)) { err = L"Cannot read the files of " + mw_w(c.id) + L" " + RE_DMG_N(j); return false; }
        for (int j = 0; j < RE_COLS; j++)
            if (!re_tex1_export(add, pf + "/color_" + std::to_string(j + 1) + "/1_p", c.sysid, j, files)) { err = L"Cannot copy the textures of " + mw_w(c.id) + L" " + RE_TEX1_N(j); return false; }
    }
    for (auto& o : re_ochar) {   // originals: what is already in the mod ("mod:") travels with the mod's own Packfile content
        int k = roster::group_vanilla(o.first);
        if (k < 0) continue;
        for (int j = 0; j < RF_N; j++) {
            const std::string& v = o.second.file[j];
            if (v.empty() || !v.compare(0, 4, "mod:")) continue;
            std::string kind = v.substr(0, 4), p = v.substr(4), f = re_orig_path(k, j);
            bool ok = !f.empty();
            if (ok && kind == "two:") { size_t b = p.find('\n'), d = f.find_last_of('/'); ok = b != std::string::npos && re_add_file(add, "Packfile/" + f, p.substr(0, b), files) && re_add_file(add, "Packfile/" + f.substr(0, d + 1) + RE_CHG_LOOP, p.substr(b + 1), files); }
            else if (ok && kind == "ttt:") ok = re_add_file(add, "Packfile/" + f + (j < RE_COLS ? ".pak" : ""), p, files);
            else if (ok && kind == "dir:") ok = re_add_dir(add, "Packfile/" + f, p, files);
            else if (ok && kind == "bt3:")
                for (size_t a = 0; ok && a < p.size();) {
                    size_t b = p.find('\n', a);
                    std::string x = p.substr(a, b == std::string::npos ? std::string::npos : b - a);
                    ok = re_add_file(add, "Packfile/" + f + "/" + re_pak_name(x.substr(x.find_last_of("/\\") + 1)), x, files);
                    a = b == std::string::npos ? p.size() : b + 1;
                }
            if (!ok) { err = L"Cannot read the files of " + mw_w(re_label(o.first)) + L" " + RE_FILE_N(j); return false; }
        }
        for (auto& l : o.second.lang) {
            std::string f = psp_original_path(k, 5);
            if (l.second.size() > 4 && !f.empty() && !re_add_dir(add, "Lang/" + l.first + "/Packfile/" + f, l.second.substr(4), files)) { err = L"Cannot read the " + mw_w(l.first) + L" voices of " + mw_w(re_label(o.first)); return false; }
        }
        for (int j = 0; j < RE_COLS; j++) {
            const std::string& v = o.second.dmg[j];
            if (v.empty() || !v.compare(0, 4, "mod:")) continue;
            std::string f = psp_colour_folder(k, j);
            if (f.empty() || !re_dmg_export(add, "Packfile/" + f + "/1_p_dmg", v, files)) { err = L"Cannot read the files of " + mw_w(re_label(o.first)) + L" " + RE_DMG_N(j); return false; }
        }
        for (int j = 0; j < RE_COLS; j++) {
            std::string f = psp_colour_folder(k, j);
            if (!o.second.tex1[j].empty() && (f.empty() || !re_tex1_export(add, "Packfile/" + f + "/1_p", o.first, j, files))) { err = L"Cannot copy the textures of " + mw_w(re_label(o.first)) + L" " + RE_TEX1_N(j); return false; }
        }
        for (int j = 0; o.second.pcm && j < RE_COLS; j++) {   // Pre CMAN of an original in a new mod: six empty cameras per colour
            std::string f = psp_colour_folder(k, j);
            if (f.empty()) continue;
            for (const char* nm : RE_PCM_N) { std::vector<uint8_t> e; files++; if (!add("Packfile/" + f + "/1_p/pre_cman/" + nm, e)) { err = L"Cannot write the Pre CMAN files of " + mw_w(re_label(o.first)); return false; } }
        }
    }
    return true;
}

// ---------------- creation ----------------
static void re_init() {
    std::vector<int> sys_of_menu;
    psp_roster_vanilla(re_vanilla, re_name_ids, sys_of_menu);
    re_sys_of_menu = sys_of_menu;
    if (re_vanilla.empty() || re_vanilla.back()[0] != -1) re_vanilla.push_back({-1});
    re_vbase.clear();
    for (auto& c : re_vanilla) if (c[0] >= 0) re_vbase.insert(c[0]);
    re_bt3.clear();
    if (FILE* f = fopen("../../games/ttt/bt3_names.txt", "r")) {
        char line[256];
        while (fgets(line, sizeof(line), f)) { std::string s = line; while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back(); if (!s.empty()) re_bt3.push_back(s); }
        fclose(f);
    }
    re_renders_scan();
    re_reset_all();
}
static void re_fill_mods(const std::string& root) {
    HWND b = GetDlgItem(cm, IDR_MOD);
    SendMessageW(b, CB_RESETCONTENT, 0, 0);
    SendMessageW(b, CB_ADDSTRING, 0, (LPARAM)L"New mod");
    for (auto& m : mods_list(root)) SendMessageW(b, CB_ADDSTRING, 0, (LPARAM)mw_w(m.id).c_str());
    SendMessageW(b, CB_SETCURSEL, 0, 0);
}
