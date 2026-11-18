// Discord Rich Presence and game invites over the local Discord IPC pipe (no SDK), included by net/online.cpp
static const char* DC_APP = "1557149829296169020";
struct DcPres { std::string details, state, party; int n = 0, max = 0; };
static std::mutex dc_m;
static DcPres dc_want;
static bool dc_dirty = true;
static std::atomic<int> dc_st{0}, dc_enabled{1};
static HANDLE dc_h = INVALID_HANDLE_VALUE;
static int64_t dc_t0 = 0;
static int dc_nonce = 0;

static void dc_log(const char* f, ...) {
    char b[512];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof(b), f, a);
    va_end(a);
    printf("discord: %s\n", b);
    fflush(stdout);
}

static void dc_close() {
    if (dc_h != INVALID_HANDLE_VALUE) CloseHandle(dc_h);
    dc_h = INVALID_HANDLE_VALUE;
    if (dc_st == 2) dc_log("desconectado");
    dc_st = dc_enabled ? 1 : 0;
}

static bool dc_write(int op, const std::string& s) {
    std::string b(8 + s.size(), '\0');
    uint32_t o = (uint32_t)op, l = (uint32_t)s.size();
    memcpy(&b[0], &o, 4); memcpy(&b[4], &l, 4); memcpy(&b[8], s.data(), s.size());
    DWORD w = 0;
    return WriteFile(dc_h, b.data(), (DWORD)b.size(), &w, nullptr) && w == b.size();
}

static bool dc_exact(void* p, DWORD n) {
    char* d = (char*)p;
    while (n) { DWORD r = 0; if (!ReadFile(dc_h, d, n, &r, nullptr) || !r) return false; d += r; n -= r; }
    return true;
}

// -1 nothing waiting, -2 pipe broken, else the frame opcode
static int dc_read(std::string& out, int wait_ms) {
    DWORD av = 0;
    for (int t = 0;; t += 20) {
        if (!PeekNamedPipe(dc_h, nullptr, 0, nullptr, &av, nullptr)) return -2;
        if (av >= 8) break;
        if (t >= wait_ms) return -1;
        Sleep(20);
    }
    uint32_t hd[2];
    if (!dc_exact(hd, 8) || hd[1] > (1u << 20)) return -2;
    out.assign(hd[1], '\0');
    if (hd[1] && !dc_exact(&out[0], hd[1])) return -2;
    return (int)hd[0];
}

static bool dc_connect() {
    const char* e = getenv("TTT_DISCORD_PIPE");
    int a = e && *e ? atoi(e) : 0, z = e && *e ? a : 9;
    for (int i = a; i <= z && dc_h == INVALID_HANDLE_VALUE; i++) {
        wchar_t n[64];
        swprintf(n, 64, L"\\\\.\\pipe\\discord-ipc-%d", i);
        dc_h = CreateFileW(n, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (dc_h != INVALID_HANDLE_VALUE) a = i;
    }
    if (dc_h == INVALID_HANDLE_VALUE) return false;
    std::string r, user;
    if (!dc_write(0, std::string("{\"v\":1,\"client_id\":\"") + DC_APP + "\"}") || dc_read(r, 3000) != 1 || r.find("\"READY\"") == std::string::npos) {
        dc_log("Discord respondio mal al saludo (tuberia %d): %.120s", a, r.c_str());
        dc_close();
        return false;
    }
    if (!js_get(r, "global_name", user) || user.empty() || user == "null") js_get(r, "username", user);
    dc_st = 2;
    { std::lock_guard<std::mutex> g(dc_m); dc_dirty = true; }
    dc_log("conectado a Discord por la tuberia %d (usuario %s)", a, user.c_str());
    return true;
}

static std::string dc_activity(const DcPres& p) {
    std::string a = "{\"timestamps\":{\"start\":" + std::to_string(dc_t0) + "},\"assets\":{\"large_image\":\"logo\",\"large_text\":\"DBZ Tenkaichi Tag Team\"}";
    if (!p.details.empty()) a += ",\"details\":\"" + js_esc(p.details) + "\"";
    if (!p.state.empty()) a += ",\"state\":\"" + js_esc(p.state) + "\"";
    if (!p.party.empty() && p.max > 0) a += ",\"party\":{\"id\":\"" + js_esc(p.party) + "\",\"size\":[" + std::to_string(p.n) + "," + std::to_string(p.max) + "]}";
    return a + "}";
}

static void dc_event(const std::string& m) {
    std::string evt, s;
    js_get(m, "evt", evt);
    if (evt == "ERROR") {
        static int n = 0;
        if (n++ < 10) dc_log("error de Discord: %.200s", m.c_str());
    }
}

// invites were removed: drops the discord-<app> launch protocol older builds registered
static void dc_unregister() {
    std::wstring app(DC_APP, DC_APP + strlen(DC_APP)), key = L"Software\\Classes\\discord-" + app;
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, key.c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return;
    RegCloseKey(k);
    if (RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str()) == ERROR_SUCCESS) RegDeleteKeyW(HKEY_CURRENT_USER, key.c_str());
    dc_log("arranque desde Discord de versiones anteriores eliminado");
}

static void dc_thread() {
    uint64_t last_try = 0, last_send = 0;
    bool told = false, registered = false;
    for (;;) {
        if (!dc_enabled) { if (dc_h != INVALID_HANDLE_VALUE) dc_close(); dc_st = 0; Sleep(300); continue; }
        if (!registered) { registered = true; dc_unregister(); }
        if (dc_h == INVALID_HANDLE_VALUE) {
            dc_st = 1;
            if (on_ms() - last_try < 5000 && last_try) { Sleep(200); continue; }
            last_try = on_ms();
            if (!dc_connect()) { if (!told) { told = true; dc_log("Discord no esta abierto, se reintenta cada 5 s"); } continue; }
            told = false;
        }
        std::string m;
        int op;
        while ((op = dc_read(m, 0)) >= 0) {
            if (op == 1) dc_event(m);
            else if (op == 3) dc_write(4, m);
            else if (op == 2) { dc_log("Discord cerro la conexion: %.120s", m.c_str()); op = -2; break; }
        }
        if (op == -2) { dc_close(); continue; }
        std::string act;
        {
            std::lock_guard<std::mutex> g(dc_m);
            if (dc_dirty && on_ms() - last_send >= 4000) { dc_dirty = false; act = dc_activity(dc_want); }
        }
        if (!act.empty()) {
            last_send = on_ms();
            std::string cmd = "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"a" + std::to_string(++dc_nonce) + "\",\"args\":{\"pid\":" + std::to_string(GetCurrentProcessId()) + ",\"activity\":" + act + "}}";
            if (!dc_write(1, cmd)) { dc_close(); continue; }
        }
        Sleep(100);
    }
}

void net_discord_enable(bool on) {
    if (dc_enabled == (int)on) return;
    dc_enabled = on;
    dc_log("integracion %s", on ? "activada" : "desactivada");
}

static void net_discord_boot() {
    const char* e = getenv("TTT_DISCORD");
    dc_enabled = !(e && *e == '0');
    dc_t0 = (int64_t)time(nullptr);
    dc_log("integracion %s", dc_enabled ? "activada" : "desactivada (TTT_DISCORD=0)");
    std::thread(dc_thread).detach();
}

int net_discord_state() { return dc_st; }

void net_discord_presence(const char* details, const char* state, const char* room, int n, int max) {
    DcPres p;
    p.details = details ? details : ""; p.state = state ? state : "";
    if (room && *room) { p.party = room; p.n = n; p.max = max; }
    std::lock_guard<std::mutex> g(dc_m);
    if (p.details == dc_want.details && p.state == dc_want.state && p.party == dc_want.party && p.n == dc_want.n && p.max == dc_want.max) return;
    bool show = p.party != dc_want.party || p.details != dc_want.details;
    dc_want = p; dc_dirty = true;
    if (show) dc_log("estado: %s | %s%s%s", p.details.c_str(), p.state.c_str(), p.party.empty() ? "" : (" | sala " + p.party + " " + std::to_string(n) + "/" + std::to_string(max)).c_str(), "");
}
