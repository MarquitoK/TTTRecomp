#pragma once
// Tools > Map renders: opens after the map renders are captured; one of the three entry camera captures of each map is kept
// (disc/renders.h gm), its big and small are made from it and the other two are deleted. Included from menu.h
int psp_render_quality();
int psp_map_render_list(int q, std::vector<std::string>& keys, std::vector<std::string>& names, std::vector<int>& mask, std::vector<int>& pick);
bool psp_map_render_preview(int q, const std::string& k, int i, int w, int h, std::vector<uint32_t>& px);
void psp_map_render_choose(int q, std::vector<std::pair<std::string, int>> picks);
int psp_map_render_choose_progress(int* total);
bool psp_map_chooser_take();

enum { IDMC_TITLE = 4800, IDMC_SUB, IDMC_LIST, IDMC_NOTE, IDMC_CLOSE, IDMC_GO, IDMC_PREV0 };
static HWND mch_w = nullptr, mch_list = nullptr;
static int mch_q = 2, mch_sel = -1;
static bool mch_applying = false;
static std::vector<std::string> mch_keys, mch_names;
static std::vector<int> mch_mask, mch_pick;
static std::vector<uint32_t> mch_img[3];
static const int MCH_X = 272, MCH_Y = 64, MCH_PW = 400, MCH_PH = 203, MCH_SX = 410, MCH_SY = 245, MCH_IW = 400, MCH_IH = 203;

static void mch_note(const std::wstring& t) { SetDlgItemTextW(mch_w, IDMC_NOTE, t.c_str()); }
static void mch_previews() {
    for (int k = 0; k < 3; k++) {
        mch_img[k].clear();
        if (mch_sel >= 0 && (mch_mask[mch_sel] >> k & 1) && psp_map_render_preview(mch_q, mch_keys[mch_sel], k, MCH_IW, MCH_IH, mch_img[k]))
            for (auto& p : mch_img[k]) p = (p & 255) << 16 | (p & 0xFF00) | ((p >> 16) & 255);   // BGR for GDI
        InvalidateRect(GetDlgItem(mch_w, IDMC_PREV0 + k), nullptr, FALSE);
    }
}
static void mch_fill() {
    mch_q = psp_render_quality();
    int n = psp_map_render_list(mch_q, mch_keys, mch_names, mch_mask, mch_pick);
    SendMessageW(mch_list, LVM_DELETEALLITEMS, 0, 0);
    for (int i = 0; i < n; i++) {
        std::wstring t = mw_w(mch_names[i]);
        LVITEMW it; memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = i; it.pszText = &t[0];
        SendMessageW(mch_list, LVM_INSERTITEMW, 0, (LPARAM)&it);
    }
    mch_sel = n ? 0 : -1;
    if (n) ListView_SetItemState(mch_list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    mch_previews();
    if (!n) mch_note(L"No map captures of this quality yet: turn on Graphics > Renders > Map renders from 3D and open the map select");
    else mch_note(std::to_wstring(n) + L" maps (quality x" + std::to_wstring(mch_q) + L"). Click the camera to keep for each map");
}
static HWND mch_ctl(const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st, E(x), E(y), E(w), E(h), mch_w, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)theme_f_normal, TRUE);
    return c;
}
static void mch_close() {
    if (!mch_w) return;
    theme_primary.erase(GetDlgItem(mch_w, IDMC_GO));
    EnableWindow(ventana, TRUE);
    SetForegroundWindow(ventana);
    DestroyWindow(mch_w);
    mch_w = nullptr;
}
static void mch_draw_prev(DRAWITEMSTRUCT* d, int k) {
    HDC dc = d->hDC; RECT r = d->rcItem;
    HBRUSH fb = CreateSolidBrush(THEME_FIELD); FillRect(dc, &r, fb); DeleteObject(fb);
    bool on = mch_sel >= 0 && mch_pick[mch_sel] == k;
    int top = E(24), pad = E(4);
    RECT box = {r.left + pad, r.top + top, r.right - pad, r.bottom - pad};
    if (on) { HBRUSH ob = CreateSolidBrush(THEME_ACCENT); RECT o = {box.left - pad, box.top - pad, box.right + pad, box.bottom + pad}; FillRect(dc, &o, ob); DeleteObject(ob); }
    if (!mch_img[k].empty()) {
        BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = MCH_IW; bi.bmiHeader.biHeight = -MCH_IH; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32;
        SetStretchBltMode(dc, HALFTONE); SetBrushOrgEx(dc, 0, 0, nullptr);
        StretchDIBits(dc, box.left, box.top, box.right - box.left, box.bottom - box.top, 0, 0, MCH_IW, MCH_IH, mch_img[k].data(), &bi, DIB_RGB_COLORS, SRCCOPY);
    }
    SetBkMode(dc, TRANSPARENT);
    HFONT of = (HFONT)SelectObject(dc, theme_f_normal);
    SetTextColor(dc, on ? THEME_ACCENT : THEME_TEXT);
    wchar_t t[64];
    if (mch_sel >= 0 && !(mch_mask[mch_sel] >> k & 1)) swprintf(t, 64, L"Camera %d (none)", k + 1);
    else swprintf(t, 64, on ? L"Camera %d (kept)" : L"Camera %d", k + 1);
    RECT tr = {r.left + pad, r.top + E(2), r.right - pad, r.top + top - E(2)};
    DrawTextW(dc, t, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SelectObject(dc, of);
}
static void mch_apply() {
    std::vector<std::pair<std::string, int>> picks;
    for (size_t i = 0; i < mch_keys.size(); i++) if (mch_pick[i] >= 0) picks.push_back({mch_keys[i], mch_pick[i]});
    if (picks.empty()) { mch_close(); return; }
    mch_applying = true;
    EnableWindow(GetDlgItem(mch_w, IDMC_GO), FALSE); EnableWindow(GetDlgItem(mch_w, IDMC_CLOSE), FALSE);
    psp_map_render_choose(mch_q, picks);
    mch_note(L"Generating...");
}

static LRESULT CALLBACK mch_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == 0x02E0) return theme_dpi_change(h, w, l);
    if (m == WM_COMMAND && mch_w) {
        int id = LOWORD(w), n = HIWORD(w);
        if (mch_applying) return 0;
        if (id >= IDMC_PREV0 && id < IDMC_PREV0 + 3 && n == STN_CLICKED && mch_sel >= 0 && (mch_mask[mch_sel] >> (id - IDMC_PREV0) & 1)) {
            mch_pick[mch_sel] = id - IDMC_PREV0;
            for (int k = 0; k < 3; k++) InvalidateRect(GetDlgItem(mch_w, IDMC_PREV0 + k), nullptr, FALSE);
        } else if (n == BN_CLICKED && id == IDMC_CLOSE) mch_close();
        else if (n == BN_CLICKED && id == IDMC_GO) mch_apply();
        return 0;
    }
    if (m == WM_NOTIFY) {
        NMHDR* nh = (NMHDR*)l;
        if (nh->idFrom == IDMC_LIST && nh->code == NM_CUSTOMDRAW) return theme_list_draw((NMLVCUSTOMDRAW*)l, false);
        if (nh->idFrom == IDMC_LIST && nh->code == LVN_ITEMCHANGED) {
            NMLISTVIEW* nv = (NMLISTVIEW*)l;
            if ((nv->uNewState & LVIS_SELECTED) && !(nv->uOldState & LVIS_SELECTED) && nv->iItem >= 0) { mch_sel = nv->iItem; mch_previews(); }
        }
        return 0;
    }
    if (m == WM_DRAWITEM) {
        DRAWITEMSTRUCT* d = (DRAWITEMSTRUCT*)l;
        if ((int)d->CtlID >= IDMC_PREV0 && (int)d->CtlID < IDMC_PREV0 + 3) mch_draw_prev(d, d->CtlID - IDMC_PREV0);
        else theme_button(d);
        return TRUE;
    }
    if (m == WM_CTLCOLORSTATIC) {
        int id = GetDlgCtrlID((HWND)l);
        if (id == IDMC_NOTE || id == IDMC_SUB) {
            SetBkMode((HDC)w, TRANSPARENT); SetBkColor((HDC)w, THEME_BG);
            SetTextColor((HDC)w, id == IDMC_NOTE ? THEME_ACCENT : THEME_TEXT2);
            return (LRESULT)theme_b_bg;
        }
    }
    { LRESULT r; if (theme_color(m, w, l, &r)) return r; }
    if (m == WM_CLOSE) { if (!mch_applying) mch_close(); return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void map_chooser_window() {
    if (mch_w) return;
    theme_init(ventana);
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = mch_proc; wc.hInstance = GetModuleHandle(nullptr); wc.lpszClassName = L"TTTRecompMapChooser";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hbrBackground = theme_b_bg;
        RegisterClassW(&wc);
        registered = true;
    }
    HANDLE ctx; ULONG_PTR cookie = 0;
    visual_styles(&ctx, &cookie);
    const int W = 1100, H = 610;
    mch_w = theme_create(L"TTTRecompMapChooser", L"Map renders", W, H, WS_EX_DLGMODALFRAME);
    mch_applying = false;
    if (mch_w) {
        HWND t = mch_ctl(L"STATIC", L"Map renders", 0, 16, 12, 400, 24, IDMC_TITLE);
        SendMessageW(t, WM_SETFONT, (WPARAM)theme_f_title, TRUE);
        mch_ctl(L"STATIC", L"Each map was captured from its three entry cameras. Keep one per map; the other two are deleted.", 0, 16, 38, 800, 18, IDMC_SUB);
        mch_list = mch_ctl(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | WS_TABSTOP, 16, MCH_Y, 240, 2 * MCH_SY - 12, IDMC_LIST);
        ListView_SetExtendedListViewStyle(mch_list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        theme_list(mch_list);
        { LVCOLUMNW c; memset(&c, 0, sizeof(c)); c.mask = LVCF_WIDTH; c.cx = E(218); SendMessageW(mch_list, LVM_INSERTCOLUMNW, 0, (LPARAM)&c); }
        for (int k = 0; k < 3; k++) mch_ctl(L"STATIC", L"", SS_OWNERDRAW | SS_NOTIFY, MCH_X + MCH_SX * (k % 2), MCH_Y + MCH_SY * (k / 2), MCH_PW, MCH_PH + 32, IDMC_PREV0 + k);
        mch_ctl(L"STATIC", L"", SS_CENTERIMAGE | SS_ENDELLIPSIS, 16, H - 44, 700, 28, IDMC_NOTE);
        mch_ctl(L"BUTTON", L"Close", BS_OWNERDRAW | WS_TABSTOP, W - 356, H - 44, 110, 28, IDMC_CLOSE);
        theme_primary.insert(mch_ctl(L"BUTTON", L"Accept", BS_OWNERDRAW | WS_TABSTOP, W - 238, H - 44, 222, 28, IDMC_GO));
    }
    if (ctx != INVALID_HANDLE_VALUE) { DeactivateActCtx(0, cookie); ReleaseActCtx(ctx); }
    if (!mch_w) return;
    EnableWindow(ventana, FALSE);
    ShowWindow(mch_w, SW_SHOW);
    SetForegroundWindow(mch_w);
    mch_fill();
}

// own loop while the window is open: the game stays paused
static void map_chooser() {
    pause_begin();
    map_chooser_window();
    while (mch_w && abierta) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { abierta = false; break; }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && mch_w && !mch_applying) { mch_close(); break; }
            if (!mch_w || !IsDialogMessageW(mch_w, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
        if (!mch_w || !abierta) break;
        if (mch_applying) {
            int tot = 0, done = psp_map_render_choose_progress(&tot);
            mch_note(L"Generating... " + std::to_wstring(done) + L" / " + std::to_wstring(tot));
            if (done >= tot) { mch_applying = false; mch_close(); break; }
        }
        theme_poll(mch_w);
        Sleep(8);
    }
    if (mch_w) { mch_applying = false; mch_close(); }
    pause_end();
}
