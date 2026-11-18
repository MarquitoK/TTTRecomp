#pragma once
// Rumble: XInput through XInputSetState, DualShock 4 / DualSense through HID output reports (USB and Bluetooth). Included from ui/window.cpp.
#include <setupapi.h>
#include <hidsdi.h>

// sending to the pad runs on its own thread: it never blocks presentation and applies each request at once
static int rumble_level = 100;                  // percent, 0 disables
static float rumble_strong = 0, rumble_weak = 0;
static double rumble_until = 0;
static CRITICAL_SECTION rumble_cs_state, rumble_cs_hid;
static HANDLE rumble_event = nullptr;
static volatile bool hid_failed = false;

static double rumble_ms() {
    static LARGE_INTEGER f = {};
    LARGE_INTEGER t;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)f.QuadPart;
}

static void rumble_cs_init() {
    static bool done = false;
    if (done) return;
    done = true;
    InitializeCriticalSection(&rumble_cs_state);
    InitializeCriticalSection(&rumble_cs_hid);
}

// XInput
static DWORD (WINAPI* xi_set)(DWORD, void*) = nullptr;
static void xi_rumble(int index, float f, float d) {
    if (!xi_set) {
        static bool searched = false;
        if (searched) return;
        searched = true;
        static const char* dll[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
        for (const char* n : dll) { HMODULE m = LoadLibraryA(n); if (m) xi_set = (DWORD(WINAPI*)(DWORD, void*))GetProcAddress(m, "XInputSetState"); if (xi_set) break; }
        if (!xi_set) return;
    }
    WORD v[2] = {(WORD)(f * 65535.0f), (WORD)(d * 65535.0f)};
    xi_set((DWORD)index, v);
}

// Sony over HID
static int hid_type = 0;                     // 1 DS4, 2 DualSense
static bool hid_bt = false;
static uint8_t hid_seq = 0;
static uint16_t hid_pid = 0;
static OVERLAPPED hid_ov;
static uint8_t hid_in[1024];
static DWORD hid_in_len = 64;
static bool hid_reading = false;
static bool hid_state_ok = false;
// Bluetooth: the first output report moves the pad to its extended report and Windows keeps the last basic report
// (0x01) forever; a D-pad held there kept an arrow pressed system-wide (Start menu, capture). Switch only at rest
static volatile bool hid_ext = false;
static volatile int hid_calm = 0;
static volatile bool hid_busy01 = false;

static uint32_t crc32_ps(const uint8_t* d, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) { c ^= d[i]; for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); }
    return ~c;
}

static void hid_close() {
    rumble_cs_init();
    EnterCriticalSection(&rumble_cs_hid);
    hid_failed = false;
    hid_pid = 0;
    if (hid_dev != INVALID_HANDLE_VALUE) { CancelIoEx(hid_dev, nullptr); CloseHandle(hid_dev); }
    if (hid_ov.hEvent) { CloseHandle(hid_ov.hEvent); hid_ov.hEvent = nullptr; }
    hid_dev = INVALID_HANDLE_VALUE;
    hid_type = 0;
    hid_reading = false;
    hid_state_ok = false;
    hid_ext = false; hid_calm = 0; hid_busy01 = false;
    LeaveCriticalSection(&rumble_cs_hid);
}

// pid 0 opens the first Sony pad; with a pid, the same pad DirectInput chose
static void hid_open(uint16_t pid) {
    hid_close();
    static HMODULE su = LoadLibraryA("setupapi.dll"), hi = LoadLibraryA("hid.dll");
    if (!su || !hi) return;
    auto GetClassDevs = (HDEVINFO(WINAPI*)(const GUID*, PCWSTR, HWND, DWORD))GetProcAddress(su, "SetupDiGetClassDevsW");
    auto EnumIface = (BOOL(WINAPI*)(HDEVINFO, PSP_DEVINFO_DATA, const GUID*, DWORD, PSP_DEVICE_INTERFACE_DATA))GetProcAddress(su, "SetupDiEnumDeviceInterfaces");
    auto Detail = (BOOL(WINAPI*)(HDEVINFO, PSP_DEVICE_INTERFACE_DATA, PSP_DEVICE_INTERFACE_DETAIL_DATA_W, DWORD, PDWORD, PSP_DEVINFO_DATA))GetProcAddress(su, "SetupDiGetDeviceInterfaceDetailW");
    auto Destroy = (BOOL(WINAPI*)(HDEVINFO))GetProcAddress(su, "SetupDiDestroyDeviceInfoList");
    auto HidGuid = (void(NTAPI*)(GUID*))GetProcAddress(hi, "HidD_GetHidGuid");
    auto Attrib = (BOOLEAN(NTAPI*)(HANDLE, PHIDD_ATTRIBUTES))GetProcAddress(hi, "HidD_GetAttributes");
    auto Prep = (BOOLEAN(NTAPI*)(HANDLE, PHIDP_PREPARSED_DATA*))GetProcAddress(hi, "HidD_GetPreparsedData");
    auto Caps = (NTSTATUS(NTAPI*)(PHIDP_PREPARSED_DATA, PHIDP_CAPS))GetProcAddress(hi, "HidP_GetCaps");
    auto FreeData = (BOOLEAN(NTAPI*)(PHIDP_PREPARSED_DATA))GetProcAddress(hi, "HidD_FreePreparsedData");
    if (!GetClassDevs || !EnumIface || !Detail || !Destroy || !HidGuid || !Attrib || !Prep || !Caps || !FreeData) return;
    GUID g;
    HidGuid(&g);
    HDEVINFO inf = GetClassDevs(&g, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (inf == INVALID_HANDLE_VALUE) return;
    SP_DEVICE_INTERFACE_DATA id;
    id.cbSize = sizeof(id);
    for (int pass = di_path[0] ? 0 : 1; pass < 2 && hid_dev == INVALID_HANDLE_VALUE; pass++)
    for (DWORD i = 0; EnumIface(inf, nullptr, &g, i, &id); i++) {
        DWORD len = 0;
        Detail(inf, &id, nullptr, 0, &len, nullptr);
        if (!len || len > 4096) continue;
        uint8_t buf[4096];
        auto det = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W)buf;
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!Detail(inf, &id, det, len, nullptr, nullptr)) continue;
        if (!pass && _wcsicmp(det->DevicePath, di_path)) continue;
        HANDLE h = CreateFileW(det->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES at;
        at.Size = sizeof(at);
        int type = 0;
        if (Attrib(h, &at) && at.VendorID == 0x054C && (!pid || at.ProductID == pid)) {
            if (at.ProductID == 0x05C4 || at.ProductID == 0x09CC || at.ProductID == 0x0BA0) type = 1;
            if (at.ProductID == 0x0CE6 || at.ProductID == 0x0DF2) type = 2;
        }
        if (!type) { CloseHandle(h); continue; }
        PHIDP_PREPARSED_DATA pd = nullptr;
        HIDP_CAPS cp;
        memset(&cp, 0, sizeof(cp));
        if (Prep(h, &pd)) { Caps(pd, &cp); FreeData(pd); }
        EnterCriticalSection(&rumble_cs_hid);
        hid_dev = h;
        hid_type = type;
        hid_pid = at.ProductID;
        hid_bt = cp.OutputReportByteLength > 64;
        hid_in_len = cp.InputReportByteLength;
        if (hid_in_len < 64 || hid_in_len > sizeof(hid_in)) hid_in_len = sizeof(hid_in);
        memset(&hid_ov, 0, sizeof(hid_ov));
        hid_ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        LeaveCriticalSection(&rumble_cs_hid);
        printf("rumble: %s por %s (pid %04X, informes de entrada %u y salida %u bytes)\n", type == 1 ? "DualShock 4" : "DualSense", hid_bt ? "Bluetooth" : "USB", at.ProductID, (unsigned)cp.InputReportByteLength, (unsigned)cp.OutputReportByteLength);
        break;
    }
    Destroy(inf);
}

// only from the rumble thread with rumble_cs_hid held
static void hid_rumble(float f, float d) {
    if (hid_dev == INVALID_HANDLE_VALUE || hid_failed) return;
    if (hid_bt && !hid_ext && hid_calm < 12) return;
    uint8_t r[78];
    memset(r, 0, sizeof(r));
    DWORD n = 0, written = 0;
    uint8_t sb = (uint8_t)(f * 255.0f), wb = (uint8_t)(d * 255.0f);
    if (hid_type == 1) {
        if (!hid_bt) { r[0] = 0x05; r[1] = 0x07; r[4] = wb; r[5] = sb; r[6] = 0; r[7] = 0x20; r[8] = 0x60; n = 32; }
        else { r[0] = 0x11; r[1] = 0xC0; r[2] = 0x20; r[3] = 0x07; r[6] = wb; r[7] = sb; r[8] = 0; r[9] = 0x20; r[10] = 0x60; n = 78; }
    } else {
        uint8_t* p = r;
        if (!hid_bt) { r[0] = 0x02; p = r + 1; n = 48; }
        else { r[0] = 0x31; r[1] = (uint8_t)(hid_seq << 4); hid_seq = (hid_seq + 1) & 15; r[2] = 0x10; p = r + 3; n = 78; }
        p[0] = 0x03; p[1] = 0x04; p[2] = wb; p[3] = sb; p[38] = 0x02; p[43] = 0x02; p[44] = 0; p[45] = 0x20; p[46] = 0x60;
    }
    if (hid_bt) {
        uint8_t crc_buf[75];
        crc_buf[0] = 0xA2;
        memcpy(crc_buf + 1, r, 74);
        uint32_t c = crc32_ps(crc_buf, 75);
        r[74] = (uint8_t)c; r[75] = (uint8_t)(c >> 8); r[76] = (uint8_t)(c >> 16); r[77] = (uint8_t)(c >> 24);
    }
    OVERLAPPED ov;
    memset(&ov, 0, sizeof(ov));
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    bool ok = WriteFile(hid_dev, r, n, &written, &ov) || GetLastError() == ERROR_IO_PENDING;
    if (ok && WaitForSingleObject(ov.hEvent, 200) != WAIT_OBJECT_0) { CancelIo(hid_dev); ok = false; }
    if (ok) ok = GetOverlappedResult(hid_dev, &ov, &written, FALSE) != 0;
    CloseHandle(ov.hEvent);
    if (!ok) { printf("rumble: fallo al escribir (error %lu), se reintenta\n", GetLastError()); hid_failed = true; }
}

// HID input: after the first Bluetooth output report the pad stops sending report 0x01, which DirectInput reads,
// so buttons and axes are read here in DirectInput order
static void hid_decode(const uint8_t* r, DWORD n) {
    const uint8_t* p;
    bool ds5 = hid_type == 2;
    if (r[0] == 0x01) p = r + 1;
    else if (r[0] == 0x11 && n >= 12) p = r + 3;
    else if (r[0] == 0x31 && n >= 12) p = r + 2;
    else return;
    DIJOYSTATE e;
    memset(&e, 0, sizeof(e));
    for (int i = 0; i < 4; i++) e.rgdwPOV[i] = 0xFFFFFFFFu;
    auto axis = [](uint8_t v) { LONG x = (LONG)((v - 128) * 1000 / 127); return x < -1000 ? -1000 : x; };
    e.lX = axis(p[0]); e.lY = axis(p[1]); e.lZ = axis(p[2]); e.lRz = axis(p[3]);
    uint8_t b0, b1, b2, l2, r2;
    if (!ds5) { b0 = p[4]; b1 = p[5]; b2 = p[6]; l2 = p[7]; r2 = p[8]; }
    else { l2 = p[4]; r2 = p[5]; b0 = p[7]; b1 = p[8]; b2 = p[9]; }
    e.lRx = (LONG)(l2 * 2000 / 255) - 1000;
    e.lRy = (LONG)(r2 * 2000 / 255) - 1000;
    uint8_t hat = b0 & 15;
    if (hat < 8) e.rgdwPOV[0] = hat * 4500;
    for (int i = 0; i < 4; i++) e.rgbButtons[i] = (b0 >> (4 + i)) & 1 ? 0x80 : 0;
    for (int i = 0; i < 8; i++) e.rgbButtons[4 + i] = (b1 >> i) & 1 ? 0x80 : 0;
    e.rgbButtons[12] = b2 & 1 ? 0x80 : 0;
    e.rgbButtons[13] = b2 & 2 ? 0x80 : 0;
    e.rgbButtons[14] = ds5 && (b2 & 4) ? 0x80 : 0;
    if (r[0] == 0x01) {
        auto mid = [](uint8_t v) { return v > 100 && v < 156; };
        bool calm = hat >= 8 && !(b0 >> 4) && !b1 && !(b2 & 3) && l2 < 30 && r2 < 30 && mid(p[0]) && mid(p[1]) && mid(p[2]) && mid(p[3]);
        hid_calm = calm ? hid_calm + 1 : 0;
        hid_busy01 = !calm;
    } else if (!hid_ext) {
        hid_ext = true;
        if (hid_bt) printf("rumble: mando en modo extendido, ultimo informe basico %s\n", hid_busy01 ? "con botones pulsados (posible tecla pegada en Windows)" : "en reposo");
    }
    hid_state = e;
    if (!hid_state_ok) printf("input: mando Sony leido por HID (informe 0x%02X)\n", r[0]);
    hid_state_ok = true;
}

// drains pending reports and leaves one read in flight; true when the state is valid
static bool hid_read() {
    if (hid_failed) { hid_close(); hid_retry_ms = GetTickCount() + 3000; }
    if (hid_dev == INVALID_HANDLE_VALUE) return false;
    for (int k = 0; k < 64; k++) {
        if (!hid_reading) {
            ResetEvent(hid_ov.hEvent);
            hid_ov.Offset = hid_ov.OffsetHigh = 0;
            DWORD n = 0;
            if (ReadFile(hid_dev, hid_in, hid_in_len, &n, &hid_ov)) { hid_decode(hid_in, n); continue; }
            if (GetLastError() != ERROR_IO_PENDING) { printf("input: fallo al leer el mando por HID (error %lu)\n", GetLastError()); hid_close(); hid_retry_ms = GetTickCount() + 3000; return false; }
            hid_reading = true;
        }
        DWORD n = 0;
        if (!GetOverlappedResult(hid_dev, &hid_ov, &n, FALSE)) {
            if (GetLastError() == ERROR_IO_INCOMPLETE) break;
            printf("input: lectura HID interrumpida (error %lu)\n", GetLastError());
            hid_close(); hid_retry_ms = GetTickCount() + 3000; return false;
        }
        hid_reading = false;
        hid_decode(hid_in, n);
    }
    return hid_state_ok;
}

// local multiplayer: each player's own pad (XInput slot or the DirectInput/HID pad), by device index of input.h
static float rd_strong[N_DEVS], rd_weak[N_DEVS];
static double rd_until[N_DEVS];
static bool rd_used = false;
static void rumble_devs_send(double now) {
    static float lf[N_DEVS] = {-1, -1, -1, -1, -1, -1}, ld[N_DEVS] = {-1, -1, -1, -1, -1, -1};
    static double ls[N_DEVS];
    static int zr[N_DEVS];
    if (!rd_used) return;
    for (int dv = DEV_XI0; dv < N_DEVS; dv++) {
        EnterCriticalSection(&rumble_cs_state);
        if (rd_until[dv] > 0 && now >= rd_until[dv]) { rd_until[dv] = 0; rd_strong[dv] = rd_weak[dv] = 0; }
        float k = rumble_level / 100.0f, f = rd_strong[dv] * k, d = rd_weak[dv] * k;
        LeaveCriticalSection(&rumble_cs_state);
        bool active = f > 0 || d > 0, same = f == lf[dv] && d == ld[dv];
        // resent every 100 ms while active, and the stop three more times: a lost stop report left the motor running
        if (active) zr[dv] = 3;
        if (same && !(now - ls[dv] >= 100.0 && (active || zr[dv] > 0))) continue;
        if (same && !active) zr[dv]--;
        lf[dv] = f; ld[dv] = d; ls[dv] = now;
        if (dv == DEV_DI) { EnterCriticalSection(&rumble_cs_hid); hid_rumble(f, d); LeaveCriticalSection(&rumble_cs_hid); }
        else xi_rumble(dv - DEV_XI0, f, d);
    }
}

static DWORD WINAPI rumble_thread(LPVOID) {
    float last_f = -1, last_d = -1;
    double last_send = 0;
    int xi_sent = -1;
    int log = getenv("TTT_RUMBLE_LOG") ? atoi(getenv("TTT_RUMBLE_LOG")) : 0;
    for (;;) {
        WaitForSingleObject(rumble_event, 10);
        double now = rumble_ms();
        rumble_devs_send(now);
        EnterCriticalSection(&rumble_cs_state);
        if (rumble_until > 0 && now >= rumble_until) { rumble_until = 0; rumble_strong = rumble_weak = 0; }
        float k = rumble_level / 100.0f;
        float f = rumble_strong * k, d = rumble_weak * k;
        LeaveCriticalSection(&rumble_cs_state);
        if (!pad_live || input_mode != 1) f = d = 0;
        bool active = f > 0 || d > 0;
        // resent every 100 ms while active: some pads stop the motor without reports
        if (f == last_f && d == last_d && !(active && now - last_send >= 100.0)) continue;
        last_f = f; last_d = d; last_send = now;
        if (log >= 3) printf("rumble: envio %.2f %.2f\n", f, d);
        int src = pad_live ? pad_src : 0;
        {
            int t = src == 1 ? xi_used : -1;
            if (xi_sent >= 0 && xi_sent != t) xi_rumble(xi_sent, 0, 0);
            xi_sent = t;
            if (t >= 0) xi_rumble(t, f, d);
        }
        if (src != 2 && !(f == 0 && d == 0)) continue;
        EnterCriticalSection(&rumble_cs_hid);
        hid_rumble(f, d);
        LeaveCriticalSection(&rumble_cs_hid);
    }
    return 0;
}

// api: strong and weak 0..1, duration in ms; extends the duration and keeps the highest strength
static void rumble_start() {
    rumble_cs_init();
    if (!rumble_event) {
        rumble_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        HANDLE h = CreateThread(nullptr, 0, rumble_thread, nullptr, 0, nullptr);
        if (h) { SetThreadPriority(h, THREAD_PRIORITY_ABOVE_NORMAL); CloseHandle(h); }
    }
}

static void rumble_request_dev(int dv, float strong, float weak, int ms) {
    if (rumble_level <= 0 || dv < DEV_XI0 || dv >= N_DEVS) return;
    rumble_start();
    double h = rumble_ms() + (ms < 1 ? 1 : ms);
    EnterCriticalSection(&rumble_cs_state);
    rd_used = true;
    bool changed = strong > rd_strong[dv] || weak > rd_weak[dv];
    if (strong > rd_strong[dv]) rd_strong[dv] = strong;
    if (weak > rd_weak[dv]) rd_weak[dv] = weak;
    if (h > rd_until[dv]) rd_until[dv] = h;
    LeaveCriticalSection(&rumble_cs_state);
    if (changed) SetEvent(rumble_event);
}

// api: strong and weak 0..1, duration in ms; extends the duration and keeps the highest strength
static void rumble_request(float strong, float weak, int ms) {
    if (rumble_level <= 0) return;
    rumble_start();
    double h = rumble_ms() + (ms < 1 ? 1 : ms);
    EnterCriticalSection(&rumble_cs_state);
    bool changed = strong > rumble_strong || weak > rumble_weak;
    if (strong > rumble_strong) rumble_strong = strong;
    if (weak > rumble_weak) rumble_weak = weak;
    if (h > rumble_until) rumble_until = h;
    LeaveCriticalSection(&rumble_cs_state);
    if (changed) SetEvent(rumble_event);
}

static void rumble_update() {}

static void rumble_stop() {
    if (!rumble_event) return;
    EnterCriticalSection(&rumble_cs_state);
    rumble_strong = rumble_weak = 0;
    rumble_until = 0;
    for (int dv = 0; dv < N_DEVS; dv++) { rd_strong[dv] = rd_weak[dv] = 0; rd_until[dv] = 0; }
    LeaveCriticalSection(&rumble_cs_state);
    SetEvent(rumble_event);
}
