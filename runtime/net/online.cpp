// Online play: rooms service (Cloudflare Worker, server/worker.js) plus ICE peers (libjuice) under the multiplayer UDP transport
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <ctime>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#define JUICE_STATIC
#include "net/juice/juice.h"
#include "net/online.h"

static const char* ON_PROTO = "5";
static const char* ON_URL = "https://ttt-online.kkmarquito.workers.dev";
static std::atomic<int> on_mode{0};   // 0 off, 1 host, 2 join
static std::string on_host_name, on_nick = "Jugador";
static std::wstring on_whost;
static int on_port = 443;
static bool on_tls = true;
static HINTERNET on_ses = nullptr;
static int on_relay_only = 0, on_wsrelay = 1;
static struct { std::string host, user, pass; int port = 0; } on_turn;

struct OnPeer { juice_agent_t* a = nullptr; int gen = 0; std::string nick, jid; std::atomic<uint64_t> mac{0}; std::atomic<int> st{0}; std::atomic<bool> gathered{false}, wsr{false}; bool logged = false; uint64_t t0 = 0; std::shared_ptr<struct OnWs> rw; std::vector<uint8_t> last_stream; };
static std::vector<OnPeer*> on_peers;
static std::recursive_mutex on_mtx;
static std::deque<std::vector<uint8_t>> on_rx;
static std::mutex on_rxm;

struct OnWs { HINTERNET con = nullptr, ws = nullptr; std::mutex sm; std::atomic<bool> alive{true}; };
static std::shared_ptr<OnWs> on_host_ws;

static uint64_t on_ms() { return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static uint64_t on_mac(const uint8_t* m) { uint64_t v = 0; for (int i = 0; i < 6; i++) v = v << 8 | m[i]; return v; }

static void on_log(const char* f, ...) {
    char b[512];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof(b), f, a);
    va_end(a);
    printf("net: en linea: %s\n", b);
    fflush(stdout);
}

static std::wstring on_w(const std::string& s) { return std::wstring(s.begin(), s.end()); }

static std::string on_q(const std::string& s) {
    static const char* hx = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (char)c;
        else { o += '%'; o += hx[c >> 4]; o += hx[c & 15]; }
    }
    return o;
}

static std::string js_esc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char t[8]; snprintf(t, sizeof(t), "\\u%04x", c); o += t; }
        else o += (char)c;
    }
    return o;
}

// value of "key" in a flat JSON object (strings unescaped, numbers and literals as text)
static bool js_get(const std::string& j, const char* key, std::string& out) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = 0;
    while ((p = j.find(k, p)) != std::string::npos) {
        size_t c = p + k.size();
        while (c < j.size() && isspace((unsigned char)j[c])) c++;
        if (c >= j.size() || j[c] != ':') { p = c; continue; }
        c++;
        while (c < j.size() && isspace((unsigned char)j[c])) c++;
        out.clear();
        if (c < j.size() && j[c] == '"') {
            for (c++; c < j.size() && j[c] != '"'; c++) {
                if (j[c] != '\\' || c + 1 >= j.size()) { out += j[c]; continue; }
                char e = j[++c];
                if (e == 'n') out += '\n';
                else if (e == 'r') out += '\r';
                else if (e == 't') out += '\t';
                else if (e == 'u' && c + 4 < j.size()) { out += (char)strtol(j.substr(c + 1, 4).c_str(), nullptr, 16); c += 4; }
                else out += e;
            }
        } else {
            while (c < j.size() && j[c] != ',' && j[c] != '}' && j[c] != ']') out += j[c++];
            while (!out.empty() && isspace((unsigned char)out.back())) out.pop_back();
        }
        return true;
    }
    return false;
}

static std::vector<std::string> js_objects(const std::string& j) {
    std::vector<std::string> v;
    int depth = 0;
    bool str = false;
    size_t start = 0;
    for (size_t i = 0; i < j.size(); i++) {
        char c = j[i];
        if (str) { if (c == '\\') i++; else if (c == '"') str = false; continue; }
        if (c == '"') str = true;
        else if (c == '{') { if (depth++ == 0) start = i; }
        else if (c == '}') { if (--depth == 0) v.push_back(j.substr(start, i - start + 1)); }
    }
    return v;
}

static bool on_parse_url(std::string s) {
    on_tls = s.rfind("http://", 0) != 0;
    size_t p = s.find("://");
    if (p != std::string::npos) s = s.substr(p + 3);
    p = s.find('/');
    if (p != std::string::npos) s = s.substr(0, p);
    on_port = on_tls ? 443 : 80;
    p = s.find(':');
    if (p != std::string::npos) { on_port = atoi(s.c_str() + p + 1); s = s.substr(0, p); }
    on_host_name = s;
    on_whost = on_w(s);
    return !s.empty();
}

static HINTERNET on_open(const char* method, const std::string& path, bool ws, int* status, HINTERNET* con_out) {
    *status = 0;
    HINTERNET con = WinHttpConnect(on_ses, on_whost.c_str(), (INTERNET_PORT)on_port, 0);
    if (!con) return nullptr;
    HINTERNET rq = WinHttpOpenRequest(con, on_w(method).c_str(), on_w(path).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, on_tls ? WINHTTP_FLAG_SECURE : 0);
    if (!rq) { WinHttpCloseHandle(con); return nullptr; }
    if (ws) WinHttpSetOption(rq, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0);
    if (WinHttpSendRequest(rq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(rq, nullptr)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        *status = (int)code;
    } else *status = -(int)GetLastError();
    *con_out = con;
    return rq;
}

static int on_get(const std::string& path, std::string& body) {
    int st = 0;
    HINTERNET con = nullptr, rq = on_open("GET", path, false, &st, &con);
    body.clear();
    if (rq && st > 0) {
        DWORD n = 0;
        while (WinHttpQueryDataAvailable(rq, &n) && n) {
            std::string b(n, 0);
            DWORD r = 0;
            if (!WinHttpReadData(rq, &b[0], n, &r) || !r) break;
            body.append(b.data(), r);
        }
    }
    if (rq) WinHttpCloseHandle(rq);
    if (con) WinHttpCloseHandle(con);
    return st;
}

static bool on_ws_send(const std::shared_ptr<OnWs>& w, const std::string& s) {
    if (!w || !w->ws || !w->alive) return false;
    std::lock_guard<std::mutex> g(w->sm);
    return WinHttpWebSocketSend(w->ws, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, (PVOID)s.data(), (DWORD)s.size()) == NO_ERROR;
}

static bool on_ws_send_bin(const std::shared_ptr<OnWs>& w, const void* d, size_t n) {
    if (!w || !w->ws || !w->alive) return false;
    std::lock_guard<std::mutex> g(w->sm);
    return WinHttpWebSocketSend(w->ws, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE, (PVOID)d, (DWORD)n) == NO_ERROR;
}

static bool on_ws_recv(const std::shared_ptr<OnWs>& w, std::string& msg, bool* bin = nullptr) {
    msg.clear();
    if (bin) *bin = false;
    char buf[8192];
    for (;;) {
        DWORD r = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE t;
        if (WinHttpWebSocketReceive(w->ws, buf, sizeof(buf), &r, &t) != NO_ERROR) return false;
        if (t == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) return false;
        msg.append(buf, r);
        if (bin && (t == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE || t == WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE)) *bin = true;
        if (t == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE || t == WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE) return true;
    }
}

static std::shared_ptr<OnWs> on_ws_open(const std::string& path, int* status) {
    HINTERNET con = nullptr, rq = on_open("GET", path, true, status, &con);
    std::shared_ptr<OnWs> w;
    if (rq && *status == 101) {
        HINTERNET h = WinHttpWebSocketCompleteUpgrade(rq, 0);
        if (h) {
            w = std::make_shared<OnWs>();
            w->con = con;
            w->ws = h;
            con = nullptr;
            std::thread([w] {
                while (w->alive) {
                    for (int i = 0; i < 200 && w->alive; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    if (w->alive) on_ws_send(w, "ping");
                }
            }).detach();
        }
    }
    if (rq) WinHttpCloseHandle(rq);
    if (con) WinHttpCloseHandle(con);
    return w;
}

static void on_ws_close(const std::shared_ptr<OnWs>& w) {
    if (!w || !w->alive.exchange(false)) return;
    std::lock_guard<std::mutex> g(w->sm);
    WinHttpWebSocketClose(w->ws, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
}

static std::atomic<int> on_gen{0}, on_status{OS_IDLE}, on_err{0}, on_nconn{0}, on_acc_state{OA_NONE}, on_list_state{0}, on_acc_ok{0};
static std::atomic<bool> on_list_busy{false};
static std::string on_room_id, on_acc_name, on_acc_key, on_acc_id, on_mods = "0", on_mods_rank = "0", on_cfg = "online.cfg";
static std::vector<OnRoomInfo> on_list;
static std::atomic<int> on_game_mode{0}, on_game_cust{0};
void net_online_set_game_mode(int m, int cust) { on_game_mode = m ? 1 : 0; on_game_cust = cust ? 1 : 0; }
static std::mutex on_sm;
static uint64_t on_turn_ms = 0;

static std::string on_auth();
static void on_turn_load() {
    if (on_turn_ms && on_ms() - on_turn_ms < 3600000) return;
    std::string b, u, c, h, p, v;
    int st = on_get("/turn?v=" + std::string(ON_PROTO) + on_auth(), b);
    on_turn_ms = on_ms();
    if (st == 200 && js_get(b, "username", u) && !u.empty() && js_get(b, "credential", c)) {
        on_turn.user = u;
        on_turn.pass = c;
        on_turn.host = js_get(b, "host", h) && !h.empty() ? h : "turn.cloudflare.com";
        on_turn.port = js_get(b, "port", p) ? atoi(p.c_str()) : 3478;
        on_log("TURN listo (%s, %s:%d)", js_get(b, "via", v) ? v.c_str() : "?", on_turn.host.c_str(), on_turn.port);
    } else { on_turn_ms = st == 401 ? 0 : on_turn_ms; on_log("TURN no disponible (codigo %d)%s", st, on_wsrelay ? ", queda el relay del servicio" : ", solo conexion directa"); }
}

static const char* on_route(juice_agent_t* a) {
    char l[JUICE_MAX_CANDIDATE_SDP_STRING_LEN] = "", r[JUICE_MAX_CANDIDATE_SDP_STRING_LEN] = "";
    if (juice_get_selected_candidates(a, l, sizeof(l), r, sizeof(r)) < 0) return "ruta desconocida";
    if (strstr(l, " typ relay") || strstr(r, " typ relay")) return "relay (TURN)";
    if (strstr(l, " typ srflx") || strstr(r, " typ srflx")) return "directo, NAT atravesado";
    return "directo, red local";
}

static void on_cb_state(juice_agent_t* a, juice_state_t s, void* u) {
    OnPeer* p = (OnPeer*)u;
    p->st = (int)s;
    if (p->gen != on_gen) return;
    if ((s == JUICE_STATE_CONNECTED || s == JUICE_STATE_COMPLETED) && !p->logged) {
        p->logged = true;
        ++on_nconn;
        on_log("conectado con %s por %s en %d ms", p->nick.c_str(), on_route(a), (int)(on_ms() - p->t0));
        if (on_mode != 1) on_status = OS_CONNECTED;
    } else if (s == JUICE_STATE_FAILED) on_log("conexion fallida con %s", p->nick.c_str());
}

static void on_cb_gather(juice_agent_t*, void* u) { ((OnPeer*)u)->gathered = true; }

static void on_rx_push(OnPeer* p, const char* d, size_t n) {
    if (p->gen != on_gen) return;
    if (n >= 20 && !memcmp(d, "TTN", 3)) p->mac = on_mac((const uint8_t*)d + 8);
    std::lock_guard<std::mutex> g(on_rxm);
    if (on_rx.size() < 4096) on_rx.emplace_back((const uint8_t*)d, (const uint8_t*)d + n);
}

static void on_cb_recv(juice_agent_t*, const char* d, size_t n, void* u) { on_rx_push((OnPeer*)u, d, n); }

static bool on_ice_up(OnPeer* p) { return p->st == JUICE_STATE_CONNECTED || p->st == JUICE_STATE_COMPLETED; }

// the service relay: the guest sends over its join socket, the host over its room socket with the 4-byte join id first
static void on_relay_up(OnPeer* p, const char* side) {
    p->wsr = true;
    if (p->logged) return;
    p->logged = true;
    ++on_nconn;
    on_log("conectado con %s por relay del servicio (WebSocket, %s) en %d ms", p->nick.c_str(), side, (int)(on_ms() - p->t0));
    if (on_mode != 1) on_status = OS_CONNECTED;
}

static void on_jlog(juice_log_level_t, const char* m) { on_log("juice: %s", m); }

static OnPeer* on_agent(int gen, const std::string& nick, const std::string& jid) {
    OnPeer* p = new OnPeer;
    p->gen = gen;
    p->nick = nick;
    p->jid = jid;
    p->t0 = on_ms();
    juice_config_t c;
    memset(&c, 0, sizeof(c));
    c.concurrency_mode = JUICE_CONCURRENCY_MODE_POLL;
    c.stun_server_host = "stun.cloudflare.com";
    c.stun_server_port = 3478;
    juice_turn_server_t ts = {on_turn.host.c_str(), on_turn.user.c_str(), on_turn.pass.c_str(), (uint16_t)on_turn.port};
    if (!on_turn.user.empty()) { c.turn_servers = &ts; c.turn_servers_count = 1; }
    c.cb_state_changed = on_cb_state;
    c.cb_gathering_done = on_cb_gather;
    c.cb_recv = on_cb_recv;
    c.user_ptr = p;
    p->a = juice_create(&c);
    if (!p->a) { delete p; on_log("no se pudo crear el agente ICE"); return nullptr; }
    std::lock_guard<std::recursive_mutex> g(on_mtx);
    if (gen != on_gen) { juice_destroy(p->a); delete p; return nullptr; }
    on_peers.push_back(p);
    return p;
}

// TTT_ONLINE_RELAY=1 also drops the peer's host and srflx candidates, so the forced side cannot pair directly
static std::string on_remote_sdp(const std::string& sdp) {
    if (!on_relay_only || on_turn.user.empty()) return sdp;
    std::string out, line;
    int cut = 0;
    for (size_t i = 0; i < sdp.size(); i++) {
        line += sdp[i];
        if (sdp[i] != '\n' && i + 1 < sdp.size()) continue;
        bool cand = line.rfind("a=candidate:", 0) == 0;
        if (cand && (line.find(" typ host") != std::string::npos || line.find(" typ srflx") != std::string::npos)) cut++; else out += line;
        line.clear();
    }
    on_log("TTT_ONLINE_RELAY: %d candidatos directos del otro descartados", cut);
    return out;
}

// gathers, waits for it and returns the local description (relay candidates only with TTT_ONLINE_RELAY=1 and TURN ready)
static std::string on_describe(OnPeer* p) {
    juice_gather_candidates(p->a);
    for (int i = 0; i < 50 && !p->gathered; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    char sdp[JUICE_MAX_SDP_STRING_LEN] = "";
    juice_get_local_description(p->a, sdp, sizeof(sdp));
    bool relay = on_relay_only && !on_turn.user.empty();
    if (on_relay_only && !relay) on_log("TTT_ONLINE_RELAY ignorado: no hay TURN");
    std::string out, line;
    int nh = 0, ns = 0, nr = 0;
    for (const char* c = sdp; *c; c++) {
        line += *c;
        if (*c != '\n' && c[1]) continue;
        bool cand = line.rfind("a=candidate:", 0) == 0;
        bool h = cand && line.find(" typ host") != std::string::npos, s = cand && line.find(" typ srflx") != std::string::npos, r = cand && line.find(" typ relay") != std::string::npos;
        nh += h;
        ns += s;
        nr += r;
        if (!(relay && (h || s))) out += line;
        line.clear();
    }
    on_log("candidatos propios: host %d, srflx %d, relay %d%s%s", nh, ns, nr, p->gathered ? "" : " (recoleccion incompleta)", relay ? ", se envian solo los relay" : "");
    return out;
}

static std::string on_auth() { return "&id=" + on_q(on_acc_id) + "&key=" + on_q(on_acc_key); }

static void on_fail(int gen, int err, const char* what) {
    if (gen != on_gen) return;
    on_err = err;
    on_status = OS_FAILED;
    on_mode = 0;
    on_log("%s (codigo %d)", what, err);
}

static std::string on_hex(const uint8_t* d, size_t n);

static bool on_rank_host = false;
static std::atomic<bool> on_rank_now{false};
static std::string on_rank_room;
static int on_rank_n = 0;
static std::atomic<int> on_rank_kind{0};
static void on_host_thread(int gen, std::string name, std::string pw, int maxp, int pub, std::string mm) {
    on_turn_load();
    int st = 0;
    on_rank_host = !mm.empty();
    std::string path = "/host?v=" + std::string(ON_PROTO) + on_auth();
    if (on_rank_host) path += "&mm=" + on_q(mm);
    else path += "&title=" + on_q(name) + "&pub=" + (pub ? "1" : "0") + "&max=" + std::to_string(maxp) + "&mods=" + on_q(on_mods) + "&mode=" + std::to_string((int)on_game_mode) + "&cust=" + std::to_string((int)on_game_cust);
    if (!pw.empty()) path += "&pw=" + on_q(pw);
    std::shared_ptr<OnWs> w = on_ws_open(path, &st);
    if (!w) { on_fail(gen, st, "el servicio no acepto la sala"); return; }
    if (gen != on_gen) { on_ws_close(w); return; }
    on_host_ws = w;
    std::string m, t;
    bool bin = false;
    while (on_ws_recv(w, m, &bin)) {
        if (gen != on_gen) break;
        if (bin) {
            if (m.size() <= 4) continue;
            std::string jid = on_hex((const uint8_t*)m.data(), 4);
            std::lock_guard<std::recursive_mutex> g(on_mtx);
            for (OnPeer* p : on_peers) if (p->jid == jid && p->wsr) { on_rx_push(p, m.data() + 4, m.size() - 4); break; }
            continue;
        }
        if (m == "pong" || !js_get(m, "t", t)) continue;
        if (t == "relay") {
            std::string jid;
            js_get(m, "jid", jid);
            std::lock_guard<std::recursive_mutex> g(on_mtx);
            for (OnPeer* p : on_peers) if (p->jid == jid && p->gen == gen) on_relay_up(p, "anfitrion");
        } else if (t == "room") {
            std::string id, pb;
            js_get(m, "id", id);
            js_get(m, "pub", pb);
            { std::lock_guard<std::mutex> g(on_sm); on_room_id = id; on_rank_room = id; on_rank_n = 0; }
            on_rank_now = on_rank_host; on_rank_kind = on_game_mode.load();
            on_status = OS_HOSTING;
            on_log("sala creada, ID %s (%s%s%s), cuenta %s", id.c_str(), pb == "true" ? "publica" : "privada", pw.empty() ? "" : ", con contrasena", on_rank_host ? ", clasificatoria" : "", on_acc_name.c_str());
        } else if (t == "offer") {
            std::string jid, nick, sdp;
            js_get(m, "jid", jid);
            js_get(m, "nick", nick);
            js_get(m, "sdp", sdp);
            on_log("oferta de %s (union %s)", nick.c_str(), jid.c_str());
            OnPeer* p = on_agent(gen, nick, jid);
            if (!p) { on_ws_send(w, "{\"t\":\"reject\",\"jid\":\"" + jid + "\",\"why\":\"ice\"}"); continue; }
            juice_set_remote_description(p->a, on_remote_sdp(sdp).c_str());
            std::string mine = on_describe(p);
            on_ws_send(w, "{\"t\":\"answer\",\"jid\":\"" + jid + "\",\"sdp\":\"" + js_esc(mine) + "\"}");
        } else if (t == "gone") {
            std::string jid;
            js_get(m, "jid", jid);
            std::lock_guard<std::recursive_mutex> g(on_mtx);
            for (OnPeer* p : on_peers) {
                if (p->jid == jid && !p->logged) on_log("%s se fue antes de conectar", p->nick.c_str());
                if (p->jid == jid && p->wsr) { p->wsr = false; on_log("%s salio del relay del servicio", p->nick.c_str()); }
            }
        }
    }
    w->alive = false;
    if (gen == on_gen) on_log("conexion con el servicio cerrada, la sala ya no aparece en la lista");
}

static OnRoomInfo on_room_parse(const std::string& o) {
    OnRoomInfo r = {};
    std::string s;
    r.cust = -1;
    if (js_get(o, "id", s)) snprintf(r.id, sizeof(r.id), "%s", s.c_str());
    if (js_get(o, "name", s)) snprintf(r.name, sizeof(r.name), "%s", s.c_str());
    if (js_get(o, "nick", s)) snprintf(r.host, sizeof(r.host), "%s", s.c_str());
    if (js_get(o, "mods", s)) snprintf(r.mods, sizeof(r.mods), "%s", s.c_str());
    if (js_get(o, "n", s)) r.n = atoi(s.c_str());
    if (js_get(o, "max", s)) r.max = atoi(s.c_str());
    if (js_get(o, "mode", s)) r.mode = atoi(s.c_str());
    if (js_get(o, "cust", s)) r.cust = atoi(s.c_str());
    if (js_get(o, "rank", s)) r.rank = s == "true" || s == "1";
    return r;
}

// the room being joined, as /rooms/ID describes it (name, mode, custom), also for private rooms
static OnRoomInfo on_joined = {};
static bool on_joined_ok = false;
bool net_online_joined(OnRoomInfo* out) { std::lock_guard<std::mutex> g(on_sm); if (on_joined_ok && out) *out = on_joined; return on_joined_ok; }

static void on_join_thread(int gen, std::string id, std::string pw) {
    on_turn_load();
    on_log("uniendose a la sala %s", id.c_str());
    std::string info;
    int ist = on_get("/rooms/" + on_q(id) + "?v=" + std::string(ON_PROTO), info);
    if (ist == 404) { on_fail(gen, 404, "la sala no existe"); return; }
    if (ist == 200) { OnRoomInfo r = on_room_parse(info); std::lock_guard<std::mutex> g(on_sm); on_joined = r; on_joined_ok = r.id[0] != 0; on_rank_room = r.id; on_rank_n = 0; on_rank_now = r.rank != 0; on_rank_kind = r.mode == 1 ? 1 : 0; }
    if (on_rank_now) on_log("la sala %s es clasificatoria", id.c_str());
    OnPeer* p = on_agent(gen, "anfitrion", "");
    if (!p) { on_fail(gen, -2, "union cancelada"); return; }
    std::string mine = on_describe(p);
    int st = 0;
    std::string path = "/join/" + on_q(id) + "?v=" + std::string(ON_PROTO) + on_auth();
    if (!pw.empty()) path += "&pw=" + on_q(pw);
    std::shared_ptr<OnWs> w = on_ws_open(path, &st);
    if (!w) {
        const char* why = st == 404 ? "la sala no existe" : st == 403 ? "contrasena incorrecta" : st == 409 ? "la sala esta llena" : st == 426 ? "version distinta del port" : st == 401 ? "cuenta no valida" : "error del servicio";
        on_fail(gen, st, why);
        return;
    }
    on_ws_send(w, "{\"t\":\"offer\",\"sdp\":\"" + js_esc(mine) + "\"}");
    std::string m, t;
    bool answered = false;
    while (gen == on_gen && on_ws_recv(w, m)) {
        if (m == "pong" || !js_get(m, "t", t)) continue;
        if (t == "answer") {
            std::string sdp;
            js_get(m, "sdp", sdp);
            on_log("respuesta del anfitrion recibida");
            if (on_wsrelay < 2) juice_set_remote_description(p->a, on_remote_sdp(sdp).c_str());
            answered = true;
            break;
        }
        if (t == "reject") { std::string why; js_get(m, "why", why); on_log("union rechazada por el anfitrion: %s", why.c_str()); break; }
    }
    for (int i = 0; answered && on_wsrelay < 2 && gen == on_gen && i < 150 && !on_ice_up(p) && p->st != JUICE_STATE_FAILED; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (answered && on_wsrelay && gen == on_gen && !on_ice_up(p)) {
        on_log(on_wsrelay >= 2 ? "TTT_ONLINE_WSRELAY=2: se fuerza el relay del servicio" : "sin conexion directa ni TURN, se usa el relay del servicio");
        p->rw = w;
        on_ws_send(w, "{\"t\":\"relay\"}");
        on_relay_up(p, "invitado");
        bool bin = false;
        while (gen == on_gen && on_ws_recv(w, m, &bin)) {
            if (bin) on_rx_push(p, m.data(), m.size());
            else if (js_get(m, "t", t) && t == "reject") { on_log("el anfitrion cerro la sala"); break; }
        }
        p->wsr = false;
        if (gen == on_gen) on_log("relay del servicio cerrado");
        on_ws_close(w);
        return;
    }
    on_ws_close(w);
    if (gen == on_gen && !p->logged) on_fail(gen, -2, "sin conexion con el anfitrion");
}

static std::mutex on_am;
static bool on_init();
static std::atomic<int> on_kind{AK_NONE}, on_steam_stop{0};
static char on_pub_name[24], on_pub_acc[24];

static void on_publish() {
    std::lock_guard<std::mutex> g(on_am);
    snprintf(on_pub_name, sizeof(on_pub_name), "%s", on_acc_name.c_str());
    snprintf(on_pub_acc, sizeof(on_pub_acc), "%s", on_kind == AK_ACCOUNT && on_acc_ok ? on_acc_name.c_str() : "");
    if (!on_acc_name.empty()) on_nick = on_acc_name;
}

static std::string on_hex(const uint8_t* d, size_t n) { static const char* h = "0123456789abcdef"; std::string s; for (size_t i = 0; i < n; i++) { s += h[d[i] >> 4]; s += h[d[i] & 15]; } return s; }

static std::vector<uint8_t> on_unhex(const std::string& s) {
    std::vector<uint8_t> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2) v.push_back((uint8_t)strtoul(s.substr(i, 2).c_str(), nullptr, 16));
    return v;
}

static std::string on_rand_hex(int n) {
    std::vector<uint8_t> b(n);
    if (BCryptGenRandom(nullptr, b.data(), n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) { std::random_device rd; for (auto& x : b) x = (uint8_t)rd(); }
    return on_hex(b.data(), n);
}

typedef BOOL (WINAPI* OnCrypt)(DATA_BLOB*, LPCWSTR, DATA_BLOB*, PVOID, PVOID, DWORD, DATA_BLOB*);
static OnCrypt on_dpapi(bool prot) {
    static HMODULE m = LoadLibraryA("crypt32.dll");
    return m ? (OnCrypt)GetProcAddress(m, prot ? "CryptProtectData" : "CryptUnprotectData") : nullptr;
}

static std::string on_seal(const std::string& k) {
    OnCrypt f = on_dpapi(true);
    DATA_BLOB in = { (DWORD)k.size(), (BYTE*)k.data() }, out = {};
    if (k.empty() || !f || !f(&in, L"TTTRecomp", nullptr, nullptr, nullptr, 1, &out)) return k;
    std::string r = "d:" + on_hex(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return r;
}

static std::string on_unseal(const std::string& k) {
    if (k.rfind("d:", 0) != 0) return k;
    OnCrypt f = on_dpapi(false);
    std::vector<uint8_t> b = on_unhex(k.substr(2));
    DATA_BLOB in = { (DWORD)b.size(), b.data() }, out = {};
    if (!f || !f(&in, nullptr, nullptr, nullptr, nullptr, 1, &out)) return "";
    std::string r((char*)out.pbData, out.cbData);
    LocalFree(out.pbData);
    return r;
}

static void on_cfg_save() {
    FILE* f = fopen(on_cfg.c_str(), "w");
    if (!f) return;
    std::lock_guard<std::mutex> g(on_am);
    if (on_kind == AK_OFFLINE) fprintf(f, "kind=offline\nname=%s\n", on_acc_name.c_str());
    else if (on_kind == AK_ACCOUNT) fprintf(f, "kind=account\nname=%s\nkey=%s\nsteam=%s\n", on_acc_name.c_str(), on_seal(on_acc_key).c_str(), on_acc_id.c_str());
    fclose(f);
}

static void on_cfg_load() {
    static bool done = false;
    if (done) return;
    done = true;
    if (const char* e = getenv("TTT_ONLINE_CFG")) if (*e) on_cfg = e;
    char full[MAX_PATH] = "";
    GetFullPathNameA(on_cfg.c_str(), MAX_PATH, full, nullptr);
    if (const char* e = getenv("TTT_ACCOUNT_RESET")) if (*e == '1') { remove(on_cfg.c_str()); on_log("perfil: TTT_ACCOUNT_RESET=1, se olvida la cuenta (%s)", full); }
    on_log("perfil: archivo %s", full);
    std::string kind, name, key, steam;
    if (FILE* f = fopen(on_cfg.c_str(), "r")) {
        char line[1200];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = 0;
            if (!strncmp(line, "kind=", 5)) kind = line + 5;
            else if (!strncmp(line, "name=", 5)) name = line + 5;
            else if (!strncmp(line, "key=", 4)) key = line + 4;
            else if (!strncmp(line, "steam=", 6)) steam = line + 6;
        }
        fclose(f);
    }
    key = on_unseal(key);
    if (name.empty()) on_kind = AK_NONE;
    else if (kind == "offline") on_kind = AK_OFFLINE;
    else if (!key.empty() && steam.size() == 17) on_kind = AK_ACCOUNT;
    else on_kind = AK_NONE;
    if (on_kind != AK_NONE) on_acc_name = name;
    if (on_kind == AK_ACCOUNT) { on_acc_key = key; on_acc_id = steam; on_acc_ok = 1; on_acc_state = OA_OK; }
    on_publish();
    on_log("perfil: %s%s", on_kind == AK_NONE ? "ninguno (primer arranque)" : on_kind == AK_OFFLINE ? "sin conexion, " : "cuenta Steam, ", on_acc_name.c_str());
}

static std::string on_post_body(const std::string& path, const std::string& json, int* st) {
    std::string body;
    *st = 0;
    HINTERNET con = WinHttpConnect(on_ses, on_whost.c_str(), (INTERNET_PORT)on_port, 0);
    if (!con) return body;
    HINTERNET rq = WinHttpOpenRequest(con, L"POST", on_w(path).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, on_tls ? WINHTTP_FLAG_SECURE : 0);
    if (rq) {
        if (WinHttpSendRequest(rq, L"Content-Type: application/json\r\n", (DWORD)-1L, (LPVOID)json.data(), (DWORD)json.size(), (DWORD)json.size(), 0) && WinHttpReceiveResponse(rq, nullptr)) {
            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
            *st = (int)code;
            DWORD n = 0;
            while (WinHttpQueryDataAvailable(rq, &n) && n) {
                std::string b(n, 0);
                DWORD r = 0;
                if (!WinHttpReadData(rq, &b[0], n, &r) || !r) break;
                body.append(b.data(), r);
            }
        } else *st = -(int)GetLastError();
        WinHttpCloseHandle(rq);
    }
    WinHttpCloseHandle(con);
    return body;
}

static std::string on_jf(const char* k, const std::string& v) { return std::string("\"") + k + "\":\"" + js_esc(v) + "\""; }

static int on_code(int st) { return st == 200 ? OA_OK : st == 401 ? OA_EXPIRED : OA_ERROR; }

static void on_validate();
static void on_signed(const std::string& body) {
    std::string n, k, id;
    js_get(body, "name", n);
    js_get(body, "key", k);
    js_get(body, "id", id);
    { std::lock_guard<std::mutex> g(on_am); on_acc_name = n; on_acc_key = k; on_acc_id = id; }
    on_kind = AK_ACCOUNT;
    on_acc_ok = 1;
    on_publish();
    on_cfg_save();
    on_validate();
}

static const char* on_oa_text(int s) {
    static const char* t[] = { "ninguno", "ocupado", "ok", "sin servicio", "sesion caducada", "cancelado" };
    return s >= 0 && s <= OA_CANCEL ? t[s] : "?";
}

template <class F> static void on_acc_task(const char* what, F fn) {
    if (!on_init() || on_acc_state == OA_BUSY) { if (!on_ses) on_acc_state = OA_ERROR; return; }
    on_acc_state = OA_BUSY;
    std::string w = what;
    std::thread([fn, w] {
        int r = fn();
        on_acc_state = r;
        on_log("cuenta: %s -> %s", w.c_str(), on_oa_text(r));
    }).detach();
}

static void av_set_own(const std::string& h);
static void on_validate() {
    if (on_kind != AK_ACCOUNT) return;
    std::thread([] {
        std::string body, n;
        { std::lock_guard<std::mutex> g(on_am); n = on_acc_name; }
        int st = on_get("/account?v=" + std::string(ON_PROTO) + on_auth(), body);
        if (st == 200) {
            std::string nn, av;
            if (js_get(body, "av", av)) av_set_own(av);
            if (js_get(body, "name", nn) && !nn.empty() && nn != n) {
                { std::lock_guard<std::mutex> g(on_am); on_acc_name = nn; }
                on_publish();
                on_cfg_save();
                on_log("cuenta: nombre de Steam actualizado: %s -> %s", n.c_str(), nn.c_str());
            }
            on_log("cuenta: sesion valida (%s)", nn.empty() ? n.c_str() : nn.c_str());
        } else if (st == 401) {
            on_acc_ok = 0;
            on_acc_state = OA_EXPIRED;
            on_publish();
            on_log("cuenta: sesion caducada (%s), hay que iniciar sesion de nuevo", n.c_str());
        } else on_log("cuenta: sin servicio al validar la sesion (codigo %d), se mantiene", st);
    }).detach();
}

static bool on_init() {
    static int done = -1;
    static std::mutex m;
    std::lock_guard<std::mutex> g(m);
    if (done >= 0) return done == 1;
    done = 0;
    on_cfg_load();
    const char* url = getenv("TTT_ONLINE_URL");
    if (!on_parse_url(url && *url ? url : ON_URL)) { on_log("direccion del servicio no valida"); return false; }
    if (const char* e = getenv("TTT_ONLINE_RELAY")) on_relay_only = atoi(e);
    if (const char* e = getenv("TTT_ONLINE_WSRELAY")) on_wsrelay = atoi(e);
    on_ses = WinHttpOpen(L"TTTRecomp", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!on_ses) on_ses = WinHttpOpen(L"TTTRecomp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!on_ses) { on_log("WinHTTP no disponible"); return false; }
    WinHttpSetTimeouts(on_ses, 8000, 8000, 8000, 60000);
    juice_set_log_level(JUICE_LOG_LEVEL_WARN);
    juice_set_log_handler(on_jlog);
    on_log("servicio %s, cuenta %s", on_host_name.c_str(), on_kind == AK_ACCOUNT ? on_acc_name.c_str() : "(ninguna)");
    done = 1;
    on_validate();
    return true;
}

bool net_online_on() { return on_mode != 0; }

void net_online_set_mods(uint64_t sig) {
    char b[20];
    snprintf(b, sizeof(b), "%llx", (unsigned long long)sig);
    on_mods = b;
}

const char* net_online_mods() { return on_mods.c_str(); }
void net_online_set_rank_mods(uint64_t sig) {
    char b[20];
    snprintf(b, sizeof(b), "%llx", (unsigned long long)sig);
    on_mods_rank = b;
}
const char* net_online_rank_mods() { return on_mods_rank.c_str(); }

const char* net_online_account() { on_init(); return on_pub_acc; }

int net_online_account_state() { on_init(); return on_acc_state; }

int net_account_kind() { on_cfg_load(); return on_kind; }
const char* net_account_name() { on_cfg_load(); return on_pub_name; }
int net_account_state() { on_init(); return on_acc_state; }

void net_account_offline(const char* name) {
    on_cfg_load();
    { std::lock_guard<std::mutex> g(on_am); on_acc_name = name; on_acc_key.clear(); on_acc_id.clear(); }
    on_kind = AK_OFFLINE;
    on_acc_ok = 0;
    on_acc_state = OA_NONE;
    on_publish();
    on_cfg_save();
    on_log("perfil: sin conexion, %s", name);
}

static std::string on_authj() { std::lock_guard<std::mutex> g(on_am); return on_jf("id", on_acc_id) + "," + on_jf("key", on_acc_key); }

static void on_signout() {
    { std::lock_guard<std::mutex> g(on_am); on_acc_name.clear(); on_acc_key.clear(); on_acc_id.clear(); }
    on_kind = AK_NONE;
    on_acc_ok = 0;
    on_publish();
    remove(on_cfg.c_str());
}

void net_account_logout() {
    on_acc_task("cerrar sesion", [] { int st; if (on_kind == AK_ACCOUNT) on_post_body("/acct/logout", "{" + on_authj() + "}", &st); on_signout(); return (int)OA_OK; });
}

void net_account_delete() {
    on_acc_task("borrar cuenta", [] { int st = 200; if (on_kind == AK_ACCOUNT) on_post_body("/acct/delete", "{" + on_authj() + "}", &st); int r = on_code(st); if (r == OA_OK) on_signout(); return r; });
}

void net_account_steam_login() {
    on_acc_task("Steam", [] {
        std::string s = on_rand_hex(16);
        int st;
        std::string b = on_post_body("/steam/begin", "{" + on_jf("s", s) + "}", &st);
        if (st != 200) return on_code(st);
        std::string origin = (on_tls ? "https://" : "http://") + on_host_name + (on_port != (on_tls ? 443 : 80) ? ":" + std::to_string(on_port) : "");
        std::string url = origin + "/steam/start?s=" + s;
        DWORD pid = 0, n = sizeof(pid);
        bool client = RegGetValueA(HKEY_CURRENT_USER, "Software\\Valve\\Steam\\ActiveProcess", "pid", RRF_RT_REG_DWORD, nullptr, &pid, &n) == ERROR_SUCCESS && pid;
        if (client) {
            // Steam's own browser only keeps Valve pages: open the OpenID page directly, as /steam/start would redirect
            const std::string sel = "http://specs.openid.net/auth/2.0/identifier_select";
            url = "steam://openurl/https://steamcommunity.com/openid/login?openid.ns=" + on_q("http://specs.openid.net/auth/2.0") + "&openid.mode=checkid_setup&openid.return_to=" +
                  on_q(origin + "/steam/back?s=" + s) + "&openid.realm=" + on_q(origin) + "&openid.identity=" + on_q(sel) + "&openid.claimed_id=" + on_q(sel);
        }
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        on_log("cuenta: %s abierto para Steam, esperando", client ? "cliente de Steam" : "navegador");
        uint64_t t0 = on_ms();
        on_steam_stop = 0;
        while (on_ms() - t0 < 600000) {
            Sleep(2000);
            if (on_steam_stop) { on_log("cuenta: Steam cancelado en el juego"); return (int)OA_CANCEL; }
            b.clear();
            st = on_get("/steam/status?s=" + s, b);
            std::string v;
            js_get(b, "st", v);
            if (st == 200 && v == "wait") continue;
            if (v == "done") { on_signed(b); return (int)OA_OK; }
            return (int)OA_ERROR;
        }
        return (int)OA_ERROR;
    });
}

void net_account_steam_cancel() { on_steam_stop = 1; }

static void on_acc_wait() { for (int i = 0; i < 6000 && on_acc_state == OA_BUSY; i++) Sleep(100); }

#include "net/discord.h"

void net_account_boot() {
    on_cfg_load();
    net_discord_boot();
    if (on_kind == AK_ACCOUNT) on_init();
    const char* e = getenv("TTT_ACCOUNT_TEST");
    if (!e || !*e) return;
    std::string all = e;
    std::thread([all] {
        on_init();
        size_t p = 0;
        while (p <= all.size()) {
            size_t q = all.find(';', p);
            std::string step = all.substr(p, q == std::string::npos ? std::string::npos : q - p);
            p = q == std::string::npos ? all.size() + 1 : q + 1;
            size_t k = step.find(':');
            std::string c = step.substr(0, k), arg = k == std::string::npos ? "" : step.substr(k + 1);
            on_log("prueba de cuenta: %s", c.c_str());
            if (c == "offline") net_account_offline(arg.c_str());
            else if (c == "logout") net_account_logout();
            else if (c == "delete") net_account_delete();
            else if (c == "steam") net_account_steam_login();
            Sleep(50);
            on_acc_wait();
            on_log("prueba de cuenta: %s -> %s; perfil %s %s", c.c_str(), on_oa_text(on_acc_state), on_kind == AK_NONE ? "ninguno" : on_kind == AK_OFFLINE ? "sin conexion" : "cuenta Steam", on_pub_name);
        }
        on_log("prueba de cuenta: fin");
    }).detach();
}

void net_online_refresh() {
    if (!on_init() || on_list_busy.exchange(true)) return;
    on_list_state = 1;
    std::thread([] {
        std::string b;
        int st = on_get("/rooms?v=" + std::string(ON_PROTO), b);
        std::vector<OnRoomInfo> v;
        if (st == 200) {
            for (const std::string& o : js_objects(b)) {
                OnRoomInfo r = on_room_parse(o);
                if (r.id[0]) v.push_back(r);
            }
        }
        { std::lock_guard<std::mutex> g(on_sm); if (st == 200) on_list = v; }
        static int logs = 0;
        if (logs++ < 20) on_log("lista de salas publicas: %d (codigo %d)", (int)v.size(), st);
        on_list_state = st == 200 ? 2 : 3;
        on_list_busy = false;
    }).detach();
}

int net_online_rooms(OnRoomInfo* out, int max, int* state) {
    std::lock_guard<std::mutex> g(on_sm);
    if (state) *state = on_list_state;
    int n = 0;
    for (int i = (int)on_list.size() - 1; i >= 0 && n < max; i--) out[n++] = on_list[i];
    return n;
}

void net_online_stop() {
    on_gen++;
    if (on_mode || on_status != OS_IDLE) on_log("sesion en linea cerrada");
    on_mode = 0;
    on_status = OS_IDLE;
    on_nconn = 0;
    on_rank_now = false;
    // WinHttpWebSocketClose waits for the close handshake while the host thread sits in Receive: close off the game thread
    if (on_host_ws) { std::shared_ptr<OnWs> w = on_host_ws; on_host_ws.reset(); std::thread([w] { on_ws_close(w); }).detach(); }
    std::vector<OnPeer*> old;
    { std::lock_guard<std::recursive_mutex> g(on_mtx); old.swap(on_peers); }
    for (OnPeer* p : old) { juice_destroy(p->a); delete p; }
    std::lock_guard<std::mutex> g(on_rxm);
    on_rx.clear();
}

bool net_online_ranked() { return on_rank_now.load() && on_kind == AK_ACCOUNT; }
int net_online_rank_mode() { return net_online_ranked() ? on_rank_kind.load() : -1; }
// ranked result n of this room, reported by every human: free battle res 1 won / 2 lost with the reporter's team, survival waves
static int on_rank_post(const std::string& fields, const char* what) {
    if (!net_online_ranked()) return 0;
    std::string room;
    int n;
    { std::lock_guard<std::mutex> g(on_sm); room = on_rank_room; n = ++on_rank_n; }
    std::string body = "{" + on_authj() + "," + on_jf("room", room) + ",\"n\":" + std::to_string(n) + "," + fields + "}";
    std::string w = what;
    std::thread([body, n, w] {
        int st = 0;
        std::string r = on_post_body("/rank/report", body, &st);
        on_log("clasificatoria: combate %d, %s, respuesta %d %s", n, w.c_str(), st, r.c_str());
    }).detach();
    return n;
}
int net_online_rank_free(int res, int team, int np) {
    char f[96], w[64];
    snprintf(f, sizeof(f), "\"mode\":\"free\",\"res\":%d,\"team\":%d,\"np\":%d", res, team, np);
    snprintf(w, sizeof(w), "batalla libre %s, equipo %c, %d humanos", res == 1 ? "ganado" : "perdido", team ? 'B' : 'A', np);
    return on_rank_post(f, w);
}
int net_online_rank_surv(int waves, int np) {
    char f[96], w[64];
    snprintf(f, sizeof(f), "\"mode\":\"surv\",\"w\":%d,\"np\":%d", waves, np);
    snprintf(w, sizeof(w), "supervivencia, %d oleadas, %d humanos", waves, np);
    return on_rank_post(f, w);
}

// global ranking: GET /ranking -> {top:[{name,elo,w,l}], me:{pos,elo,w,l}}; me.pos 0 = no ranked games yet
static std::vector<OnRankRow> on_rk[2];
static OnRankRow on_rk_me[2];
static bool on_rk_has_me[2] = {};
static std::atomic<int> on_rk_state[2];
static std::atomic<bool> on_rk_busy[2];
// value of a top-level array/object member, brackets matched outside strings
static std::string on_js_member(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    int depth = 0;
    bool str = false;
    for (size_t i = 0; i < j.size(); i++) {
        char c = j[i];
        if (str) { if (c == '\\') i++; else if (c == '"') { str = false; } continue; }
        if (c == '{' || c == '[') depth++;
        else if (c == '}' || c == ']') depth--;
        else if (c == '"') {
            if (depth == 1 && !j.compare(i, k.size(), k)) {
                size_t a = j.find_first_of("[{", i + k.size());
                if (a == std::string::npos) return "";
                int d = 0; bool q = false;
                for (size_t e = a; e < j.size(); e++) {
                    char x = j[e];
                    if (q) { if (x == '\\') e++; else if (x == '"') q = false; continue; }
                    if (x == '"') q = true;
                    else if (x == '[' || x == '{') d++;
                    else if ((x == ']' || x == '}') && --d == 0) return j.substr(a, e - a + 1);
                }
                return "";
            }
            str = true;
        }
    }
    return "";
}
static OnRankRow on_rk_parse(const std::string& o) {
    OnRankRow r = {};
    std::string s;
    if (js_get(o, "name", s)) snprintf(r.name, sizeof(r.name), "%s", s.c_str());
    if (js_get(o, "pos", s)) r.pos = atoi(s.c_str());
    if (js_get(o, "elo", s)) r.elo = atoi(s.c_str());
    if (js_get(o, "w", s)) r.w = atoi(s.c_str());
    if (js_get(o, "l", s)) r.l = atoi(s.c_str());
    return r;
}
void net_online_ranking_refresh(int bd) {
    bd = bd ? 1 : 0;
    if (!on_init() || on_rk_busy[bd].exchange(true)) return;
    on_rk_state[bd] = 1;
    std::thread([bd] {
        std::string b;
        int st = on_get("/ranking?v=" + std::string(ON_PROTO) + "&board=" + (bd ? "surv" : "free") + on_auth(), b);
        std::vector<OnRankRow> v;
        OnRankRow me = {};
        bool has = false;
        if (st == 200) {
            std::string top = on_js_member(b, "top"), mine = on_js_member(b, "me");
            for (const std::string& o : js_objects(top)) { OnRankRow r = on_rk_parse(o); r.pos = (int)v.size() + 1; v.push_back(r); }
            if (!mine.empty()) { me = on_rk_parse(mine); snprintf(me.name, sizeof(me.name), "%s", on_pub_acc); has = true; }
        }
        { std::lock_guard<std::mutex> g(on_sm); if (st == 200) { on_rk[bd] = v; on_rk_me[bd] = me; on_rk_has_me[bd] = has; } }
        static int logs = 0;
        if (logs++ < 20) on_log("clasificacion %s: %d jugadores, mi posicion %d (%d %d %d), codigo %d", bd ? "supervivencia" : "batalla libre", (int)v.size(), me.pos, me.elo, me.w, me.l, st);
        on_rk_state[bd] = st == 200 ? 2 : 3;
        on_rk_busy[bd] = false;
    }).detach();
}
int net_online_ranking(int bd, OnRankRow* out, int max, int* state, OnRankRow* me) {
    bd = bd ? 1 : 0;
    std::lock_guard<std::mutex> g(on_sm);
    if (state) *state = on_rk_state[bd];
    if (me) { *me = on_rk_me[bd]; if (!on_rk_has_me[bd]) me->elo = -1; }
    int n = 0;
    for (; n < max && n < (int)on_rk[bd].size(); n++) out[n] = on_rk[bd][n];
    return n;
}

void net_online_host(const char* name, const char* pw, int maxp, int pub) {
    net_online_stop();
    if (!on_init()) { on_err = -1; on_status = OS_FAILED; return; }
    on_status = OS_BUSY;
    on_mode = 1;
    std::thread(on_host_thread, (int)on_gen, std::string(name ? name : ""), std::string(pw ? pw : ""), maxp, pub, std::string()).detach();
}

void net_online_host_mm(const char* mid, int np, int surv) {
    net_online_stop();
    if (!on_init()) { on_err = -1; on_status = OS_FAILED; return; }
    on_game_mode = surv ? 1 : 0; on_game_cust = 0;
    on_status = OS_BUSY;
    on_mode = 1;
    on_log("emparejamiento: crea la sala de la partida %s (%d jugadores, %s)", mid, np, surv ? "supervivencia" : "batalla libre");
    std::thread(on_host_thread, (int)on_gen, std::string("Ranked"), std::string(), np, 0, std::string(mid ? mid : "")).detach();
}

// matchmaking: one poll thread per search (generation), state copied under on_mmx
static std::mutex on_mmx;
static OnMatch on_mm = {};
static OnGroup on_grp = {};
static std::atomic<int> on_mm_gen{0};
static std::atomic<bool> on_mm_on{false};
static OnMatch on_mm_parse(const std::string& b, int code) {
    OnMatch m = {};
    std::string s;
    m.st = MMS_ERROR;
    if (code != 200 || !js_get(b, "st", s)) { snprintf(m.why, sizeof(m.why), "%d", code); return m; }
    m.st = s == "wait" ? MMS_WAIT : s == "match" ? MMS_MATCH : MMS_IDLE;
    auto num = [&](const char* k) { std::string v; return js_get(b, k, v) ? atoi(v.c_str()) : 0; };
    m.s = num("s"); m.win = num("win"); m.n = num("n"); m.grp = num("grp"); m.back = num("back");
    m.team = num("team"); m.ord = num("ord"); m.np = num("np"); m.map = num("map"); m.full = num("full"); m.rn = num("rn"); m.fmt = num("fmt");
    if (js_get(b, "mode", s)) m.mode = s == "surv";
    if (js_get(b, "role", s)) m.host = s == "host";
    if (js_get(b, "mid", s)) snprintf(m.mid, sizeof(m.mid), "%s", s.c_str());
    if (js_get(b, "room", s)) snprintf(m.room, sizeof(m.room), "%s", s.c_str());
    if (js_get(b, "why", s)) snprintf(m.why, sizeof(m.why), "%s", s.c_str());
    std::string nm = on_js_member(b, "names");
    int t = -1, k = 0;
    for (size_t i = 0; i < nm.size() && k < 4; i++) {
        if (nm[i] == '[' && i > 0) t++;
        else if (nm[i] == '"' && t >= 0 && t < 2) {
            size_t e = nm.find('"', i + 1);
            if (e == std::string::npos) break;
            snprintf(m.names[k++], 24, "%s", nm.substr(i + 1, e - i - 1).c_str());
            m.nteam[t]++;
            i = e;
        }
    }
    return m;
}

static void on_mm_set(const OnMatch& m, int gen) {
    std::lock_guard<std::mutex> g(on_mmx);
    if (gen != on_mm_gen) return;
    bool chg = m.st != on_mm.st || strcmp(m.room, on_mm.room) || m.full != on_mm.full || m.back != on_mm.back || m.rn != on_mm.rn;
    on_mm = m;
    if (!chg) return;
    if (m.st == MMS_WAIT) on_log("emparejamiento: buscando (%d s, ventana %d, %d en cola%s)", m.s, m.win, m.n, m.back ? ", de vuelta en la cola" : "");
    else if (m.st == MMS_MATCH) on_log("emparejamiento: partida %s, %s, equipo %c, orden %d de %d, sala %s, en sala %d%s", m.mid, m.host ? "anfitrion" : "invitado", m.team ? 'B' : 'A', m.ord, m.np, m.room[0] ? m.room : "-", m.rn, m.full ? ", completa" : "");
    else if (m.st == MMS_IDLE) on_log("emparejamiento: fuera de la cola%s%s", m.why[0] ? ", motivo " : "", m.why);
    else on_log("emparejamiento: error del servicio (%s)", m.why);
}

static void on_mm_thread(int gen, std::string first, std::string body, bool follow) {
    int st = 0;
    std::string b = on_post_body(first, body, &st);
    OnMatch m = on_mm_parse(b, st);
    on_mm_set(m, gen);
    int errs = m.st == MMS_ERROR ? 1 : 0;
    while (gen == on_mm_gen && on_mm_on) {
        { std::lock_guard<std::mutex> g(on_mmx); if ((on_mm.st == MMS_IDLE && !(follow && !on_mm.why[0])) || (on_mm.st == MMS_MATCH && on_mm.full)) break; }
        if (errs >= 5) break;
        for (int i = 0; i < 20 && gen == on_mm_gen && on_mm_on; i++) Sleep(100);
        if (gen != on_mm_gen || !on_mm_on) break;
        b = on_post_body("/mm/poll?v=" + std::string(ON_PROTO), "{" + on_authj() + "}", &st);
        m = on_mm_parse(b, st);
        errs = m.st == MMS_ERROR ? errs + 1 : 0;
        if (m.st != MMS_ERROR || errs >= 5) on_mm_set(m, gen);
    }
}

static void on_mm_start(const std::string& path, const std::string& body, bool follow = false) {
    if (on_kind != AK_ACCOUNT || !on_init()) { OnMatch m = {}; m.st = MMS_ERROR; snprintf(m.why, sizeof(m.why), "account"); std::lock_guard<std::mutex> g(on_mmx); on_mm = m; return; }
    int gen = ++on_mm_gen;
    on_mm_on = true;
    { std::lock_guard<std::mutex> g(on_mmx); on_mm = {}; on_mm.st = MMS_WAIT; }
    std::thread(on_mm_thread, gen, path, body, follow).detach();
}

void net_mm_queue(int surv, int fmt) {
    on_log("emparejamiento: busca %s %s", surv ? "supervivencia" : "batalla libre", surv ? "" : fmt == 2 ? "2vs2" : "1vs1");
    on_mm_start("/mm/queue?v=" + std::string(ON_PROTO), "{" + on_authj() + ",\"mode\":\"" + (surv ? "surv" : "free") + "\",\"fmt\":" + std::to_string(fmt) + "}");
}

void net_mm_follow() { on_mm_start("/mm/poll?v=" + std::string(ON_PROTO), "{" + on_authj() + "}", true); }

void net_mm_cancel() {
    bool was = on_mm_on.exchange(false);
    ++on_mm_gen;
    { std::lock_guard<std::mutex> g(on_mmx); on_mm = {}; }
    if (!was || on_kind != AK_ACCOUNT) return;
    on_log("emparejamiento: busqueda cancelada");
    std::string body = "{" + on_authj() + "}";
    std::thread([body] { int st; on_post_body("/mm/cancel?v=" + std::string(ON_PROTO), body, &st); }).detach();
}

int net_mm_state(OnMatch* out) {
    std::lock_guard<std::mutex> g(on_mmx);
    if (out) *out = on_mm;
    return on_mm.st;
}

// op 0 refresh, 1 new, 2 join code, 3 leave
void net_mm_group(int op, const char* code) {
    if (on_kind != AK_ACCOUNT || !on_init()) return;
    static const char* P[4] = {"/mm/group", "/mm/group/new", "/mm/group/join", "/mm/group/leave"};
    std::string body = "{" + on_authj() + (op == 2 ? "," + on_jf("code", code ? code : "") : std::string()) + "}", path = std::string(P[op & 3]) + "?v=" + ON_PROTO;
    { std::lock_guard<std::mutex> g(on_mmx); if (op) on_grp.st = MG_BUSY; }
    std::thread([body, path, op] {
        int st = 0;
        std::string b = on_post_body(path, body, &st), s;
        OnGroup r = {};
        r.st = st != 200 ? MG_ERROR : js_get(b, "st", s) && s == "ok" ? MG_OK : MG_NONE;
        if (js_get(b, "code", s)) snprintf(r.code, sizeof(r.code), "%s", s.c_str());
        if (js_get(b, "lead", s)) r.lead = atoi(s.c_str());
        std::string mem = on_js_member(b, "mem");
        for (size_t i = 0; i < mem.size() && r.n < 2; i++) if (mem[i] == '"') { size_t e = mem.find('"', i + 1); if (e == std::string::npos) break; snprintf(r.names[r.n++], 24, "%s", mem.substr(i + 1, e - i - 1).c_str()); i = e; }
        if (op || r.st == MG_ERROR) on_log("emparejamiento: grupo %s, respuesta %d%s%s (%d miembros%s)", op == 1 ? "nuevo" : op == 2 ? "union" : op == 3 ? "salida" : "estado", st, r.code[0] ? ", codigo " : "", r.code, r.n, r.lead ? ", lider" : "");
        std::lock_guard<std::mutex> g(on_mmx);
        on_grp = r;
    }).detach();
}

int net_mm_group_state(OnGroup* out) {
    std::lock_guard<std::mutex> g(on_mmx);
    if (out) *out = on_grp;
    return on_grp.st;
}

// players in the game room (room info +0x1D1), shown as "En sala" in the rooms list
void net_online_set_players(int n) {
    if (on_mode != 1 || !on_host_ws || n < 1) return;
    on_ws_send(on_host_ws, "{\"t\":\"n\",\"n\":" + std::to_string(n) + "}");
    on_log("jugadores en la sala: %d", n);
}

void net_online_join(const char* id, const char* pw) {
    net_online_stop();
    { std::lock_guard<std::mutex> g(on_sm); on_joined_ok = false; }
    if (!on_init()) { on_err = -1; on_status = OS_FAILED; return; }
    on_status = OS_BUSY;
    on_mode = 2;
    std::thread(on_join_thread, (int)on_gen, std::string(id ? id : ""), std::string(pw ? pw : "")).detach();
}

int net_online_status(int* err, char* id) {
    if (err) *err = on_err;
    if (id) { std::lock_guard<std::mutex> g(on_sm); snprintf(id, 8, "%s", on_room_id.c_str()); }
    return on_status;
}

// test switches: TTT_ONLINE_HOST=1 creates a public room at startup, TTT_ONLINE_JOIN=ID|auto joins one
void net_online_start() {
    static bool done = false;
    if (done) return;
    done = true;
    const char* host = getenv("TTT_ONLINE_HOST");
    const char* join = getenv("TTT_ONLINE_JOIN");
    if (!(host && atoi(host)) && !(join && *join)) return;
    if (!on_init() || on_acc_state != OA_OK) { on_log("las pruebas por variables necesitan una cuenta, crea una en En linea"); return; }
    std::string pw = getenv("TTT_ONLINE_PASS") ? getenv("TTT_ONLINE_PASS") : "";
    if (host && atoi(host)) { const char* pv = getenv("TTT_ONLINE_PRIVATE"); net_online_host(on_acc_name.c_str(), pw.c_str(), 4, !(pv && atoi(pv))); return; }
    std::string id = join;
    std::thread([id, pw] {
        std::string r = id;
        for (int i = 0; r == "auto" && i < 15; i++) {
            net_online_refresh();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            OnRoomInfo v[1];
            if (net_online_rooms(v, 1, nullptr)) r = v[0].id;
        }
        if (r != "auto") net_online_join(r.c_str(), pw.c_str());
    }).detach();
}

static bool on_peer_send(OnPeer* p, const uint8_t* d, int n) {
    if (on_ice_up(p)) { juice_send(p->a, (const char*)d, n); return true; }
    if (!p->wsr) return false;
    if (n > 4 && d[4] == 17) return true;   // P_PDPU: the relay over WebSocket loses nothing, the copy would only cost requests
    if (n > 4 && d[4] == 18) {   // P_STREAM: a repeat adds nothing on a lossless relay
        if (p->last_stream.size() == (size_t)n && !memcmp(p->last_stream.data(), d, n)) return true;
        p->last_stream.assign(d, d + n);
    }
    if (p->rw) return on_ws_send_bin(p->rw, d, n);
    std::shared_ptr<OnWs> w = on_host_ws;
    if (!w || p->jid.size() != 8) return false;
    std::vector<uint8_t> f(4 + n);
    for (int i = 0; i < 4; i++) f[i] = (uint8_t)strtoul(p->jid.substr(i * 2, 2).c_str(), nullptr, 16);
    memcpy(f.data() + 4, d, n);
    return on_ws_send_bin(w, f.data(), f.size());
}

void net_online_send(const void* d, int n) {
    const uint8_t* b = (const uint8_t*)d;
    uint64_t dst = n >= 20 ? on_mac(b + 14) : 0;
    std::lock_guard<std::recursive_mutex> g(on_mtx);
    bool sent = false;
    for (OnPeer* p : on_peers)
        if (p->mac && p->mac == dst && on_peer_send(p, b, n)) sent = true;
    if (!sent)
        for (OnPeer* p : on_peers) on_peer_send(p, b, n);
}

int net_online_recv(void* d, int max) {
    std::lock_guard<std::mutex> g(on_rxm);
    if (on_rx.empty()) return -1;
    std::vector<uint8_t>& v = on_rx.front();
    int n = (int)v.size() < max ? (int)v.size() : max;
    memcpy(d, v.data(), n);
    on_rx.pop_front();
    return n;
}

// Steam avatars (medium, 64x64 JPEG on Steam's CDN): own hash from /account, room members' from /rooms/ID (name -> hash)
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#include "third_party/stb_image.h"
#include <map>

struct AvImg { int st = 0, id = 0; std::vector<uint8_t> rgba; };
static std::mutex av_m;
static std::map<std::string, AvImg> av_img;
static std::map<std::string, std::string> av_nick;
static std::string av_own;
static std::atomic<int> av_next{1};
static std::atomic<bool> av_room_busy{false};
static uint64_t av_room_ms = 0;
static int av_room_n = -1;

static bool av_hash_ok(const std::string& h) {
    if (h.size() != 40) return false;
    for (char c : h) if (!isxdigit((unsigned char)c)) return false;
    return true;
}

static int av_https_get(const wchar_t* host, const std::string& path, std::string& body) {
    body.clear();
    HINTERNET con = WinHttpConnect(on_ses, host, 443, 0);
    if (!con) return -1;
    HINTERNET rq = WinHttpOpenRequest(con, L"GET", on_w(path).c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    int st = 0;
    if (rq && WinHttpSendRequest(rq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(rq, nullptr)) {
        DWORD code = 0, sz = sizeof(code);
        WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
        st = (int)code;
        DWORD n = 0;
        while (st == 200 && WinHttpQueryDataAvailable(rq, &n) && n && body.size() < (1u << 20)) {
            std::string b(n, 0);
            DWORD r = 0;
            if (!WinHttpReadData(rq, &b[0], n, &r) || !r) break;
            body.append(b.data(), r);
        }
    } else st = -(int)GetLastError();
    if (rq) WinHttpCloseHandle(rq);
    WinHttpCloseHandle(con);
    return st;
}

// any size to 64x64 RGBA (box average when larger, nearest when smaller)
static std::vector<uint8_t> av_fit(const uint8_t* p, int w, int h) {
    std::vector<uint8_t> o(64 * 64 * 4);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        int x0 = x * w / 64, x1 = std::max(x0 + 1, (x + 1) * w / 64), y0 = y * h / 64, y1 = std::max(y0 + 1, (y + 1) * h / 64);
        unsigned s[4] = {0, 0, 0, 0}, n = 0;
        for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++, n++) for (int k = 0; k < 4; k++) s[k] += p[(yy * w + xx) * 4 + k];
        for (int k = 0; k < 4; k++) o[(y * 64 + x) * 4 + k] = (uint8_t)(s[k] / n);
    }
    return o;
}

static void av_fetch(const std::string& h) {
    if (!av_hash_ok(h)) return;
    {
        std::lock_guard<std::mutex> g(av_m);
        AvImg& a = av_img[h];
        if (a.st == 1 || a.st == 2) return;
        a.st = 1;
    }
    std::thread([h] {
        std::string body;
        int st = av_https_get(L"avatars.steamstatic.com", "/" + h + "_medium.jpg", body), w = 0, hh = 0, n = 0;
        uint8_t* px = st == 200 ? stbi_load_from_memory((const uint8_t*)body.data(), (int)body.size(), &w, &hh, &n, 4) : nullptr;
        std::lock_guard<std::mutex> g(av_m);
        AvImg& a = av_img[h];
        if (px && w > 0 && hh > 0) { a.rgba = av_fit(px, w, hh); a.id = av_next++; a.st = 2; }
        else a.st = 3;
        if (px) stbi_image_free(px);
        on_log("avatar: %s %s (%dx%d, codigo %d)", h.substr(0, 8).c_str(), a.st == 2 ? "descargado" : "fallo", w, hh, st);
    }).detach();
}

static void av_set_own(const std::string& h) {
    { std::lock_guard<std::mutex> g(av_m); if (av_own == h) return; av_own = h; }
    on_log("avatar: propio %s", h.empty() ? "(sin avatar)" : h.substr(0, 8).c_str());
    av_fetch(h);
}

// "av":{"name":"hash",...} of /rooms/ID
static void av_parse_room(const std::string& j) {
    size_t p = j.find("\"av\"");
    if (p == std::string::npos || (p = j.find('{', p)) == std::string::npos) return;
    std::vector<std::string> s;
    std::string cur;
    bool str = false;
    for (size_t i = p + 1; i < j.size(); i++) {
        char c = j[i];
        if (!str) { if (c == '}') break; if (c == '"') { str = true; cur.clear(); } continue; }
        if (c == '\\' && i + 1 < j.size()) {
            char e = j[++i];
            if (e == 'u' && i + 4 < j.size()) { cur += (char)strtol(j.substr(i + 1, 4).c_str(), nullptr, 16); i += 4; }
            else cur += e;
        } else if (c == '"') { str = false; s.push_back(cur); }
        else cur += c;
    }
    std::vector<std::string> fetch;
    {
        std::lock_guard<std::mutex> g(av_m);
        for (size_t i = 0; i + 1 < s.size(); i += 2) if (av_hash_ok(s[i + 1])) { av_nick[s[i]] = s[i + 1]; fetch.push_back(s[i + 1]); }
    }
    for (auto& h : fetch) av_fetch(h);
}

void net_avatar_room(int members) {
    if (!net_online_on() || !on_ses) return;
    uint64_t now = on_ms();
    if (members == av_room_n && now - av_room_ms < 15000) return;
    if (av_room_busy.exchange(true)) return;
    av_room_n = members; av_room_ms = now;
    std::string id;
    { std::lock_guard<std::mutex> g(on_sm); id = on_mode == 1 ? on_room_id : on_joined.id; }
    if (id.empty()) { av_room_busy = false; return; }
    std::thread([id] {
        std::string body;
        int st = on_get("/rooms/" + id, body);
        if (st == 200) av_parse_room(body);
        on_log("avatar: sala %s, codigo %d", id.c_str(), st);
        av_room_busy = false;
    }).detach();
}

int net_avatar(const char* name, uint8_t* rgba) {
    std::lock_guard<std::mutex> g(av_m);
    std::string h = av_own;
    if (name && *name) { auto it = av_nick.find(name); h = it == av_nick.end() ? "" : it->second; }
    if (!name || !*name) { if (on_kind != AK_ACCOUNT) return 0; }
    auto it = av_img.find(h);
    if (h.empty() || it == av_img.end() || it->second.st != 2) return 0;
    if (rgba) memcpy(rgba, it->second.rgba.data(), 64 * 64 * 4);
    return it->second.id;
}

#else
#include "net/online.h"
bool net_online_on() { return false; }
void net_online_start() {}
void net_online_send(const void*, int) {}
int net_online_recv(void*, int) { return -1; }
void net_online_set_mods(uint64_t) {}
const char* net_online_mods() { return "0"; }
void net_online_set_rank_mods(uint64_t) {}
const char* net_online_rank_mods() { return "0"; }
const char* net_online_account() { return ""; }
int net_account_kind() { return AK_OFFLINE; }
void net_account_boot() {}
const char* net_account_name() { return ""; }
int net_account_state() { return OA_ERROR; }
void net_account_offline(const char*) {}
void net_account_logout() {}
void net_account_delete() {}
void net_account_steam_login() {}
void net_account_steam_cancel() {}
int net_online_account_state() { return OA_ERROR; }
void net_online_refresh() {}
int net_online_rooms(OnRoomInfo*, int, int* state) { if (state) *state = 3; return 0; }
void net_online_host(const char*, const char*, int, int) {}
void net_online_set_game_mode(int, int) {}
bool net_online_joined(OnRoomInfo*) { return false; }
void net_online_join(const char*, const char*) {}
int net_online_status(int* err, char*) { if (err) *err = -1; return OS_FAILED; }
void net_online_stop() {}
void net_online_host_mm(const char*, int, int) {}
void net_mm_queue(int, int) {}
void net_mm_follow() {}
void net_mm_cancel() {}
int net_mm_state(OnMatch* out) { if (out) *out = {}; return MMS_IDLE; }
void net_mm_group(int, const char*) {}
int net_mm_group_state(OnGroup* out) { if (out) *out = {}; return MG_NONE; }
bool net_online_ranked() { return false; }
int net_online_rank_mode() { return -1; }
int net_online_rank_free(int, int, int) { return 0; }
int net_online_rank_surv(int, int) { return 0; }
void net_online_ranking_refresh(int) {}
int net_online_ranking(int, OnRankRow*, int, int* state, OnRankRow* me) { if (state) *state = 3; if (me) me->elo = -1; return 0; }
void net_discord_enable(bool) {}
int net_discord_state() { return 0; }
void net_discord_presence(const char*, const char*, const char*, int, int) {}
void net_avatar_room(int) {}
int net_avatar(const char*, uint8_t*) { return 0; }
#endif
