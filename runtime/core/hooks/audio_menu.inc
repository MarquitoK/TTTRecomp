#include "patches/audio_menu.h"
// Options audio page (overlay 21_15, item 32): voices volume on its own row, voice language row from a voice pack mod
void psp_voice_want(const std::string& id);
std::string psp_voice_current();
int psp_voice_packs(std::vector<std::string>* names, std::vector<std::string>* ids, int lang);
void psp_voice_store(const std::string& id);

static const uint32_t AU_PROFILE = 0x08A7F708u, AU_SOUND = 0x08A78EE8u, AU_PAD = 0x08B4E990u;
static const uint32_t AU_MUSIC = 0x52C2, AU_FX = 0x52C3, AU_VOICE = 0x52C4;

struct AudioPage { uint32_t page, row3, bar3, row4; int n; };
static AudioPage au{};
static uint32_t au_mem = 0;
static bool au_vol_ready = false, au_in_init = false;
static const uint32_t AU_AGS_LANG = 71;   // menu_settings entry 71: the page with the language row (asset patch menu_settings)
static char au_snap[16] = "";

static uint32_t au_str(int k) { return au_mem + 0x20 * k; }   // 0 volume_bar_3, 1 volume_bar_4, 2 VOICE_LANG, 3 VOLUME_BAR, 4 floats, 5 ints, 6 style
static uint32_t au_text_buf() { return au_mem + 0x100; }

static void au_setup() {
    if (au_mem) return;
    au_mem = psp_reserve(0x200);
    static const char* names[4] = {"volume_bar_3", "volume_bar_4", "VOICE_LANG", "VOLUME_BAR"};
    for (int k = 0; k < 4; k++) memcpy(psp_p(au_str(k)), names[k], strlen(names[k]) + 1);
}

static uint32_t au_prof() { return psp_l32(AU_PROFILE); }
static int au_byte(uint32_t off) { uint32_t p = au_prof(); return p ? psp_l8(p + off) : 0; }

static void au_volume(const Ctx& c, uint32_t cat, float v) { mpm_callf(c, 0x0883A998u, psp_l32(AU_SOUND), cat, v); }

static uint32_t au_scene(uint32_t h) { uint32_t r = h ? psp_l32(h) : 0; return r ? psp_l32(r + 0x30) : 0; }
static uint32_t au_first_scene(uint32_t h) { uint32_t l = h ? psp_l32(h + 4) : 0; uint32_t r = l ? psp_l32(l + 4) : 0; return r ? psp_l32(r + 0x30) : 0; }
static uint32_t au_slot(const Ctx& c, uint32_t scene, int name) { return scene ? mpm_call(c, 0x08840DC4u, scene, au_str(name)) : 0; }
static uint32_t au_page_scene(uint32_t page) { uint32_t s = au_scene(psp_l32(page + 0x84)); return s ? s : au_first_scene(psp_l32(page + 0x84)); }

static uint32_t au_record(uint32_t h, int st) {
    uint32_t list = psp_l32(h + 4), n = psp_l32(h + 0xC);
    for (uint32_t i = 0; i < n; i++) if ((int)psp_l32(list + 8 * i) == st) return psp_l32(list + 8 * i + 4);
    return 0;
}

static void au_restart(const Ctx& c, uint32_t rec, uint32_t a1) {
    uint32_t obj = psp_l32(psp_l32(rec + 0x30)), e = psp_l32(obj) + 0x40;
    mpm_call(c, psp_l32(e + 4), obj + (uint32_t)(int32_t)(int16_t)psp_l16(e), a1);
}

// row state as 0x08A8F1C4 sets it
static void au_bind(const Ctx& c, uint32_t h, uint32_t slot, int st) {
    if (!h) return;
    uint32_t step = mpm_call(c, 0x08837A0Cu);
    psp_s32(h + 8, slot);
    mpm_wf(h + 0x10, (float)(int32_t)step);
    uint32_t cur = psp_l32(h);
    if (cur) mpm_wf(cur + 0x2C, 0.0f);
    uint32_t rec = au_record(h, st);
    psp_s32(h, rec);
    if (rec) mpm_wf(rec + 0x2C, 0.0f);
    psp_s8(h + 0x14, 1);
    if (rec) au_restart(c, rec, 1);
}

// bar fill as 0x08A8F450 sets it: frame = value * 2, stopped
static void au_bar(const Ctx& c) {
    if (!au.bar3 || !au_slot(c, au_page_scene(au.page), 0) || !au_slot(c, au_scene(au.row3) ? au_scene(au.row3) : au_first_scene(au.row3), 3)) return;
    uint32_t h = au.bar3, step = mpm_call(c, 0x08837A0Cu);
    psp_s32(h + 8, 0);
    mpm_wf(h + 0x10, (float)(int32_t)step);
    uint32_t cur = psp_l32(h);
    if (cur) mpm_wf(cur + 0x2C, 0.0f);
    uint32_t rec = au_record(h, 0);
    psp_s32(h, rec);
    if (rec) mpm_wf(rec + 0x2C, 0.0f);
    mpm_wf(h + 0x10, 0.0f);
    int v = au_byte(AU_VOICE);
    if (v > 10) v = 10;
    if (rec) mpm_wf(rec + 0x2C, (float)v * 0.1f * 2.0f * 10.0f);
    psp_s8(h + 0x14, 0);
    if (rec) au_restart(c, rec, 0);
}

static uint32_t au_new(const Ctx& c, uint32_t handle, int first, int last) {
    uint32_t h = mpm_call(c, 0x089BFDF4u, 0x54);
    if (h) mpm_call(c, 0x089D1EE8u, h, handle, (uint32_t)first, (uint32_t)last);
    if (h && (!mpm_has_state(h, first) || !mpm_has_state(h, last))) { mpm_call(c, 0x089D248Cu, h, 3); return 0; }
    return h;
}

static void au_free(const Ctx& c, uint32_t& h) { if (h) mpm_call(c, 0x089D248Cu, h, 3); h = 0; }

static void au_create(const Ctx& c, uint32_t page) {
    au = AudioPage{};
    au_setup();
    uint32_t h26 = psp_l32(page + 0x6C), h24 = psp_l32(page + 0x68), scene = au_first_scene(psp_l32(page + 0x84));
    if (!h26 || h26 == 0xFFFFFFFFu || !h24 || h24 == 0xFFFFFFFFu || !scene) return;
    bool s3 = au_slot(c, scene, 0) != 0, s4 = au_slot(c, scene, 1) != 0;
    int packs = psp_voice_packs(nullptr, nullptr, 0);
    if (!s3) { printf("audio: AGS 33 sin volume_bar_3 (assets sin el parche audio_menu): pagina original de 2 filas\n"); return; }
    au.page = page;
    au.row3 = au_new(c, h26, 6, 7);
    au.bar3 = au.row3 ? au_new(c, h24, 0, 0) : 0;
    if (!au.row3 || !au.bar3) {
        au_free(c, au.row3); au_free(c, au.bar3); au.page = 0;
        printf("audio: AGS 26 sin los estados 6/7 (assets sin el parche audio_menu): pagina original de 2 filas\n");
        return;
    }
    bool lang = patch_voice_lang.val && packs > 0 && s4;
    if (lang) au.row4 = au_new(c, h26, 8, 9);
    au.n = au.row4 ? 4 : 3;
    printf("audio: pagina de audio con %d filas (volume_bar_4 %s, parche voice_lang %d, paquetes de voces %d%s)\n", au.n, s4 ? "si" : "no", patch_voice_lang.val, packs,
           lang && !au.row4 ? ", AGS 26 sin los estados 8/9" : "");
}

static void au_select(const Ctx& c, uint32_t page) {
    uint32_t scene = au_page_scene(page);
    int cur = (int)psp_l32(page + 0xA4);
    au_bind(c, au.row3, au_slot(c, scene, 0), cur == 2 ? 7 : 6);
    if (au.row4) au_bind(c, au.row4, au_slot(c, scene, 1), cur == 3 ? 9 : 8);
    uint32_t desc = psp_l32(page + 0xC);
    if (desc && psp_l32(page + 0x48) == 1 && (cur == 2 || cur == 3)) mpm_call(c, 0x08A87080u, desc, cur == 2 ? 0x5213u : 0x5210u);
}

static int au_lang_index(std::vector<std::string>* ids) {
    psp_voice_packs(nullptr, ids, 0);
    std::string cur = psp_voice_current();
    for (size_t k = 0; k < ids->size(); k++) if ((*ids)[k] == cur) return (int)k + 1;
    return 0;
}

static bool au_step(const Ctx& c, uint32_t page, int row, int d) {
    uint32_t p = au_prof();
    if (!p) return false;
    if (row == 3) {
        std::vector<std::string> ids;
        int k = au_lang_index(&ids), n = (int)ids.size() + 1;
        k = ((k + d) % n + n) % n;
        std::string id = k ? ids[k - 1] : "";
        psp_voice_store(id);
        printf("audio: idioma de voces %s\n", id.empty() ? "original" : id.c_str());
        return true;
    }
    uint32_t off = row == 0 ? AU_FX : row == 1 ? AU_MUSIC : AU_VOICE;
    int v = psp_l8(p + off), nv = v + d;
    if (nv < 0 || nv > 10) return false;
    psp_s8(p + off, (uint8_t)nv);
    if (row == 1) { au_volume(c, 0, (float)nv * 0.1f); mpm_call(c, 0x08A8F450u, page); }
    else if (row == 0) mpm_call(c, 0x08A8F5F8u, page);
    else { au_volume(c, 1, (float)nv * 0.1f); au_bar(c); }
    static int logs = 0;
    if (logs++ < 30) printf("audio: %s %d\n", row == 0 ? "efectos" : row == 1 ? "musica" : "voces", nv);
    return true;
}

// 0x08A8F7A0 with 2..4 rows; the leave paths stay the game's own
static void au_input(const Ctx& c, uint32_t page, void (*orig)(Ctx&), Ctx& cc) {
    uint32_t pr = psp_l32(AU_PAD + 0x30);
    if ((pr & psp_l32(0x08A793D0u)) || (pr & psp_l32(0x08A793D4u))) { orig(cc); return; }
    int cur = (int)psp_l32(page + 0xA4);
    if ((pr & 8) && cur > 0) { mpm_sfx(c, 1, 3); psp_s32(page + 0xA4, (uint32_t)(cur - 1)); mpm_call(c, 0x08A8F1C4u, page); }
    pr = psp_l32(AU_PAD + 0x30);
    cur = (int)psp_l32(page + 0xA4);
    if ((pr & 4) && cur < au.n - 1) { mpm_sfx(c, 1, 3); psp_s32(page + 0xA4, (uint32_t)(cur + 1)); mpm_call(c, 0x08A8F1C4u, page); }
    cur = (int)psp_l32(page + 0xA4);
    uint32_t rep = psp_l32(AU_PAD + 0x34);
    if (cur < 0 || cur >= au.n) return;
    if ((rep & 1) && au_step(c, page, cur, -1)) mpm_sfx(c, 1, 3);
    rep = psp_l32(AU_PAD + 0x34);
    if ((rep & 2) && au_step(c, page, cur, 1)) mpm_sfx(c, 1, 3);
}

// the game's own update/draw loops over the 9 holders run once more with only the new rows in them
static void au_holders_call(Ctx c0, uint32_t page, void (*orig)(Ctx&)) {
    uint32_t keep[9];
    for (int k = 0; k < 9; k++) keep[k] = psp_l32(page + 0x74 + 4 * k);
    uint32_t mine[3] = {au.row3, au.bar3, au.row4};
    for (int k = 0; k < 9; k++) psp_s32(page + 0x74 + 4 * k, k < 3 ? mine[k] : 0);
    c0.r[4] = page;
    orig(c0);
    for (int k = 0; k < 9; k++) psp_s32(page + 0x74 + 4 * k, keep[k]);
}

// bar 3 placed as the draw loop places bar 1 (volume_bar_1 + VOLUME_BAR)
static void au_place_bar(const Ctx& c) {
    uint32_t f = au_str(4), i = au_str(5);
    for (int k = 0; k < 4; k++) { psp_s32(i + 4 * k, 0); mpm_wf(f + 4 * k, 0.0f); }
    uint32_t s = au_slot(c, au_page_scene(au.page), 0);
    if (s) mpm_call(c, 0x08840BA4u, s, f, f + 4, f + 8, f + 12);
    uint32_t rs = au_scene(au.row3), s2 = rs ? au_slot(c, rs, 3) : 0;
    if (s2) mpm_call(c, 0x08840B04u, s2, i, i + 4, i + 8, i + 12);
    if (!psp_l32(au.bar3)) return;
    psp_s32(au.bar3 + 0x18, psp_l32(i)); psp_s32(au.bar3 + 0x1C, psp_l32(i + 4));
    for (int k = 0; k < 4; k++) psp_s32(au.bar3 + 0x40 + 4 * k, psp_l32(f + 4 * k));
}

static uint32_t au_utf16(const char* src) {
    uint32_t a = au_text_buf();
    const unsigned char* p = (const unsigned char*)src;
    int n = 0;
    while (*p && n < 60) {
        uint32_t u = *p++;
        if (u >= 0xC0 && *p) u = ((u & 0x1F) << 6) | (*p++ & 0x3F);
        psp_s16(a + 2 * n++, u);
    }
    psp_s16(a + 2 * n, 0);
    return a;
}

// language name in VOICE_LANG (244, 157, 200 x 16): white, centred
static void au_lang_text(const Ctx& c) {
    uint32_t scene = au_scene(au.row4), s = scene ? au_slot(c, scene, 2) : 0, font = psp_l32(0x08A78EECu);
    if (!s || !font || !psp_l8(au.row4 + 0x15)) return;
    std::vector<std::string> names, ids;
    int k = au_lang_index(&ids);
    psp_voice_packs(&names, nullptr, mpm_li(mpm_lang(c)));
    static const char* ORIG[2] = {"English", "Ingl\u00E9s"};
    int li = mpm_li(mpm_lang(c));
    uint32_t str = au_utf16(k > 0 && k <= (int)names.size() ? names[k - 1].c_str() : ORIG[li]);
    mpm_call(c, 0x08832448u, font);
    uint32_t st = au_str(6), curst = psp_l32(psp_l32(font) + 0x10000u - 0x76E8u);
    for (int j = 0; j < 8; j++) psp_s32(st + 4 * j, psp_l32(curst + 4 * j));
    uint32_t keep = psp_l32(st + 8);
    psp_s32(st + 8, 0xFFFFFFFFu);
    mpm_call(c, 0x088321ECu, font, st);
    int w = (int)mpm_call(c, 0x08831F68u, font, str);
    int x = (int)psp_l32(s + 0xE4) + 100 - w / 2, y = (int)psp_l32(s + 0xE8);
    mpm_call(c, 0x08832530u, font, (uint32_t)x, (uint32_t)y, str, 0xFFFFFFFFu, 0xFFFFFFFFu);
    psp_s32(st + 8, keep);
    mpm_call(c, 0x088321ECu, font, st);
    mpm_call(c, 0x08832500u, font);
    static int logs = 0;
    if (logs++ < 1) printf("audio: idioma de voces dibujado en (%d, %d), ancho %d\n", x, y, w);
}

static bool au_wrap(uint32_t dir, Ctx& c, void (*orig)(Ctx&)) {
    if (!patch_audio_menu.val) return false;
    if (dir == 0x088404D8u) {   // page AGS 33 -> 71 while a voice pack is installed
        if (au_in_init && c.r[6] == 0x21 && patch_voice_lang.val && psp_voice_packs(nullptr, nullptr, 0) > 0) {
            bool has = hud_has(c.r[5], AU_AGS_LANG);
            if (has) c.r[6] = AU_AGS_LANG;
            static int logs = 0;
            if (logs++ < 3) printf("audio: pagina con fila de idioma %s\n", has ? "desde la entrada 71" : "no disponible (menu_settings sin la entrada 71): se usa la 33");
        }
        return false;
    }
    if (dir == 0x0883A998u) {   // categories 1 and 2 are only AT3 streams (voices, announcer); SFX read the effects byte (0x0881AA74)
        uint32_t cat = c.r[5];
        if ((cat != 1 && cat != 2) || !au_prof()) return false;
        float v = (float)au_byte(AU_VOICE) * 0.1f;
        Ctx c1 = c, c2 = c;
        c1.r[5] = 1; c1.f[12].f = v; orig(c1);
        c2.r[5] = 2; c2.f[12].f = v; orig(c2);
        au_vol_ready = true;
        static int last = -1;
        int key = au_byte(AU_FX) * 16 + au_byte(AU_VOICE);
        if (key != last) { last = key; printf("audio: volumen voces %d (categorias 1 y 2), efectos %d (SFX)\n", au_byte(AU_VOICE), au_byte(AU_FX)); }
        return true;
    }
    if (dir == 0x0881AA74u) {
        if (!au_vol_ready || !au_prof()) return false;
        float e = (float)au_byte(AU_FX) * 0.1f;
        int r = (int)(e * (float)(int32_t)c.r[4]);
        c.r[2] = (uint32_t)(r < 0 ? 0 : r > 127 ? 127 : r);
        return true;
    }
    if (dir < 0x08A8F1C4u || dir > 0x08A90930u) return false;
    uint32_t page = c.r[4];
    Ctx c0 = c;
    if (dir == 0x08A90930u) { au_in_init = true; orig(c); au_in_init = false; au_create(c0, page); return true; }
    bool mine = au.page && au.page == page && au.n >= 3;
    if (!mine) return false;
    if (dir == 0x08A8F1C4u) { orig(c); au_select(c0, page); return true; }
    if (dir == 0x08A8F450u) { orig(c); au_bar(c0); return true; }
    if (dir == 0x08A8F7A0u) { au_input(c0, page, orig, c); return true; }
    if (dir == 0x08A8FB70u) {
        orig(c);
        std::string s = psp_voice_current();
        snprintf(au_snap, sizeof(au_snap), "%s", s.c_str());
        return true;
    }
    if (dir == 0x08A8FC08u) {
        if (psp_l8(page + 0xA8) && au.row4) { psp_voice_store(au_snap); printf("audio: cancelado, idioma de voces %s\n", *au_snap ? au_snap : "original"); }
        uint32_t mine3[3] = {au.row3, au.bar3, au.row4};
        for (int k = 0; k < 3; k++) if (!psp_l32(page + 0x74 + 4 * k)) psp_s32(page + 0x74 + 4 * k, mine3[k]);
        au = AudioPage{};
        orig(c);
        return true;
    }
    if (dir == 0x08A8FE70u) { orig(c); au_holders_call(c0, page, orig); return true; }
    if (dir == 0x08A8FFC4u) {
        orig(c);
        au_place_bar(c0);
        au_holders_call(c0, page, orig);
        if (au.row4) au_lang_text(c0);
        return true;
    }
    return false;
}
