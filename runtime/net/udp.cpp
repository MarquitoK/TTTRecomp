// Multiplayer UDP transport: LAN (broadcast on every interface) plus instances on this PC (127.0.0.1), ports 27312..27319
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET (-1)
#define closesocket close
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>
#include <chrono>

static SOCKET udp = INVALID_SOCKET;
static int udp_port = 0, udp_base = 27312, udp_n = 8, udp_lan = 1;
static std::vector<uint32_t> udp_bcast;   // directed broadcast of each IPv4 interface (host order)
static uint64_t udp_bcast_ms = 0;
static std::map<uint64_t, sockaddr_in> udp_peers;   // mac -> real address, learned on receive
static std::mutex udp_mtx;

bool net_online_on();
void net_online_start();
void net_online_send(const void* d, int n);
int net_online_recv(void* d, int max);

static uint64_t udp_mac(const unsigned char* m) { uint64_t v = 0; for (int i = 0; i < 6; i++) v = v << 8 | m[i]; return v; }

int net_udp_port() { return udp_port; }

bool net_udp_open() {
    if (udp != INVALID_SOCKET) return true;
#ifdef _WIN32
    WSADATA w;
    if (WSAStartup(MAKEWORD(2, 2), &w)) return false;
#endif
    if (const char* e = getenv("TTT_NET_PORT")) udp_base = atoi(e);
    if (const char* e = getenv("TTT_NET_LAN")) udp_lan = atoi(e);
    for (int k = 0; k < udp_n; k++) {
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return false;
        sockaddr_in a = {};
        a.sin_family = AF_INET;
        a.sin_port = htons((unsigned short)(udp_base + k));
        a.sin_addr.s_addr = htonl(udp_lan ? INADDR_ANY : INADDR_LOOPBACK);
        if (bind(s, (sockaddr*)&a, sizeof(a)) == 0) {
            int yes = 1, buf = 4 << 20;
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof(yes));
            setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char*)&buf, sizeof(buf));
            setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char*)&buf, sizeof(buf));
#ifdef _WIN32
            // without this, every send to an empty port returns WSAECONNRESET on the next read
            BOOL off = FALSE;
            DWORD r = 0;
            WSAIoctl(s, SIO_UDP_CONNRESET, &off, sizeof(off), nullptr, 0, &r, nullptr, nullptr);
            u_long nb = 1;
            ioctlsocket(s, FIONBIO, &nb);
#else
            fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
            udp = s;
            udp_port = udp_base + k;
            printf("net: UDP en el puerto %d (%s)\n", udp_port, udp_lan ? "LAN y este PC" : "solo este PC");
            net_online_start();
            return true;
        }
        closesocket(s);
    }
    printf("net: no hay puerto UDP libre entre %d y %d\n", udp_base, udp_base + udp_n - 1);
    return false;
}

static uint64_t udp_ms() { return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static void udp_interfaces() {
    uint64_t t = udp_ms();
    if (udp_bcast_ms && t - udp_bcast_ms < 10000) return;
    udp_bcast_ms = t;
    std::vector<uint32_t> v;
#ifdef _WIN32
    INTERFACE_INFO list[32];
    DWORD bytes = 0;
    if (WSAIoctl(udp, SIO_GET_INTERFACE_LIST, nullptr, 0, list, sizeof(list), &bytes, nullptr, nullptr) == 0) {
        int k = (int)(bytes / sizeof(INTERFACE_INFO));
        for (int i = 0; i < k; i++) {
            if (!(list[i].iiFlags & IFF_UP) || (list[i].iiFlags & IFF_LOOPBACK) || !(list[i].iiFlags & IFF_BROADCAST)) continue;
            uint32_t ip = ntohl(list[i].iiAddress.AddressIn.sin_addr.s_addr), mask = ntohl(list[i].iiNetmask.AddressIn.sin_addr.s_addr);
            if (!ip || !mask) continue;
            uint32_t b = ip | ~mask;
            bool dup = false;
            for (uint32_t x : v) if (x == b) dup = true;
            if (!dup) v.push_back(b);
        }
    }
#endif
    if (v.empty()) v.push_back(INADDR_BROADCAST);
    if (v != udp_bcast) {
        udp_bcast = v;
        printf("net: LAN por %d interfaces:", (int)v.size());
        for (uint32_t b : v) printf(" %u.%u.%u.%u", b >> 24, (b >> 16) & 255, (b >> 8) & 255, b & 255);
        printf("\n");
        fflush(stdout);
    }
}

static void udp_to(uint32_t ip, int port, const void* d, int n) {
    sockaddr_in a = {};
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(ip);
    sendto(udp, (const char*)d, n, 0, (sockaddr*)&a, sizeof(a));
}

// a known destination gets a unicast; otherwise every port of this PC and, on a LAN, each interface's broadcast
void net_udp_send(const void* d, int n) {
    if (net_online_on()) { net_online_send(d, n); return; }
    if (udp == INVALID_SOCKET) return;
    const unsigned char* p = (const unsigned char*)d;
    if (n >= 32) {
        std::lock_guard<std::mutex> g(udp_mtx);
        auto it = udp_peers.find(udp_mac(p + 14));
        if (it != udp_peers.end()) { sendto(udp, (const char*)d, n, 0, (sockaddr*)&it->second, sizeof(it->second)); return; }
    }
    if (udp_lan) udp_interfaces();
    for (int k = 0; k < udp_n; k++) {
        int port = udp_base + k;
        if (port != udp_port) udp_to(INADDR_LOOPBACK, port, d, n);
        if (udp_lan) for (uint32_t b : udp_bcast) udp_to(b, port, d, n);
    }
}

int net_udp_recv(void* d, int max) {
    if (net_online_on()) return net_online_recv(d, max);
    if (udp == INVALID_SOCKET) return -1;
    for (int tries = 0; tries < 64; tries++) {
        sockaddr_in a;
        socklen_t l = sizeof(a);
        int r = (int)recvfrom(udp, (char*)d, max, 0, (sockaddr*)&a, &l);
        if (r > 0) {
            if (r >= 32 && !memcmp(d, "TTN", 3)) {
                std::lock_guard<std::mutex> g(udp_mtx);
                udp_peers[udp_mac((const unsigned char*)d + 8)] = a;
            }
            return r;
        }
#ifdef _WIN32
        int e = WSAGetLastError();
        if (e == WSAECONNRESET || e == WSAEMSGSIZE) continue;
#else
        if (r < 0 && errno == ECONNREFUSED) continue;
#endif
        return -1;
    }
    return -1;
}
