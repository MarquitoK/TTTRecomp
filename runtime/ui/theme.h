#pragma once
// Dark theme for the port windows: colors, fonts, hand-drawn buttons, menu bar and lists
#include <set>
#include <commctrl.h>

#define THEME_BG   RGB(27, 31, 39)
#define THEME_PANEL   RGB(38, 43, 53)
#define THEME_FIELD   RGB(30, 34, 43)
#define THEME_BORDER   RGB(58, 65, 80)
#define THEME_TEXT   RGB(230, 233, 239)
#define THEME_TEXT2  RGB(154, 163, 178)
#define THEME_ACCENT  RGB(242, 163, 58)
#define THEME_ACCENT2 RGB(217, 142, 44)
#define THEME_LINK  RGB(109, 182, 255)
#define THEME_SEL     RGB(51, 64, 79)
#define THEME_BAR   RGB(22, 25, 32)

static HBRUSH theme_b_bg, theme_b_panel, theme_b_field, theme_b_accent, theme_b_sel, theme_b_bar;
static HPEN theme_p_border, theme_p_accent;
static HFONT theme_f_normal, theme_f_bold, theme_f_title, theme_f_link;
static std::set<HWND> theme_primary, theme_exists, theme_switch, theme_modded;
static HWND theme_hot = nullptr;
static HICON theme_icon_big = nullptr, theme_icon_small = nullptr;
static double theme_scale = 1.0, theme_font_scale = 0;

// per-monitor DPI awareness before any window exists: Windows stops scaling the bitmap (blurry)
// and every window draws at the real pixels of its screen
static struct ThemeDpi { ThemeDpi() {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *Ctx)(HANDLE);
    typedef BOOL (WINAPI *Simple)();
    Ctx c = u ? (Ctx)GetProcAddress(u, "SetProcessDpiAwarenessContext") : nullptr;
    if (c && c((HANDLE)-4)) return;
    HMODULE sh = LoadLibraryW(L"shcore.dll");
    typedef HRESULT (WINAPI *Aw)(int);
    Aw a = sh ? (Aw)GetProcAddress(sh, "SetProcessDpiAwareness") : nullptr;
    if (a && a(2) == S_OK) return;
    Simple s = u ? (Simple)GetProcAddress(u, "SetProcessDPIAware") : nullptr;
    if (s) s();
} } theme_dpi_init;

static double theme_dpi_of(HWND h) {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    typedef UINT (WINAPI *Fn)(HWND);
    Fn f = u ? (Fn)GetProcAddress(u, "GetDpiForWindow") : nullptr;
    UINT d = f && h ? f(h) : 0;
    if (!d) { HDC dc = GetDC(nullptr); d = (UINT)GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(nullptr, dc); }
    return d ? d / 96.0 : 1.0;
}

static int E(int v) { return (int)(v * theme_scale + (v < 0 ? -0.5 : 0.5)); }

static void theme_fonts() {
    if (theme_font_scale == theme_scale) return;
    theme_font_scale = theme_scale;
    for (HFONT* f : {&theme_f_normal, &theme_f_bold, &theme_f_title, &theme_f_link}) if (*f) DeleteObject(*f);
    theme_f_normal = CreateFontW(E(-13), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    theme_f_bold = CreateFontW(E(-13), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    theme_f_title = CreateFontW(E(-17), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    theme_f_link = CreateFontW(E(-13), 0, 0, 0, FW_NORMAL, 0, 1, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

static void theme_init(HWND ref = nullptr) {
    if (ref) theme_scale = theme_dpi_of(ref);
    if (theme_b_bg) { theme_fonts(); return; }
    theme_b_bg = CreateSolidBrush(THEME_BG); theme_b_panel = CreateSolidBrush(THEME_PANEL); theme_b_field = CreateSolidBrush(THEME_FIELD);
    theme_b_accent = CreateSolidBrush(THEME_ACCENT); theme_b_sel = CreateSolidBrush(THEME_SEL); theme_b_bar = CreateSolidBrush(THEME_BAR);
    theme_p_border = CreatePen(PS_SOLID, 1, THEME_BORDER); theme_p_accent = CreatePen(PS_SOLID, 1, THEME_ACCENT);
    theme_fonts();
}

static DWORD theme_build() {
    static DWORD b = 0;
    if (b) return b;
    typedef LONG (WINAPI *Fn)(OSVERSIONINFOW*);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    Fn f = nt ? (Fn)GetProcAddress(nt, "RtlGetVersion") : nullptr;
    OSVERSIONINFOW v; memset(&v, 0, sizeof(v)); v.dwOSVersionInfoSize = sizeof(v);
    b = f && f(&v) == 0 ? v.dwBuildNumber : 1;
    return b;
}

// dark title bar (Windows 10 1809+) and dark popup menus (1903+)
static void theme_window(HWND h) {
    theme_init(ventana);
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    typedef HRESULT (WINAPI *Fn)(HWND, DWORD, LPCVOID, DWORD);
    Fn f = dwm ? (Fn)GetProcAddress(dwm, "DwmSetWindowAttribute") : nullptr;
    if (f) { BOOL si = TRUE; if (f(h, 20, &si, sizeof(si)) != S_OK) f(h, 19, &si, sizeof(si)); }
    static bool menus = false;
    if (!menus && theme_build() >= 18362) {
        menus = true;
        HMODULE ux = LoadLibraryW(L"uxtheme.dll");
        typedef int (WINAPI *Mode)(int);
        typedef void (WINAPI *Flush)();
        Mode mode = ux ? (Mode)GetProcAddress(ux, MAKEINTRESOURCEA(135)) : nullptr;
        Flush flush = ux ? (Flush)GetProcAddress(ux, MAKEINTRESOURCEA(136)) : nullptr;
        if (mode) mode(2);
        if (flush) flush();
    }
}

// system dark theme for scroll bars, lists and dropdowns
static void theme_control(HWND c, const wchar_t* name = L"DarkMode_Explorer") {
    HMODULE ux = LoadLibraryW(L"uxtheme.dll");
    typedef HRESULT (WINAPI *Fn)(HWND, LPCWSTR, LPCWSTR);
    Fn f = ux ? (Fn)GetProcAddress(ux, "SetWindowTheme") : nullptr;
    if (f) f(c, name, nullptr);
}

// background and text colors of system controls; returns true if it handled the message
static bool theme_color(UINT m, WPARAM w, LPARAM l, LRESULT* res) {
    HDC dc = (HDC)w;
    if (m == WM_CTLCOLORDLG) { *res = (LRESULT)theme_b_bg; return true; }
    if (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLORBTN) {
        wchar_t cls[32] = L"";
        GetClassNameW((HWND)l, cls, 32);
        bool field = !wcscmp(cls, L"Edit") || (GetWindowLongW((HWND)l, GWL_EXSTYLE) & WS_EX_CLIENTEDGE);
        SetBkMode(dc, TRANSPARENT);
        SetBkColor(dc, field ? THEME_FIELD : THEME_BG);
        SetTextColor(dc, IsWindowEnabled((HWND)l) ? THEME_TEXT : THEME_TEXT2);
        *res = (LRESULT)(field ? theme_b_field : theme_b_bg);
        return true;
    }
    if (m == WM_CTLCOLOREDIT || m == WM_CTLCOLORLISTBOX) {
        SetBkColor(dc, THEME_FIELD); SetTextColor(dc, THEME_TEXT);
        *res = (LRESULT)theme_b_field;
        return true;
    }
    return false;
}

// flat button: fill, 1 px border, 4 px corners; the primary one uses the accent color
static void theme_button(DRAWITEMSTRUCT* d) {
    HDC dc = d->hDC; RECT r = d->rcItem;
    if (theme_switch.count(d->hwndItem)) {   // toggle: pill track with a knob, label on the right; on = accent
        bool on = theme_primary.count(d->hwndItem) > 0, hot = d->hwndItem == theme_hot;
        FillRect(dc, &r, theme_b_bg);
        int h = E(14), w = E(26), x = r.left + E(2), y = (r.top + r.bottom - h) / 2;
        HBRUSH tb = CreateSolidBrush(on ? (hot ? RGB(255, 185, 87) : THEME_ACCENT) : (hot ? THEME_SEL : THEME_FIELD));
        HGDIOBJ ob = SelectObject(dc, tb), op = SelectObject(dc, on ? theme_p_accent : theme_p_border);
        RoundRect(dc, x, y, x + w, y + h, h, h);
        SelectObject(dc, ob); DeleteObject(tb);
        int kd = h - E(4), kx = on ? x + w - kd - E(2) : x + E(2);
        HBRUSH kb = CreateSolidBrush(on ? RGB(24, 18, 8) : THEME_TEXT2);
        ob = SelectObject(dc, kb);
        Ellipse(dc, kx, y + E(2), kx + kd, y + E(2) + kd);
        SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(kb);
        wchar_t t[128]; GetWindowTextW(d->hwndItem, t, 128);
        RECT tr = r; tr.left = x + w + E(6);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, on ? THEME_TEXT : THEME_TEXT2);
        HGDIOBJ vf = SelectObject(dc, theme_f_normal);
        DrawTextW(dc, t, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, vf);
        return;
    }
    bool primary = theme_primary.count(d->hwndItem) > 0, pressed = d->itemState & ODS_SELECTED, disabled = d->itemState & ODS_DISABLED, hot = d->hwndItem == theme_hot;
    bool exists = !primary && theme_exists.count(d->hwndItem) > 0, modded = primary && theme_modded.count(d->hwndItem) > 0;
    COLORREF bg = modded ? (pressed ? RGB(98, 70, 156) : hot ? RGB(146, 110, 216) : RGB(122, 88, 190)) : primary ? (pressed ? THEME_ACCENT2 : hot ? RGB(255, 185, 87) : THEME_ACCENT) : exists ? (pressed ? RGB(36, 70, 108) : hot ? RGB(58, 104, 156) : RGB(45, 86, 132)) : (pressed ? THEME_FIELD : hot ? THEME_SEL : THEME_PANEL);
    COLORREF text = disabled ? THEME_TEXT2 : modded ? RGB(255, 255, 255) : primary ? RGB(24, 18, 8) : THEME_TEXT;
    if (disabled) bg = modded ? RGB(70, 56, 100) : primary ? RGB(120, 92, 48) : exists ? RGB(36, 52, 72) : THEME_BG;
    FillRect(dc, &r, theme_b_bg);
    HBRUSH b = CreateSolidBrush(bg);
    HGDIOBJ vb = SelectObject(dc, b), vp = SelectObject(dc, (d->itemState & ODS_FOCUS) && !disabled ? theme_p_accent : theme_p_border);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, E(8), E(8));
    SelectObject(dc, vb); SelectObject(dc, vp); DeleteObject(b);
    wchar_t t[128]; GetWindowTextW(d->hwndItem, t, 128);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, text);
    HGDIOBJ vf = SelectObject(dc, primary ? theme_f_bold : theme_f_normal);
    DrawTextW(dc, t, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, vf);
}

// button hot state: polled from each window loop (children get the mouse, the parent does not)
static void theme_poll(HWND parent) {
    POINT p; GetCursorPos(&p);
    HWND h = nullptr;
    if (parent && IsWindow(parent)) { ScreenToClient(parent, &p); h = ChildWindowFromPointEx(parent, p, CWP_SKIPINVISIBLE | CWP_SKIPDISABLED); }
    wchar_t cls[32] = L"";
    if (h && h != parent) GetClassNameW(h, cls, 32);
    if (h == parent || (h && (wcscmp(cls, L"Button") || !(GetWindowLongW(h, GWL_STYLE) & BS_OWNERDRAW)))) h = nullptr;
    if (h == theme_hot) return;
    HWND before = theme_hot; theme_hot = h;
    if (before && IsWindow(before)) InvalidateRect(before, nullptr, TRUE);
    if (h) InvalidateRect(h, nullptr, TRUE);
}

// ---------------- menu bar ----------------
typedef union { struct { DWORD cx, cy; } rgsizeBar[2]; struct { DWORD cx, cy; } rgsizePopup[4]; } UAHMENUITEMMETRICS;
typedef struct { DWORD rgcx[4]; DWORD fUpdateMaxWidths : 2; } UAHMENUPOPUPMETRICS;
typedef struct { HMENU hmenu; HDC hdc; DWORD dwFlags; } UAHMENU;
typedef struct { int iPosition; UAHMENUITEMMETRICS umim; UAHMENUPOPUPMETRICS umpm; } UAHMENUITEM;
typedef struct { DRAWITEMSTRUCT dis; UAHMENU um; UAHMENUITEM umi; } UAHDRAWMENUITEM;
#define WM_UAHDRAWMENU 0x0091
#define WM_UAHDRAWMENUITEM 0x0092

static void theme_bar_line(HWND h) {
    MENUBARINFO mbi; mbi.cbSize = sizeof(mbi);
    if (!GetMenuBarInfo(h, OBJID_MENU, 0, &mbi)) return;
    RECT rw; GetWindowRect(h, &rw);
    RECT r = {mbi.rcBar.left - rw.left, mbi.rcBar.bottom - rw.top, mbi.rcBar.right - rw.left, mbi.rcBar.bottom - rw.top + 1};
    HDC dc = GetWindowDC(h);
    FillRect(dc, &r, theme_b_bar);
    ReleaseDC(h, dc);
}

static bool theme_menu_proc(HWND h, UINT m, WPARAM w, LPARAM l, LRESULT* res) {
    theme_init();
    if (m == WM_UAHDRAWMENU) {
        UAHMENU* pm = (UAHMENU*)l;
        MENUBARINFO mbi; mbi.cbSize = sizeof(mbi);
        if (!GetMenuBarInfo(h, OBJID_MENU, 0, &mbi)) return false;
        RECT rw; GetWindowRect(h, &rw);
        RECT r = mbi.rcBar; OffsetRect(&r, -rw.left, -rw.top);
        FillRect(pm->hdc, &r, theme_b_bar);
        *res = 0; return true;
    }
    if (m == WM_UAHDRAWMENUITEM) {
        UAHDRAWMENUITEM* p = (UAHDRAWMENUITEM*)l;
        wchar_t t[64] = L"";
        MENUITEMINFOW mii; memset(&mii, 0, sizeof(mii)); mii.cbSize = sizeof(mii); mii.fMask = MIIM_STRING; mii.dwTypeData = t; mii.cch = 64;
        GetMenuItemInfoW(p->um.hmenu, p->umi.iPosition, TRUE, &mii);
        bool hot = p->dis.itemState & (ODS_HOTLIGHT | ODS_SELECTED);
        RECT r = p->dis.rcItem;
        FillRect(p->um.hdc, &r, theme_b_bar);
        if (hot) { HGDIOBJ vb = SelectObject(p->um.hdc, theme_b_sel), vp = SelectObject(p->um.hdc, GetStockObject(NULL_PEN)); RoundRect(p->um.hdc, r.left + 1, r.top + 2, r.right, r.bottom - 1, 6, 6); SelectObject(p->um.hdc, vb); SelectObject(p->um.hdc, vp); }
        SetBkMode(p->um.hdc, TRANSPARENT);
        SetTextColor(p->um.hdc, (p->dis.itemState & ODS_GRAYED) ? THEME_TEXT2 : THEME_TEXT);
        HGDIOBJ vf = SelectObject(p->um.hdc, theme_f_normal);
        DrawTextW(p->um.hdc, t, -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | ((p->dis.itemState & ODS_NOACCEL) ? DT_HIDEPREFIX : 0));
        SelectObject(p->um.hdc, vf);
        *res = 0; return true;
    }
    if (m == WM_NCPAINT || m == WM_NCACTIVATE) {
        *res = DefWindowProcW(h, m, w, l);
        if (GetMenu(h)) theme_bar_line(h);
        return true;
    }
    return false;
}

// ---------------- list (ListView) ----------------
static WNDPROC theme_header_proc_old = nullptr;

static LRESULT CALLBACK theme_header_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        FillRect(dc, &rc, theme_b_panel);
        int n = (int)SendMessageW(h, HDM_GETITEMCOUNT, 0, 0);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, THEME_TEXT2);
        HGDIOBJ vf = SelectObject(dc, theme_f_bold), vp = SelectObject(dc, theme_p_border);
        for (int i = 0; i < n; i++) {
            RECT r; SendMessageW(h, HDM_GETITEMRECT, i, (LPARAM)&r);
            wchar_t t[128] = L"";
            HDITEMW hi; memset(&hi, 0, sizeof(hi)); hi.mask = HDI_TEXT; hi.pszText = t; hi.cchTextMax = 128;
            SendMessageW(h, HDM_GETITEMW, i, (LPARAM)&hi);
            RECT rt = {r.left + 8, r.top, r.right - 4, r.bottom};
            DrawTextW(dc, t, -1, &rt, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            MoveToEx(dc, r.right - 1, r.top + 4, nullptr); LineTo(dc, r.right - 1, r.bottom - 4);
        }
        MoveToEx(dc, rc.left, rc.bottom - 1, nullptr); LineTo(dc, rc.right, rc.bottom - 1);
        SelectObject(dc, vf); SelectObject(dc, vp);
        EndPaint(h, &ps);
        return 0;
    }
    return CallWindowProcW(theme_header_proc_old, h, m, w, l);
}

static void theme_list(HWND lv) {
    theme_control(lv);
    SendMessageW(lv, LVM_SETBKCOLOR, 0, THEME_FIELD);
    SendMessageW(lv, LVM_SETTEXTBKCOLOR, 0, THEME_FIELD);
    SendMessageW(lv, LVM_SETTEXTCOLOR, 0, THEME_TEXT);
    HWND header = (HWND)SendMessageW(lv, LVM_GETHEADER, 0, 0);
    if (header && !theme_header_proc_old) theme_header_proc_old = (WNDPROC)SetWindowLongPtrW(header, GWLP_WNDPROC, (LONG_PTR)theme_header_proc);
    else if (header) SetWindowLongPtrW(header, GWLP_WNDPROC, (LONG_PTR)theme_header_proc);
}

// selected rows in the selection color and hand-drawn checkboxes (12 px, accent when checked)
static LRESULT theme_list_draw(NMLVCUSTOMDRAW* cd, bool checks) {
    if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
    if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        int i = (int)cd->nmcd.dwItemSpec;
        bool sel = ListView_GetItemState(cd->nmcd.hdr.hwndFrom, i, LVIS_SELECTED) & LVIS_SELECTED;
        cd->clrText = THEME_TEXT; cd->clrTextBk = sel ? THEME_SEL : THEME_FIELD;
        return CDRF_NEWFONT | (checks ? CDRF_NOTIFYPOSTPAINT : 0);
    }
    if (cd->nmcd.dwDrawStage == CDDS_ITEMPOSTPAINT && checks) {
        int i = (int)cd->nmcd.dwItemSpec;
        RECT r; ListView_GetItemRect(cd->nmcd.hdr.hwndFrom, i, &r, LVIR_BOUNDS);
        bool checked = ListView_GetCheckState(cd->nmcd.hdr.hwndFrom, i) != 0, sel = ListView_GetItemState(cd->nmcd.hdr.hwndFrom, i, LVIS_SELECTED) & LVIS_SELECTED;
        RECT z = {r.left, r.top, r.left + E(20), r.bottom};
        FillRect(cd->nmcd.hdc, &z, sel ? theme_b_sel : theme_b_field);
        int cy = (r.top + r.bottom) / 2;
        RECT c = {r.left + E(4), cy - E(6), r.left + E(17), cy + E(7)};
        HGDIOBJ vb = SelectObject(cd->nmcd.hdc, checked ? theme_b_accent : theme_b_panel), vp = SelectObject(cd->nmcd.hdc, checked ? theme_p_accent : theme_p_border);
        RoundRect(cd->nmcd.hdc, c.left, c.top, c.right, c.bottom, 4, 4);
        if (checked) {
            HPEN p = CreatePen(PS_SOLID, 2, RGB(24, 18, 8));
            SelectObject(cd->nmcd.hdc, p);
            MoveToEx(cd->nmcd.hdc, c.left + E(3), cy, nullptr); LineTo(cd->nmcd.hdc, c.left + E(6), cy + E(3)); LineTo(cd->nmcd.hdc, c.right - E(3), cy - E(3));
            SelectObject(cd->nmcd.hdc, vp); DeleteObject(p);
        }
        SelectObject(cd->nmcd.hdc, vb); SelectObject(cd->nmcd.hdc, vp);
        return CDRF_DODEFAULT;
    }
    return CDRF_DODEFAULT;
}

static void theme_frame(HDC dc, RECT r) {
    HGDIOBJ vb = SelectObject(dc, GetStockObject(NULL_BRUSH)), vp = SelectObject(dc, theme_p_border);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, E(8), E(8));
    SelectObject(dc, vb); SelectObject(dc, vp);
}

static void theme_frame_e(HDC dc, int x, int y, int w, int h) { RECT r = {E(x), E(y), E(x + w), E(y + h)}; theme_frame(dc, r); }

// secondary window centered over the game window, already scaled to that screen DPI
static HWND theme_create(const wchar_t* class_name, const wchar_t* title, int cw, int ch, DWORD ex) {
    DWORD st = WS_CAPTION | WS_SYSMENU | WS_POPUP;
    RECT r = {0, 0, E(cw), E(ch)};
    AdjustWindowRectEx(&r, st, FALSE, ex);
    RECT g; GetWindowRect(ventana, &g);
    int w = r.right - r.left, h = r.bottom - r.top;
    int x = g.left + ((g.right - g.left) - w) / 2, y = g.top + ((g.bottom - g.top) - h) / 2;
    MONITORINFO mi; mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(ventana, MONITOR_DEFAULTTONEAREST), &mi)) {
        if (x + w > mi.rcWork.right) x = mi.rcWork.right - w;
        if (y + h > mi.rcWork.bottom) y = mi.rcWork.bottom - h;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top) y = mi.rcWork.top;
    }
    HWND v = CreateWindowExW(ex, class_name, title, st, x, y, w, h, ventana, nullptr, GetModuleHandle(nullptr), nullptr);
    if (v) theme_window(v);
    if (v && theme_icon_small) { SendMessageW(v, WM_SETICON, ICON_SMALL, (LPARAM)theme_icon_small); SendMessageW(v, WM_SETICON, ICON_BIG, (LPARAM)theme_icon_big); }
    return v;
}

// the window moved to a monitor with another scale: new fonts, rescaled children and the size Windows suggests
struct ThemeChild { HWND h; RECT r; int font; };
static BOOL CALLBACK theme_child_cb(HWND c, LPARAM l) {
    auto* v = (std::vector<ThemeChild>*)l;
    ThemeChild x; x.h = c;
    GetWindowRect(c, &x.r); MapWindowPoints(nullptr, GetParent(c), (POINT*)&x.r, 2);
    HFONT f = (HFONT)SendMessageW(c, WM_GETFONT, 0, 0);
    x.font = f == theme_f_bold ? 1 : f == theme_f_title ? 2 : f == theme_f_link ? 3 : 0;
    v->push_back(x);
    return TRUE;
}

static LRESULT theme_dpi_change(HWND h, WPARAM w, LPARAM l) {
    double dpi_new = HIWORD(w) / 96.0, k = dpi_new / theme_scale;
    std::vector<ThemeChild> children;
    EnumChildWindows(h, theme_child_cb, (LPARAM)&children);
    theme_scale = dpi_new;
    theme_fonts();
    HFONT fs[4] = {theme_f_normal, theme_f_bold, theme_f_title, theme_f_link};
    for (auto& x : children) {
        if (GetParent(x.h) != h) continue;
        MoveWindow(x.h, (int)(x.r.left * k), (int)(x.r.top * k), (int)((x.r.right - x.r.left) * k), (int)((x.r.bottom - x.r.top) * k), FALSE);
        SendMessageW(x.h, WM_SETFONT, (WPARAM)fs[x.font], FALSE);
        wchar_t cls[32] = L""; GetClassNameW(x.h, cls, 32);
        if (!wcscmp(cls, WC_LISTVIEWW)) for (int i = 0; i < 8; i++) { int a = (int)SendMessageW(x.h, LVM_GETCOLUMNWIDTH, i, 0); if (!a) break; SendMessageW(x.h, LVM_SETCOLUMNWIDTH, i, (int)(a * k)); }
    }
    RECT* r = (RECT*)l;
    SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
    InvalidateRect(h, nullptr, TRUE);
    return 0;
}
