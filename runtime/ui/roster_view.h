#pragma once
// Characters page of the mod creator: list of characters (left), header with the render and the forms, tabs General / Labels / Parameters.
// The cell grid lives in its own page (Select screen). Also draws the navigation rail and the tabs.
enum { IDV_LIST = 3800, IDV_HEAD, IDV_SEARCH };
static HWND rv_list = nullptr, rv_head = nullptr;
static std::vector<int> rv_rows;   // cell index, or -10 - group for a group caption
static int rv_scroll = 0, rv_hot = -1, rv_head_hot = -1;
static std::wstring rv_filter;
static const COLORREF RV_RAIL = RGB(22, 25, 32), RV_SELBG = RGB(66, 48, 24), RV_HOVER = RGB(44, 50, 62);

static bool rv_ochar_used(int s) {   // re_fchar fills re_ochar on select: only real content counts
    auto it = re_ochar.find(s);
    if (it == re_ochar.end()) return false;
    const ReChar& o = it->second;
    if (o.pcm) return true;
    for (auto& f : o.file) if (!f.empty()) return true;
    for (int j = 0; j < RE_COLS; j++) if (!o.dmg[j].empty() || !o.tex1[j].empty() || !o.bt3[j].empty()) return true;
    for (auto& l : o.lang) if (!l.second.empty()) return true;
    return false;
}
static bool rv_in_mod(int s) { return re_find(s) || re_orig.count(s) || rv_ochar_used(s) || re_lab_any(s) || re_fref.count(s) || re_refs.count(s) || pe_count(s) > 0; }
static void rv_fill() {
    rv_rows.clear();
    int last = (int)re_cells.size() - 1;
    for (int g = 0; g < 2; g++) {
        bool any = false;
        for (int i = 0; i < last; i++) {
            int s = re_cells[i][0];
            if (rv_in_mod(s) != (g == 0)) continue;
            if (!rv_filter.empty()) {
                std::wstring n = mw_w(re_label(s)), f = rv_filter;
                std::transform(n.begin(), n.end(), n.begin(), ::towlower); std::transform(f.begin(), f.end(), f.begin(), ::towlower);
                if (n.find(f) == std::wstring::npos) continue;
            }
            if (!any) { rv_rows.push_back(-10 - g); any = true; }
            rv_rows.push_back(i);
        }
    }
}
static int rv_row_h(int r) { return rv_rows[r] <= -10 ? 30 : 44; }
static int rv_row_y(int r) { int y = 4; for (int i = 0; i < r; i++) y += rv_row_h(i); return y; }
static int rv_total() { return rv_rows.empty() ? 0 : rv_row_y((int)rv_rows.size() - 1) + rv_row_h((int)rv_rows.size() - 1) + 4; }
static void rv_round(HDC dc, int x, int y, int w, int h, int r, COLORREF fill) {
    HBRUSH b = CreateSolidBrush(fill); HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, x, y, x + w + 1, y + h + 1, r, r);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
}
static void rv_text(HDC dc, const std::wstring& t, RECT r, COLORREF c, HFONT f, UINT fl = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS) {
    SetTextColor(dc, c); HGDIOBJ of = SelectObject(dc, f); DrawTextW(dc, t.c_str(), -1, &r, fl); SelectObject(dc, of);
}
static void rv_thumb(HDC dc, int s, int x, int y, int w, int h) {
    HBITMAP bmp = re_bitmap(s, re_kind(s, true));
    rv_round(dc, x, y, w, h, E(8), THEME_FIELD);
    if (!bmp) return;
    HDC im = CreateCompatibleDC(dc); HGDIOBJ o = SelectObject(im, bmp);
    SetStretchBltMode(dc, HALFTONE);
    StretchBlt(dc, x, y, w, h, im, 0, 0, E(RE_IW), E(RE_IH), SRCCOPY);
    SelectObject(im, o); DeleteDC(im);
}
static void rv_paint_list(HDC dc, RECT cr) {
    FillRect(dc, &cr, theme_b_bg);
    SetBkMode(dc, TRANSPARENT);
    for (int r = 0; r < (int)rv_rows.size(); r++) {
        int y = E(rv_row_y(r)) - rv_scroll, h = E(rv_row_h(r));
        if (y > cr.bottom || y + h < 0) continue;
        int i = rv_rows[r];
        if (i <= -10) {
            RECT t = {E(10), y + E(8), cr.right - E(8), y + h};
            rv_text(dc, i == -10 ? L"In this mod" : L"Game characters", t, THEME_TEXT2, theme_f_normal);
            continue;
        }
        bool sel = i == re_cur, hot = r == rv_hot;
        if (sel || hot) rv_round(dc, E(4), y + E(2), cr.right - E(8), h - E(4), E(10), sel ? RV_SELBG : RV_HOVER);
        int s = re_cells[i][0];
        rv_thumb(dc, s, E(10), y + E(6), E(56), E(32));
        RECT t = {E(74), y + E(4), cr.right - E(8), y + E(24)};
        rv_text(dc, mw_w(re_label(s)), t, sel ? RGB(255, 210, 150) : THEME_TEXT, sel ? theme_f_bold : theme_f_normal);
        int nf = (int)re_cells[i].size() - 1;
        std::wstring sub = re_find(s) ? L"New" : rv_in_mod(s) ? L"Edited" : L"Original";
        if (nf) sub += L"  \x00B7  " + std::to_wstring(nf) + (nf == 1 ? L" form" : L" forms");
        RECT u = {E(74), y + E(22), cr.right - E(8), y + E(40)};
        rv_text(dc, sub, u, THEME_TEXT2, theme_f_normal);
    }
}
static void rv_list_scrollbar() {
    RECT r; GetClientRect(rv_list, &r);
    int total = E(rv_total());
    rv_scroll = std::max(0, std::min(rv_scroll, std::max(0, total - (int)r.bottom)));
    SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS; si.nMin = 0; si.nMax = std::max(0, total - 1); si.nPage = r.bottom; si.nPos = rv_scroll;
    SetScrollInfo(rv_list, SB_VERT, &si, TRUE);
}
static void rv_reveal() {   // keep the selected row in view
    RECT r; GetClientRect(rv_list, &r);
    for (int k = 0; k < (int)rv_rows.size(); k++) if (rv_rows[k] == re_cur) {
        int y = E(rv_row_y(k)), h = E(rv_row_h(k));
        if (y < rv_scroll) rv_scroll = y - E(30);
        else if (y + h > rv_scroll + r.bottom) rv_scroll = y + h - r.bottom + E(4);
    }
}
static int rv_list_hit(int my) {
    int y = (int)((my + rv_scroll) / theme_scale);
    for (int r = 0; r < (int)rv_rows.size(); r++) { int a = rv_row_y(r); if (y >= a && y < a + rv_row_h(r)) return rv_rows[r] <= -10 ? -1 : r; }
    return -1;
}
static LRESULT CALLBACK rv_list_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
            HDC md = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, r.right, r.bottom); HGDIOBJ ob = SelectObject(md, bm);
            rv_paint_list(md, r);
            BitBlt(dc, 0, 0, r.right, r.bottom, md, 0, 0, SRCCOPY);
            SelectObject(md, ob); DeleteObject(bm); DeleteDC(md); EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEWHEEL: rv_scroll -= GET_WHEEL_DELTA_WPARAM(w) * E(88) / WHEEL_DELTA; rv_list_scrollbar(); InvalidateRect(h, nullptr, FALSE); return 0;
        case WM_VSCROLL: {
            SCROLLINFO si; si.cbSize = sizeof(si); si.fMask = SIF_ALL; GetScrollInfo(h, SB_VERT, &si);
            switch (LOWORD(w)) { case SB_LINEUP: rv_scroll -= E(44); break; case SB_LINEDOWN: rv_scroll += E(44); break; case SB_PAGEUP: rv_scroll -= (int)si.nPage; break; case SB_PAGEDOWN: rv_scroll += (int)si.nPage; break; case SB_THUMBTRACK: case SB_THUMBPOSITION: rv_scroll = si.nTrackPos; break; }
            rv_list_scrollbar(); InvalidateRect(h, nullptr, FALSE); return 0;
        }
        case WM_MOUSEMOVE: {
            int hit = rv_list_hit(GET_Y_LPARAM(l));
            if (hit != rv_hot) { rv_hot = hit; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0}; TrackMouseEvent(&t);
            return 0;
        }
        case WM_MOUSELEAVE: if (rv_hot >= 0) { rv_hot = -1; InvalidateRect(h, nullptr, FALSE); } return 0;
        case WM_LBUTTONDOWN: {
            SetFocus(h);
            int hit = rv_list_hit(GET_Y_LPARAM(l));
            if (hit >= 0) { re_cur = rv_rows[hit]; re_form = -1; re_rebuild(); }
            return 0;
        }
        case WM_KEYDOWN: {
            if (w != VK_UP && w != VK_DOWN) break;
            int k = -1;
            for (int r = 0; r < (int)rv_rows.size(); r++) if (rv_rows[r] == re_cur) k = r;
            for (int r = k + (w == VK_DOWN ? 1 : -1); r >= 0 && r < (int)rv_rows.size(); r += w == VK_DOWN ? 1 : -1)
                if (rv_rows[r] > -10) { re_cur = rv_rows[r]; re_form = -1; re_rebuild(); break; }
            return 0;
        }
        case WM_GETDLGCODE: return DLGC_WANTARROWS;
    }
    return DefWindowProcW(h, m, w, l);
}
// header: render, name, where it is saved, and the forms as chips (Base, form 1...)
static std::vector<RECT> rv_chips;
static void rv_paint_head(HDC dc, RECT cr) {
    FillRect(dc, &cr, theme_b_bg);
    SetBkMode(dc, TRANSPARENT);
    rv_chips.clear();
    if (re_cur < 0 || re_cur >= (int)re_cells.size() - 1) {
        RECT t = {0, 0, cr.right, cr.bottom};
        rv_text(dc, L"Choose a character from the list", t, THEME_TEXT2, theme_f_normal);
        return;
    }
    int base = re_cells[re_cur][0], s = re_selected();
    rv_thumb(dc, s, 0, E(4), E(150), E(84));
    RECT t = {E(168), E(6), cr.right, E(34)};
    rv_text(dc, mw_w(re_label(s)), t, THEME_TEXT, theme_f_title);
    std::wstring sub = re_find(s) ? L"New character of this mod" : L"Game character  \x00B7  your changes are saved in this mod";
    RECT u = {E(168), E(34), cr.right, E(54)};
    rv_text(dc, sub, u, THEME_TEXT2, theme_f_normal);
    int x = E(168);
    for (size_t k = 0; k < re_cells[re_cur].size(); k++) {
        int v = re_cells[re_cur][k];
        std::wstring n = k == 0 ? L"Base" : mw_w(re_label(v));
        if (k && n.size() > 22) n = n.substr(0, 21) + L"\x2026";
        HGDIOBJ of = SelectObject(dc, theme_f_normal); SIZE sz; GetTextExtentPoint32W(dc, n.c_str(), (int)n.size(), &sz); SelectObject(dc, of);
        int w = sz.cx + E(28);
        bool on = (int)k - 1 == re_form, hot = (int)k == rv_head_hot;
        RECT c = {x, E(62), x + w, E(92)};
        if (on) rv_round(dc, c.left, c.top, w, c.bottom - c.top, E(14), THEME_ACCENT);
        else { rv_round(dc, c.left, c.top, w, c.bottom - c.top, E(14), THEME_BORDER); rv_round(dc, c.left + 1, c.top + 1, w - 2, c.bottom - c.top - 2, E(13), hot ? RV_HOVER : THEME_BG); }
        rv_text(dc, n, c, on ? RGB(30, 20, 6) : THEME_TEXT, on ? theme_f_bold : theme_f_normal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        rv_chips.push_back(c);
        x += w + E(8);
        (void)base;
    }
}
static LRESULT CALLBACK rv_head_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT r; GetClientRect(h, &r);
            HDC md = CreateCompatibleDC(dc); HBITMAP bm = CreateCompatibleBitmap(dc, r.right, r.bottom); HGDIOBJ ob = SelectObject(md, bm);
            rv_paint_head(md, r);
            BitBlt(dc, 0, 0, r.right, r.bottom, md, 0, 0, SRCCOPY);
            SelectObject(md, ob); DeleteObject(bm); DeleteDC(md); EndPaint(h, &ps);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_MOUSEMOVE: {
            POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)}; int hit = -1;
            for (int k = 0; k < (int)rv_chips.size(); k++) if (PtInRect(&rv_chips[k], p)) hit = k;
            if (hit != rv_head_hot) { rv_head_hot = hit; InvalidateRect(h, nullptr, FALSE); }
            TRACKMOUSEEVENT t = {sizeof(t), TME_LEAVE, h, 0}; TrackMouseEvent(&t);
            return 0;
        }
        case WM_MOUSELEAVE: if (rv_head_hot >= 0) { rv_head_hot = -1; InvalidateRect(h, nullptr, FALSE); } return 0;
        case WM_LBUTTONDOWN: {
            POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            for (int k = 0; k < (int)rv_chips.size(); k++) if (PtInRect(&rv_chips[k], p)) { re_form = k - 1; re_rebuild(); break; }
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}
static void rv_create(int H) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.hInstance = GetModuleHandle(nullptr); wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
        wc.lpfnWndProc = rv_list_proc; wc.lpszClassName = L"TTTCharList"; RegisterClassW(&wc);
        wc.lpfnWndProc = rv_head_proc; wc.lpszClassName = L"TTTCharHead"; RegisterClassW(&wc);
        reg = true;
    }
    rv_list = CreateWindowExW(0, L"TTTCharList", L"", WS_CHILD | WS_VSCROLL | WS_TABSTOP, E(184), E(52), E(212), E(H - 72), cm, (HMENU)(INT_PTR)IDV_LIST, GetModuleHandle(nullptr), nullptr);
    theme_control(rv_list);
    rv_head = CreateWindowExW(0, L"TTTCharHead", L"", WS_CHILD, E(412), E(16), E(852), E(100), cm, (HMENU)(INT_PTR)IDV_HEAD, GetModuleHandle(nullptr), nullptr);
    cm_page[1].push_back(rv_list); cm_page[1].push_back(rv_head);
}
// which roster controls belong to which page: Characters (re_view 0) or Select screen (re_view 1)

static void re_view_set(int v) { re_view = v; re_page = v ? re_spage : 2; }
static void re_view_apply() {
    if (!cm || cm_tab != 1) return;
    bool ch = re_view == 0;
    for (int id : std::initializer_list<int>{IDR_NEW, IDR_COPY, IDR_FORM, IDR_SUM, IDR_FORMS_L, IDR_PG0, IDR_PG1}) ShowWindow(GetDlgItem(cm, id), ch ? SW_HIDE : SW_SHOW);
    for (HWND hw : {re_main.h, re_forms.h}) if (hw) ShowWindow(hw, ch ? SW_HIDE : SW_SHOW);
    for (int id : std::initializer_list<int>{IDV_SEARCH}) ShowWindow(GetDlgItem(cm, id), ch ? SW_SHOW : SW_HIDE);
    ShowWindow(rv_list, ch ? SW_SHOW : SW_HIDE);
    ShowWindow(rv_head, ch ? SW_SHOW : SW_HIDE);
    ShowWindow(GetDlgItem(cm, IDR_TITLE), SW_HIDE);
    if (ch) { rv_fill(); rv_reveal(); rv_list_scrollbar(); InvalidateRect(rv_list, nullptr, FALSE); InvalidateRect(rv_head, nullptr, FALSE); }
    for (int id : std::initializer_list<int>{IDR_PG0, IDR_PG1}) InvalidateRect(GetDlgItem(cm, id), nullptr, FALSE);
    InvalidateRect(cm, nullptr, TRUE);
}

// rail navigation and page tabs, drawn here (the rest of the buttons keep theme_button)
static int rv_nav_sel(int id) { return id == IDC_TAB0 ? cm_tab == 0 : id == IDC_TAB2 ? cm_tab == 2 : id == IDC_TAB1 ? cm_tab == 1 && re_view == 0 : cm_tab == 1 && re_view == 1; }
static bool rv_draw(DRAWITEMSTRUCT* d) {
    int id = (int)d->CtlID;
    bool nav = id == IDC_TAB0 || id == IDC_TAB1 || id == IDC_TAB2 || id == IDC_TAB3, tab = id == IDR_PG0 || id == IDR_PG1;
    if (!nav && !tab) return false;
    HDC dc = d->hDC; RECT r = d->rcItem;
    bool hot = d->hwndItem == theme_hot;
    SetBkMode(dc, TRANSPARENT);
    if (nav) {
        bool on = rv_nav_sel(id);
        HBRUSH bg = CreateSolidBrush(RV_RAIL); FillRect(dc, &r, bg); DeleteObject(bg);
        if (on || hot) rv_round(dc, r.left, r.top, r.right - r.left - 1, r.bottom - r.top - 1, E(10), on ? RV_SELBG : RV_HOVER);
        if (on) rv_round(dc, r.left, r.top + E(9), E(3), r.bottom - r.top - E(18), E(3), THEME_ACCENT);
        static HFONT ic = nullptr; static double ics = 0; static int fluent = -1;
        if (!ic || ics != theme_scale) { if (ic) DeleteObject(ic); ics = theme_scale; fluent = -1; ic = CreateFontW(E(-16), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe Fluent Icons"); }
        // Windows 10 has no Segoe Fluent Icons: the same glyphs come from Segoe MDL2 Assets
        if (fluent < 0) { wchar_t face[64] = L""; HGDIOBJ of = SelectObject(dc, ic); GetTextFaceW(dc, 64, face); SelectObject(dc, of); fluent = !wcscmp(face, L"Segoe Fluent Icons"); }
        if (fluent == 0) { DeleteObject(ic); ic = CreateFontW(E(-16), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe MDL2 Assets"); fluent = 2; }
        const wchar_t* glyph = id == IDC_TAB0 ? L"\xE7B8" : id == IDC_TAB1 ? L"\xE716" : id == IDC_TAB3 ? L"\xE8A9" : L"\xE81E";
        RECT gi = {r.left + E(14), r.top, r.left + E(38), r.bottom};
        rv_text(dc, glyph, gi, on ? THEME_ACCENT : THEME_TEXT2, ic, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        wchar_t t[64] = L""; GetWindowTextW(d->hwndItem, t, 64);
        RECT tr = {r.left + E(46), r.top, r.right - E(6), r.bottom};
        rv_text(dc, t, tr, on ? RGB(255, 210, 150) : THEME_TEXT, on ? theme_f_bold : theme_f_normal);
        return true;
    }
    FillRect(dc, &r, theme_b_bg);
    bool on = re_page == id - IDR_PG0;
    wchar_t t[64] = L""; GetWindowTextW(d->hwndItem, t, 64);
    rv_text(dc, t, r, on ? THEME_TEXT : hot ? THEME_TEXT : THEME_TEXT2, on ? theme_f_bold : theme_f_normal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (on) rv_round(dc, r.left + E(10), r.bottom - E(3), r.right - r.left - E(20), E(3), E(3), THEME_ACCENT);
    return true;
}
