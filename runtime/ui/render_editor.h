#pragma once
// Tools > Render editor (E.6): opens after the captures of the generated renders; zoom and move each of the six cuts globally,
// per character or per colour on the saved captures, then cut every render again (disc/renders.h). Included from menu.h
int psp_render_quality();
int psp_render_list(int q, std::vector<int>& sc);
std::string psp_render_name(int s);
bool psp_render_edit(int s, int c, int k, float* v, bool set);
void psp_render_edits_save();
void psp_render_edits_reload();
bool psp_render_preview(int q, int s, int c, int k, std::vector<uint32_t>& px, int* w, int* h);
int psp_render_preview_extra(int q, int s, int c, int which, std::vector<uint32_t>& px, int* w, int* h);
void psp_render_apply(int q);
int psp_render_apply_progress(int* total);
bool psp_render_editor_take();

enum { IDE_TITLE = 4700, IDE_SUB, IDE_LIST, IDE_COLOR, IDE_COLLAB, IDE_SCOPE0, IDE_NOTE = IDE_SCOPE0 + 3, IDE_RESET, IDE_CLOSE, IDE_GO, IDE_PREV0, IDE_BTN0 = IDE_PREV0 + 6, IDE_EX0 = IDE_BTN0 + 42 };
static HWND rd_w = nullptr, rd_list = nullptr;
static int rd_q = 2, rd_scope = 0, rd_sel = -1, rd_col = 0;
static bool rd_dirty = false, rd_applying = false;
static std::vector<int> rd_sc;   // sysid, colour pairs
static std::vector<int> rd_chars;   // distinct sysids
static std::vector<uint32_t> rd_img[6];
static int rd_iw[6], rd_ih[6];
static std::vector<uint32_t> rd_ximg[3];   // read-only: SZ HUD, BT3 HUD, game room
static int rd_xw[3], rd_xh[3], rd_xst[3];
static const wchar_t* RD_XKIND[3] = {L"SZ HUD (from Big)", L"BT3 HUD (from Mid)", L"Game room (from Big)"};
static const wchar_t* RD_KIND[6] = {L"Big", L"Mid", L"Small", L"HUD 1", L"HUD 2", L"Beam clash"};
static const int RD_CX = 272, RD_CY = 104, RD_CW = 270, RD_PH = 176, RD_SX = 282, RD_SY = 250;

static bool rd_kind_on(int k) { return rd_col == 0 || (k != 1 && k != 2); }   // mid and small exist for colour 1 only
static void rd_scope_key(int* s, int* c) { int sys = rd_sel >= 0 && rd_sel < (int)rd_chars.size() ? rd_chars[rd_sel] : -1; *s = rd_scope == 0 ? -1 : sys; *c = rd_scope == 2 ? rd_col : -1; }
static void rd_over_field(std::vector<uint32_t>& img) {   // over the field colour, BGR for GDI
    uint32_t fr = GetRValue(THEME_FIELD), fg = GetGValue(THEME_FIELD), fb = GetBValue(THEME_FIELD);
    for (auto& p : img) {
        uint32_t a = p >> 24, r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255;
        r = (r * a + fr * (255 - a)) / 255; g = (g * a + fg * (255 - a)) / 255; b = (b * a + fb * (255 - a)) / 255;
        p = r << 16 | g << 8 | b;
    }
}
static void rd_previews() {
    int sys = rd_sel >= 0 && rd_sel < (int)rd_chars.size() ? rd_chars[rd_sel] : -1;
    for (int k = 0; k < 3; k++) {
        rd_ximg[k].clear(); rd_xw[k] = rd_xh[k] = 0;
        rd_xst[k] = psp_render_preview_extra(rd_q, sys, rd_col, k, rd_ximg[k], &rd_xw[k], &rd_xh[k]);
        if (rd_xst[k] >= 1) rd_over_field(rd_ximg[k]); else rd_ximg[k].clear();
        HWND x = GetDlgItem(rd_w, IDE_EX0 + k);
        ShowWindow(x, rd_xst[k] == -1 ? SW_HIDE : SW_SHOW);
        InvalidateRect(x, nullptr, FALSE);
    }
    for (int k = 0; k < 6; k++) {
        rd_img[k].clear(); rd_iw[k] = rd_ih[k] = 0;
        if (sys >= 0 && rd_kind_on(k) && psp_render_preview(rd_q, sys, rd_col, k, rd_img[k], &rd_iw[k], &rd_ih[k])) rd_over_field(rd_img[k]);
        InvalidateRect(GetDlgItem(rd_w, IDE_PREV0 + k), nullptr, FALSE);
        for (int b = 0; b < 7; b++) EnableWindow(GetDlgItem(rd_w, IDE_BTN0 + 7 * k + b), rd_kind_on(k) && !rd_applying);
    }
}
static void rd_note(const std::wstring& t) { SetDlgItemTextW(rd_w, IDE_NOTE, t.c_str()); }
static void rd_scope_set(int k) {
    rd_scope = k;
    for (int i = 0; i < 3; i++) { HWND b = GetDlgItem(rd_w, IDE_SCOPE0 + i); if (i == k) theme_primary.insert(b); else theme_primary.erase(b); InvalidateRect(b, nullptr, TRUE); }
    for (int i = 0; i < 6; i++) InvalidateRect(GetDlgItem(rd_w, IDE_PREV0 + i), nullptr, FALSE);
}
static void rd_colours() {
    HWND cb = GetDlgItem(rd_w, IDE_COLOR);
    SendMessageW(cb, CB_RESETCONTENT, 0, 0);
    int sys = rd_sel >= 0 && rd_sel < (int)rd_chars.size() ? rd_chars[rd_sel] : -1, n = 0;
    for (size_t i = 0; i + 1 < rd_sc.size(); i += 2) if (rd_sc[i] == sys) { wchar_t t[32]; int cc = rd_sc[i + 1]; swprintf(t, 32, cc >= 64 ? L"Color %d damaged" : L"Color %d", cc % 64 + 1); SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)t); n++; }
    rd_col = 0;
    SendMessageW(cb, CB_SETCURSEL, 0, 0);
    EnableWindow(cb, n > 1);
}
static void rd_step(int k, int b) {   // 0 zoom out, 1 zoom in, 2 left, 3 up, 4 down, 5 right, 6 reset
    int s, c; rd_scope_key(&s, &c);
    if (rd_scope > 0 && s < 0) return;
    int ek = k;
    float v[3];
    psp_render_edit(s, c, ek, v, false);
    if (b == 0) v[0] /= 1.05f; else if (b == 1) v[0] *= 1.05f;
    else if (b == 2) v[1] -= 0.02f; else if (b == 5) v[1] += 0.02f;
    else if (b == 3) v[2] -= 0.02f; else if (b == 4) v[2] += 0.02f;
    else { v[0] = 1; v[1] = 0; v[2] = 0; }
    v[0] = std::min(4.0f, std::max(0.25f, v[0]));
    psp_render_edit(s, c, ek, v, true);
    rd_dirty = true;
    rd_previews();
    rd_note(L"Changed. Accept and generate cuts again only the renders these values change");
}
static void rd_fill() {
    rd_q = psp_render_quality();
    int n = psp_render_list(rd_q, rd_sc);
    rd_chars.clear();
    for (size_t i = 0; i + 1 < rd_sc.size(); i += 2) if (rd_chars.empty() || rd_chars.back() != rd_sc[i]) rd_chars.push_back(rd_sc[i]);
    SendMessageW(rd_list, LVM_DELETEALLITEMS, 0, 0);
    for (size_t i = 0; i < rd_chars.size(); i++) {
        std::wstring t = mw_w(psp_render_name(rd_chars[i]));
        LVITEMW it; memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = (int)i; it.pszText = &t[0];
        SendMessageW(rd_list, LVM_INSERTITEMW, 0, (LPARAM)&it);
    }
    rd_sel = rd_chars.empty() ? -1 : 0;
    if (rd_sel >= 0) ListView_SetItemState(rd_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    rd_colours();
    rd_previews();
    if (!n) rd_note(L"No captures of this quality yet: turn on Graphics > Renders and open the character select");
    else rd_note(std::to_wstring(n) + L" renders (quality x" + std::to_wstring(rd_q) + L"). Adjust the cuts or generate them as they are");
}

static HWND rd_ctl(const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st, E(x), E(y), E(w), E(h), rd_w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)theme_f_normal, TRUE);
    return c;
}
static void rd_close() {
    if (!rd_w) return;
    if (!rd_applying && rd_dirty) psp_render_edits_reload();   // closed without generating: the saved values stay
    for (int i = 0; i < 3; i++) theme_primary.erase(GetDlgItem(rd_w, IDE_SCOPE0 + i));
    theme_primary.erase(GetDlgItem(rd_w, IDE_GO));
    EnableWindow(ventana, TRUE);
    SetForegroundWindow(ventana);
    DestroyWindow(rd_w);
    rd_w = nullptr;
}
static void rd_draw_extra(DRAWITEMSTRUCT* d, int k) {
    HDC dc = d->hDC; RECT r = d->rcItem;
    HBRUSH fb = CreateSolidBrush(THEME_FIELD); FillRect(dc, &r, fb); DeleteObject(fb);
    int pad = E(8), top = E(22), bot = E(20);
    RECT box = {r.left + pad, r.top + top, r.right - pad, r.bottom - bot};
    if (!rd_ximg[k].empty()) {
        float sc = std::min((float)(box.right - box.left) / rd_xw[k], (float)(box.bottom - box.top) / rd_xh[k]);
        int dw = std::max(1, (int)(rd_xw[k] * sc)), dh = std::max(1, (int)(rd_xh[k] * sc));
        int x = box.left + (box.right - box.left - dw) / 2, y = box.top + (box.bottom - box.top - dh) / 2;
        BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = rd_xw[k]; bi.bmiHeader.biHeight = -rd_xh[k]; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, x, y, dw, dh, 0, 0, rd_xw[k], rd_xh[k], rd_ximg[k].data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    }
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, theme_f_normal);
    SetTextColor(dc, THEME_TEXT);
    RECT t = {r.left + pad, r.top + E(3), r.right - pad, r.top + top};
    DrawTextW(dc, RD_XKIND[k], -1, &t, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SetTextColor(dc, THEME_TEXT2);
    if (rd_xst[k] == -2) DrawTextW(dc, L"Open the game room once\nto learn its cut", -1, &box, DT_CENTER | DT_WORDBREAK);
    RECT t2 = {r.left + pad, r.bottom - bot, r.right - pad, r.bottom - E(2)};
    DrawTextW(dc, rd_xst[k] == 2 ? L"Panel cut after one BT3 battle" : L"Preview only", -1, &t2, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, of);
}
static void rd_draw_prev(DRAWITEMSTRUCT* d, int k) {
    HDC dc = d->hDC; RECT r = d->rcItem;
    HBRUSH fb = CreateSolidBrush(THEME_FIELD); FillRect(dc, &r, fb); DeleteObject(fb);
    int pad = E(8), top = E(22), bot = E(20);
    RECT box = {r.left + pad, r.top + top, r.right - pad, r.bottom - bot};
    if (!rd_img[k].empty()) {
        float sc = std::min((float)(box.right - box.left) / rd_iw[k], (float)(box.bottom - box.top) / rd_ih[k]);
        int dw = std::max(1, (int)(rd_iw[k] * sc)), dh = std::max(1, (int)(rd_ih[k] * sc));
        int x = box.left + (box.right - box.left - dw) / 2, y = box.top + (box.bottom - box.top - dh) / 2;
        BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = rd_iw[k]; bi.bmiHeader.biHeight = -rd_ih[k]; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, x, y, dw, dh, 0, 0, rd_iw[k], rd_ih[k], rd_img[k].data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    }
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, theme_f_normal);
    SetTextColor(dc, THEME_TEXT);
    RECT t = {r.left + pad, r.top + E(3), r.right - pad, r.top + top};
    DrawTextW(dc, RD_KIND[k], -1, &t, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    std::wstring v;
    if (!rd_kind_on(k)) v = L"Color 1 only";
    else {
        int s, c; rd_scope_key(&s, &c);
        float e[3]; psp_render_edit(s, c, k, e, false);
        wchar_t b[96]; swprintf(b, 96, L"Zoom %.2f   X %+.2f   Y %+.2f", e[0], e[1], e[2]); v = b;
    }
    SetTextColor(dc, THEME_TEXT2);
    RECT t2 = {r.left + pad, r.bottom - bot, r.right - pad, r.bottom - E(2)};
    DrawTextW(dc, v.c_str(), -1, &t2, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, of);
}

static LRESULT CALLBACK rd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == 0x02E0) return theme_dpi_change(h, w, l);
    if (m == WM_COMMAND && rd_w) {
        int id = LOWORD(w), n = HIWORD(w);
        if (id == IDE_COLOR && n == CBN_SELCHANGE) {
            int i = (int)SendDlgItemMessageW(rd_w, IDE_COLOR, CB_GETCURSEL, 0, 0), sys = rd_sel >= 0 ? rd_chars[rd_sel] : -1, j = 0;
            for (size_t x = 0; x + 1 < rd_sc.size(); x += 2) if (rd_sc[x] == sys) { if (j == i) rd_col = rd_sc[x + 1]; j++; }
            rd_previews();
            for (int k = 0; k < 6; k++) InvalidateRect(GetDlgItem(rd_w, IDE_PREV0 + k), nullptr, FALSE);
        } else if (n == BN_CLICKED && !rd_applying) {
            if (id >= IDE_SCOPE0 && id < IDE_SCOPE0 + 3) rd_scope_set(id - IDE_SCOPE0);
            else if (id >= IDE_BTN0 && id < IDE_BTN0 + 42) rd_step((id - IDE_BTN0) / 7, (id - IDE_BTN0) % 7);
            else if (id == IDE_RESET) {
                int s, c; rd_scope_key(&s, &c);
                float v[3] = {1, 0, 0};
                if (!(rd_scope > 0 && s < 0)) for (int k = 0; k < 6; k++) psp_render_edit(s, c, k, v, true);
                rd_dirty = true; rd_previews();
                rd_note(rd_scope == 0 ? L"Global values back to default" : rd_scope == 1 ? L"This character back to the global values" : L"This color back to its character's values");
            } else if (id == IDE_CLOSE) rd_close();
            else if (id == IDE_GO) {
                if (!rd_dirty) { rd_close(); return 0; }
                rd_applying = true;
                for (int k = 0; k < 6; k++) for (int b = 0; b < 7; b++) EnableWindow(GetDlgItem(rd_w, IDE_BTN0 + 7 * k + b), FALSE);
                EnableWindow(GetDlgItem(rd_w, IDE_GO), FALSE); EnableWindow(GetDlgItem(rd_w, IDE_RESET), FALSE);
                psp_render_apply(rd_q);
                rd_note(L"Generating...");
            }
        }
        return 0;
    }
    if (m == WM_NOTIFY) {
        NMHDR* nh = (NMHDR*)l;
        if (nh->idFrom == IDE_LIST && nh->code == NM_CUSTOMDRAW) return theme_list_draw((NMLVCUSTOMDRAW*)l, false);
        if (nh->idFrom == IDE_LIST && nh->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* nv = (NMLISTVIEW*)l;
            if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED) && nv->iItem >= 0) { rd_sel = nv->iItem; rd_colours(); rd_previews(); }
        }
        return 0;
    }
    if (m == WM_DRAWITEM) {
        DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)l;
        if ((int)d->CtlID >= IDE_PREV0 && (int)d->CtlID < IDE_PREV0 + 6) rd_draw_prev(d, d->CtlID - IDE_PREV0);
        else if ((int)d->CtlID >= IDE_EX0 && (int)d->CtlID < IDE_EX0 + 3) rd_draw_extra(d, d->CtlID - IDE_EX0);
        else theme_button(d);
        return TRUE;
    }
    if (m == WM_MOUSEWHEEL && rd_w && !rd_applying) {   // wheel over a cut: zoom
        POINT p = {(short)LOWORD(l), (short)HIWORD(l)};
        for (int k = 0; k < 6; k++) {
            RECT r; GetWindowRect(GetDlgItem(rd_w, IDE_PREV0 + k), &r);
            if (PtInRect(&r, p) && rd_kind_on(k)) { rd_step(k, GET_WHEEL_DELTA_WPARAM(w) > 0 ? 1 : 0); break; }
        }
        return 0;
    }
    if (m == WM_CTLCOLORSTATIC) {
        int id = GetDlgCtrlID((HWND)l);
        if (id == IDE_NOTE || id == IDE_SUB || id == IDE_COLLAB) {
            SetBkMode((HDC)w, TRANSPARENT); SetBkColor((HDC)w, THEME_BG);
            SetTextColor((HDC)w, id == IDE_NOTE ? THEME_ACCENT : THEME_TEXT2);
            return (LRESULT)theme_b_bg;
        }
    }
    { LRESULT r; if (theme_color(m, w, l, &r)) return r; }
    if (m == WM_CLOSE) { if (!rd_applying) rd_close(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void render_editor_window() {
    if (rd_w) return;
    theme_init(ventana);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = rd_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRecompRenderEditor";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
        RegisterClassW(&wc);
        registered = true;
    }
    HANDLE ctx; ULONG_PTR cookie = 0;
    visual_styles(&ctx, &cookie);
    const int W = 1340, H = 690;
    rd_w = theme_create(L"TTTRecompRenderEditor", L"Render editor", W, H, WS_EX_DLGMODALFRAME);
    rd_dirty = false; rd_applying = false;
    if (rd_w) {
        HWND t = rd_ctl(L"STATIC", L"Render editor", 0, 16, 12, 400, 24, IDE_TITLE);
        SendMessageW(t, WM_SETFONT, (WPARAM)theme_f_title, TRUE);
        rd_ctl(L"STATIC", L"Zoom and move each cut. Mouse wheel over a cut zooms it.", 0, 16, 38, 700, 18, IDE_SUB);
        rd_list = rd_ctl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | WS_TABSTOP, 16, 64, 240, 486, IDE_LIST);
        ListView_SetExtendedListViewStyle(rd_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        theme_list(rd_list);
        { LVCOLUMNW c; memset(&c, 0, sizeof(c)); c.mask = LVCF_WIDTH; c.cx = E(218); SendMessageW(rd_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&c); }
        rd_ctl(L"STATIC", L"Color", SS_CENTERIMAGE, 16, 560, 60, 26, IDE_COLLAB);
        HWND cb = rd_ctl(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 76, 560, 180, 300, IDE_COLOR);
        theme_control(cb, L"DarkMode_CFD");
        static const wchar_t* SC[3] = {L"All characters", L"This character", L"This color"};
        for (int i = 0; i < 3; i++) rd_ctl(L"BUTTON", SC[i], BS_OWNERDRAW | WS_TABSTOP, RD_CX + 160 * i, 64, 152, 30, IDE_SCOPE0 + i);
        static const wchar_t* BT[7] = {L"-", L"+", L"◀", L"▲", L"▼", L"▶", L"↺"};
        for (int k = 0; k < 6; k++) {
            int x = RD_CX + RD_SX * (k % 3), y = RD_CY + RD_SY * (k / 3);
            rd_ctl(L"STATIC", L"", SS_OWNERDRAW, x, y, RD_CW, RD_PH + 30, IDE_PREV0 + k);
            for (int b = 0; b < 7; b++) rd_ctl(L"BUTTON", BT[b], BS_OWNERDRAW | WS_TABSTOP, x + 38 * b + 2, y + RD_PH + 36, 34, 26, IDE_BTN0 + 7 * k + b);
        }
        for (int k = 0; k < 3; k++) rd_ctl(L"STATIC", L"", SS_OWNERDRAW, RD_CX + 3 * RD_SX, RD_CY + 164 * k, 204, 158, IDE_EX0 + k);
        rd_ctl(L"STATIC", L"", SS_CENTERIMAGE | SS_ENDELLIPSIS, 16, H - 44, 600, 28, IDE_NOTE);
        rd_ctl(L"BUTTON", L"Reset", BS_OWNERDRAW | WS_TABSTOP, W - 474, H - 44, 110, 28, IDE_RESET);
        rd_ctl(L"BUTTON", L"Close", BS_OWNERDRAW | WS_TABSTOP, W - 356, H - 44, 110, 28, IDE_CLOSE);
        theme_primary.insert(rd_ctl(L"BUTTON", L"Accept and generate", BS_OWNERDRAW | WS_TABSTOP, W - 238, H - 44, 222, 28, IDE_GO));
        rd_scope_set(0);
    }
    if (ctx != INVALID_HANDLE_VALUE) { DeactivateActCtx(0, cookie); ReleaseActCtx(ctx); }
    if (!rd_w) return;
    EnableWindow(ventana, FALSE);
    ShowWindow(rd_w, SW_SHOW);
    SetForegroundWindow(rd_w);
    rd_fill();
}

// own loop while the window is open: the game stays paused
static void render_editor() {
    pause_begin();
    render_editor_window();
    while (rd_w && abierta) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { abierta = false; break; }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && rd_w && !rd_applying) { rd_close(); break; }
            if (!rd_w || !IsDialogMessageW(rd_w, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        if (!rd_w || !abierta) break;
        if (rd_applying) {
            int tot = 0, done = psp_render_apply_progress(&tot);
            rd_note(L"Generating... " + std::to_wstring(done) + L" / " + std::to_wstring(tot));
            if (done >= tot) { rd_applying = false; rd_close(); break; }
        }
        theme_poll(rd_w);
        Sleep(8);
    }
    if (rd_w) { rd_applying = false; rd_close(); }
    pause_end();
}
