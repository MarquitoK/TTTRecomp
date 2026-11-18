#pragma once
// Material editor (Tools > Material editor): per material of a character, Shaders (a BT3-style ramp added to the texture,
// saved as 1_p/NNN_toon.tpal, last entry of the colour pak) and Textures (mNN.png inside 1_p/011_textures.atex, port only).
#include "third_party/stb_image.h"

enum { IDT_LIST = 4300, IDT_COLOR, IDT_MATS, IDT_IMPORT, IDT_INVERT, IDT_COPYALL, IDT_NORIM, IDT_NOLIGHT, IDT_SAVE, IDT_CLOSE, IDT_TITLE, IDT_NOTICE, IDT_LABEL_C, IDT_LABEL_M, IDT_LABEL_R, IDT_MODE, IDT_TAB0, IDT_TAB1, IDT_TMATS, IDT_TIMPORT, IDT_TRANDOM, IDT_TOPEN, IDT_TPREV, IDT_TLABEL, IDT_GLOW, IDT_SW0 = 4340 };
struct TpUnit { std::string key, nombre; std::vector<std::string> parts, folders, models, src, bases; std::vector<bool> open; };
struct TxMat { int mat, part, flag; bool on, lower; std::string file; };
struct TpMat { int mat, part, flag; bool on; uint8_t flags, mode, glow; uint8_t rgba[64]; };
static const uint8_t TP_GLOW[] = {0, 0x1E, 0x3C, 0x4B, 0x64};   // BT3 glow levels (texture entry +0x2D)
static HWND tp = nullptr, tp_list = nullptr, tp_color = nullptr, tp_mats = nullptr, tp_notice = nullptr, tx_mats = nullptr;
static int tp_tab = 0;
static std::vector<TxMat> tx_m;
static std::vector<uint32_t> tx_prev; static int tx_prev_w = 0, tx_prev_h = 0;
void psp_mattex_live(uint32_t sig, int mat, const uint32_t* px, int w, int h);
static std::vector<TpUnit> tp_units;
static std::vector<TpMat> tp_m;
static std::string tp_folder;
static uint32_t tp_sig = 0;
static bool tp_live_on = false;
void psp_tpal_live(uint32_t sig, const uint8_t* d, int n);
uint32_t psp_tpal_sig(const uint8_t* m, size_t n);
static bool tp_busy = false, tp_dirty = false, tp_loading = false;

static HWND tp_ctl(const wchar_t* cls, const wchar_t* txt, DWORD st, int id, HFONT f = nullptr) {
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st, 0, 0, 10, 10, tp, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)(f ? f : theme_f_normal), TRUE);
    return c;
}
static void tp_move(int id, int x, int y, int w, int h) { MoveWindow(GetDlgItem(tp, id), E(x), E(y), E(w), E(h), TRUE); }
static void tp_notify(const std::wstring& t) { SetWindowTextW(tp_notice, t.c_str()); }

static void tp_layout() {
    RECT rc; GetClientRect(tp, &rc);
    int W = (int)(rc.right / theme_scale), H = (int)(rc.bottom / theme_scale), cx = 250;
    tp_move(IDT_TITLE, 16, 14, W - 32, 26);
    tp_move(IDT_LIST, 16, 50, cx - 32, H - 110);
    tp_move(IDT_LABEL_C, cx + 290, 50, 60, 22);
    tp_move(IDT_COLOR, cx + 354, 48, 140, 300);
    tp_move(IDT_LABEL_M, cx, 120, 400, 20);
    tp_move(IDT_MATS, cx, 142, W - cx - 16, H - 336);
    tp_move(IDT_LABEL_R, cx, H - 186, 300, 20);
    for (int i = 0; i < 16; i++) tp_move(IDT_SW0 + i, cx + i * 30, H - 164, 28, 28);
    tp_move(IDT_IMPORT, cx, H - 126, 160, 30);
    tp_move(IDT_INVERT, cx + 168, H - 126, 80, 30);
    tp_move(IDT_COPYALL, cx + 256, H - 126, 110, 30);
    tp_move(IDT_GLOW, cx + 374, H - 124, 126, 300);
    tp_move(IDT_NORIM, cx, H - 90, 120, 22);
    tp_move(IDT_NOLIGHT, cx + 130, H - 90, 190, 22);
    tp_move(IDT_MODE, cx + 330, H - 92, 170, 300);
    tp_move(IDT_TAB0, cx, 48, 130, 28);
    tp_move(IDT_TAB1, cx + 136, 48, 130, 28);
    tp_move(IDT_TLABEL, cx, 120, W - cx - 16, 20);
    tp_move(IDT_TMATS, cx, 142, W - cx - 196, H - 210);
    tp_move(IDT_TPREV, W - 176, 142, 160, 160);
    tp_move(IDT_TIMPORT, W - 176, 314, 160, 30);
    tp_move(IDT_TRANDOM, W - 176, 352, 160, 30);
    tp_move(IDT_TOPEN, W - 176, 390, 160, 30);
    tp_move(IDT_NOTICE, 16, H - 48, W - 260, 32);
    theme_primary.insert(GetDlgItem(tp, IDT_SAVE));
    tp_move(IDT_SAVE, W - 216, H - 48, 100, 32);
    tp_move(IDT_CLOSE, W - 106, H - 48, 90, 32);
    LVCOLUMNW c; memset(&c, 0, sizeof(c)); c.mask = LVCF_WIDTH;
    c.cx = E(cx - 36) - GetSystemMetrics(SM_CXVSCROLL); SendMessageW(tp_list, LVM_SETCOLUMNW, 0, (LPARAM)&c);
    c.cx = E(W - cx - 20) - GetSystemMetrics(SM_CXVSCROLL); SendMessageW(tp_mats, LVM_SETCOLUMNW, 0, (LPARAM)&c);
    c.cx = E(W - cx - 200) - GetSystemMetrics(SM_CXVSCROLL); SendMessageW(tx_mats, LVM_SETCOLUMNW, 0, (LPARAM)&c);
    InvalidateRect(tp, nullptr, TRUE);
}

static bool tp_run(const std::wstring& args, std::string& out) {
    wchar_t tmp[MAX_PATH], file_path[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    GetTempFileNameW(tmp, L"ttt", 0, file_path);
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof(sa); sa.lpSecurityDescriptor = nullptr; sa.bInheritHandle = TRUE;
    HANDLE f = CreateFileW(file_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { out = "cannot create a temporary file"; return false; }
    const char* e = getenv("TTT_PYTHON");
    const char* as = getenv("TTT_ASSETS");
    std::wstring pys[3] = {e && *e ? mw_w(e) : L"", L"python3", L"python"};
    bool launched = false, ok = false;
    for (auto& py : pys) {
        if (py.empty()) continue;
        std::wstring line = L"\"" + py + L"\" \"../../tools/tpal.py\" " + (as && *as && strcmp(as, "0") ? L"--assets " + pm_quote(as) + L" " : L"") + args;
        STARTUPINFOW si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = f; si.hStdError = f; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION pi;
        std::vector<wchar_t> cmd(line.begin(), line.end()); cmd.push_back(0);
        if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) continue;
        launched = true; tp_busy = true;
        SetCursor(LoadCursor(nullptr, IDC_WAIT));
        while (WaitForSingleObject(pi.hProcess, 30) == WAIT_TIMEOUT) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) { abierta = false; break; } TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (!abierta) break;
        }
        tp_busy = false;
        DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        ok = code == 0;
        break;
    }
    CloseHandle(f);
    out.clear();
    if (FILE* r = _wfopen(file_path, L"rb")) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), r)) > 0) out.append(b, n); fclose(r); }
    DeleteFileW(file_path);
    if (!launched) out = "Python not found. Install it or set TTT_PYTHON to its path.";
    return launched && ok;
}

static std::vector<std::string> tp_split(const std::string& l, char sep) {
    std::vector<std::string> c;
    for (size_t a = 0, b; ; a = b + 1) { b = l.find(sep, a); c.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a)); if (b == std::string::npos) break; }
    return c;
}

static bool tp_load_units() {
    std::string s;
    bool ok = tp_run(L"--list", s);
    tp_units.clear();
    for (std::string& l : tp_split(s, '\n')) {
        while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.pop_back();
        std::vector<std::string> c = tp_split(l, '\t');
        if (c.size() < 8) continue;
        TpUnit* u = nullptr;
        for (auto& x : tp_units) if (x.key == c[1]) u = &x;
        if (!u) { tp_units.push_back({c[1], c[2], {}, {}, {}, {}, {}, {}}); u = &tp_units.back(); }
        u->parts.push_back(c[3]); u->folders.push_back(c[4]); u->open.push_back(c[5] == "1"); u->models.push_back(c[6]); u->src.push_back(c[7]); u->bases.push_back(c.size() > 8 ? c[8] : "");
    }
    if (!ok || tp_units.empty()) { tp_notify(pm_error(s)); return false; }
    return true;
}

static const TpUnit* tp_sel() {
    int k = (int)SendMessageW(tp_list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    return k >= 0 && k < (int)tp_units.size() ? &tp_units[k] : nullptr;
}
static int tp_part_sel() { return (int)SendMessageW(tp_color, CB_GETCURSEL, 0, 0); }
static int tp_mat_sel() { return (int)SendMessageW(tp_mats, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); }

static bool tp_read_file(const std::string& path, std::vector<uint8_t>& d) {
    FILE* f = _wfopen(mw_w(path).c_str(), L"rb");
    if (!f) return false;
    uint8_t b[65536]; size_t n;
    while ((n = fread(b, 1, sizeof(b), f)) > 0) d.insert(d.end(), b, b + n);
    fclose(f);
    return true;
}
static uint32_t tp_u32(const std::vector<uint8_t>& d, size_t o) { return o + 4 <= d.size() ? (uint32_t)d[o] | (uint32_t)d[o + 1] << 8 | (uint32_t)d[o + 2] << 16 | (uint32_t)d[o + 3] << 24 : 0; }

// existing tpal file of the colour folder, or 1_p/999_toon.tpal (a number no real entry uses; the pak builder puts it last)
static std::string tp_tpal_path(const std::string& folder, bool* exists) {
    std::string dir = folder + "\\1_p", found;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((mw_w(dir) + L"\\*.tpal").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) { found = mw_u8(fd.cFileName); FindClose(h); }
    *exists = !found.empty();
    return dir + "\\" + (found.empty() ? std::string("999_toon.tpal") : found);
}

// the model as the game sees it: a loose pmdl, or entry 2 of a closed colour .pak
static bool tp_read_model(const std::string& path, std::vector<uint8_t>& d) {
    if (!tp_read_file(path, d)) return false;
    if (path.size() > 4 && _stricmp(path.c_str() + path.size() - 4, ".pak") == 0) {
        auto be = [&](size_t o) { return o + 4 <= d.size() ? (uint32_t)d[o] << 24 | (uint32_t)d[o + 1] << 16 | (uint32_t)d[o + 2] << 8 | d[o + 3] : 0; };
        uint32_t a = be(12) & ~3u, b = be(16) & ~3u;
        if (be(0) < 3 || a >= b || b > d.size()) return false;
        d = std::vector<uint8_t>(d.begin() + a, d.begin() + b);
    }
    return tp_u32(d, 0) == 0x6C644D70u;
}

// multiply mode: 128 = the texture as is; default two tones with a small highlight at the lit end
static void tp_default_ramp(uint8_t* r) { for (int i = 0; i < 16; i++) { uint8_t v = (uint8_t)(i < 7 ? 80 : i < 14 ? 128 : 150); r[i * 4] = v; r[i * 4 + 1] = v; r[i * 4 + 2] = v; r[i * 4 + 3] = 255; } }

static void tp_fill_mats();
static void tp_live();
static void tx_load();
static void tp_load_model() {
    if (tp_dirty && tp_sig) psp_tpal_live(tp_sig, nullptr, 0);
    tp_m.clear(); tp_folder.clear(); tp_dirty = false; tp_sig = 0;
    const TpUnit* u = tp_sel();
    int k = tp_part_sel();
    if (!u || k < 0 || k >= (int)u->folders.size()) { tp_fill_mats(); tx_load(); return; }
    tp_folder = u->folders[k];
    std::vector<uint8_t> d;
    if (!tp_read_model(u->models[k], d)) { tp_notify(L"No model found for this colour"); tp_fill_mats(); tx_load(); return; }
    tp_sig = psp_tpal_sig(d.data(), d.size());
    std::wstring where = u->src[k] == "assets" ? L"Editing the original assets" : L"Editing mod " + mw_w(u->src[k]);
    tp_notify(where + (u->open[k] ? L"" : L" (closed .pak: opens as a folder when you save)"));
    uint32_t n = tp_u32(d, 0x5C), pt = tp_u32(d, 0x60);
    for (uint32_t i = 0; i < n && n <= 64; i++) {
        size_t e = pt + 0x20 * i;
        if (e + 0x20 > d.size()) break;
        TpMat m; m.mat = d[e]; m.part = (int)i; m.flag = (int)(tp_u32(d, e + 0xC) & 0xFFFF); m.on = false; m.flags = 0; m.mode = 2; m.glow = 0;
        tp_default_ramp(m.rgba);
        bool dup = false; for (auto& x : tp_m) if (x.mat == m.mat) dup = true;
        if (!dup) tp_m.push_back(m);
    }
    bool exists = false;
    std::string tpal = tp_tpal_path(tp_folder, &exists);
    std::vector<uint8_t> t;
    if (exists && tp_read_file(tpal, t) && tp_u32(t, 0) == 0x4C415054u) {
        uint32_t cnt = t.size() >= 8 ? (uint32_t)t[6] | (uint32_t)t[7] << 8 : 0;
        for (uint32_t i = 0; i < cnt; i++) {
            size_t e = 8 + 72 * i;
            if (e + 72 > t.size()) break;
            for (auto& m : tp_m) if (m.mat == t[e]) { m.on = t[e + 1] != 0; m.mode = t[e + 1] == 1 ? 1 : 2; m.flags = t[e + 2]; m.glow = t[e + 3]; memcpy(m.rgba, &t[e + 8], 64); }
        }
    }
    tp_fill_mats();
    tx_load();
}

static void tp_show_ramp() {
    int k = tp_mat_sel();
    bool ok = k >= 0 && k < (int)tp_m.size();
    for (int i = 0; i < 16; i++) { EnableWindow(GetDlgItem(tp, IDT_SW0 + i), ok); InvalidateRect(GetDlgItem(tp, IDT_SW0 + i), nullptr, TRUE); }
    for (int id : {IDT_IMPORT, IDT_INVERT, IDT_COPYALL, IDT_NORIM, IDT_NOLIGHT, IDT_MODE, IDT_GLOW}) EnableWindow(GetDlgItem(tp, id), ok);
    SendDlgItemMessageW(tp, IDT_MODE, CB_SETCURSEL, ok && tp_m[k].mode == 1 ? 1 : 0, 0);
    int gs = 0;
    if (ok) for (int i = 1; i < 5; i++) if (tp_m[k].glow >= (TP_GLOW[i - 1] + TP_GLOW[i] + 1) / 2) gs = i;
    SendDlgItemMessageW(tp, IDT_GLOW, CB_SETCURSEL, gs, 0);
    CheckDlgButton(tp, IDT_NORIM, ok && (tp_m[k].flags & 1) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(tp, IDT_NOLIGHT, ok && (tp_m[k].flags & 2) ? BST_CHECKED : BST_UNCHECKED);
    wchar_t b[96];
    if (ok) swprintf(b, 96, L"Ramp of material %d: shadow (left) to light (right)", tp_m[k].mat); else wcscpy(b, L"Ramp");
    SetWindowTextW(GetDlgItem(tp, IDT_LABEL_R), b);
}

static void tp_fill_mats() {
    tp_loading = true;
    SendMessageW(tp_mats, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < tp_m.size(); i++) {
        wchar_t b[96];
        swprintf(b, 96, L"Material %d  (part %d%s)", tp_m[i].mat, tp_m[i].part, tp_m[i].flag == 6 ? L", face" : L"");
        LVITEMW it; memset(&it, 0, sizeof(it)); it.mask = LVIF_TEXT; it.iItem = (int)i; it.pszText = b;
        SendMessageW(tp_mats, LVM_INSERTITEMW, 0, (LPARAM)&it);
        ListView_SetCheckState(tp_mats, (int)i, tp_m[i].on);
    }
    if (!tp_m.empty()) ListView_SetItemState(tp_mats, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    tp_loading = false;
    tp_show_ramp();
}

static void tp_fill_colors() {
    SendMessageW(tp_color, CB_RESETCONTENT, 0, 0);
    const TpUnit* u = tp_sel();
    if (u) for (auto& p : u->parts) SendMessageW(tp_color, CB_ADDSTRING, 0, (LPARAM)pm_part_n(p).c_str());
    SendMessageW(tp_color, CB_SETCURSEL, 0, 0);
    tp_load_model();
}

static void tp_fill() {
    SendMessageW(tp_list, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < tp_units.size(); i++) {
        std::wstring n = mw_w(tp_units[i].nombre);
        LVITEMW it; memset(&it, 0, sizeof(it)); it.mask = LVIF_TEXT; it.iItem = (int)i; it.pszText = &n[0];
        SendMessageW(tp_list, LVM_INSERTITEMW, 0, (LPARAM)&it);
    }
    if (!tp_units.empty()) ListView_SetItemState(tp_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    tp_fill_colors();
}

// BT3 shading palette (16x16 CT32, CSM1) indexed per light step by BT3's light texture (resident_battle_param.pak member 1)
static const uint8_t tp_bt3_idx[16] = {247, 231, 216, 199, 183, 167, 151, 135, 119, 103, 87, 71, 55, 38, 23, 7};
static bool tp_import_png(const std::string& path, uint8_t* out) {
    std::vector<uint8_t> d;
    if (!tp_read_file(path, d)) return false;
    int w = 0, h = 0, n = 0;
    uint8_t* px = stbi_load_from_memory(d.data(), (int)d.size(), &w, &h, &n, 4);
    if (!px) return false;
    bool ok = false;
    if (w == 16 && h == 16) {
        uint32_t lin[256], best[16] = {};
        int bestlum = -1;
        for (int flip = 0; flip < 2; flip++) {   // exports may be bottom-up: keep the orientation whose lit end is brighter
            for (int y = 0; y < 16; y++) memcpy(lin + y * 16, px + (flip ? 15 - y : y) * 64, 64);
            uint32_t r[16];
            for (int i = 0; i < 16; i++) { int k = tp_bt3_idx[i]; r[i] = lin[(k & ~0x18) | ((k & 8) << 1) | ((k & 16) >> 1)]; }
            int lum = (r[15] & 255) + ((r[15] >> 8) & 255) + ((r[15] >> 16) & 255);
            if (lum > bestlum) { bestlum = lum; memcpy(best, r, sizeof(best)); }
        }
        for (int i = 0; i < 16; i++) { memcpy(out + i * 4, &best[i], 3); out[i * 4 + 3] = 255; }
        ok = true;
    } else if (h == 1 || w == 1) {   // a plain strip: resampled to 16
        int len = w * h;
        for (int i = 0; i < 16; i++) { int s = (int)((i + 0.5) * len / 16); memcpy(out + i * 4, px + s * 4, 4); out[i * 4 + 3] = 255; }
        ok = true;
    }
    stbi_image_free(px);
    return ok;
}

static void tp_import() {
    int k = tp_mat_sel();
    if (k < 0 || k >= (int)tp_m.size()) return;
    HWND keep = pm; pm = tp;
    pause_begin();
    std::string f = pm_dialog(false, false, L"BT3 palette image (16x16) or a ramp strip", L"", L"PNG", L"*.png");
    pause_end();
    pm = keep;
    if (f.empty()) return;
    uint8_t r[64];
    if (!tp_import_png(f, r)) { tp_notify(L"Could not read that image as a palette or ramp"); return; }
    memcpy(tp_m[k].rgba, r, 64); tp_m[k].on = true; tp_m[k].mode = 1; ListView_SetCheckState(tp_mats, k, TRUE);
    tp_dirty = true; tp_show_ramp(); tp_live(); tp_notify(L"BT3 palette imported through the BT3 light table (Add mode)");
}

static std::vector<uint8_t> tp_bytes() {
    std::vector<uint8_t> t = {'T', 'P', 'A', 'L', 1, 0, 0, 0};
    int n = 0;
    for (auto& m : tp_m) {
        if (!m.on && m.flags == 0 && !m.glow) continue;
        uint8_t e[72] = {0};
        e[0] = (uint8_t)m.mat; e[1] = m.on ? m.mode : 0; e[2] = m.flags; e[3] = m.glow;
        memcpy(e + 8, m.rgba, 64);
        t.insert(t.end(), e, e + 72); n++;
    }
    t[6] = (uint8_t)n; t[7] = (uint8_t)(n >> 8);
    return t;
}
static void tp_live() {   // what the editor shows goes to the game at once
    if (!tp_sig) return;
    std::vector<uint8_t> t = tp_bytes();
    psp_tpal_live(tp_sig, t.data(), (int)t.size());
    tp_live_on = true;
}
static bool tp_ensure_open() {   // a closed colour .pak becomes a folder in the same place (mod or assets)
    const TpUnit* u = tp_sel();
    int k = tp_part_sel();
    if (u && k >= 0 && k < (int)u->open.size() && !u->open[k]) {
        std::string out;
        tp_notify(L"Opening the .pak as a folder...");
        pause_begin();
        bool opened = tp_run(L"--open " + pm_quote(u->key) + L" " + pm_quote(u->parts[k]), out);
        pause_end();
        if (!opened) { tp_notify(L"Could not open the pak: " + pm_error(out)); return false; }
        TpUnit& uu = tp_units[u - &tp_units[0]];
        uu.open[k] = true;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((mw_w(uu.folders[k]) + L"\\1_p\\002*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) { uu.models[k] = uu.folders[k] + "\\1_p\\" + mw_u8(fd.cFileName); FindClose(h); }
        std::vector<TpMat> keep = tp_m;
        bool was = tp_dirty;
        tp_dirty = false;
        tp_load_model();
        if (tp_m.empty()) return false;
        tp_m = keep; tp_dirty = was;
        tp_fill_mats();
    }
    return true;
}
static void tp_save() {
    if (tp_folder.empty() || tp_busy) return;
    if (!tp_ensure_open()) return;
    bool exists = false;
    std::string path = tp_tpal_path(tp_folder, &exists);
    std::vector<uint8_t> t = tp_bytes();
    tp_live();
    if (t[6] == 0 && t[7] == 0) { DeleteFileW(mw_w(path).c_str()); tp_dirty = false; tp_notify(L"No material enabled: palette file removed"); return; }
    CreateDirectoryW(mw_w(tp_folder + "\\1_p").c_str(), nullptr);
    FILE* f = _wfopen(mw_w(path).c_str(), L"wb");
    if (!f) { tp_notify(L"Could not write " + mw_w(path)); return; }
    fwrite(t.data(), 1, t.size(), f); fclose(f);
    tp_dirty = false;
    tp_notify(L"Saved in " + mw_w(path));
}

// ---- Textures tab: 1_p/011_textures.atex/mNN.png, one image per material, drawn by the port only ----
static std::string tx_atex_in(const std::string& dir1p, bool* loose) {   // entry 11 of 1_p: folder or loose png
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((mw_w(dir1p) + L"\\011*").c_str(), &fd);
    std::string r;
    *loose = false;
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        std::string n = mw_u8(fd.cFileName), ln = n;
        for (auto& ch : ln) ch = (char)tolower((unsigned char)ch);
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ln.size() > 5 && ln.compare(ln.size() - 5, 5, ".atex") == 0) { r = n; *loose = false; break; }
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && ln.find(".atex") != std::string::npos && ln.size() > 4 && ln.compare(ln.size() - 4, 4, ".png") == 0) { r = n; *loose = true; }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return r;
}
static std::string tx_mat_file(const std::string& atex, int mat) {   // mNN.png or mNN@Kx.png
    wchar_t pat[32]; swprintf(pat, 32, L"\\m%02d*.png", mat);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((mw_w(atex) + pat).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return "";
    std::string r = atex + "\\" + mw_u8(fd.cFileName);
    FindClose(h);
    return r;
}
static std::string tx_target_atex(bool* loose) { std::string d = tp_folder + "\\1_p", n = tx_atex_in(d, loose); return n.empty() ? "" : d + "\\" + n; }
static std::string tx_base_atex() {
    const TpUnit* u = tp_sel(); int k = tp_part_sel();
    if (!u || k < 0 || k >= (int)u->bases.size() || u->bases[k].empty() || _stricmp(u->bases[k].c_str(), tp_folder.c_str()) == 0) return "";
    bool loose = false; std::string d = u->bases[k] + "\\1_p", n = tx_atex_in(d, &loose);
    return n.empty() || loose ? "" : d + "\\" + n;
}

static void tx_preview_load() {
    tx_prev.clear(); tx_prev_w = tx_prev_h = 0;
    int k = (int)SendMessageW(tx_mats, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (k >= 0 && k < (int)tx_m.size() && !tx_m[k].file.empty()) {
        std::vector<uint8_t> d;
        int w = 0, h = 0, n = 0;
        if (tp_read_file(tx_m[k].file, d)) if (uint8_t* px = stbi_load_from_memory(d.data(), (int)d.size(), &w, &h, &n, 4)) {
            tx_prev.resize((size_t)w * h);
            for (size_t i = 0; i < tx_prev.size(); i++) tx_prev[i] = (uint32_t)px[i * 4 + 2] | (uint32_t)px[i * 4 + 1] << 8 | (uint32_t)px[i * 4] << 16;   // BGR for GDI
            tx_prev_w = w; tx_prev_h = h;
            stbi_image_free(px);
        }
    }
    for (int id : {IDT_TIMPORT, IDT_TRANDOM}) EnableWindow(GetDlgItem(tp, id), k >= 0 && k < (int)tx_m.size());
    InvalidateRect(GetDlgItem(tp, IDT_TPREV), nullptr, TRUE);
}
static void tx_preview_draw(DRAWITEMSTRUCT* d) {
    FillRect(d->hDC, &d->rcItem, theme_b_bg);
    if (!tx_prev.empty()) {
        BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = tx_prev_w; bi.bmiHeader.biHeight = -tx_prev_h; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        SetStretchBltMode(d->hDC, HALFTONE);
        StretchDIBits(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right - d->rcItem.left, d->rcItem.bottom - d->rcItem.top, 0, 0, tx_prev_w, tx_prev_h, tx_prev.data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    }
    HGDIOBJ vb = SelectObject(d->hDC, GetStockObject(NULL_BRUSH)), vp = SelectObject(d->hDC, theme_p_border);
    Rectangle(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom);
    SelectObject(d->hDC, vb); SelectObject(d->hDC, vp);
}

static void tx_fill() {
    tp_loading = true;
    SendMessageW(tx_mats, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < tx_m.size(); i++) {
        wchar_t b[160];
        std::wstring f = tx_m[i].file.empty() ? L"" : mw_w(tx_m[i].file.substr(tx_m[i].file.find_last_of('\\') + 1));
        swprintf(b, 160, L"Material %d  (part %d%s)%s%s%s", tx_m[i].mat, tx_m[i].part, tx_m[i].flag == 6 ? L", face" : L"", f.empty() ? L"" : L"   ", f.c_str(), tx_m[i].lower ? L" (from a lower layer)" : L"");
        LVITEMW it; memset(&it, 0, sizeof(it)); it.mask = LVIF_TEXT; it.iItem = (int)i; it.pszText = b;
        SendMessageW(tx_mats, LVM_INSERTITEMW, 0, (LPARAM)&it);
        ListView_SetCheckState(tx_mats, (int)i, tx_m[i].on);
    }
    if (!tx_m.empty()) ListView_SetItemState(tx_mats, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    tp_loading = false;
    tx_preview_load();
}
static void tx_load() {
    tx_m.clear();
    bool loose = false;
    std::string atex = tx_target_atex(&loose), base = tx_base_atex();
    for (auto& m : tp_m) {
        TxMat t; t.mat = m.mat; t.part = m.part; t.flag = m.flag; t.lower = false;
        if (!atex.empty() && !loose) t.file = tx_mat_file(atex, m.mat);
        if (t.file.empty() && !base.empty()) { t.file = tx_mat_file(base, m.mat); t.lower = !t.file.empty(); }
        t.on = !t.file.empty();
        tx_m.push_back(t);
    }
    if (tx_mats) tx_fill();
}

static void tx_crc_tab(uint32_t* t) { for (uint32_t n = 0; n < 256; n++) { uint32_t c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[n] = c; } }
static bool tx_write_png(const std::string& path, const std::vector<uint8_t>& rgba, int w, int h) {   // stored deflate: small code, any viewer reads it
    static uint32_t ct[256]; static bool ready = false; if (!ready) { tx_crc_tab(ct); ready = true; }
    std::vector<uint8_t> raw; raw.reserve((size_t)(w * 4 + 1) * h);
    for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgba.begin() + (size_t)y * w * 4, rgba.begin() + (size_t)(y + 1) * w * 4); }
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t o = 0; o < raw.size() || o == 0; o += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - o);
        z.push_back(o + n >= raw.size() ? 1 : 0); z.push_back((uint8_t)n); z.push_back((uint8_t)(n >> 8)); z.push_back((uint8_t)~n); z.push_back((uint8_t)(~n >> 8));
        z.insert(z.end(), raw.begin() + o, raw.begin() + o + n);
        if (o + n >= raw.size()) break;
    }
    uint32_t ad = b << 16 | a; for (int i = 3; i >= 0; i--) z.push_back((uint8_t)(ad >> (i * 8)));
    std::vector<uint8_t> f = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    auto chunk = [&](const char* t, const std::vector<uint8_t>& d) {
        for (int i = 3; i >= 0; i--) f.push_back((uint8_t)(d.size() >> (i * 8)));
        size_t s = f.size(); f.insert(f.end(), t, t + 4); f.insert(f.end(), d.begin(), d.end());
        uint32_t c = 0xFFFFFFFFu; for (size_t i = s; i < f.size(); i++) c = ct[(c ^ f[i]) & 255] ^ (c >> 8); c ^= 0xFFFFFFFFu;
        for (int i = 3; i >= 0; i--) f.push_back((uint8_t)(c >> (i * 8)));
    };
    std::vector<uint8_t> hd(13, 0);
    for (int i = 0; i < 4; i++) { hd[3 - i] = (uint8_t)(w >> (i * 8)); hd[7 - i] = (uint8_t)(h >> (i * 8)); }
    hd[8] = 8; hd[9] = 6;
    chunk("IHDR", hd); chunk("IDAT", z); chunk("IEND", {});
    FILE* o = _wfopen(mw_w(path).c_str(), L"wb");
    if (!o) return false;
    bool ok = fwrite(f.data(), 1, f.size(), o) == f.size();
    fclose(o);
    return ok;
}

static void tx_live_file(int mat, const std::string& file) {   // what the file holds goes to the game at once
    if (!tp_sig) return;
    std::vector<uint8_t> d; int w = 0, h = 0, n = 0;
    if (file.empty() || !tp_read_file(file, d)) { psp_mattex_live(tp_sig, mat, nullptr, 0, 0); return; }
    uint8_t* px = stbi_load_from_memory(d.data(), (int)d.size(), &w, &h, &n, 4);
    if (!px) { psp_mattex_live(tp_sig, mat, nullptr, 0, 0); return; }
    std::vector<uint32_t> v((size_t)w * h);
    for (size_t i = 0; i < v.size(); i++) v[i] = (uint32_t)px[i * 4] | (uint32_t)px[i * 4 + 1] << 8 | (uint32_t)px[i * 4 + 2] << 16 | (uint32_t)((px[i * 4 + 3] + 1) >> 1) << 24;   // alpha 0..128 like the character textures
    stbi_image_free(px);
    psp_mattex_live(tp_sig, mat, v.data(), w, h);
}

// the colour's 011_textures.atex becomes a folder with the atlas as 01.png (moved if it is here, copied from assets if not)
static std::string tx_prepare() {
    if (!tp_ensure_open()) return "";
    std::string dir1p = tp_folder + "\\1_p";
    CreateDirectoryW(mw_w(tp_folder).c_str(), nullptr);
    CreateDirectoryW(mw_w(dir1p).c_str(), nullptr);
    bool loose = false;
    std::string n = tx_atex_in(dir1p, &loose);
    if (!n.empty() && !loose) return dir1p + "\\" + n;
    auto split = [](const std::string& f, std::string& stem, std::string& suf) {   // 011_textures.atex@2x.png -> 011_textures.atex, @2x
        std::string l = f; for (auto& ch : l) ch = (char)tolower((unsigned char)ch);
        size_t a = l.find(".atex"); stem = f.substr(0, a + 5); suf = f.substr(a + 5, f.size() - a - 5 - 4);
    };
    std::string stem, suf;
    if (!n.empty()) {
        split(n, stem, suf);
        std::string dir = dir1p + "\\" + stem;
        if (!CreateDirectoryW(mw_w(dir).c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return "";
        if (!MoveFileW(mw_w(dir1p + "\\" + n).c_str(), mw_w(dir + "\\01" + suf + ".png").c_str())) return "";
        return dir;
    }
    const TpUnit* u = tp_sel(); int k = tp_part_sel();
    std::string src;
    if (u && k >= 0 && k < (int)u->bases.size() && !u->bases[k].empty()) {
        bool bl = false; std::string bd = u->bases[k] + "\\1_p", bn = tx_atex_in(bd, &bl);
        if (!bn.empty() && bl) { split(bn, stem, suf); src = bd + "\\" + bn; }
        else if (!bn.empty()) {
            stem = bn;
            WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((mw_w(bd + "\\" + bn) + L"\\01*.png").c_str(), &fd);
            if (h != INVALID_HANDLE_VALUE) { std::string f = mw_u8(fd.cFileName); src = bd + "\\" + bn + "\\" + f; suf = f.substr(2, f.size() - 6); FindClose(h); }
        }
    }
    if (src.empty()) return "";
    std::string dir = dir1p + "\\" + stem;
    CreateDirectoryW(mw_w(dir).c_str(), nullptr);
    if (!CopyFileW(mw_w(src).c_str(), mw_w(dir + "\\01" + suf + ".png").c_str(), TRUE)) return "";
    return dir;
}

static void tx_enable(int i, const std::string& import_path) {
    if (i < 0 || i >= (int)tx_m.size()) return;
    std::string dir = tx_prepare();
    if (dir.empty()) { tp_notify(L"Could not prepare 011_textures.atex as a folder"); tx_load(); return; }
    int mat = tx_m[i].mat;
    std::string old = tx_mat_file(dir, mat);
    if (!old.empty()) DeleteFileW(mw_w(old).c_str());
    char nm[32];
    std::string file;
    if (!import_path.empty()) {
        std::string b = import_path.substr(import_path.find_last_of("\\/") + 1), lb = b, sc;
        for (auto& ch : lb) ch = (char)tolower((unsigned char)ch);
        size_t at = lb.rfind('@'); if (at != std::string::npos && lb.size() > at + 6 && lb.compare(lb.size() - 6, 6, "x.png") == 0) sc = b.substr(at, b.size() - at - 4);
        snprintf(nm, sizeof(nm), "m%02d%s.png", mat, sc.c_str());
        file = dir + "\\" + nm;
        if (!CopyFileW(mw_w(import_path).c_str(), mw_w(file).c_str(), FALSE)) { tp_notify(L"Could not copy the image"); return; }
    } else {
        std::vector<uint8_t> px((size_t)256 * 256 * 4);
        uint32_t r = (uint32_t)GetTickCount() * 2654435761u ^ (uint32_t)mat * 40503u;
        uint8_t c[3] = {(uint8_t)(64 + (r & 127)), (uint8_t)(64 + ((r >> 8) & 127)), (uint8_t)(64 + ((r >> 16) & 127))};
        for (size_t p = 0; p < px.size(); p += 4) { px[p] = c[0]; px[p + 1] = c[1]; px[p + 2] = c[2]; px[p + 3] = 255; }
        snprintf(nm, sizeof(nm), "m%02d.png", mat);
        file = dir + "\\" + nm;
        if (!tx_write_png(file, px, 256, 256)) { tp_notify(L"Could not write " + mw_w(file)); return; }
    }
    tx_live_file(mat, file);
    tx_load();
    ListView_SetItemState(tx_mats, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    tp_notify(L"Material " + std::to_wstring(mat) + L" uses " + mw_w(file.substr(file.find_last_of('\\') + 1)) + L" (256 x 256 maps the whole UV; bigger is fine)");
}
static void tx_disable(int i) {
    if (i < 0 || i >= (int)tx_m.size()) return;
    int mat = tx_m[i].mat;
    bool loose = false;
    std::string atex = tx_target_atex(&loose), f = atex.empty() || loose ? "" : tx_mat_file(atex, mat);
    if (!f.empty()) DeleteFileW(mw_w(f).c_str());
    tx_live_file(mat, "");
    tx_load();
    ListView_SetItemState(tx_mats, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    tp_notify(tx_m[i].on ? L"Removed here, but a lower layer still has a texture for this material" : L"Material " + std::to_wstring(mat) + L" back to the original texture");
}
static void tx_import() {
    int k = (int)SendMessageW(tx_mats, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    if (k < 0 || k >= (int)tx_m.size()) return;
    HWND keep = pm; pm = tp;
    pause_begin();
    std::string f = pm_dialog(false, false, L"Texture for this material (256 x 256 or bigger)", L"", L"PNG", L"*.png");
    pause_end();
    pm = keep;
    if (!f.empty()) tx_enable(k, f);
}
static void tx_open_folder() {
    bool loose = false;
    std::string d = tx_target_atex(&loose);
    if (d.empty() || loose) d = tp_folder + "\\1_p";
    ShellExecuteW(tp, L"open", mw_w(d).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
static void tp_set_tab(int t) {
    tp_tab = t;
    static const int SH[] = {IDT_LABEL_M, IDT_MATS, IDT_LABEL_R, IDT_IMPORT, IDT_INVERT, IDT_COPYALL, IDT_NORIM, IDT_NOLIGHT, IDT_MODE, IDT_GLOW, IDT_SAVE};
    static const int TX[] = {IDT_TLABEL, IDT_TMATS, IDT_TIMPORT, IDT_TRANDOM, IDT_TOPEN, IDT_TPREV};
    for (int id : SH) ShowWindow(GetDlgItem(tp, id), t == 0 ? SW_SHOW : SW_HIDE);
    for (int i = 0; i < 16; i++) ShowWindow(GetDlgItem(tp, IDT_SW0 + i), t == 0 ? SW_SHOW : SW_HIDE);
    for (int id : TX) ShowWindow(GetDlgItem(tp, id), t == 1 ? SW_SHOW : SW_HIDE);
    for (int i = 0; i < 2; i++) { HWND b = GetDlgItem(tp, IDT_TAB0 + i); if (i == t) theme_primary.insert(b); else theme_primary.erase(b); InvalidateRect(b, nullptr, TRUE); }
    InvalidateRect(tp, nullptr, TRUE);
}

static void tp_close() {
    if (!tp) return;
    if (tp_dirty) {
        pause_begin();
        int r = MessageBoxW(tp, L"Discard the unsaved shader changes?", L"Material editor", MB_YESNO | MB_ICONQUESTION);
        pause_end();
        if (r != IDYES) return;
    }
    if (tp_dirty && tp_sig) psp_tpal_live(tp_sig, nullptr, 0);   // back to what the pak holds
    KillTimer(tp, 1);
    SetForegroundWindow(ventana);
    theme_primary.erase(GetDlgItem(tp, IDT_SAVE));
    for (int i = 0; i < 2; i++) theme_primary.erase(GetDlgItem(tp, IDT_TAB0 + i));
    DestroyWindow(tp);
    tx_mats = nullptr;
    tp = nullptr;
}

static void tp_swatch(DRAWITEMSTRUCT* d) {
    int i = (int)d->CtlID - IDT_SW0, k = tp_mat_sel();
    bool ok = k >= 0 && k < (int)tp_m.size() && (d->itemState & ODS_DISABLED) == 0;
    const uint8_t* c = ok ? tp_m[k].rgba + i * 4 : nullptr;
    HBRUSH b = CreateSolidBrush(ok ? RGB(c[0], c[1], c[2]) : THEME_PANEL);
    FillRect(d->hDC, &d->rcItem, b); DeleteObject(b);
    HGDIOBJ vb = SelectObject(d->hDC, GetStockObject(NULL_BRUSH)), vp = SelectObject(d->hDC, theme_p_border);
    Rectangle(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom);
    SelectObject(d->hDC, vb); SelectObject(d->hDC, vp);
}

static void tp_pick(int i) {
    int k = tp_mat_sel();
    if (k < 0 || k >= (int)tp_m.size()) return;
    static COLORREF custom[16] = {0};
    uint8_t* c = tp_m[k].rgba + i * 4;
    CHOOSECOLORW cc; memset(&cc, 0, sizeof(cc));
    cc.lStructSize = sizeof(cc); cc.hwndOwner = tp; cc.rgbResult = RGB(c[0], c[1], c[2]); cc.lpCustColors = custom; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    pause_begin();
    BOOL picked = ChooseColorW(&cc);
    pause_end();
    if (!picked) return;
    c[0] = GetRValue(cc.rgbResult); c[1] = GetGValue(cc.rgbResult); c[2] = GetBValue(cc.rgbResult); c[3] = 255;
    tp_m[k].on = true; ListView_SetCheckState(tp_mats, k, TRUE);
    tp_dirty = true; tp_show_ramp(); tp_live();
}

static LRESULT CALLBACK tp_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == 0x02E0) return theme_dpi_change(h, w, l);
    if (m == WM_COMMAND && tp) {
        int id = LOWORD(w), n = HIWORD(w);
        if (id == IDT_COLOR && n == CBN_SELCHANGE) tp_load_model();
        else if (id == IDT_MODE && n == CBN_SELCHANGE) {
            int k = tp_mat_sel();
            if (k >= 0 && k < (int)tp_m.size()) {   // a ramp made for the other mode means nothing here: start from that mode's default
                uint8_t md = SendDlgItemMessageW(tp, IDT_MODE, CB_GETCURSEL, 0, 0) == 1 ? 1 : 2;
                if (md != tp_m[k].mode) {
                    tp_m[k].mode = md;
                    if (md == 2) tp_default_ramp(tp_m[k].rgba);
                    else for (int i = 0; i < 16; i++) { uint8_t v = (uint8_t)(i < 7 ? 0 : i < 14 ? 24 : 48); tp_m[k].rgba[i * 4] = tp_m[k].rgba[i * 4 + 1] = tp_m[k].rgba[i * 4 + 2] = v; tp_m[k].rgba[i * 4 + 3] = 255; }
                    tp_notify(md == 2 ? L"Multiply: 128 keeps the texture, lower darkens, higher brightens" : L"Add: black adds nothing, brighter colours add light");
                }
                tp_dirty = true; tp_show_ramp(); tp_live();
            }
        }
        else if (id == IDT_GLOW && n == CBN_SELCHANGE) {
            int k = tp_mat_sel(), g = (int)SendDlgItemMessageW(tp, IDT_GLOW, CB_GETCURSEL, 0, 0);
            if (k >= 0 && k < (int)tp_m.size() && g >= 0 && g < 5) { tp_m[k].glow = TP_GLOW[g]; tp_dirty = true; tp_live(); tp_notify(L"BT3 glow: seen in battle with Graphics \"BT3 glow\" on"); }
        }
        else if (n == BN_CLICKED) {
            int k = tp_mat_sel();
            if (id >= IDT_SW0 && id < IDT_SW0 + 16) tp_pick(id - IDT_SW0);
            else if (id == IDT_IMPORT) tp_import();
            else if (id == IDT_INVERT && k >= 0 && k < (int)tp_m.size()) { uint8_t r[64]; for (int i = 0; i < 16; i++) memcpy(r + i * 4, tp_m[k].rgba + (15 - i) * 4, 4); memcpy(tp_m[k].rgba, r, 64); tp_dirty = true; tp_show_ramp(); tp_live(); }
            else if (id == IDT_COPYALL && k >= 0 && k < (int)tp_m.size()) { for (size_t i = 0; i < tp_m.size(); i++) if ((int)i != k) { memcpy(tp_m[i].rgba, tp_m[k].rgba, 64); } tp_dirty = true; tp_notify(L"Ramp copied to every material (enable them in the list)"); }
            else if ((id == IDT_NORIM || id == IDT_NOLIGHT) && k >= 0 && k < (int)tp_m.size()) {
                uint8_t bit = id == IDT_NORIM ? 1 : 2;
                if (IsDlgButtonChecked(tp, id) == BST_CHECKED) tp_m[k].flags |= bit; else tp_m[k].flags &= (uint8_t)~bit;
                tp_dirty = true; tp_live();
            }
            else if (id == IDT_SAVE) tp_save();
            else if (id == IDT_TAB0 || id == IDT_TAB1) tp_set_tab(id - IDT_TAB0);
            else if (id == IDT_TIMPORT) tx_import();
            else if (id == IDT_TRANDOM) { int t = (int)SendMessageW(tx_mats, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED); tx_enable(t, ""); }
            else if (id == IDT_TOPEN) tx_open_folder();
            else if (id == IDT_CLOSE) tp_close();
        }
        return 0;
    }
    if (m == WM_DRAWITEM) {
        DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)l;
        if ((int)d->CtlID >= IDT_SW0 && (int)d->CtlID < IDT_SW0 + 16) tp_swatch(d); else if ((int)d->CtlID == IDT_TPREV) tx_preview_draw(d); else theme_button(d);
        return TRUE;
    }
    if (m == WM_NOTIFY) {
        NMHDR* nh = (NMHDR*)l;
        if (nh->idFrom == IDT_LIST && nh->code == NM_CUSTOMDRAW) return theme_list_draw((NMLVCUSTOMDRAW*)l, false);
        if (nh->idFrom == IDT_MATS && nh->code == NM_CUSTOMDRAW) return theme_list_draw((NMLVCUSTOMDRAW*)l, true);
        if (nh->idFrom == IDT_TMATS && nh->code == NM_CUSTOMDRAW) return theme_list_draw((NMLVCUSTOMDRAW*)l, true);
        if (nh->idFrom == IDT_TMATS && nh->code == LVN_ITEMCHANGED && !tp_loading) {
            NMLISTVIEW* nv = (NMLISTVIEW*)l;
            if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED)) tx_preview_load();
            if ((nv->uChanged & LVIF_STATE) && ((nv->uNewState ^ nv->uOldState) & LVIS_STATEIMAGEMASK) && nv->iItem >= 0 && nv->iItem < (int)tx_m.size()) {
                bool on = ListView_GetCheckState(tx_mats, nv->iItem) != 0;
                if (on != tx_m[nv->iItem].on) PostMessageW(tp, WM_APP + 7, (WPARAM)nv->iItem, on ? 1 : 0);   // not inside the list's own notification
            }
        }
        if (nh->idFrom == IDT_LIST && nh->code == LVN_ITEMCHANGED) { NMLISTVIEW* nv = (NMLISTVIEW*)l; if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED)) tp_fill_colors(); }
        if (nh->idFrom == IDT_MATS && nh->code == LVN_ITEMCHANGED && !tp_loading) {
            NMLISTVIEW* nv = (NMLISTVIEW*)l;
            if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED)) tp_show_ramp();
            if ((nv->uChanged & LVIF_STATE) && ((nv->uNewState ^ nv->uOldState) & LVIS_STATEIMAGEMASK) && nv->iItem >= 0 && nv->iItem < (int)tp_m.size()) {
                bool on = ListView_GetCheckState(tp_mats, nv->iItem) != 0;
                if (on != tp_m[nv->iItem].on) { tp_m[nv->iItem].on = on; tp_dirty = true; tp_live(); }
            }
        }
        return 0;
    }
    if (m == WM_CTLCOLORSTATIC) {
        int id = GetDlgCtrlID((HWND)l);
        if (id == IDT_NOTICE || id == IDT_LABEL_C || id == IDT_LABEL_M || id == IDT_LABEL_R || id == IDT_NORIM || id == IDT_NOLIGHT || id == IDT_TLABEL) {
            SetBkMode((HDC)w, TRANSPARENT); SetBkColor((HDC)w, THEME_BG);
            SetTextColor((HDC)w, id == IDT_NOTICE ? THEME_ACCENT : id == IDT_LABEL_R ? THEME_TEXT : THEME_TEXT2);
            return (LRESULT)theme_b_bg;
        }
    }
    { LRESULT r; if (theme_color(m, w, l, &r)) return r; }
    if (m == WM_SIZE && tp) { tp_layout(); return 0; }
    if (m == WM_GETMINMAXINFO) { MINMAXINFO* mm = (MINMAXINFO*)l; mm->ptMinTrackSize.x = E(860); mm->ptMinTrackSize.y = E(600); return 0; }
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        for (HWND lv : {tp_list, tp_tab == 0 ? tp_mats : tx_mats}) { RECT r; GetWindowRect(lv, &r); MapWindowPoints(nullptr, h, (POINT*)&r, 2); InflateRect(&r, 1, 1); theme_frame(dc, r); }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_TIMER && tp) { theme_poll(tp); return 0; }
    if (m == WM_APP + 7 && tp) { if (l) tx_enable((int)w, ""); else tx_disable((int)w); return 0; }
    if (m == WM_CLOSE) { if (!tp_busy) tp_close(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void tp_open() {
    theme_init(ventana);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = tp_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRecompTpal";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
        RegisterClassW(&wc);
        registered = true;
    }
    HANDLE ctx; ULONG_PTR cookie = 0;
    visual_styles(&ctx, &cookie);
    tp = theme_create(L"TTTRecompTpal", L"Material editor", 860, 600, 0);
    if (tp) {
        SetWindowLongW(tp, GWL_STYLE, GetWindowLongW(tp, GWL_STYLE) | WS_THICKFRAME | WS_MAXIMIZEBOX);
        SetWindowPos(tp, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        tp_ctl(L"STATIC", L"Material editor", 0, IDT_TITLE, theme_f_title);
        tp_ctl(L"BUTTON", L"Shaders", BS_OWNERDRAW | WS_TABSTOP, IDT_TAB0);
        tp_ctl(L"BUTTON", L"Textures", BS_OWNERDRAW | WS_TABSTOP, IDT_TAB1);
        tp_list = tp_ctl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | WS_TABSTOP, IDT_LIST);
        ListView_SetExtendedListViewStyle(tp_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        theme_list(tp_list);
        LVCOLUMNW c; memset(&c, 0, sizeof(c)); c.mask = LVCF_WIDTH; c.cx = 100;
        SendMessageW(tp_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&c);
        tp_ctl(L"STATIC", L"Colour", SS_CENTERIMAGE, IDT_LABEL_C);
        tp_color = tp_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, IDT_COLOR);
        theme_control(tp_color, L"DarkMode_CFD");
        tp_ctl(L"STATIC", L"Materials (checked = BT3 ramp, unchecked = automatic)", 0, IDT_LABEL_M);
        tp_mats = tp_ctl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | WS_TABSTOP, IDT_MATS);
        ListView_SetExtendedListViewStyle(tp_mats, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);
        theme_list(tp_mats);
        SendMessageW(tp_mats, LVM_INSERTCOLUMNW, 0, (LPARAM)&c);
        tp_ctl(L"STATIC", L"Materials (checked = own texture mNN.png, unchecked = the original texture)", 0, IDT_TLABEL);
        tx_mats = tp_ctl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | WS_TABSTOP, IDT_TMATS);
        ListView_SetExtendedListViewStyle(tx_mats, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);
        theme_list(tx_mats);
        SendMessageW(tx_mats, LVM_INSERTCOLUMNW, 0, (LPARAM)&c);
        tp_ctl(L"STATIC", L"", SS_OWNERDRAW, IDT_TPREV);
        tp_ctl(L"BUTTON", L"Import texture...", BS_OWNERDRAW | WS_TABSTOP, IDT_TIMPORT);
        tp_ctl(L"BUTTON", L"New flat colour", BS_OWNERDRAW | WS_TABSTOP, IDT_TRANDOM);
        tp_ctl(L"BUTTON", L"Open folder", BS_OWNERDRAW | WS_TABSTOP, IDT_TOPEN);
        tp_ctl(L"STATIC", L"Ramp", 0, IDT_LABEL_R);
        for (int i = 0; i < 16; i++) tp_ctl(L"BUTTON", L"", BS_OWNERDRAW, IDT_SW0 + i);
        tp_ctl(L"BUTTON", L"Import BT3 palette...", BS_OWNERDRAW | WS_TABSTOP, IDT_IMPORT);
        tp_ctl(L"BUTTON", L"Invert", BS_OWNERDRAW | WS_TABSTOP, IDT_INVERT);
        tp_ctl(L"BUTTON", L"Copy to all", BS_OWNERDRAW | WS_TABSTOP, IDT_COPYALL);
        tp_ctl(L"BUTTON", L"No rim light", BS_AUTOCHECKBOX | WS_TABSTOP, IDT_NORIM);
        tp_ctl(L"BUTTON", L"No map or effects light", BS_AUTOCHECKBOX | WS_TABSTOP, IDT_NOLIGHT);
        HWND md = tp_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, IDT_MODE);
        SendMessageW(md, CB_ADDSTRING, 0, (LPARAM)L"Multiply (128 = texture)");
        SendMessageW(md, CB_ADDSTRING, 0, (LPARAM)L"Add (BT3)");
        theme_control(md, L"DarkMode_CFD");
        HWND gc = tp_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, IDT_GLOW);
        for (const wchar_t* g : {L"Glow: off", L"Glow: low", L"Glow: medium", L"Glow: high", L"Glow: max"}) SendMessageW(gc, CB_ADDSTRING, 0, (LPARAM)g);
        theme_control(gc, L"DarkMode_CFD");
        theme_control(GetDlgItem(tp, IDT_NORIM)); theme_control(GetDlgItem(tp, IDT_NOLIGHT));
        tp_notice = tp_ctl(L"STATIC", L"", SS_CENTERIMAGE | SS_ENDELLIPSIS, IDT_NOTICE);
        tp_ctl(L"BUTTON", L"Save", BS_OWNERDRAW | WS_TABSTOP, IDT_SAVE);
        tp_ctl(L"BUTTON", L"Close", BS_OWNERDRAW | WS_TABSTOP, IDT_CLOSE);
        tp_layout();
        tp_set_tab(tp_tab);
    }
    if (ctx != INVALID_HANDLE_VALUE) { DeactivateActCtx(0, cookie); ReleaseActCtx(ctx); }
    if (!tp) return;
    ShowWindow(tp, SW_SHOW);
    SetForegroundWindow(tp);
    SetTimer(tp, 1, 30, nullptr);
    tp_notify(L"Loading...");
    UpdateWindow(tp);
    pause_begin();
    bool ok = tp_load_units();
    pause_end();
    if (ok) { tp_fill(); if (GetWindowTextLengthW(tp_notice) == 0) tp_notify(L"Pause the battle (Start) and edit: changes show at once"); }
}

// not modal: the game thread pumps this window's messages while it keeps drawing, so every change shows live
static void tpal_window() {
    if (tp) { ShowWindow(tp, SW_SHOWNORMAL); SetForegroundWindow(tp); return; }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    tp_open();
}
