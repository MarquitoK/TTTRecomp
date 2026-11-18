// item 71: planet destruction. A technique whose definition carries BT3's flag 0x200000 (kept in TTT's parameter files) hits a
// fighter below half life -> the game's own mid-battle stage change (0x088540C4) to the destroyed version of the map.
// Targets: Namek Land / Village -> Namek Destroyed (or a mod map with planet=namek); Earth originals -> a mod map with planet=earth;
// new maps: destroy_to in stages.txt, else the BT3 stage kind of their MAPD (1 earth, 2 namek). Active through a mod: the planet's
// transition pack (BT3 files 0x1C0 / 0x1BF) at Packfile/Planet/earth.pak / namek.pak of an enabled mod (Map Destruction).
#include <mutex>
#include <map>
#include <memory>
#include <thread>
#include <chrono>
std::atomic<int> psp_dbg_planet{0};
static bool pl_net();   // lockstep battle (wrappers60.inc): the change loads files at its own pace, never online
static uint32_t pl_task[64];
static int32_t pl_life[4];   // life at the previous tick: the hit's damage is already applied in the tick that sees it
static int pl_logs = 0;

std::string psp_cr_file(const std::string& rel);
void psp_burst_load(const std::string& path);   // gpu/burst.h: the explosion drawn during the transition
static std::string pl_pak(bool namek) { return psp_cr_file(namek ? "Packfile/Planet/namek.pak" : "Packfile/Planet/earth.pak"); }
static std::mutex pl_mx;
static std::map<std::string, std::shared_ptr<const std::vector<uint8_t>>> pl_files;   // planet packs read once (ahead of the change when possible)
static std::shared_ptr<const std::vector<uint8_t>> pl_bytes(const std::string& fn) {
    { std::lock_guard<std::mutex> lk(pl_mx); auto it = pl_files.find(fn); if (it != pl_files.end()) return it->second; }
    FILE* fp = fn.empty() ? nullptr : fopen(fn.c_str(), "rb");
    if (!fp) return nullptr;
    auto d = std::make_shared<std::vector<uint8_t>>();
    uint8_t buf[65536]; size_t k;
    while ((k = fread(buf, 1, sizeof(buf), fp)) > 0) d->insert(d->end(), buf, buf + k);
    fclose(fp);
    std::lock_guard<std::mutex> lk(pl_mx);
    return pl_files.emplace(fn, d).first->second;
}

static int pl_kind() {
    uint32_t st = psp_l32(0x08A7E678u), md = guest_ptr(st) ? psp_l32(st + 0x30) : 0, inf = guest_ptr(md) ? psp_l32(md + 0x14) : 0;
    return guest_ptr(inf) ? (int)psp_l32(inf + 0xC) : 0;
}

static int pl_target(int cur, int* kind) {
    auto& d = stages::defs();
    auto planet = [&](const char* p) { for (size_t j = 0; j < d.maps.size(); j++) if (d.maps[j].planet == p) return stages::OLD + (int)j; return -1; };
    *kind = 0;
    if (cur >= stages::OLD) {
        size_t j = (size_t)(cur - stages::OLD);
        if (j >= d.maps.size()) return -1;
        const std::string& t = d.maps[j].destroy_to;
        if (!t.empty()) {
            if (t == "none") return -1;
            int x = atoi(t.c_str());
            if (x >= 1 && x <= stages::OLD && t.find_first_not_of("0123456789") == std::string::npos) return x - 1;
            for (size_t k = 0; k < d.maps.size(); k++) if (d.maps[k].id == t) return stages::OLD + (int)k;
            return -1;
        }
        if (!d.maps[j].planet.empty()) return -1;
        *kind = pl_kind();
    } else *kind = cur == 4 || cur == 5 ? 2 : cur == 6 || cur == 9 ? 0 : 1;
    if (*kind == 2) { int t = planet("namek"); return t >= 0 ? t : 6; }
    if (*kind == 1) return planet("earth");
    return -1;
}

static uint32_t pl_guest(const Ctx& c, uint32_t fn, uint32_t a0, uint32_t a1 = 0) {
    Ctx c2 = c;
    c2.r[4] = a0; c2.r[5] = a1; c2.r[31] = 0;
    psp_call(c2, fn);
    return c2.r[2];
}

// transition pack of the game's fade change ([0x08B41A24], entries 1-3 with big endian offsets): the BT3 pack's camera (CMAn) as
// entry 1, a dummy entry 2 (TTT never reads the model) and an empty 6-texture ATEX as entry 3 (only relocated, never drawn)
static uint32_t pl_pack(const Ctx& c, bool namek) {
    std::string fn = pl_pak(namek);
    const char* nm = fn.c_str();
    auto dp = pl_bytes(fn);
    if (!dp) { printf("planeta: sin paquete de transicion de %s en los mods, cambio directo\n", namek ? "Namek" : "la Tierra"); return 0; }
    const std::vector<uint8_t>& d = *dp;
    auto rd = [&](size_t o) { uint32_t v = 0; if (o + 4 <= d.size()) memcpy(&v, &d[o], 4); return v; };
    uint32_t o1 = rd(4), o2 = rd(8);
    if (rd(0) != 3 || o1 >= o2 || o2 > d.size() || memcmp(&d[o1], "CMAn", 4)) { printf("planeta: %s no es un paquete de transicion de BT3\n", nm); return 0; }
    uint32_t cl = (o2 - o1 + 15) & ~15u, e1 = 0x40, e2 = e1 + cl, e3 = e2 + 0x10, tot = e3 + 0x20 + 6 * 0x60;
    uint32_t p = pl_guest(c, 0x089BFD60u, tot, 0x40);
    if (!guest_ptr(p)) { printf("planeta: sin memoria para la transicion (%u bytes)\n", tot); return 0; }
    uint8_t* g = (uint8_t*)psp_p(p);
    memset(g, 0, tot);
    auto be = [&](uint32_t o, uint32_t v) { g[o] = (uint8_t)(v >> 24); g[o + 1] = (uint8_t)(v >> 16); g[o + 2] = (uint8_t)(v >> 8); g[o + 3] = (uint8_t)v; };
    g[0] = 3; be(4, e1); be(8, e2); be(12, e3);
    memcpy(g + e1, &d[o1], o2 - o1);
    g[e3] = 6; g[e3 + 4] = 8;
    return p;
}

static bool pl_namek(int tgt) {
    auto& d = stages::defs();
    if (tgt == 6) return true;
    return tgt >= stages::OLD && (size_t)(tgt - stages::OLD) < d.maps.size() && d.maps[tgt - stages::OLD].planet == "namek";
}

static void pl_change(const Ctx& c, int tgt) {
    auto ms = [](std::chrono::steady_clock::time_point a) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count(); };
    auto t0 = std::chrono::steady_clock::now();
    uint32_t pk = psp_l32(0x08B41A24u) ? 0 : pl_pack(c, pl_namek(tgt));
    if (pk) {
        double a = ms(t0); auto t1 = std::chrono::steady_clock::now();
        psp_s32(0x08B41A24u, pk);
        uint32_t ok = pl_guest(c, 0x088541CCu, (uint32_t)tgt);
        double b = ms(t1); auto t2 = std::chrono::steady_clock::now();
        if (ok) psp_burst_load(pl_pak(pl_namek(tgt)));
        printf("planeta: cambio con transicion al mapa %d (%s), paquete 0x%08X, %s | ms: paquete %.1f, juego %.1f, explosion %.1f\n", tgt, pl_namek(tgt) ? "Namek" : "Tierra", pk, ok ? "aceptado" : "rechazado", a, b, ms(t2));
        fflush(stdout);
        if (ok) return;
        pl_guest(c, 0x089BFDB0u, pk);
        psp_s32(0x08B41A24u, 0);
    }
    pl_guest(c, 0x088540C4u, (uint32_t)tgt);
}

static void planet_tick(const Ctx& c) {
    uint32_t L = psp_l32(0x08A7BBE0u), t = psp_l32(0x08A7DBC8u);
    if (!guest_ptr(L) || !guest_ptr(t)) return;
    uint32_t n = psp_l32(t), b = psp_l32(t + 4);
    if (!n || n > 4 || !guest_ptr(b)) return;
    {   // the planet pack of this map read and parsed off the game thread, once per map
        static int ahead = -1; static int last = -1;
        if (ahead < 0) { const char* e = getenv("TTT_PLANET_AHEAD"); ahead = e && *e ? atoi(e) : 1; }
        int cur = (int)psp_l32(0x08B48A4Cu);
        if (ahead > 0 && cur != last && !pl_net()) {
            last = cur; int kind = 0, tgt = pl_target(cur, &kind);
            std::string fn = tgt >= 0 && tgt != cur ? pl_pak(pl_namek(tgt)) : std::string();
            if (!fn.empty()) std::thread([fn] { auto t0 = std::chrono::steady_clock::now(); pl_bytes(fn); psp_burst_load(fn);
                printf("planeta: paquete leido por adelantado (%s) en %.0f ms\n", fn.c_str(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()); fflush(stdout); }).detach();
        }
    }
    int32_t prev[4];
    for (uint32_t k = 0; k < 4; k++) { prev[k] = pl_life[k]; pl_life[k] = k < n ? (int32_t)psp_l32(b + 0x1A90u * k + 0x1A34) : 0; }
    for (int i = 0; i < 64; i++) {
        uint32_t r = L + 0x1A0u * i;
        uint32_t src = psp_l32(r + 0x74), def = guest_ptr(src) ? psp_l32(src + 0x28) : 0, node = psp_l32(r + 0x70);
        uint32_t tf = psp_l32(r + 0x10) == 1 && guest_ptr(def) && guest_ptr(node) ? psp_l32(node + 4) : 0;
        bool hit = (tf & 1u) && !(pl_task[i] & 1u);
        pl_task[i] = tf;
        if (!hit || !(psp_l32(def + 0x3C) & 0x200000u) || pl_net()) continue;
        uint32_t own = psp_l32(r), team = 2;
        for (uint32_t k = 0; k < n; k++) { uint32_t f = b + 0x1A90u * k; if (psp_l32(f + 0xC) && (psp_l32(f + 0x2C) & 3u) == own && team == 2) team = psp_l32(f + 4) & 1u; }
        int vk = -1; int32_t vl = 0, vm = 0;
        for (uint32_t k = 0; k < n && vk < 0; k++) {
            uint32_t f = b + 0x1A90u * k;
            int32_t life = (int32_t)psp_l32(f + 0x1A34), mx = (int32_t)psp_l32(f + 0x1A38), was = prev[k];
            if ((psp_l32(f + 4) & 1u) != team && psp_l32(f + 0xC) && life > 0 && was > 0 && (int64_t)was * 2 < mx) { vk = (int)k; vl = was; vm = mx; }
        }
        int id = (int)(int16_t)psp_l16(def), cur = (int)psp_l32(0x08B48A4Cu), kind = 0;
        if (vk < 0) {
            if (pl_logs < 20) {
                pl_logs++;
                printf("planeta: tecnica %d golpeo (dueno %u, equipo %u), ningun rival por debajo de la mitad:", id, own, team);
                for (uint32_t k = 0; k < n; k++) { uint32_t f = b + 0x1A90u * k; printf(" [%u: ranura %u equipo %u activo %u vida %d (antes %d) de %d]", k, psp_l32(f + 0x2C), psp_l32(f + 4), psp_l32(f + 0xC), (int32_t)psp_l32(f + 0x1A34), prev[k], (int32_t)psp_l32(f + 0x1A38)); }
                printf("\n"); fflush(stdout);
            }
            continue;
        }
        int tgt = pl_target(cur, &kind);
        if (tgt < 0 || tgt == cur) { if (pl_logs < 20) { pl_logs++; printf("planeta: tecnica %d, el mapa %d (tipo %d) no tiene version destruida\n", id, cur, kind); fflush(stdout); } continue; }
        if (pl_pak(pl_namek(tgt)).empty()) { if (pl_logs < 20) { pl_logs++; printf("planeta: tecnica %d, sin mod de destruccion de mapas para %s\n", id, pl_namek(tgt) ? "Namek" : "la Tierra"); fflush(stdout); } continue; }
        if (pl_logs < 40) { pl_logs++; printf("planeta: destruido por la tecnica %d (luchador %u), rival %d con vida %d de %d, mapa %d (tipo %d) -> %d\n", id, own, vk, vl, vm, cur, kind, tgt); fflush(stdout); }
        pl_change(c, tgt);
        break;
    }
}

// test key (Home, offline): the change to the map's destroyed version, the same rule as a real hit
static void pl_key(const Ctx& c) {
    if (!psp_dbg_planet.exchange(0) || pl_net()) return;
    int cur = (int)psp_l32(0x08B48A4Cu), kind = 0, tgt = pl_target(cur, &kind);
    printf("planeta: tecla Inicio, mapa %d (tipo %d) -> %d%s\n", cur, kind, tgt, tgt < 0 || tgt == cur ? " (sin version destruida, nada que hacer)" : "");
    fflush(stdout);
    if (tgt < 0 || tgt == cur) return;
    pl_change(c, tgt);
}
