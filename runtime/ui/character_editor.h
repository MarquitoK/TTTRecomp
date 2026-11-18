#pragma once
// Characters and maps window: exports or imports a part (costume, SFX, voices, map) with tools/personajes.py
#include <shobjidl.h>

enum { IDP_TYPE = 4000, IDP_LIST, IDP_PART, IDP_EXP_FOLDER, IDP_EXP_PAK, IDP_IMPORT, IDP_NOTICE, IDP_RESTART, IDP_CLOSE, IDP_TITLE, IDP_PART_LABEL, IDP_IMP_FOLDER, IDP_ZONE };
struct PmUnidad { std::string type, key, nombre; std::vector<std::string> parts; };
static HWND pm = nullptr, pm_type = nullptr, pm_list = nullptr, pm_part = nullptr, pm_notice = nullptr, pm_zone = nullptr;
static std::vector<PmUnidad> pm_units, pm_view;
static bool pm_busy = false;
static const int PM_N = 5;
static const char* PM_TYPES[PM_N] = {"personajes", "extra", "historia", "objetos", "mapas"};
static const wchar_t* PM_TYPES_N[PM_N] = {L"Characters", L"Extra characters", L"Story characters", L"Story objects", L"Maps"};

static std::wstring pm_part_n(const std::string& p) {
    if (p.compare(0, 6, "color_") == 0) return L"Color " + mw_w(p.substr(6));
    if (p == "sfx") return L"Sounds (SFX)";
    if (p == "voces") return L"Voices";
    if (p == "mapa") return L"Stage";
    return L"Full";
}

static bool ends_with_pm(const std::string& s, const char* f) { size_t n = strlen(f); return s.size() >= n && s.compare(s.size() - n, n, f) == 0; }

static HWND pm_ctl(const wchar_t* cls, const wchar_t* txt, DWORD st, int id, HFONT f = nullptr) {
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st, 0, 0, 10, 10, pm, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)(f ? f : theme_f_normal), TRUE);
    return c;
}

static void pm_move(int id, int x, int y, int w, int h) { MoveWindow(GetDlgItem(pm, id), E(x), E(y), E(w), E(h), TRUE); }

// fluid layout: list on the left grows with the window, fixed 240 action column on the right
static void pm_layout() {
    RECT rc; GetClientRect(pm, &rc);
    int W = (int)(rc.right / theme_scale), H = (int)(rc.bottom / theme_scale), cx = W - 256;
    pm_move(IDP_TITLE, 16, 14, cx - 32, 26);
    pm_move(IDP_TYPE, 16, 50, cx - 32, 300);
    pm_move(IDP_LIST, 16, 86, cx - 32, H - 150);
    pm_move(IDP_PART_LABEL, cx, 52, 240, 20);
    pm_move(IDP_PART, cx, 76, 240, 300);
    pm_move(IDP_EXP_FOLDER, cx, 122, 240, 32);
    pm_move(IDP_EXP_PAK, cx, 162, 240, 32);
    pm_move(IDP_IMPORT, cx, 214, 240, 32);
    pm_move(IDP_IMP_FOLDER, cx, 254, 240, 32);
    pm_move(IDP_ZONE, cx, 298, 240, std::max(60, H - 298 - 64));
    pm_move(IDP_NOTICE, 16, H - 48, W - 240, 32);
    pm_move(IDP_RESTART, W - 216, H - 48, 100, 32);
    pm_move(IDP_CLOSE, W - 106, H - 48, 90, 32);
    LVCOLUMNW c; memset(&c, 0, sizeof(c)); c.mask = LVCF_WIDTH; c.cx = E(cx - 36) - GetSystemMetrics(SM_CXVSCROLL);
    SendMessageW(pm_list, LVM_SETCOLUMNW, 0, (LPARAM)&c);
    InvalidateRect(pm, nullptr, TRUE);
}

static std::wstring pm_quote(const std::string& s) { std::wstring w = mw_w(s); for (auto& c : w) if (c == L'"') c = L'\''; return L"\"" + w + L"\""; }

// runs the Python tool without blocking the window; the output goes to a temp file
static bool pm_run(const std::wstring& args, std::string& output) {
    wchar_t tmp[MAX_PATH], file_path[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    GetTempFileNameW(tmp, L"ttt", 0, file_path);
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof(sa); sa.lpSecurityDescriptor = nullptr; sa.bInheritHandle = TRUE;
    HANDLE f = CreateFileW(file_path, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { output = "cannot create a temporary file"; return false; }
    const char* e = getenv("TTT_PYTHON");
    const char* as = getenv("TTT_ASSETS");
    std::wstring pys[3] = {e && *e ? mw_w(e) : L"", L"python3", L"python"};
    bool launched = false, ok = false;
    for (auto& py : pys) {
        if (py.empty()) continue;
        std::wstring line = L"\"" + py + L"\" \"../../tools/characters.py\" " + (as && *as && strcmp(as, "0") ? L"--assets " + pm_quote(as) + L" " : L"") + args;
        STARTUPINFOW si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = f; si.hStdError = f; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION pi;
        std::vector<wchar_t> cmd(line.begin(), line.end()); cmd.push_back(0);
        if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) continue;
        launched = true;
        pm_busy = true;
        SetCursor(LoadCursor(nullptr, IDC_WAIT));
        while (WaitForSingleObject(pi.hProcess, 30) == WAIT_TIMEOUT) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { if (msg.message == WM_QUIT) { abierta = false; break; } TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (!abierta) break;
        }
        pm_busy = false;
        DWORD exit_code = 1; GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        ok = exit_code == 0;
        break;
    }
    CloseHandle(f);
    output.clear();
    if (FILE* r = _wfopen(file_path, L"rb")) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), r)) > 0) output.append(b, n); fclose(r); }
    DeleteFileW(file_path);
    if (!launched) output = "Python not found. Install it or set TTT_PYTHON to its path.";
    return launched && ok;
}

static void pm_notify(const std::wstring& t) { SetWindowTextW(pm_notice, t.c_str()); }

// last useful output line, to show a short error
static std::wstring pm_error(const std::string& s) {
    std::string l;
    size_t p = 0;
    while (p < s.size()) {
        size_t f = s.find('\n', p);
        std::string x = s.substr(p, f == std::string::npos ? std::string::npos : f - p);
        while (!x.empty() && (x.back() == '\r' || x.back() == ' ')) x.pop_back();
        if (!x.empty()) l = x;
        p = f == std::string::npos ? s.size() : f + 1;
    }
    return mw_w(l);
}

static const PmUnidad* pm_sel() {
    int k = (int)SendMessageW(pm_list, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
    return k >= 0 && k < (int)pm_view.size() ? &pm_view[k] : nullptr;
}

static std::string pm_part_sel() {
    const PmUnidad* u = pm_sel();
    int k = (int)SendMessageW(pm_part, CB_GETCURSEL, 0, 0);
    return u && k >= 0 && k < (int)u->parts.size() ? u->parts[k] : "";
}

static void pm_status() {
    const PmUnidad* u = pm_sel();
    std::string p = pm_part_sel();
    EnableWindow(pm_part, u && u->parts.size() > 1);
    EnableWindow(GetDlgItem(pm, IDP_EXP_FOLDER), !p.empty());
    EnableWindow(GetDlgItem(pm, IDP_EXP_PAK), !p.empty() && p != "voces");
    EnableWindow(GetDlgItem(pm, IDP_IMPORT), !p.empty() && p != "voces");
    EnableWindow(GetDlgItem(pm, IDP_IMP_FOLDER), !p.empty());
}

static void pm_parts() {
    std::string before = pm_part_sel();
    SendMessageW(pm_part, CB_RESETCONTENT, 0, 0);
    const PmUnidad* u = pm_sel();
    int sel = 0;
    if (u) for (size_t i = 0; i < u->parts.size(); i++) { SendMessageW(pm_part, CB_ADDSTRING, 0, (LPARAM)pm_part_n(u->parts[i]).c_str()); if (u->parts[i] == before) sel = (int)i; }
    SendMessageW(pm_part, CB_SETCURSEL, sel, 0);
    pm_status();
}

static void pm_fill() {
    int t = (int)SendMessageW(pm_type, CB_GETCURSEL, 0, 0);
    if (t < 0 || t >= PM_N) t = 0;
    pm_view.clear();
    SendMessageW(pm_list, LVM_DELETEALLITEMS, 0, 0);
    for (auto& u : pm_units) {
        if (u.type != PM_TYPES[t]) continue;
        std::wstring n = mw_w(u.nombre);
        LVITEMW it; memset(&it, 0, sizeof(it)); it.mask = LVIF_TEXT; it.iItem = (int)pm_view.size(); it.pszText = &n[0];
        SendMessageW(pm_list, LVM_INSERTITEMW, 0, (LPARAM)&it);
        pm_view.push_back(u);
    }
    if (!pm_view.empty()) ListView_SetItemState(pm_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    pm_parts();
}

static bool pm_load() {
    std::string s;
    bool ok = pm_run(L"--listar", s);
    pm_units.clear();
    size_t p = 0;
    while (ok && p < s.size()) {
        size_t f = s.find('\n', p);
        std::string l = s.substr(p, f == std::string::npos ? std::string::npos : f - p);
        p = f == std::string::npos ? s.size() : f + 1;
        while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.pop_back();
        std::vector<std::string> c;
        for (size_t a = 0, b; ; a = b + 1) { b = l.find('\t', a); c.push_back(l.substr(a, b == std::string::npos ? std::string::npos : b - a)); if (b == std::string::npos) break; }
        if (c.size() < 5) continue;
        PmUnidad u; u.type = c[0]; u.key = c[1]; u.nombre = c[2];
        for (size_t a = 0, b; ; a = b + 1) { b = c[4].find(',', a); std::string x = c[4].substr(a, b == std::string::npos ? std::string::npos : b - a); if (!x.empty()) u.parts.push_back(x); if (b == std::string::npos) break; }
        pm_units.push_back(u);
    }
    if (!ok || pm_units.empty()) { pm_notify(pm_error(s)); return false; }
    return true;
}

// ---------------- explorer dialogs ----------------
static std::string pm_dialog(bool save, bool folder, const wchar_t* title, const std::wstring& nombre, const wchar_t* filter_n, const wchar_t* filter) {
    std::string r;
    IFileDialog* d = nullptr;
    if (FAILED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, save ? IID_IFileSaveDialog : IID_IFileOpenDialog, (void**)&d))) return r;
    DWORD op = 0; d->GetOptions(&op);
    d->SetOptions(op | FOS_FORCEFILESYSTEM | (folder ? FOS_PICKFOLDERS : 0) | (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
    d->SetTitle(title);
    if (!nombre.empty()) d->SetFileName(nombre.c_str());
    if (filter) { COMDLG_FILTERSPEC f = {filter_n, filter}; d->SetFileTypes(1, &f); if (save) d->SetDefaultExtension(filter + 2); }
    IShellItem* it = nullptr;
    if (SUCCEEDED(d->Show(pm)) && SUCCEEDED(d->GetResult(&it))) {
        PWSTR p = nullptr;
        if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &p))) { r = mw_u8(p); CoTaskMemFree(p); }
        it->Release();
    }
    d->Release();
    return r;
}

static std::wstring pm_base(const PmUnidad& u, const std::string& p) {
    std::wstring n = mw_w(u.nombre) + L" " + pm_part_n(p), r;
    for (wchar_t c : n) r += (c == L'/' || c == L'\\' || c == L':' || c == L'*' || c == L'?' || c == L'"' || c == L'<' || c == L'>' || c == L'|' || c == L'(' || c == L')') ? L' ' : c;
    std::wstring s;
    for (wchar_t c : r) { if (c == L' ') { if (!s.empty() && s.back() != L'_') s += L'_'; } else s += c; }
    while (!s.empty() && s.back() == L'_') s.pop_back();
    return s;
}

static void pm_export(bool pak) {
    const PmUnidad* u = pm_sel();
    std::string p = pm_part_sel();
    if (!u || p.empty() || pm_busy) return;
    std::string dest;
    if (pak) dest = pm_dialog(true, false, L"Export as .pak", pm_base(*u, p) + L".pak", L"Pak", L"*.pak");
    else {
        std::string base = pm_dialog(false, true, L"Choose where to export", L"", nullptr, nullptr);
        if (!base.empty()) dest = base + "\\" + mw_u8(pm_base(*u, p).c_str());
    }
    if (dest.empty()) return;
    pm_notify(L"Exporting...");
    std::string s;
    bool ok = pm_run(L"--exportar " + pm_quote(u->key) + L" " + pm_quote(p) + L" " + pm_quote(dest) + (pak ? L" --pak" : L""), s);
    if (ok) pm_notify(L"Exported");
    else pm_notify(L"Could not export: " + pm_error(s));
}

static bool pm_is_pak(const std::string& r) {
    std::string b = r; for (auto& c : b) c = (char)tolower((unsigned char)c);
    return ends_with_pm(b, ".pak") || ends_with_pm(b, ".pck1");
}

// empty source: chosen with the explorer; dropped: checked to be a .pak, .pck1 or a folder
static void pm_import(std::string src_path, bool folder) {
    const PmUnidad* u = pm_sel();
    std::string p = pm_part_sel();
    if (!u || p.empty() || pm_busy) return;
    if (src_path.empty()) {
        if (!folder && p == "voces") { pm_notify(L"Voices are imported as a folder"); return; }
        src_path = folder ? pm_dialog(false, true, L"Import folder", L"", nullptr, nullptr) : pm_dialog(false, false, L"Import .pak", L"", L"Pak", L"*.pak;*.pck1");
        if (src_path.empty()) return;
    } else {
        std::error_code ec;
        bool dir = std::filesystem::is_directory(std::filesystem::u8path(src_path), ec);
        if (!dir && !pm_is_pak(src_path)) { pm_notify(L"Drop a .pak, a .pck1 or a folder"); return; }
        if (!dir && p == "voces") { pm_notify(L"Voices are imported as a folder"); return; }
        if (MessageBoxW(pm, (L"Replace " + mw_w(u->nombre) + L", " + pm_part_n(p) + L"?").c_str(), L"Import", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    }
    pm_notify(L"Importing...");
    std::string s;
    bool ok = pm_run(L"--importar " + pm_quote(u->key) + L" " + pm_quote(p) + L" " + pm_quote(src_path), s);
    if (ok) { pm_notify(L"Imported. Restart the game to apply."); ShowWindow(GetDlgItem(pm, IDP_RESTART), SW_SHOW); }
    else pm_notify(L"Could not import: " + pm_error(s));
}

static void pm_close() {
    if (!pm) return;
    EnableWindow(ventana, TRUE);
    SetForegroundWindow(ventana);
    theme_primary.erase(GetDlgItem(pm, IDP_IMPORT));
    theme_primary.erase(GetDlgItem(pm, IDP_RESTART));
    theme_primary.erase(GetDlgItem(pm, IDP_IMP_FOLDER));
    DestroyWindow(pm);
    pm = nullptr;
}

static LRESULT CALLBACK pm_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == 0x02E0) return theme_dpi_change(h, w, l);
    if (m == WM_COMMAND && pm) {
        int id = LOWORD(w), n = HIWORD(w);
        if (id == IDP_TYPE && n == CBN_SELCHANGE) pm_fill();
        else if (id == IDP_PART && n == CBN_SELCHANGE) pm_status();
        else if (n == BN_CLICKED) {
            if (id == IDP_EXP_FOLDER) pm_export(false);
            else if (id == IDP_EXP_PAK) pm_export(true);
            else if (id == IDP_IMPORT) pm_import("", false);
            else if (id == IDP_IMP_FOLDER) pm_import("", true);
            else if (id == IDP_RESTART) { restart_requested = true; pm_close(); }
            else if (id == IDP_CLOSE) pm_close();
        }
        return 0;
    }
    if (m == WM_DRAWITEM) { theme_button((DRAWITEMSTRUCT*)l); return TRUE; }
    if (m == WM_NOTIFY) {
        NMHDR* nh = (NMHDR*)l;
        if (nh->idFrom == IDP_LIST && nh->code == NM_CUSTOMDRAW) return theme_list_draw((NMLVCUSTOMDRAW*)l, false);
        if (nh->idFrom == IDP_LIST && nh->code == LVN_ITEMCHANGED) { NMLISTVIEW* nv = (NMLISTVIEW*)l; if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED)) pm_parts(); }
        return 0;
    }
    if (m == WM_CTLCOLORSTATIC) {
        int id = GetDlgCtrlID((HWND)l);
        if (id == IDP_ZONE) {
            SetBkMode((HDC)w, TRANSPARENT); SetTextColor((HDC)w, THEME_TEXT2);
            return (LRESULT)theme_b_bg;
        }
        if (id == IDP_NOTICE || id == IDP_PART_LABEL) {
            SetBkMode((HDC)w, TRANSPARENT); SetBkColor((HDC)w, THEME_BG);
            SetTextColor((HDC)w, id == IDP_NOTICE ? THEME_ACCENT : THEME_TEXT2);
            return (LRESULT)theme_b_bg;
        }
    }
    { LRESULT r; if (theme_color(m, w, l, &r)) return r; }
    if (m == WM_SIZE && pm) { pm_layout(); return 0; }
    if (m == WM_GETMINMAXINFO) { MINMAXINFO* mm = (MINMAXINFO*)l; mm->ptMinTrackSize.x = E(620); mm->ptMinTrackSize.y = E(440); return 0; }
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT r; GetWindowRect(pm_list, &r); MapWindowPoints(nullptr, h, (POINT*)&r, 2);
        InflateRect(&r, 1, 1); theme_frame(dc, r);
        RECT z; GetWindowRect(pm_zone, &z); MapWindowPoints(nullptr, h, (POINT*)&z, 2);
        HPEN pz = CreatePen(PS_DOT, 1, THEME_BORDER);
        HGDIOBJ vb = SelectObject(dc, GetStockObject(NULL_BRUSH)), vp = SelectObject(dc, pz);
        RoundRect(dc, z.left, z.top, z.right, z.bottom, E(10), E(10));
        SelectObject(dc, vb); SelectObject(dc, vp); DeleteObject(pz);
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_DROPFILES) {
        HDROP hd = (HDROP)w;
        wchar_t fpath[4096];
        if (DragQueryFileW(hd, 0, fpath, 4096)) pm_import(mw_u8(fpath), false);
        DragFinish(hd);
        return 0;
    }
    if (m == WM_CLOSE) { if (!pm_busy) pm_close(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void pm_open() {
    theme_init(ventana);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = pm_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRecompPersonajes";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
        RegisterClassW(&wc);
        registered = true;
    }
    HANDLE ctx; ULONG_PTR cookie = 0;
    visual_styles(&ctx, &cookie);
    pm = theme_create(L"TTTRecompPersonajes", L"Characters and maps", 680, 500, WS_EX_ACCEPTFILES);
    if (pm) {
        SetWindowLongW(pm, GWL_STYLE, GetWindowLongW(pm, GWL_STYLE) | WS_THICKFRAME | WS_MAXIMIZEBOX);
        SetWindowPos(pm, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        pm_ctl(L"STATIC", L"Characters and maps", 0, IDP_TITLE, theme_f_title);
        pm_type = pm_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, IDP_TYPE);
        for (auto n : PM_TYPES_N) SendMessageW(pm_type, CB_ADDSTRING, 0, (LPARAM)n);
        SendMessageW(pm_type, CB_SETCURSEL, 0, 0);
        theme_control(pm_type, L"DarkMode_CFD");
        pm_list = pm_ctl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | WS_TABSTOP, IDP_LIST);
        ListView_SetExtendedListViewStyle(pm_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        theme_list(pm_list);
        LVCOLUMNW c; memset(&c, 0, sizeof(c)); c.mask = LVCF_WIDTH; c.cx = 100; SendMessageW(pm_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&c);
        pm_ctl(L"STATIC", L"Part", 0, IDP_PART_LABEL);
        pm_part = pm_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, IDP_PART);
        theme_control(pm_part, L"DarkMode_CFD");
        pm_ctl(L"BUTTON", L"Export folder", BS_OWNERDRAW | WS_TABSTOP, IDP_EXP_FOLDER);
        pm_ctl(L"BUTTON", L"Export .pak", BS_OWNERDRAW | WS_TABSTOP, IDP_EXP_PAK);
        theme_primary.insert(pm_ctl(L"BUTTON", L"Import .pak", BS_OWNERDRAW | WS_TABSTOP, IDP_IMPORT));
        theme_primary.insert(pm_ctl(L"BUTTON", L"Import folder", BS_OWNERDRAW | WS_TABSTOP, IDP_IMP_FOLDER));
        pm_zone = pm_ctl(L"STATIC", L"Drop a .pak or a folder here", SS_CENTER | SS_CENTERIMAGE, IDP_ZONE);
        pm_notice = pm_ctl(L"STATIC", L"", SS_CENTERIMAGE | SS_ENDELLIPSIS, IDP_NOTICE);
        theme_primary.insert(pm_ctl(L"BUTTON", L"Restart", BS_OWNERDRAW | WS_TABSTOP, IDP_RESTART));
        ShowWindow(GetDlgItem(pm, IDP_RESTART), SW_HIDE);
        pm_ctl(L"BUTTON", L"Close", BS_OWNERDRAW | WS_TABSTOP, IDP_CLOSE);
        pm_layout();
        DragAcceptFiles(pm, TRUE);
    }
    if (ctx != INVALID_HANDLE_VALUE) { DeactivateActCtx(0, cookie); ReleaseActCtx(ctx); }
    if (!pm) return;
    EnableWindow(ventana, FALSE);
    ShowWindow(pm, SW_SHOW);
    SetForegroundWindow(pm);
    pm_notify(L"Loading...");
    if (pm_load()) { pm_fill(); pm_notify(L""); }
}

static void characters_window() {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    pause_begin();
    pm_open();
    while (pm && abierta) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { abierta = false; break; }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && pm && !pm_busy) { pm_close(); break; }
            if (!pm || !IsDialogMessageW(pm, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        if (!pm || !abierta) break;
        theme_poll(pm);
        Sleep(8);
    }
    if (pm) pm_close();
    pause_end();
}
