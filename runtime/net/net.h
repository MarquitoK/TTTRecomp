#pragma once
// PSP network (sceNet, Adhoc, Adhocctl, Matching, Wlan) over UDP between instances or on a LAN (net/udp.cpp)
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <map>
#include <set>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include "net/online.h"
#include <vector>

bool net_udp_open();
void net_udp_send(const void* d, int n);
int net_udp_recv(void* d, int max);

#define NET_WOULD_BLOCK 0x80410709u
#define NET_TIMEOUT 0x80410715u

enum { P_PRESENCE = 1, P_HELLO, P_JOIN, P_ACCEPT, P_CANCEL, P_BYE, P_DATA, P_DATAACK, P_PDP, P_ACK, P_MODS, P_INTRO, P_STATE, P_DELAY, P_PING, P_PONG, P_PDPU, P_STREAM, P_QUIT };
enum { M_HELLO = 1, M_REQUEST, M_LEAVE, M_DENY, M_CANCEL, M_ACCEPT, M_ESTABLISHED, M_TIMEOUT, M_ERROR, M_BYE, M_DATA, M_DATA_ACK };

static int net_log = -1;
static uint32_t net_handler = 0, net_handler_arg = 0;
static int net_state = 0;   // 0 disconnected, 1 connected
static char net_group[8] = {};
static uint8_t net_mac[6];
static char net_nick[32] = "";
static bool net_ready = false;

struct NetEvent { bool ctl; int type; uint8_t mac[6]; std::vector<uint8_t> opt; };
static std::deque<NetEvent> net_queue;

struct NetPeer { char nick[32]; char group[8]; uint64_t seen; };
static std::map<uint64_t, NetPeer> net_peers;

struct MtPeer { int state = 0; std::vector<uint8_t> hello; uint64_t seen = 0; };   // 0 seen, 1 pending, 2 established
static struct { bool alive = false, active = false; int mode = 0, port = 0, maxp = 0; uint32_t cb = 0; uint64_t hello_us = 1000000, last = 0; std::vector<uint8_t> opt; std::map<uint64_t, MtPeer> peers; } mt;

struct PdpPacket { uint8_t mac[6]; uint16_t port; std::vector<uint8_t> d; };
struct Pdp { uint16_t port; std::deque<PdpPacket> queue; };
static std::map<int, Pdp> pdp;
static int pdp_next = 1;
static uint64_t net_last_presence = 0;

// own reliable layer: one channel per peer with sequence, cumulative ack, retransmission and reordering
struct NetPending { uint32_t seq; std::vector<uint8_t> p; uint64_t t; int retries; };
struct NetChannel {
    uint32_t tx_seq = 1, rx_seq = 1;
    std::deque<NetPending> pend;
    std::map<uint32_t, std::vector<uint8_t>> stored;
    bool ack_due = false;
    double rtt = 0, rtt_var = 0;
    uint64_t rto = 20, last_report = 0;
    uint32_t last_ack = 0;
    int ack_dups = 0;
    long resends = 0, duplicates = 0, out_of_order = 0;
    uint64_t last_rtt = 0, last_tx = 0, last_ping = 0;
};
static std::map<uint64_t, NetChannel> net_channels;
static std::recursive_mutex net_mtx;
static std::thread net_thread;
static std::atomic<bool> net_thread_alive{false};
static void net_loop();

static int net_env(const char* a, const char* b, int def) { const char* e = getenv(a); if (!e) e = getenv(b); return e && *e ? atoi(e) : def; }
static int net_reliable() { static int v = -1; if (v < 0) v = net_env("TTT_NET_RELIABLE", "TTT_RED_FIABLE", 1); return v; }
static int net_threaded() { static int v = -1; if (v < 0) v = net_env("TTT_NET_THREAD", "TTT_RED_HILO", 1); return v; }
static long net_copies = 0;
static std::atomic<bool> net_stream{false};
void psp_net_stream_rx(const uint8_t* d, int n);
void psp_net_set_stream(bool on) { net_stream = on; }
static int net_input_copy() { static int v = -1; if (v < 0) v = net_env("TTT_NET_INPUT_COPY", "TTT_NET_INPUT_COPY", 1); return v; }
static uint64_t net_rto_base() { static int v = -1; if (v < 0) v = net_env("TTT_NET_RTO", "TTT_RED_RTO", 12); return (uint64_t)v; }

// mods rule: every packet carries the sender's mods signature (0 = no mods); a room with other mods cannot be joined
void psp_net_notice(const char* full, const char* missing, const char* extra, const char* version, int kind);
void psp_net_state_rx(const uint8_t* d, int n);
void psp_net_quit_rx(const uint8_t* d, int n);
static std::atomic<uint32_t> net_intro_id{0}, net_intro_cnt{0};
static std::atomic<uint64_t> net_delay_pack{0};
static uint64_t net_mods_sig = 0;
static std::string net_mods_txt;
static std::map<uint64_t, uint64_t> net_peer_sig;
static std::map<uint64_t, std::string> net_peer_mods;

static uint64_t net_rank_sig = 0;
static uint64_t net_sig() { return net_online_ranked() ? net_rank_sig : net_mods_sig; }   // ranked rooms compare only gameplay mods
// rooms and LAN: exactly the host's mods; ranked: nobody with character or map mods (character textures allowed)
static bool net_sig_ok(uint64_t peer) { return net_online_ranked() ? peer == 0 && net_rank_sig == 0 : peer == net_mods_sig; }
static void net_mods_init() {
    static bool done = false;
    if (done) return;
    done = true;
    if (mod_active.empty()) return;
    auto t0 = std::chrono::steady_clock::now();
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void* d, size_t n) { const uint8_t* q = (const uint8_t*)d; for (size_t i = 0; i < n; i++) h = (h ^ q[i]) * 1099511628211ull; };
    for (auto& m : mod_active) net_mods_txt += m.first + "\t" + m.second + "\n";
    mix(net_mods_txt.data(), net_mods_txt.size());
    for (auto& f : mod_files) mix(f.first.data(), f.first.size() + 1);   // names only: reading every file froze the multiplayer menu
    net_mods_sig = h ? h : 1;
    // ranked: any file of Maps/ and any non-texture file of a character colour counts; damaged textures (level 1) are a break, not cosmetic
    uint64_t g = 1469598103934665603ull;
    unsigned gn = 0;
    std::string first;
    for (auto& f : mod_files) {
        std::string k = f.first;
        if (!k.compare(0, 9, "packfile/")) k = k.substr(9);
        bool map = !k.compare(0, 5, "maps/");
        if (!map && (k.compare(0, 11, "characters/") || k.find("/color_") == std::string::npos)) continue;   // characters: only the colour folders (voices, sfx allowed)
        size_t d = k.find_last_of('.');
        std::string ext = d == std::string::npos ? "" : k.substr(d + 1);
        bool tex = ext == "png" || ext == "atex" || ext == "ctex" || ext == "dbt" || ext == "rhg" || ext == "dds" || ext == "tga" || ext == "bmp";
        if (!map && tex && k.find("_dmg") == std::string::npos) continue;
        if (first.empty()) first = k;
        for (char ch : k) g = (g ^ (uint8_t)ch) * 1099511628211ull;
        gn++;
    }
    net_rank_sig = gn ? (g ? g : 1) : 0;
    net_online_set_rank_mods(net_rank_sig);
    printf("net: %u archivos de mods cambian personajes o mapas (clasificatoria %s%s%s)\n", gn, gn ? "bloqueada, primero " : "permitida", first.c_str(), "");
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("net: %d mods activos, firma %016llX (%u archivos, %.0f ms)\n", (int)mod_active.size(), (unsigned long long)net_mods_sig, (unsigned)mod_files.size(), ms);
    net_online_set_mods(net_mods_sig);
}
void psp_net_mods_ensure() { net_mods_init(); }

static std::vector<std::pair<std::string, std::string>> net_mods_parse(const std::string& t) {
    std::vector<std::pair<std::string, std::string>> v;
    size_t i = 0;
    while (i < t.size()) {
        size_t e = t.find('\n', i); if (e == std::string::npos) e = t.size();
        std::string l = t.substr(i, e - i); i = e + 1;
        size_t k = l.find('\t');
        if (!l.empty()) v.push_back({l.substr(0, k), k == std::string::npos ? "" : l.substr(k + 1)});
    }
    return v;
}

static void net_mods_refused(uint64_t host) {
    if (net_online_ranked()) {
        printf("net: union rechazada en clasificatoria (propia %016llX)\n", (unsigned long long)net_rank_sig);
        psp_net_notice("You can't play ranked with these mods.\n\nRanked only allows mods that change character textures. Mods that change or add characters, damaged costumes or maps are not allowed.\n\nTurn them off in Tools > Mods and restart the game.", "", "", "", 3);
        return;
    }
    uint64_t hs = net_peer_sig.count(host) ? net_peer_sig[host] : 0;
    auto mine = net_mods_parse(net_mods_txt);
    auto his = net_mods_parse(hs && net_peer_mods.count(host) ? net_peer_mods[host] : std::string());
    std::string missing, extra, version;
    auto add = [](std::string& s, const std::string& x) { s += (s.empty() ? "" : ", ") + x; };
    for (auto& a : his) {
        bool found = false;
        for (auto& b : mine) if (a.first == b.first) { found = true; if (a.second != b.second) add(version, a.first); }
        if (!found) add(missing, a.first);
    }
    for (auto& b : mine) { bool found = false; for (auto& a : his) if (a.first == b.first) found = true; if (!found) add(extra, b.first); }
    std::string m = "You can't join this room: your mods don't match the host's.\n";
    if (hs && !net_peer_mods.count(host)) m += "\nThe host uses mods.";
    if (!missing.empty()) m += "\nMissing: " + missing;
    if (!extra.empty()) m += "\nNot used by the host: " + extra;
    if (!version.empty()) m += "\nDifferent version: " + version;
    if (missing.empty() && extra.empty() && version.empty() && (!hs || net_peer_mods.count(host))) m += "\nThe mod order or files are different.";
    m += "\n\nUse exactly the host's mods, in the same order (Tools > Mods), and restart the game.";
    printf("net: union rechazada por mods (propia %016llX, anfitrion %016llX)\n", (unsigned long long)net_mods_sig, (unsigned long long)hs);
    int kind = hs && !net_peer_mods.count(host) ? 1 : missing.empty() && extra.empty() && version.empty() ? 2 : 0;
    psp_net_notice(m.c_str(), missing.c_str(), extra.c_str(), version.c_str(), kind);
}
static uint32_t net_get32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void net_put32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static uint64_t net_ms() { return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static uint64_t mac64(const uint8_t* m) { uint64_t v = 0; for (int i = 0; i < 6; i++) v = v << 8 | m[i]; return v; }
static void mac_from64(uint64_t v, uint8_t* m) { for (int i = 5; i >= 0; i--) { m[i] = (uint8_t)v; v >>= 8; } }
static void mac_read(uint32_t a, uint8_t* m) { for (int i = 0; i < 6; i++) m[i] = a ? psp_l8(a + i) : 0xFF; }
static void mac_write(uint32_t a, const uint8_t* m) { if (a) for (int i = 0; i < 6; i++) psp_s8(a + i, m[i]); }
static std::vector<uint8_t> net_bytes(uint32_t a, int n) { std::vector<uint8_t> v; if (a && n > 0) for (int i = 0; i < n; i++) v.push_back(psp_l8(a + i)); return v; }

static void net_init() {
    if (net_ready) return;
    net_ready = true;
    std::random_device rd;
    uint32_t r = rd() ^ (uint32_t)net_ms();
    net_mac[0] = 0x02; net_mac[1] = 0x54; net_mac[2] = 0x54; net_mac[3] = (uint8_t)(r >> 16); net_mac[4] = (uint8_t)(r >> 8); net_mac[5] = (uint8_t)r;
    const char* e = getenv("TTT_NICK");
    const char* acc = net_online_account();
    snprintf(net_nick, sizeof(net_nick), "%s", e && *e ? e : acc && *acc ? acc : "Jugador");
    if ((!e || !*e) && !(acc && *acc)) snprintf(net_nick + strlen(net_nick), sizeof(net_nick) - strlen(net_nick), " %02X", net_mac[5]);
    printf("net: iniciando (UDP)\n"); fflush(stdout);
    net_udp_open();
    printf("net: iniciando (firma de mods)\n"); fflush(stdout);
    net_mods_init();
    if (net_log < 0) { const char* e = getenv("TTT_NET_LOG"); net_log = e ? atoi(e) : 1; }
    if (net_threaded()) { net_thread_alive = true; net_thread = std::thread(net_loop); }
    printf("net: MAC %02X:%02X:%02X:%02X:%02X:%02X, nombre \"%s\"\n", net_mac[0], net_mac[1], net_mac[2], net_mac[3], net_mac[4], net_mac[5], net_nick);
}

// 48-byte header: TTN3, type, flags, source port, source mac, destination mac, destination port, length, group, sequence, ack, mods signature
// flags: bit 0 carries a sequence, bit 1 carries an ack
static void net_build(std::vector<uint8_t>& p, int type, const uint8_t* dst, int port, int dst_port, const uint8_t* d, int n, uint8_t flags, uint32_t seq, uint32_t ack) {
    p.assign(48 + n, 0);
    memcpy(p.data(), "TTN3", 4);
    p[4] = (uint8_t)type;
    p[5] = flags;
    p[6] = (uint8_t)port; p[7] = (uint8_t)(port >> 8);
    memcpy(&p[8], net_mac, 6);
    if (dst) memcpy(&p[14], dst, 6); else memset(&p[14], 0xFF, 6);
    p[20] = (uint8_t)dst_port; p[21] = (uint8_t)(dst_port >> 8);
    p[22] = (uint8_t)n; p[23] = (uint8_t)(n >> 8);
    memcpy(&p[24], net_group, 8);
    net_put32(&p[32], seq);
    net_put32(&p[36], ack);
    for (int i = 0; i < 8; i++) p[40 + i] = (uint8_t)(net_sig() >> (8 * i));
    if (n) memcpy(&p[48], d, n);
}

// round trip sample: acks of packets sent once, and pings (with a long round trip every packet was resent before its ack,
// so acks alone never gave a sample and the timeout stayed at 20 ms)
static void net_rtt_sample(NetChannel& c, double m) {
    c.rtt_var = c.rtt > 0 ? c.rtt_var * 0.75 + fabs(m - c.rtt) * 0.25 : m / 2;
    c.rtt = c.rtt > 0 ? c.rtt * 0.875 + m * 0.125 : m;
    uint64_t r = (uint64_t)(c.rtt + 4 * c.rtt_var) + net_rto_base();
    c.rto = r > 300 ? 300 : r;
    c.last_rtt = net_ms();
}

static bool net_type_reliable(int type) {
    return net_reliable() && (type == P_JOIN || type == P_ACCEPT || type == P_CANCEL || type == P_BYE || type == P_DATA || type == P_PDP);
}

static void net_channel_send(uint64_t k, int type, int port, int dst_port, const uint8_t* d, int n) {
    NetChannel& c = net_channels[k];
    uint8_t m[6]; mac_from64(k, m);
    std::vector<uint8_t> p;
    net_build(p, type, m, port, dst_port, d, n, 3, c.tx_seq, c.rx_seq - 1);
    c.pend.push_back({c.tx_seq, p, net_ms(), 0});
    c.last_tx = net_ms();
    c.tx_seq++;
    c.ack_due = false;
    net_udp_send(p.data(), (int)p.size());
}

static std::vector<uint64_t> net_targets() {
    std::vector<uint64_t> v;
    uint64_t t = net_ms();
    for (auto& x : net_peers) if (t - x.second.seen < 5000) v.push_back(x.first);
    return v;
}

// to broadcast: one reliable send per known peer; with none, an unacknowledged broadcast
static void net_send(int type, const uint8_t* dst, int port, int dst_port, const uint8_t* d, int n) {
    static const uint8_t all_macs[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (net_type_reliable(type)) {
        if (dst && memcmp(dst, all_macs, 6)) { net_channel_send(mac64(dst), type, port, dst_port, d, n); return; }
        std::vector<uint64_t> v = net_targets();
        if (!v.empty()) { for (uint64_t k : v) net_channel_send(k, type, port, dst_port, d, n); return; }
    }
    std::vector<uint8_t> p;
    net_build(p, type, dst, port, dst_port, d, n, 0, 0, 0);
    net_udp_send(p.data(), (int)p.size());
}

static void mt_event(int type, const uint8_t* mac, const std::vector<uint8_t>& opt) {
    NetEvent e; e.ctl = false; e.type = type; memcpy(e.mac, mac, 6); e.opt = opt;
    net_queue.push_back(e);
}

static void net_event(int ev) {
    NetEvent e; e.ctl = true; e.type = ev; memset(e.mac, 0, 6);
    net_queue.push_back(e);
}

// 0x98-byte peer record: next, name[128], mac, padding, flags, last receive (us)
static void net_peer_record(uint32_t e, uint64_t k, const NetPeer& p, uint32_t next) {
    for (int j = 0; j < 0x98; j++) psp_s8(e + j, 0);
    psp_s32(e, next);
    for (int j = 0; j < 31 && p.nick[j]; j++) psp_s8(e + 4 + j, (uint8_t)p.nick[j]);
    uint8_t m[6]; mac_from64(k, m); mac_write(e + 0x84, m);
    psp_s32(e + 0x8C, 0x400);
    uint64_t t = psp_time_us(), age = (net_ms() - p.seen) * 1000;
    t = t > age ? t - age : 0;
    psp_s32(e + 0x90, (uint32_t)t); psp_s32(e + 0x94, (uint32_t)(t >> 32));
}

static int mt_busy() { int n = 0; for (auto& p : mt.peers) if (p.second.state) n++; return n; }

static void net_dispatch(const uint8_t* p, int n) {
    const uint8_t* src = p + 8;
    int type = p[4], port = p[6] | p[7] << 8, dst_port = p[20] | p[21] << 8, len = p[22] | p[23] << 8;
    if (48 + len > n) return;
    std::vector<uint8_t> d(p + 48, p + 48 + len);
    uint64_t k = mac64(src);
    if (memcmp(p + 24, net_group, 8)) return;
    NetPeer& peer = net_peers[k];
    peer.seen = net_ms();
    if (type == P_PDP || type == P_PDPU) {
        if (type == P_PDPU) net_copies++;
        for (auto& x : pdp)
            if (x.second.port == dst_port) {
                if (x.second.queue.size() >= 4096) { static int warned = 0; if (warned++ < 5) printf("net: cola PDP %d desbordada\n", x.first); continue; }
                PdpPacket q; memcpy(q.mac, src, 6); q.port = (uint16_t)port; q.d = d;
                x.second.queue.push_back(q);
            }
        return;
    }
    if (!mt.alive || !mt.active || port != mt.port) return;
    bool exists = mt.peers.count(k) != 0;
    MtPeer& m = mt.peers[k];
    switch (type) {
    case P_HELLO:
        m.seen = net_ms();
        if (mt.mode == 1 || m.state) break;
        if (!exists || m.hello != d) { m.hello = d; mt_event(M_HELLO, src, d); }
        break;
    case P_JOIN:
        if (mt.mode != 1) break;
        if (m.state == 0 && mt_busy() >= mt.maxp - 1) { net_send(P_CANCEL, src, mt.port, 0, nullptr, 0); break; }
        if (!net_sig_ok(net_peer_sig[k])) {
            net_send(P_CANCEL, src, mt.port, 0, nullptr, 0);
            printf("net: %02X:%02X:%02X rechazado, sus mods no coinciden\n", src[3], src[4], src[5]);
            break;
        }
        m.state = 1; m.seen = net_ms();
        mt_event(M_REQUEST, src, d);
        break;
    case P_ACCEPT:
        if (mt.mode == 1 || m.state != 1) break;
        m.state = 2;
        mt_event(M_ACCEPT, src, d);
        mt_event(M_ESTABLISHED, src, {});
        break;
    case P_CANCEL:
        if (m.state == 1) mt_event(mt.mode == 1 ? M_CANCEL : M_DENY, src, d);
        else if (m.state == 2) mt_event(mt.mode == 1 ? M_LEAVE : M_BYE, src, d);
        mt.peers.erase(k);
        break;
    case P_BYE:
        if (m.state) mt_event(M_BYE, src, {});
        mt.peers.erase(k);
        break;
    case P_DATA:
        if (m.state != 2) break;
        mt_event(M_DATA, src, d);
        net_send(P_DATAACK, src, mt.port, 0, nullptr, 0);
        break;
    case P_DATAACK:
        if (m.state == 2) mt_event(M_DATA_ACK, src, {});
        break;
    }
    if (!exists && !mt.peers[k].state && mt.peers[k].hello.empty() && type != P_HELLO) mt.peers.erase(k);
}

// ack, duplicate drop and in-order delivery before dispatch
static void net_process(const uint8_t* p, int n) {
    if (n < 48 || memcmp(p, "TTN3", 4)) return;
    const uint8_t* src = p + 8;
    if (!memcmp(src, net_mac, 6)) return;
    int type = p[4], len = p[22] | p[23] << 8;
    if (48 + len > n) return;
    uint8_t flags = p[5];
    uint64_t k = mac64(src);
    uint64_t sig = 0;
    for (int i = 7; i >= 0; i--) sig = sig << 8 | p[40 + i];
    net_peer_sig[k] = sig;
    if (type == P_MODS) { net_peer_mods[k].assign((const char*)(p + 48), len); return; }
    if (type == P_INTRO && len >= 8) {
        uint32_t id = net_get32(p + 48), cnt = net_get32(p + 52);
        if (id != net_intro_id.load()) { net_intro_id = id; net_intro_cnt = cnt; }
        else if (id == net_intro_id.load() && cnt > net_intro_cnt.load()) net_intro_cnt = cnt;
        return;
    }
    if (type == P_STATE) { psp_net_state_rx(p + 48, len); return; }
    if (type == P_STREAM) { psp_net_stream_rx(p + 48, len); return; }
    if (type == P_QUIT) { psp_net_quit_rx(p + 48, len); return; }
    if (type == P_DELAY && len >= 8) { net_delay_pack = (uint64_t)net_get32(p + 48) << 32 | net_get32(p + 52); return; }
    if (type == P_PRESENCE) {
        NetPeer& peer = net_peers[k];
        peer.seen = net_ms();
        memcpy(peer.group, p + 24, 8);
        snprintf(peer.nick, sizeof(peer.nick), "%.*s", len > 31 ? 31 : len, (const char*)(p + 48));
        return;
    }
    static const uint8_t all_macs[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (memcmp(p + 14, all_macs, 6) && memcmp(p + 14, net_mac, 6)) return;
    if (type == P_PING && len >= 8) { std::vector<uint8_t> a; net_build(a, P_PONG, src, 0, 0, p + 48, 8, 0, 0, 0); net_udp_send(a.data(), (int)a.size()); return; }
    if (type == P_PONG && len >= 8) {
        uint64_t s = 0;
        for (int i = 7; i >= 0; i--) s = s << 8 | p[48 + i];
        uint64_t now = net_ms();
        if (now >= s && now - s < 5000) net_rtt_sample(net_channels[k], (double)(now - s));
        return;
    }
    if (flags & 2) {
        NetChannel& c = net_channels[k];
        uint32_t ack = net_get32(p + 36);
        while (!c.pend.empty() && c.pend.front().seq <= ack) {
            NetPending& q = c.pend.front();
            if (!q.retries) net_rtt_sample(c, (double)(net_ms() - q.t));
            c.pend.pop_front();
        }
        if (ack == c.last_ack) c.ack_dups++; else { c.last_ack = ack; c.ack_dups = 0; }
        // fast retransmit: three repeated acks mean a gap, no wait for the timeout
        if (c.ack_dups >= 3 && !c.pend.empty()) {
            c.ack_dups = 0;
            NetPending& q = c.pend.front();
            q.t = net_ms(); q.retries++; c.resends++;
            q.p[5] |= 2;
            net_put32(&q.p[36], c.rx_seq - 1);
            net_udp_send(q.p.data(), (int)q.p.size());
        }
    }
    if (type == P_ACK) return;
    if (!(flags & 1)) { net_dispatch(p, n); return; }
    NetChannel& c = net_channels[k];
    uint32_t seq = net_get32(p + 32);
    c.ack_due = true;
    if (seq < c.rx_seq) { c.duplicates++; return; }
    if (seq > c.rx_seq) {
        if (c.stored.size() < 2048 && !c.stored.count(seq)) { c.stored[seq].assign(p, p + n); c.out_of_order++; }
        return;
    }
    net_dispatch(p, n);
    c.rx_seq++;
    for (;;) {
        auto it = c.stored.find(c.rx_seq);
        if (it == c.stored.end()) break;
        std::vector<uint8_t> q = it->second;
        c.stored.erase(it);
        net_dispatch(q.data(), (int)q.size());
        c.rx_seq++;
    }
}

// receives everything pending, sends periodic packets, retransmits unacked ones and expires rooms without HELLO
static void net_pump() {
    if (!net_ready) return;
    std::lock_guard<std::recursive_mutex> lock(net_mtx);
    static uint8_t buf[65536];
    int n;
    while ((n = net_udp_recv(buf, sizeof(buf))) > 0) net_process(buf, n);
    uint64_t t = net_ms();
    if (net_state == 1 && t - net_last_presence >= 1000) {
        net_last_presence = t;
        net_send(P_PRESENCE, nullptr, 0, 0, (const uint8_t*)net_nick, (int)strlen(net_nick) + 1);
    }
    if (mt.alive && mt.active && mt.mode == 1 && t - mt.last >= mt.hello_us / 1000) {
        mt.last = t;
        net_send(P_HELLO, nullptr, mt.port, 0, mt.opt.data(), (int)mt.opt.size());
        if (net_sig()) net_send(P_MODS, nullptr, mt.port, 0, (const uint8_t*)net_mods_txt.data(), (int)net_mods_txt.size());
    }
    if (mt.alive && mt.active && mt.mode != 1) {
        for (auto it = mt.peers.begin(); it != mt.peers.end();) {
            if (!it->second.state && it->second.seen && t - it->second.seen > 5000) {
                uint8_t m[6]; mac_from64(it->first, m);
                mt_event(M_LEAVE, m, {});
                if (net_log) printf("net: sala de %02X:%02X:%02X caducada sin HELLO\n", m[3], m[4], m[5]);
                it = mt.peers.erase(it);
            } else ++it;
        }
    }
    for (auto& x : net_channels) {
        NetChannel& c = x.second;
        uint8_t m[6]; mac_from64(x.first, m);
        if (((c.last_tx && t - c.last_tx < 10000) || (c.last_rtt && t - c.last_rtt < 10000)) && t - c.last_ping >= 250) {
            c.last_ping = t;
            uint8_t d[8]; for (int i = 0; i < 8; i++) d[i] = (uint8_t)(t >> (8 * i));
            std::vector<uint8_t> a; net_build(a, P_PING, m, 0, 0, d, 8, 0, 0, 0); net_udp_send(a.data(), (int)a.size());
        }
        for (auto& q : c.pend) {
            uint64_t timeout = c.rto << (q.retries > 3 ? 3 : q.retries);
            if (t - q.t < timeout) continue;
            q.t = t; q.retries++; c.resends++;
            q.p[5] |= 2;
            net_put32(&q.p[36], c.rx_seq - 1);
            net_udp_send(q.p.data(), (int)q.p.size());
        }
        if (c.ack_due) {
            c.ack_due = false;
            std::vector<uint8_t> a;
            net_build(a, P_ACK, m, 0, 0, nullptr, 0, 2, 0, c.rx_seq - 1);
            net_udp_send(a.data(), (int)a.size());
        }
        if (net_log > 0 && t - c.last_report >= 5000 && (c.resends || c.out_of_order || c.duplicates || !c.pend.empty())) {
            c.last_report = t;
            printf("net: par %02X:%02X:%02X rtt %.1f ms, fluctuacion %.1f ms, reenvios %ld, duplicados %ld, fuera de orden %ld, pendientes %d, copias de entradas %ld\n",
                   m[3], m[4], m[5], c.rtt, c.rtt_var, c.resends, c.duplicates, c.out_of_order, (int)c.pend.size(), net_copies);
        }
    }
}

static void net_loop() {
    while (net_thread_alive.load()) {
        net_pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

static struct NetShutdown { ~NetShutdown() { net_thread_alive = false; if (net_thread.joinable()) net_thread.join(); } } net_shutdown;

// events reach the game on the thread making the next network call, with mac and data on its stack
static void net_deliver(Ctx& c) {
    static bool delivering = false;
    if (delivering) return;
    delivering = true;
    for (;;) {
        NetEvent e;
        {
            std::lock_guard<std::recursive_mutex> lock(net_mtx);
            if (net_queue.empty()) break;
            e = net_queue.front();
            net_queue.pop_front();
        }
        uint32_t fn = e.ctl ? net_handler : mt.cb;
        Fn f = fn ? psp_resolve(fn) : nullptr;
        if (!f) continue;
        Ctx h = c;
        uint32_t base = (c.r[29] - 0x2000) & ~0xFu;
        if (e.ctl) {
            printf("net: evento %d al manejador 0x%08X\n", e.type, fn);
            h.r[4] = (uint32_t)e.type; h.r[5] = 0; h.r[6] = net_handler_arg;
        } else {
            int n = (int)std::min<size_t>(e.opt.size(), 0x1C00);
            mac_write(base + 0x10, e.mac);
            for (int i = 0; i < n; i++) psp_s8(base + 0x20 + i, e.opt[i]);
            printf("net: emparejamiento evento %d de %02X:%02X:%02X con %d bytes\n", e.type, e.mac[3], e.mac[4], e.mac[5], n);
            h.r[4] = 1; h.r[5] = (uint32_t)e.type; h.r[6] = base + 0x10; h.r[7] = (uint32_t)n; h.r[8] = n ? base + 0x20 : 0;
        }
        h.r[29] = base;
        h.r[31] = 0;
        f(h);
    }
    delivering = false;
}

static void net_trace(Ctx& c, const char* n, uint32_t res) {
    if (net_log < 0) { const char* e = getenv("TTT_NET_LOG"); net_log = e ? atoi(e) : 1; }
    if (!net_log) return;
    static std::map<std::string, int> seen_calls;
    char k[160];
    snprintf(k, sizeof(k), "%s %08X", n, c.r[31]);
    int& v = seen_calls[k];
    static int errors = 0;
    bool err = res && res != NET_WOULD_BLOCK && (!strcmp(n, "sceNetAdhocPdpRecv") || !strcmp(n, "sceNetAdhocPdpSend") || (!strncmp(n, "sceNetAdhocMatching", 19) && strcmp(n, "sceNetAdhocMatchingCreate")));
    if (err && errors++ < 100) { printf("net: ERROR %s(%08X, %08X, %08X, %08X, %08X, %08X) = %08X desde 0x%08X, vblank %u\n", n, c.r[4], c.r[5], c.r[6], c.r[7], c.r[8], c.r[9], res, c.r[31] - 8, psp_l32(0x08A78E14u)); return; }
    if (!res && (!strcmp(n, "sceNetAdhocPdpSend") || !strcmp(n, "sceNetAdhocPdpRecv"))) {
        static std::set<uint64_t> sizes;
        uint32_t t = !strcmp(n, "sceNetAdhocPdpSend") ? c.r[8] : (c.r[8] ? psp_l32(c.r[8]) : 0);
        if (sizes.size() < 64 && sizes.insert((uint64_t)c.r[31] << 32 | t).second)
            printf("net: tamano nuevo %s %u bytes desde 0x%08X, vblank %u\n", n + 11, t, c.r[31] - 8, psp_l32(0x08A78E14u));
    }
    if (++v > 4 && net_log < 2) { if (v == 5) printf("net: %s desde 0x%08X se repite, no se muestra mas\n", n, c.r[31] - 8); return; }
    printf("net: %s(%08X, %08X, %08X, %08X, %08X, %08X) = %08X desde 0x%08X\n", n, c.r[4], c.r[5], c.r[6], c.r[7], c.r[8], c.r[9], res, c.r[31] - 8);
}

static uint32_t net_pdp_recv(Ctx& c) {
    uint32_t id = c.r[4], amac = c.r[5], aport = c.r[6], abuf = c.r[7], alen = c.r[8], timeout = c.r[9], nb = c.r[10];
    uint64_t deadline = timeout ? net_ms() + timeout / 1000 : 0;
    for (;;) {
        PdpPacket q;
        bool found_any = false;
        {
            std::lock_guard<std::recursive_mutex> lock(net_mtx);
            if (!net_thread_alive.load()) net_pump();
            auto it = pdp.find((int)id);
            if (it == pdp.end()) return 0x80410717u;
            if (!it->second.queue.empty()) {
                q = it->second.queue.front();
                int cap = alen ? (int)psp_l32(alen) : 0;
                if ((int)q.d.size() > cap) { if (alen) psp_s32(alen, (uint32_t)q.d.size()); return 0x80410706u; }
                it->second.queue.pop_front();
                found_any = true;
            }
        }
        if (found_any) {
            for (size_t i = 0; i < q.d.size(); i++) psp_s8(abuf + (uint32_t)i, q.d[i]);
            if (alen) psp_s32(alen, (uint32_t)q.d.size());
            mac_write(amac, q.mac);
            if (aport) { psp_s8(aport, (uint8_t)q.port); psp_s8(aport + 1, (uint8_t)(q.port >> 8)); }
            return 0;
        }
        if (nb) return NET_WOULD_BLOCK;
        if (deadline && net_ms() >= deadline) return NET_TIMEOUT;
        psp_block(psp_time_us() + 1000);
    }
}

static bool net_hle(Ctx& c, const char* n, uint32_t* out) {
    if (strncmp(n, "sceNet", 6) && strncmp(n, "sceWlan", 7)) return false;
    net_init();
    {
        static uint32_t last_vals[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
        uint32_t esc = psp_l32(0x08A7EA50u), red = psp_l32(0x08A78A28u), v[4] = {0, 0, 0, 0};
        if (esc >= 0x08800000u && esc < 0x10000000u) { v[0] = psp_l32(esc + 0x18); v[1] = psp_l32(esc + 0x14); }
        if (red >= 0x08800000u && red < 0x10000000u) { v[2] = psp_l32(red + 0x2950); v[3] = psp_l32(red + 0x29A4); }
        if (memcmp(v, last_vals, sizeof(v))) { memcpy(last_vals, v, sizeof(v)); printf("net: escena %u (%u), fase de red %d, error 0x%08X, cuadro de %s\n", v[0], v[1], (int)v[2], v[3], n); }
    }
    if (!net_thread_alive.load()) net_pump();
    net_deliver(c);
    if (!strcmp(n, "sceNetAdhocPdpRecv")) { uint32_t r = net_pdp_recv(c); net_trace(c, n, r); *out = r; return true; }
    std::lock_guard<std::recursive_mutex> lock(net_mtx);
    uint32_t a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7], t0 = c.r[8], t1 = c.r[9], t2 = c.r[10];
    uint32_t res = 0;
    if (!strcmp(n, "sceWlanGetSwitchState")) {
        const char* e = getenv("TTT_WLAN");
        res = e && !atoi(e) ? 0 : 1;
    } else if (!strcmp(n, "sceWlanGetEtherAddr") || !strcmp(n, "sceNetGetLocalEtherAddr")) {
        mac_write(a0, net_mac);
    } else if (!strcmp(n, "sceNetAdhocctlGetState")) {
        if (a0) psp_s32(a0, (uint32_t)net_state);
    } else if (!strcmp(n, "sceNetAdhocctlConnect") || !strcmp(n, "sceNetAdhocctlCreate") || !strcmp(n, "sceNetAdhocctlJoin")) {
        uint32_t g = !strcmp(n, "sceNetAdhocctlJoin") ? a0 + 8 : a0;
        memset(net_group, 0, sizeof(net_group));
        if (g) for (int i = 0; i < 8; i++) net_group[i] = (char)psp_l8(g + i);
        net_state = 1;
        net_last_presence = 0;
        net_event(1);
        printf("net: conectado al grupo \"%.8s\"\n", net_group);
    } else if (!strcmp(n, "sceNetAdhocctlScan")) {
        net_event(3);
    } else if (!strcmp(n, "sceNetAdhocctlDisconnect")) {
        net_state = 0;
        memset(net_group, 0, sizeof(net_group));
        net_event(2);
    } else if (!strcmp(n, "sceNetAdhocctlGetScanInfo")) {
        // groups seen through presence: 28 bytes per group (next, channel, name, bssid, padding)
        uint64_t t = net_ms();
        std::vector<std::pair<std::string, uint64_t>> g;
        for (auto& p : net_peers) {
            if (t - p.second.seen >= 5000 || !p.second.group[0]) continue;
            std::string name(p.second.group, 8);
            bool found = false;
            for (auto& x : g) if (x.first == name) found = true;
            if (!found) g.push_back({name, p.first});
        }
        uint32_t cap = a0 ? psp_l32(a0) : 0;
        if (!a1) { if (a0) psp_s32(a0, (uint32_t)g.size() * 28); }
        else {
            uint32_t k = std::min<uint32_t>((uint32_t)g.size(), cap / 28);
            for (uint32_t i = 0; i < k; i++) {
                uint32_t e = a1 + i * 28;
                for (int j = 0; j < 28; j++) psp_s8(e + j, 0);
                psp_s32(e, i + 1 < k ? e + 28 : 0);
                psp_s32(e + 4, 1);
                for (int j = 0; j < 8; j++) psp_s8(e + 8 + j, (uint8_t)g[i].first[j]);
                uint8_t m[6]; mac_from64(g[i].second, m); mac_write(e + 16, m);
            }
            if (a0) psp_s32(a0, k * 28);
        }
        if (net_log) printf("net: escaneo: %u grupos\n", (unsigned)g.size());
    } else if (!strcmp(n, "sceNetAdhocctlGetPeerList")) {
        uint64_t t = net_ms();
        std::vector<std::pair<uint64_t, NetPeer*>> v;
        for (auto& p : net_peers) if (t - p.second.seen < 5000) v.push_back({p.first, &p.second});
        uint32_t cap = a0 ? psp_l32(a0) : 0, n152 = (uint32_t)v.size() * 152;
        if (!a1) { if (a0) psp_s32(a0, n152); }
        else {
            uint32_t k = std::min<uint32_t>((uint32_t)v.size(), cap / 152);
            for (uint32_t i = 0; i < k; i++) net_peer_record(a1 + i * 152, v[i].first, *v[i].second, i + 1 < k ? a1 + (i + 1) * 152 : 0);
            psp_s32(a0, k * 152);
        }
    } else if (!strcmp(n, "sceNetAdhocctlGetPeerInfo")) {
        uint8_t m[6]; mac_read(a0, m);
        auto it = net_peers.find(mac64(m));
        if (it == net_peers.end() || net_ms() - it->second.seen > 5000) res = 0x80410716u;
        else if (a2 && a1 >= 0x98) net_peer_record(a2, it->first, it->second, 0);
    } else if (!strcmp(n, "sceNetAdhocctlGetNameByAddr")) {
        uint8_t m[6]; mac_read(a0, m);
        const char* nick = !memcmp(m, net_mac, 6) ? net_nick : net_peers.count(mac64(m)) ? net_peers[mac64(m)].nick : "";
        if (a1) { for (int i = 0; i < 128; i++) psp_s8(a1 + i, 0); for (int i = 0; nick[i] && i < 127; i++) psp_s8(a1 + i, (uint8_t)nick[i]); }
    } else if (!strcmp(n, "sceNetAdhocctlGetParameter")) {
        if (a0) {
            for (int i = 0; i < 0x92; i++) psp_s8(a0 + i, 0);
            psp_s32(a0, 1);
            for (int i = 0; i < 8; i++) psp_s8(a0 + 4 + i, (uint8_t)net_group[i]);
            mac_write(a0 + 0xC, net_mac);
            for (int i = 0; net_nick[i]; i++) psp_s8(a0 + 0x12 + i, (uint8_t)net_nick[i]);
        }
    } else if (!strcmp(n, "sceNetAdhocctlAddHandler")) {
        net_handler = a0; net_handler_arg = a1;
        res = 1;
    } else if (!strcmp(n, "sceNetAdhocMatchingCreate")) {
        mt.alive = true; mt.active = false; mt.mode = (int)a0; mt.maxp = (int)a1; mt.port = (int)a2;
        mt.hello_us = t0 ? t0 : 1000000; mt.cb = psp_l32(c.r[29]); mt.peers.clear(); mt.opt.clear();
        res = 1;
        printf("net: emparejamiento modo %d, %d jugadores, puerto %d, callback 0x%08X\n", mt.mode, mt.maxp, mt.port, mt.cb);
    } else if (!strcmp(n, "sceNetAdhocMatchingStart")) {
        mt.active = true; mt.last = 0;
        mt.opt = net_bytes(t2, (int)t1);
    } else if (!strcmp(n, "sceNetAdhocMatchingStop") || !strcmp(n, "sceNetAdhocMatchingDelete")) {
        for (auto& p : mt.peers) if (p.second.state) { uint8_t m[6]; mac_from64(p.first, m); net_send(P_BYE, m, mt.port, 0, nullptr, 0); }
        mt.peers.clear();
        mt.active = false;
        if (n[19] == 'D') mt.alive = false;
    } else if (!strcmp(n, "sceNetAdhocMatchingSelectTarget")) {
        uint8_t m[6]; mac_read(a1, m);
        std::vector<uint8_t> o = net_bytes(a3, (int)a2);
        MtPeer& p = mt.peers[mac64(m)];
        if (mt.mode == 1) {
            if (p.state == 1) { net_send(P_ACCEPT, m, mt.port, 0, o.data(), (int)o.size()); p.state = 2; mt_event(M_ESTABLISHED, m, {}); }
        } else if (net_peer_sig.count(mac64(m)) && !net_sig_ok(net_peer_sig[mac64(m)])) {
            mt.peers.erase(mac64(m));
            mt_event(M_DENY, m, {});
            net_mods_refused(mac64(m));
        } else {
            net_send(P_JOIN, m, mt.port, 0, o.data(), (int)o.size());
            p.state = 1;
        }
    } else if (!strcmp(n, "sceNetAdhocMatchingCancelTarget") || !strcmp(n, "sceNetAdhocMatchingCancelTargetWithOpt")) {
        uint8_t m[6]; mac_read(a1, m);
        std::vector<uint8_t> o = n[31] == 'W' ? net_bytes(a3, (int)a2) : std::vector<uint8_t>();
        net_send(P_CANCEL, m, mt.port, 0, o.data(), (int)o.size());
        mt.peers.erase(mac64(m));
    } else if (!strcmp(n, "sceNetAdhocMatchingSendData")) {
        uint8_t m[6]; mac_read(a1, m);
        std::vector<uint8_t> o = net_bytes(a3, (int)a2);
        net_send(P_DATA, m, mt.port, 0, o.data(), (int)o.size());
    } else if (!strcmp(n, "sceNetAdhocMatchingSetHelloOpt")) {
        mt.opt = net_bytes(a2, (int)a1);
    } else if (!strcmp(n, "sceNetAdhocMatchingGetHelloOpt")) {
        uint32_t cap = a1 ? psp_l32(a1) : 0;
        for (uint32_t i = 0; a2 && i < std::min<uint32_t>(cap, (uint32_t)mt.opt.size()); i++) psp_s8(a2 + i, mt.opt[i]);
        if (a1) psp_s32(a1, (uint32_t)mt.opt.size());
    } else if (!strcmp(n, "sceNetAdhocMatchingGetMembers")) {
        std::vector<uint64_t> v{mac64(net_mac)};
        for (auto& p : mt.peers) if (p.second.state == 2) v.push_back(p.first);
        uint32_t cap = a1 ? psp_l32(a1) : 0;
        if (!a2) { if (a1) psp_s32(a1, (uint32_t)v.size() * 12); }
        else {
            uint32_t k = std::min<uint32_t>((uint32_t)v.size(), cap / 12);
            for (uint32_t i = 0; i < k; i++) {
                uint32_t e = a2 + i * 12;
                psp_s32(e, i + 1 < k ? e + 12 : 0);
                uint8_t m[6]; mac_from64(v[i], m); mac_write(e + 4, m);
                psp_s8(e + 10, 0); psp_s8(e + 11, 0);
            }
            psp_s32(a1, k * 12);
        }
    } else if (!strcmp(n, "sceNetAdhocPdpCreate")) {
        int id = pdp_next++;
        pdp[id].port = (uint16_t)a1;
        res = (uint32_t)id;
    } else if (!strcmp(n, "sceNetAdhocPdpDelete")) {
        pdp.erase((int)a0);
    } else if (!strcmp(n, "sceNetAdhocPdpSend")) {
        auto it = pdp.find((int)a0);
        if (it == pdp.end()) res = 0x80410717u;
        else {
            uint8_t m[6]; mac_read(a1, m);
            std::vector<uint8_t> d = net_bytes(a3, (int)t0);
            // lockstep input packet (client type 3, host type 4 with the slot states): with the input stream its entries are
            // ignored and it only carries the slot states, repeated every frame, so it goes unreliable; without it an extra copy
            bool in = (d.size() == 43 && d[0] == 3) || (d.size() == 47 && d[0] == 4);
            if (!in || !net_stream) net_send(P_PDP, m, it->second.port, (int)(a2 & 0xFFFF), d.data(), (int)d.size());
            if (in && (net_stream || net_input_copy())) net_send(P_PDPU, m, it->second.port, (int)(a2 & 0xFFFF), d.data(), (int)d.size());
        }
    } else if (!strcmp(n, "sceNetAdhocGetPdpStat")) {
        uint32_t cap = a0 ? psp_l32(a0) : 0;
        if (!a1) { if (a0) psp_s32(a0, (uint32_t)pdp.size() * 20); }
        else {
            uint32_t i = 0;
            for (auto& x : pdp) {
                if ((i + 1) * 20 > cap) break;
                uint32_t e = a1 + i * 20, pend = x.second.queue.empty() ? 0 : (uint32_t)x.second.queue.front().d.size();
                psp_s32(e, (i + 1) * 20 < cap && i + 1 < pdp.size() ? e + 20 : 0);
                psp_s32(e + 4, (uint32_t)x.first);
                mac_write(e + 8, net_mac);
                psp_s8(e + 14, (uint8_t)x.second.port); psp_s8(e + 15, (uint8_t)(x.second.port >> 8));
                psp_s32(e + 16, pend);
                i++;
            }
            psp_s32(a0, i * 20);
        }
    }
    (void)t1; (void)t2;
    net_trace(c, n, res);
    *out = res;
    return true;
}

// port packets outside the game's protocol: intro skip (host -> all) and the per-frame state check
void psp_net_extra(int type, const uint8_t* d, int n) {
    if (!net_ready) return;
    std::lock_guard<std::recursive_mutex> lock(net_mtx);
    net_send(type, nullptr, 0, 0, d, n);
}
uint32_t psp_net_intro(uint32_t* id) { *id = net_intro_id.load(); return net_intro_cnt.load(); }
uint32_t psp_net_delay(uint32_t* epoch) { uint64_t v = net_delay_pack.load(); *epoch = (uint32_t)(v >> 32); return (uint32_t)v; }
void psp_net_delay_reset() { net_delay_pack = 0; }
void psp_net_dump() {
    if (!net_ready) return;
    std::lock_guard<std::recursive_mutex> lock(net_mtx);
    uint64_t t = net_ms();
    for (auto& x : net_channels) {
        NetChannel& c = x.second;
        uint8_t m[6]; mac_from64(x.first, m);
        printf("net: canal %02X:%02X:%02X: enviados %u, esperado %u, pendientes %d (el mas viejo %llu ms, %d reenvios), guardados %d, ultimo rtt hace %llu ms\n",
               m[3], m[4], m[5], c.tx_seq - 1, c.rx_seq, (int)c.pend.size(), c.pend.empty() ? 0ull : (unsigned long long)(t - c.pend.front().t), c.pend.empty() ? 0 : c.pend.front().retries,
               (int)c.stored.size(), c.last_rtt ? (unsigned long long)(t - c.last_rtt) : 0ull);
    }
    for (auto& x : net_peers) { uint8_t m[6]; mac_from64(x.first, m); printf("net: par %02X:%02X:%02X visto hace %llu ms\n", m[3], m[4], m[5], (unsigned long long)(t - x.second.seen)); }
}
int psp_net_rtt_ms() {
    if (!net_ready) return 0;
    std::lock_guard<std::recursive_mutex> lock(net_mtx);
    uint64_t t = net_ms();
    double m = 0;
    for (auto& x : net_channels) if (x.second.rtt > 0 && t - x.second.last_rtt < 10000) { double v = x.second.rtt + 2 * x.second.rtt_var; if (v > m) m = v; }
    return (int)m;
}
void psp_net_rtt_parts(int* rtt, int* var) {
    *rtt = 0; *var = 0;
    if (!net_ready) return;
    std::lock_guard<std::recursive_mutex> lock(net_mtx);
    uint64_t t = net_ms();
    double m = 0;
    for (auto& x : net_channels) if (x.second.rtt > 0 && t - x.second.last_rtt < 10000) { double v = x.second.rtt + 2 * x.second.rtt_var; if (v > m) { m = v; *rtt = (int)x.second.rtt; *var = (int)ceil(x.second.rtt_var); } }
}

// matching and adhocctl events reach the game only inside a network call; screens that make none (our online room) poll here
int psp_net_poll(Ctx& c) {
    if (!net_ready) return 0;
    if (!net_thread_alive.load()) net_pump();
    size_t n;
    { std::lock_guard<std::recursive_mutex> lock(net_mtx); n = net_queue.size(); }
    if (n) net_deliver(c);
    return (int)n;
}
