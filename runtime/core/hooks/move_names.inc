// technique names per character (item G 61): the port ids that disc.h writes in a fighter's 043 are answered here,
// before the game's text lookup 0x0883EE74 (id -> UTF-16 string), in the game's language (en, es)
#include "disc/roster_def.h"
int psp_mv_text(uint32_t id, int lang, std::string& out, uint32_t* orig);
uint32_t psp_mv_gen();
void psp_mv_set_lang(int li);
static bool mv_wrap(Ctx& c, void (*orig)(Ctx&)) {
    uint32_t id = c.r[4];
    bool own_name = (id >= roster::NAME_OLD && id < roster::NAME_OLD + (uint32_t)roster::K_OLD) || roster::rank_of_name(id) >= roster::NEW;
    if (!own_name && (id < 7000 || id >= 32768 || (id >= 10000 && id < 30000))) return false;
    int li = mpm_li(mpm_lang(c));
    psp_mv_set_lang(li);
    std::string s; uint32_t o = 0;
    int r = psp_mv_text(id, li, s, &o);
    if (!r) return false;
    if (r == 2) { c.r[4] = o; orig(c); return true; }
    static std::map<uint64_t, uint32_t> buf; static uint32_t gen = 0;
    uint32_t g = psp_mv_gen();
    if (g != gen) { buf.clear(); gen = g; }
    uint64_t k = (uint64_t)id << 8 | (uint32_t)li;
    auto it = buf.find(k);
    if (it == buf.end()) {
        std::vector<uint16_t> w;
        for (size_t i = 0; i < s.size();) {
            uint32_t ch = (uint8_t)s[i], extra = ch >= 0xF0 ? 3 : ch >= 0xE0 ? 2 : ch >= 0xC0 ? 1 : 0;
            if (extra) ch &= 0x3F >> extra;
            i++;
            for (uint32_t e = 0; e < extra && i < s.size(); e++, i++) ch = ch << 6 | ((uint8_t)s[i] & 0x3F);
            if (ch >= 0x10000) { ch -= 0x10000; w.push_back((uint16_t)(0xD800 | ch >> 10)); w.push_back((uint16_t)(0xDC00 | (ch & 0x3FF))); }
            else w.push_back((uint16_t)ch);
        }
        w.push_back(0);
        uint32_t p = psp_reserve((uint32_t)(w.size() * 2 + 15) & ~15u);
        for (size_t i = 0; i < w.size(); i++) psp_s16(p + 2 * (uint32_t)i, w[i]);
        it = buf.emplace(k, p).first;
        static int logs = 0;
        if (logs++ < 8) printf("mv: texto propio %u (%s) servido\n", id, li == 1 ? "es" : "en");
    }
    c.r[2] = it->second;
    return true;
}
static void mv_lang_tick(const Ctx& c) {   // the language a BT3 character's move list is served in
    static int n = 0;
    if ((n++ % 60) == 0) psp_mv_set_lang(mpm_li(mpm_lang(c)));
}
