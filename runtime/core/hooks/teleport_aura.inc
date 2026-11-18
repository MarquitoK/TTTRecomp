// bit 2 of +0xA60 (aura visible): 0x02000000 turns it off, 0x04000000 on
static void aura_bit(Ctx& c, void (*orig)(Ctx&)) {
    static int on = -1;
    if (on < 0) { const char* e = getenv("TTT_AURA"); on = e ? atoi(e) : 1; }
    static std::unordered_map<uint32_t, int> held;
    uint32_t ent = c.r[4], d = ent + 0xA60u;
    if (!battle60()) orig(c);
    else {
        auto it = held.find(ent);
        bool pending = it != held.end();
        if (pending) psp_s32(d, psp_l32(d) & ~2u);
        bool before = psp_l32(d) & 2u;
        orig(c);
        bool ahora = psp_l32(d) & 2u;
        if (pending) {
            if (ahora || --it->second <= 0 || !on) held.erase(it);
            else psp_s32(d, psp_l32(d) | 2u);
        } else if (on && before && !ahora && battle_active()) { held[ent] = 2; psp_s32(d, psp_l32(d) | 2u); }
    }
}

