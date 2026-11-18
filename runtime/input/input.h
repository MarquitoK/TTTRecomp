#pragma once
// Input: actions per context (menus, battle, story) bound to controller and keyboard/mouse, translated to the PSP pad.
// Included from ui/window.cpp.
#define DIRECTINPUT_VERSION 0x0800
#include <algorithm>
#include <initguid.h>
#include <dinput.h>
#include <cwchar>
#include "patches/all.h"

int psp_input_context();
int psp_input_clash();
int psp_input_prompt(int* codes, int max, uint32_t* serial);
int psp_input_prompt_of(int k, int* codes, int max, uint32_t* serial);
float psp_input_grab_dist();
static int input_log = -1;
static bool input_log_on() { if (input_log < 0) { const char* e = getenv("TTT_INPUT_LOG"); input_log = e ? atoi(e) : 0; } return input_log > 0; }

enum { PB_SELECT = 0x1, PB_START = 0x8, PB_UP = 0x10, PB_RIGHT = 0x20, PB_DOWN = 0x40, PB_LEFT = 0x80, PB_L = 0x100, PB_R = 0x200,
       PB_TRI = 0x1000, PB_CIR = 0x2000, PB_CRO = 0x4000, PB_SQU = 0x8000 };

// binding code: type in the high byte, value in the rest
#define C_KEY(v) (0x01000000u | (v))
#define C_MOUSE(v) (0x02000000u | (v))   // 1 left, 2 right, 3 wheel click, 4 side 1, 5 side 2, 6 wheel up, 7 wheel down
#define C_PAD(v) (0x05000000u | (v))
enum { P_SOUTH, P_EAST, P_WEST, P_NORTH, P_L1, P_R1, P_L2, P_R2, P_L3, P_R3, P_SELECT, P_START,
       P_DUP, P_DDOWN, P_DLEFT, P_DRIGHT, P_LSU, P_LSD, P_LSL, P_LSR, P_RSU, P_RSD, P_RSL, P_RSR, N_PADCODES };
enum { N_BINDS = 4 };
enum { CTX_MENU, CTX_BATTLE, CTX_STORY };
enum { TAB_MENU, TAB_BATTLE, TAB_SPECIAL, TAB_STORY, N_TABS };
static const int TAB_CTX[N_TABS] = {CTX_MENU, CTX_BATTLE, CTX_BATTLE, CTX_STORY};
enum { K_NONE, K_STICK, K_HOLD, K_TAP, K_DOUBLE, K_TAPREL, K_TAPLS };
enum { MK_TAP, MK_TWICE, MK_HOLD };
enum { D_NONE, D_UP, D_DOWN, D_LEFT, D_RIGHT };

enum {
    A_M_UP, A_M_DOWN, A_M_LEFT, A_M_RIGHT, A_M_ACCEPT, A_M_ACCSTART, A_M_START, A_M_BACK, A_M_RANDOM, A_M_PAGEL, A_M_PAGER, A_M_SWITCH,
    A_B_UP, A_B_DOWN, A_B_LEFT, A_B_RIGHT, A_B_HIT, A_B_BLAST, A_B_GUARD, A_B_DASH, A_B_CHARGE, A_B_TARGET,
    A_B_RISE, A_B_FALL, A_B_RISEF, A_B_FALLF, A_B_GRAB, A_B_EMOTE, A_B_TRANSF, A_B_FUSION, A_B_PAUSE, A_B_PLEFT, A_B_PRIGHT,
    A_S_SK1, A_S_SK2, A_S_DEX, A_S_AT1, A_S_AT2, A_S_AT3, A_S_FU1, A_S_FU2, A_S_FU3, A_S_TR1, A_S_TR2, A_S_TR3, A_S_DETR, A_S_CAML, A_S_CAMR, A_S_CAMD,
    A_H_UP, A_H_DOWN, A_H_LEFT, A_H_RIGHT, A_H_CONC, A_H_ABIL, A_H_FLY, A_H_NEXT, A_H_SKIP, A_H_MAP, A_H_BACK,
    A_H_MUP, A_H_MDOWN, A_H_PLEFT, A_H_PRIGHT, A_H_PAGER,
    N_ACTIONS
};

// kind per device (k_pad, k_kb); from_*: binding shared with another action (shown, not edited); mod_*: action that must be held
struct Action {
    const char* id; int tab; const wchar_t* name;
    int k_pad, k_kb;
    uint32_t base, out, off; int dir, mk; bool center;
    int mod_pad, mod_kb, from_pad, from_kb;
    uint32_t pad_def[N_BINDS], kb_def[N_BINDS];
};

#define KB(v) C_KEY(v)
#define MS(v) C_MOUSE(v)
#define PD(v) C_PAD(v)
static const Action ACT[N_ACTIONS] = {
    {"menu_up", TAB_MENU, L"Navigate up", K_STICK, K_STICK, 0, 0, 0, D_UP, 0, false, -1, -1, -1, -1, {PD(P_LSU), PD(P_DUP)}, {KB('W'), KB(VK_UP)}},
    {"menu_down", TAB_MENU, L"Navigate down", K_STICK, K_STICK, 0, 0, 0, D_DOWN, 0, false, -1, -1, -1, -1, {PD(P_LSD), PD(P_DDOWN)}, {KB('S'), KB(VK_DOWN)}},
    {"menu_left", TAB_MENU, L"Navigate left", K_STICK, K_STICK, 0, 0, 0, D_LEFT, 0, false, -1, -1, -1, -1, {PD(P_LSL), PD(P_DLEFT)}, {KB('A'), KB(VK_LEFT)}},
    {"menu_right", TAB_MENU, L"Navigate right", K_STICK, K_STICK, 0, 0, 0, D_RIGHT, 0, false, -1, -1, -1, -1, {PD(P_LSR), PD(P_DRIGHT)}, {KB('D'), KB(VK_RIGHT)}},
    {"menu_accept", TAB_MENU, L"Accept", K_HOLD, K_HOLD, 0, PB_CRO, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_SOUTH)}, {MS(1), KB(VK_SPACE)}},
    {"menu_accept_start", TAB_MENU, L"Accept / Start", K_HOLD, K_HOLD, 0, PB_CRO | PB_START, 0, 0, 0, false, -1, -1, -1, -1, {0}, {KB(VK_RETURN)}},
    {"menu_start", TAB_MENU, L"Start", K_HOLD, K_HOLD, 0, PB_START, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_START)}, {KB(VK_TAB)}},
    {"menu_back", TAB_MENU, L"Back", K_HOLD, K_HOLD, 0, PB_CIR, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_NORTH), PD(P_EAST)}, {KB(VK_ESCAPE)}},
    {"menu_random", TAB_MENU, L"Random", K_HOLD, K_HOLD, 0, PB_TRI, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_SELECT)}, {KB('R')}},
    {"menu_page_left", TAB_MENU, L"Previous page", K_HOLD, K_HOLD, 0, PB_L, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_L1), PD(P_L2)}, {MS(6)}},
    {"menu_page_right", TAB_MENU, L"Next page", K_HOLD, K_HOLD, 0, PB_R, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_R1), PD(P_R2)}, {MS(7)}},
    {"menu_switch", TAB_MENU, L"Switch / Details", K_HOLD, K_HOLD, 0, PB_SQU, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_WEST)}, {KB('F')}},

    {"battle_up", TAB_BATTLE, L"Move forward", K_STICK, K_STICK, 0, 0, 0, D_UP, 0, false, -1, -1, -1, -1, {PD(P_LSU)}, {KB('W')}},
    {"battle_down", TAB_BATTLE, L"Move back", K_STICK, K_STICK, 0, 0, 0, D_DOWN, 0, false, -1, -1, -1, -1, {PD(P_LSD)}, {KB('S')}},
    {"battle_left", TAB_BATTLE, L"Move left", K_STICK, K_STICK, 0, 0, 0, D_LEFT, 0, false, -1, -1, -1, -1, {PD(P_LSL)}, {KB('A')}},
    {"battle_right", TAB_BATTLE, L"Move right", K_STICK, K_STICK, 0, 0, 0, D_RIGHT, 0, false, -1, -1, -1, -1, {PD(P_LSR)}, {KB('D')}},
    {"battle_hit", TAB_BATTLE, L"Hit", K_HOLD, K_HOLD, 0, PB_SQU, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_WEST)}, {MS(1)}},
    {"battle_blast", TAB_BATTLE, L"Ki blast", K_HOLD, K_HOLD, 0, PB_TRI, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_NORTH)}, {MS(2)}},
    {"battle_guard", TAB_BATTLE, L"Guard", K_HOLD, K_HOLD, 0, PB_CIR, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_EAST)}, {KB('E')}},
    {"battle_dash", TAB_BATTLE, L"Dash", K_HOLD, K_HOLD, 0, PB_CRO, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_SOUTH)}, {KB('F')}},
    {"battle_charge", TAB_BATTLE, L"Charge ki", K_HOLD, K_HOLD, 0, PB_L, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_L2)}, {KB(VK_LSHIFT)}},
    {"battle_target", TAB_BATTLE, L"Change target", K_HOLD, K_HOLD, 0, PB_R, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_L1)}, {MS(3)}},
    {"battle_rise", TAB_BATTLE, L"Move up", K_HOLD, K_HOLD, 0, PB_UP, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_R1), PD(P_DUP)}, {KB(VK_SPACE)}},
    {"battle_fall", TAB_BATTLE, L"Move down", K_HOLD, K_HOLD, 0, PB_DOWN, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_R2), PD(P_DDOWN)}, {KB(VK_LCONTROL)}},
    {"battle_rise_fast", TAB_BATTLE, L"Move up fast", K_DOUBLE, K_DOUBLE, PB_L | PB_UP, 0, 0, 0, MK_HOLD, false, -1, -1, A_B_RISE, A_B_RISE, {0}, {0}},
    {"battle_fall_fast", TAB_BATTLE, L"Move down fast", K_DOUBLE, K_DOUBLE, PB_L | PB_DOWN, 0, 0, 0, MK_HOLD, false, -1, -1, A_B_FALL, A_B_FALL, {0}, {0}},
    {"battle_grab", TAB_BATTLE, L"Grab", K_NONE, K_TAP, 0, PB_CRO, PB_CIR | PB_SQU, 0, MK_TWICE, false, -1, A_B_GUARD, A_B_DASH, -1, {0}, {MS(1)}},
    {"battle_emote", TAB_BATTLE, L"Emote", K_TAP, K_TAP, 0, PB_L, 0, 0, MK_TWICE, false, -1, -1, -1, -1, {PD(P_L3)}, {KB('C')}},
    {"battle_transform", TAB_BATTLE, L"Transform", K_TAPLS, K_TAPREL, 0, PB_SELECT, 0, 0, MK_TAP, true, -1, -1, -1, -1, {PD(P_R3)}, {KB('Z')}},
    {"battle_fusion", TAB_BATTLE, L"Fusion", K_HOLD, K_NONE, 0, PB_SELECT, 0, 0, 0, false, A_B_CHARGE, -1, -1, -1, {PD(P_SELECT)}, {0}},
    {"battle_pause", TAB_BATTLE, L"Pause", K_HOLD, K_HOLD, 0, PB_START, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_START)}, {KB(VK_TAB)}},
    {"battle_partner_left", TAB_BATTLE, L"Partner mode left", K_HOLD, K_HOLD, 0, PB_LEFT, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_DLEFT)}, {MS(7)}},
    {"battle_partner_right", TAB_BATTLE, L"Partner mode right", K_HOLD, K_HOLD, 0, PB_RIGHT, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_DRIGHT)}, {MS(6)}},

    {"special_skill1", TAB_SPECIAL, L"Skill 1", K_NONE, K_TAP, PB_L, PB_CIR, 0, D_NONE, MK_TAP, true, -1, A_B_CHARGE, -1, -1, {0}, {KB('1')}},
    {"special_skill2", TAB_SPECIAL, L"Skill 2", K_NONE, K_TAP, PB_L, PB_CIR, 0, D_UP, MK_TAP, true, -1, A_B_CHARGE, -1, -1, {0}, {KB('2')}},
    {"special_dexplosion", TAB_SPECIAL, L"Double explosion", K_NONE, K_TAP, PB_L, PB_CIR, 0, D_DOWN, MK_TAP, true, -1, A_B_CHARGE, -1, -1, {0}, {KB('6')}},
    {"special_attack1", TAB_SPECIAL, L"Special attack 1", K_NONE, K_TAP, PB_L, PB_TRI, 0, D_NONE, MK_TAP, true, -1, A_B_CHARGE, -1, -1, {0}, {KB('3')}},
    {"special_attack2", TAB_SPECIAL, L"Special attack 2", K_NONE, K_TAP, PB_L, PB_TRI, 0, D_UP, MK_TAP, true, -1, A_B_CHARGE, -1, -1, {0}, {KB('4')}},
    {"special_attack3", TAB_SPECIAL, L"Ultimate attack", K_NONE, K_TAP, PB_L, PB_TRI, 0, D_DOWN, MK_TAP, true, -1, A_B_CHARGE, -1, -1, {0}, {KB('5')}},
    {"special_fusion1", TAB_SPECIAL, L"Fusion 1", K_NONE, K_TAP, PB_L, PB_SELECT, 0, D_NONE, MK_TAP, true, -1, A_B_TRANSF, -1, -1, {0}, {KB('4')}},
    {"special_fusion2", TAB_SPECIAL, L"Fusion 2", K_NONE, K_TAP, PB_L, PB_SELECT, 0, D_UP, MK_TAP, true, -1, A_B_TRANSF, -1, -1, {0}, {KB('5')}},
    {"special_fusion3", TAB_SPECIAL, L"Fusion 3", K_NONE, K_TAP, PB_L, PB_SELECT, 0, D_RIGHT, MK_TAP, true, -1, A_B_TRANSF, -1, -1, {0}, {KB('6')}},
    {"special_transform1", TAB_SPECIAL, L"Transformation 1", K_NONE, K_NONE, 0, PB_SELECT, 0, D_NONE, MK_TAP, true, -1, -1, A_B_TRANSF, A_B_TRANSF, {0}, {0}},
    {"special_transform2", TAB_SPECIAL, L"Transformation 2", K_NONE, K_TAP, 0, PB_SELECT, 0, D_UP, MK_TAP, true, -1, A_B_TRANSF, -1, -1, {0}, {KB('2')}},
    {"special_transform3", TAB_SPECIAL, L"Transformation 3", K_NONE, K_TAP, 0, PB_SELECT, 0, D_RIGHT, MK_TAP, true, -1, A_B_TRANSF, -1, -1, {0}, {KB('3')}},
    {"special_detransform", TAB_SPECIAL, L"Revert transformation", K_NONE, K_TAP, 0, PB_SELECT, 0, D_DOWN, MK_TAP, true, -1, A_B_TRANSF, -1, -1, {0}, {KB('1')}},
    {"camera_left", TAB_SPECIAL, L"BT3 camera left", K_HOLD, K_HOLD, 0, 0, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_RSL)}, {MS(4)}},
    {"camera_right", TAB_SPECIAL, L"BT3 camera right", K_HOLD, K_HOLD, 0, 0, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_RSR)}, {MS(5)}},
    {"camera_distance", TAB_SPECIAL, L"BT3 camera distance", K_HOLD, K_HOLD, 0, 0, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_SELECT)}, {KB('V')}},

    {"story_up", TAB_STORY, L"Move forward", K_STICK, K_STICK, 0, 0, 0, D_UP, 0, false, -1, -1, -1, -1, {PD(P_LSU)}, {KB('W')}},
    {"story_down", TAB_STORY, L"Move back", K_STICK, K_STICK, 0, 0, 0, D_DOWN, 0, false, -1, -1, -1, -1, {PD(P_LSD)}, {KB('S')}},
    {"story_left", TAB_STORY, L"Move left", K_STICK, K_STICK, 0, 0, 0, D_LEFT, 0, false, -1, -1, -1, -1, {PD(P_LSL)}, {KB('A')}},
    {"story_right", TAB_STORY, L"Move right", K_STICK, K_STICK, 0, 0, 0, D_RIGHT, 0, false, -1, -1, -1, -1, {PD(P_LSR)}, {KB('D')}},
    {"story_concentration", TAB_STORY, L"Concentration", K_HOLD, K_HOLD, 0, PB_TRI, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_L2)}, {KB(VK_LSHIFT)}},
    {"story_ability", TAB_STORY, L"Ability", K_HOLD, K_HOLD, 0, PB_SQU, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_NORTH)}, {MS(2)}},
    {"story_fly", TAB_STORY, L"Fly fast", K_HOLD, K_HOLD, 0, PB_CIR, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_SOUTH)}, {KB(VK_LCONTROL)}},
    {"story_next", TAB_STORY, L"Next dialogue / Accept", K_HOLD, K_HOLD, 0, PB_CRO, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_SOUTH)}, {MS(1), KB(VK_RETURN)}},
    {"story_skip", TAB_STORY, L"Skip dialogues", K_HOLD, K_HOLD, 0, PB_START, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_START)}, {KB(VK_TAB)}},
    {"story_map", TAB_STORY, L"Map / previous page", K_HOLD, K_HOLD, 0, PB_L, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_L1), PD(P_R2)}, {KB('M')}},
    {"story_back", TAB_STORY, L"Back", K_HOLD, K_HOLD, 0, PB_CIR, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_EAST)}, {KB(VK_ESCAPE)}},
    {"story_menu_up", TAB_STORY, L"Menu up", K_HOLD, K_HOLD, 0, PB_UP, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_DUP)}, {KB(VK_UP)}},
    {"story_menu_down", TAB_STORY, L"Menu down", K_HOLD, K_HOLD, 0, PB_DOWN, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_DDOWN)}, {KB(VK_DOWN)}},
    {"story_partner_left", TAB_STORY, L"Partner mode left", K_HOLD, K_HOLD, 0, PB_LEFT, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_DLEFT)}, {MS(7)}},
    {"story_partner_right", TAB_STORY, L"Partner mode right", K_HOLD, K_HOLD, 0, PB_RIGHT, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_DRIGHT)}, {MS(6)}},
    {"story_page_right", TAB_STORY, L"Next page", K_HOLD, K_HOLD, 0, PB_R, 0, 0, 0, false, -1, -1, -1, -1, {PD(P_R1)}, {0}},
};
#undef KB
#undef MS
#undef PD

struct Config {
    int rumble;
    uint32_t pad[N_ACTIONS][N_BINDS], kb[N_ACTIONS][N_BINDS];
};
static Config cfg;
static bool icons_title();

static bool pad_editable(int a) { return ACT[a].k_pad != K_NONE && ACT[a].from_pad < 0; }
static bool kb_editable(int a) { return ACT[a].k_kb != K_NONE && ACT[a].from_kb < 0; }

static void cfg_default_tab(Config& c, int tab) {
    for (int a = 0; a < N_ACTIONS; a++) {
        if (tab >= 0 && ACT[a].tab != tab) continue;
        memcpy(c.pad[a], ACT[a].pad_def, sizeof(c.pad[a]));
        memcpy(c.kb[a], ACT[a].kb_def, sizeof(c.kb[a]));
    }
}

static const uint32_t* binds_of(int a, bool pad) {
    int f = pad ? ACT[a].from_pad : ACT[a].from_kb;
    if (f >= 0) a = f;
    return pad ? cfg.pad[a] : cfg.kb[a];
}

static const char* cfg_path() {
    const char* e = getenv("TTT_CONTROLS");
    return e && *e ? e : "controls.cfg";
}

static void cfg_save() {
    FILE* f = fopen(cfg_path(), "w");
    if (!f) { printf("input: no puedo escribir %s\n", cfg_path()); return; }
    fprintf(f, "version 5\nrumble %d\n", cfg.rumble);
    for (int a = 0; a < N_ACTIONS; a++)
        for (int d = 0; d < 2; d++) {
            if (d == 0 ? !pad_editable(a) : !kb_editable(a)) continue;
            const uint32_t* l = d == 0 ? cfg.pad[a] : cfg.kb[a];
            fprintf(f, "%s %s", d == 0 ? "pad" : "key", ACT[a].id);
            for (int k = 0; k < N_BINDS; k++) if (l[k]) fprintf(f, " %08X", l[k]);
            fprintf(f, "\n");
        }
    fclose(f);
    printf("input: configuracion guardada en %s\n", cfg_path());
}

static void cfg_cargar() {
    memset(&cfg, 0, sizeof(cfg));
    cfg.rumble = 100;
    cfg_default_tab(cfg, -1);
    FILE* f = fopen(cfg_path(), "r");
    if (!f) return;
    char line[512];
    int ver = 1;
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
        int p;
        char kind[8], id[64];
        if (sscanf(line, "version %d", &p) == 1) ver = p;
        else if ((sscanf(line, "rumble %d", &p) == 1 || sscanf(line, "vibracion %d", &p) == 1) && p >= 0 && p <= 100) cfg.rumble = p;
        else if (sscanf(line, "%7s %63s", kind, id) == 2 && (!strcmp(kind, "pad") || !strcmp(kind, "key"))) {
            int a = -1;
            for (int i = 0; i < N_ACTIONS; i++) if (!strcmp(ACT[i].id, id)) a = i;
            if (a < 0) continue;
            uint32_t* l = kind[0] == 'p' ? cfg.pad[a] : cfg.kb[a];
            memset(l, 0, sizeof(uint32_t) * N_BINDS);
            char* s = line + strlen(kind) + 1 + strlen(id);
            for (int k = 0; k < N_BINDS; k++) {
                char* end;
                unsigned long v = strtoul(s, &end, 16);
                if (end == s) break;
                l[k] = (uint32_t)v; s = end;
            }
        }
    }
    fclose(f);
    // version 3 changed these defaults: an older file takes them
    if (ver == 2)
        for (const char* id : {"menu_back", "story_ability", "story_fly", "story_map"})
            for (int a = 0; a < N_ACTIONS; a++)
                if (!strcmp(ACT[a].id, id)) { memcpy(cfg.pad[a], ACT[a].pad_def, sizeof(cfg.pad[a])); memcpy(cfg.kb[a], ACT[a].kb_def, sizeof(cfg.kb[a])); }
    // version 4 added Space to Accept in menus
    if (ver >= 2 && ver < 4) {
        uint32_t* l = cfg.kb[A_M_ACCEPT];
        int k = 0, has = 0;
        for (; k < N_BINDS && l[k]; k++) if (l[k] == C_KEY(VK_SPACE)) has = 1;
        if (!has && k < N_BINDS) l[k] = C_KEY(VK_SPACE);
    }
    // version 5: the controller Start is only Start in menus (it also pressed Cross: the select took the character instead of skipping)
    if (ver >= 2 && ver < 5) {
        uint32_t* a = cfg.pad[A_M_ACCSTART]; uint32_t* st = cfg.pad[A_M_START];
        int moved = 0;
        for (int k = 0; k < N_BINDS; k++) if (a[k] == C_PAD(P_START)) { for (int j = k; j + 1 < N_BINDS; j++) a[j] = a[j + 1]; a[N_BINDS - 1] = 0; moved = 1; break; }
        if (moved) {
            int k = 0, has = 0;
            for (; k < N_BINDS && st[k]; k++) if (st[k] == C_PAD(P_START)) has = 1;
            if (!has && k < N_BINDS) st[k] = C_PAD(P_START);
            printf("input: Start del mando pasa a ser solo Start en menus\n");
        }
    }
    printf("input: configuracion leida de %s%s\n", cfg_path(), ver < 2 ? " (formato anterior: acciones por defecto)" : "");
}

// instances on one PC: each claims a number; with a pad per instance every one reads its own, even unfocused
static int inst_id = -1;
static volatile bool pad_live = false;
static volatile int pad_src = 0;              // 1 XInput, 2 DirectInput
static int pad_family = 1;                    // 1 Xbox, 2 PlayStation, 3 generic
static int input_mode = 0;                    // last device used: 0 keyboard and mouse, 1 controller
static volatile LONG pad_press_n = 0;         // controller presses, for the cursor hide

static int inst_claim() {
    if (inst_id >= 0) return inst_id;
    for (int i = 0; i < 4 && inst_id < 0; i++) {
        char n[48];
        snprintf(n, sizeof(n), "Local\\TTTRecomp_instance_%d", i);
        HANDLE h = CreateMutexA(nullptr, FALSE, n);
        if (h && GetLastError() != ERROR_ALREADY_EXISTS) inst_id = i;
        else if (h) CloseHandle(h);
    }
    if (inst_id < 0) inst_id = 0;
    printf("input: instancia %d\n", inst_id);
    return inst_id;
}

static int inst_count() {
    static unsigned next = 0;
    static int n = 1;
    unsigned now = GetTickCount();
    if (now < next) return n;
    next = now + 1000;
    n = 1;
    for (int i = 0; i < 4; i++) {
        char nm[48];
        snprintf(nm, sizeof(nm), "Local\\TTTRecomp_instance_%d", i);
        HANDLE h = OpenMutexA(SYNCHRONIZE, FALSE, nm);
        if (h) { n = i + 1; CloseHandle(h); }
    }
    return n;
}

// pad slot of this instance among n connected pads; -1 = shared (first pad, only while focused)
static int pad_slot(int n) {
    if (!patch_pad_background.val) return -1;
    int i = inst_claim();
    return n >= inst_count() && i < n ? i : -1;
}

// XInput
struct XiState { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; };
static DWORD (WINAPI* xi_get)(DWORD, XiState*) = nullptr;
static bool xi_loaded = false;
static XiState xi_state;
static int xi_used = -1;
static volatile LONG xi_present = 0;          // bit per connected slot, kept by the device thread

static void xi_init() {
    if (xi_loaded) return;
    xi_loaded = true;
    static const char* dll[] = {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"};
    for (const char* d : dll) {
        HMODULE m = LoadLibraryA(d);
        if (!m) continue;
        xi_get = (DWORD(WINAPI*)(DWORD, XiState*))GetProcAddress(m, "XInputGetState");
        if (xi_get) { printf("input: xinput cargado de %s\n", d); return; }
    }
    printf("input: xinput no disponible\n");
}

// DirectInput (pads that are not XInput)
struct DiDevice { GUID inst, prod; wchar_t name[128]; wchar_t path[260]; };
static std::vector<DiDevice> di_list;
static IDirectInput8W* di = nullptr;
static IDirectInputDevice8W* di_dev = nullptr;
static int di_opened = -1;
static DIJOYSTATE di_state;
static wchar_t di_path[260];
static volatile bool input_devices_changed = true;
static CRITICAL_SECTION input_cs;

static DIOBJECTDATAFORMAT di_objects[44];
static DIDATAFORMAT di_format;

static void di_prepare_format() {
    static const GUID* axes[6] = {&GUID_XAxis, &GUID_YAxis, &GUID_ZAxis, &GUID_RxAxis, &GUID_RyAxis, &GUID_RzAxis};
    int n = 0;
    for (int i = 0; i < 6; i++) di_objects[n++] = {axes[i], (DWORD)(i * 4), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, 0};
    for (int i = 0; i < 2; i++) di_objects[n++] = {&GUID_Slider, (DWORD)(24 + i * 4), DIDFT_OPTIONAL | DIDFT_AXIS | DIDFT_ANYINSTANCE, 0};
    for (int i = 0; i < 4; i++) di_objects[n++] = {&GUID_POV, (DWORD)(32 + i * 4), DIDFT_OPTIONAL | DIDFT_POV | DIDFT_ANYINSTANCE, 0};
    for (int i = 0; i < 32; i++) di_objects[n++] = {nullptr, (DWORD)(48 + i), DIDFT_OPTIONAL | DIDFT_BUTTON | DIDFT_ANYINSTANCE, 0};
    di_format = {sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_ABSAXIS, sizeof(DIJOYSTATE), (DWORD)n, di_objects};
}

static BOOL CALLBACK di_enum(LPCDIDEVICEINSTANCEW d, LPVOID ctx) {
    DiDevice x;
    memset(&x, 0, sizeof(x));
    x.inst = d->guidInstance; x.prod = d->guidProduct;
    wcsncpy(x.name, d->tszProductName, 127); x.name[127] = 0;
    ((std::vector<DiDevice>*)ctx)->push_back(x);
    return DIENUM_CONTINUE;
}

static bool di_sony() { return di_opened >= 0 && di_opened < (int)di_list.size() && (di_list[di_opened].prod.Data1 & 0xFFFF) == 0x054C; }
static uint16_t di_pid() { return di_sony() ? (uint16_t)((di_list[di_opened].prod.Data1 >> 16) & 0xFFFF) : 0; }
static bool hid_read();
static HANDLE hid_dev = INVALID_HANDLE_VALUE;
static unsigned hid_retry_ms = 0;
static void hid_open(uint16_t pid);
static void hid_close();
static DIJOYSTATE hid_state;

static bool di_path_of(IDirectInputDevice8W* d, wchar_t* out) {
    DIPROPGUIDANDPATH gp;
    gp.diph.dwSize = sizeof(DIPROPGUIDANDPATH); gp.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    gp.diph.dwHow = DIPH_DEVICE; gp.diph.dwObj = 0;
    out[0] = 0;
    if (FAILED(d->GetProperty(DIPROP_GUIDANDPATH, &gp.diph))) return false;
    wcsncpy(out, gp.wszPath, 259); out[259] = 0;
    return true;
}

static void di_close() {
    if (di_dev) { di_dev->Unacquire(); di_dev->Release(); di_dev = nullptr; }
    di_opened = -1;
}

// XInput pads also appear in DirectInput: their path carries "IG_" and they are left out.
// Runs on the device thread; the open device stays open when it is still attached
static void di_enumerate() {
    static int n_before = -1;
    if (!di) {
        HMODULE m = LoadLibraryA("dinput8.dll");
        auto create = m ? (HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN))GetProcAddress(m, "DirectInput8Create") : nullptr;
        IDirectInput8W* d8 = nullptr;
        if (!create || FAILED(create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (LPVOID*)&d8, nullptr))) {
            printf("input: directinput no disponible\n");
            return;
        }
        di_prepare_format();
        di = d8;
    }
    std::vector<DiDevice> list;
    di->EnumDevices(DI8DEVCLASS_GAMECTRL, di_enum, &list, DIEDFL_ATTACHEDONLY);
    for (size_t i = 0; i < list.size();) {
        IDirectInputDevice8W* d = nullptr;
        bool xinput = false;
        if (SUCCEEDED(di->CreateDevice(list[i].inst, &d, nullptr))) {
            if (di_path_of(d, list[i].path)) {
                wchar_t low[260];
                for (int k = 0; k < 260; k++) low[k] = towlower(list[i].path[k]);
                xinput = wcsstr(low, L"ig_") != nullptr;
            }
            d->Release();
        }
        if (xinput) list.erase(list.begin() + i);
        else i++;
    }
    auto rank = [](const DiDevice& x) { uint32_t v = x.prod.Data1 & 0xFFFF, p = (x.prod.Data1 >> 16) & 0xFFFF; return v == 0x054C ? 0 : v == 0x1234 && p == 0xBEAD ? 2 : 1; };
    std::stable_sort(list.begin(), list.end(), [&](const DiDevice& a, const DiDevice& b) { return rank(a) < rank(b); });   // Sony pads first, vJoy (virtual) last: the port reads one DirectInput pad
    EnterCriticalSection(&input_cs);
    int keep = -1;
    if (di_opened >= 0 && di_opened < (int)di_list.size())
        for (int i = 0; i < (int)list.size(); i++) if (list[i].inst == di_list[di_opened].inst) keep = i;
    if (keep < 0) { di_close(); hid_close(); }
    di_list = list;
    di_opened = keep;
    LeaveCriticalSection(&input_cs);
    if ((int)list.size() == n_before) return;
    n_before = (int)list.size();
    printf("input: directinput, %d mandos\n", n_before);
    for (auto& x : list) {
        char text[256];
        WideCharToMultiByte(CP_UTF8, 0, x.name, -1, text, sizeof(text), nullptr, nullptr);
        printf("input:   %s (vid %04X pid %04X)\n", text, (unsigned)(x.prod.Data1 & 0xFFFF), (unsigned)((x.prod.Data1 >> 16) & 0xFFFF));
    }
}

// device thread: XInput presence (an empty slot costs milliseconds per query) and DirectInput changes, never on the game thread
static DWORD WINAPI input_dev_thread(LPVOID) {
    for (;;) {
        if (xi_get) {
            LONG bits = 0;
            XiState e;
            for (int i = 0; i < 4; i++) if (xi_get((DWORD)i, &e) == 0) bits |= 1 << i;
            InterlockedExchange(&xi_present, bits);
        }
        if (input_devices_changed) { input_devices_changed = false; di_enumerate(); }
        Sleep(500);
    }
    return 0;
}

static void di_open(int chosen) {
    if (chosen == di_opened && di_dev) return;
    di_close();
    if (!di || chosen < 0 || chosen >= (int)di_list.size()) return;
    if (FAILED(di->CreateDevice(di_list[chosen].inst, &di_dev, nullptr))) { di_dev = nullptr; return; }
    di_dev->SetDataFormat(&di_format);
    di_dev->SetCooperativeLevel(ventana, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
    DIPROPRANGE r;
    r.diph.dwSize = sizeof(DIPROPRANGE); r.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    r.diph.dwHow = DIPH_DEVICE; r.diph.dwObj = 0;
    r.lMin = -1000; r.lMax = 1000;
    di_dev->SetProperty(DIPROP_RANGE, &r.diph);
    wcsncpy(di_path, di_list[chosen].path, 259);
    di_dev->Acquire();
    di_opened = chosen;
    if (di_sony()) hid_open(di_pid());
    else hid_close();
}

static bool di_read() {
    if (!di_dev) return false;
    if (di_sony() && hid_dev == INVALID_HANDLE_VALUE && GetTickCount() >= hid_retry_ms) { hid_retry_ms = GetTickCount() + 3000; hid_open(di_pid()); }
    if (di_sony() && hid_read()) { di_state = hid_state; return true; }
    di_dev->Poll();
    HRESULT h = di_dev->GetDeviceState(sizeof(DIJOYSTATE), &di_state);
    if (h == DIERR_INPUTLOST || h == DIERR_NOTACQUIRED) { di_dev->Acquire(); h = di_dev->GetDeviceState(sizeof(DIJOYSTATE), &di_state); }
    if (FAILED(h)) { di_close(); return false; }
    return true;
}

// mouse: the wheel arrives by message and is held 90 ms so the game sees it
static unsigned wheel_until[2] = {0, 0};
static volatile LONG wheel_acc = 0;
static void input_wheel(int delta) { wheel_until[delta > 0 ? 0 : 1] = GetTickCount() + 90; InterlockedAdd(&wheel_acc, delta > 0 ? 1 : -1); }

static bool cursor_in(HWND h) {
    POINT p;
    RECT r;
    if (!GetCursorPos(&p) || !ScreenToClient(h, &p) || !GetClientRect(h, &r)) return false;
    return p.x >= r.left && p.y >= r.top && p.x < r.right && p.y < r.bottom;
}

static float deadzone(float v) {
    const float dz = 0.25f;
    return v <= dz ? 0.0f : (v >= 1.0f ? 1.0f : (v - dz) / (1.0f - dz));
}

// controller state in logical codes, whatever the device
static float padv_main[N_PADCODES];
static float* padv = padv_main;

static void pad_axes(float lx, float ly, float rx, float ry) {
    padv[P_LSU] = deadzone(ly); padv[P_LSD] = deadzone(-ly); padv[P_LSL] = deadzone(-lx); padv[P_LSR] = deadzone(lx);
    padv[P_RSU] = deadzone(ry); padv[P_RSD] = deadzone(-ry); padv[P_RSL] = deadzone(-rx); padv[P_RSR] = deadzone(rx);
}

static void pad_from_xi(const XiState& s) {
    static const int map[16] = {P_DUP, P_DDOWN, P_DLEFT, P_DRIGHT, P_START, P_SELECT, P_L3, P_R3, P_L1, P_R1, -1, -1, P_SOUTH, P_EAST, P_WEST, P_NORTH};
    for (int b = 0; b < 16; b++) if (map[b] >= 0) padv[map[b]] = (s.buttons >> b) & 1 ? 1.0f : 0.0f;
    padv[P_L2] = s.lt <= 30 ? 0.0f : (s.lt - 30) / 225.0f;
    padv[P_R2] = s.rt <= 30 ? 0.0f : (s.rt - 30) / 225.0f;
    pad_axes(s.lx / 32767.0f, s.ly / 32767.0f, s.rx / 32767.0f, s.ry / 32767.0f);
}

// DirectInput in DualShock order (HID reads are decoded to the same order)
static void pad_from_di(const DIJOYSTATE& s) {
    static const int map[12] = {P_WEST, P_SOUTH, P_EAST, P_NORTH, P_L1, P_R1, P_L2, P_R2, P_SELECT, P_START, P_L3, P_R3};
    for (int b = 0; b < 12; b++) padv[map[b]] = (s.rgbButtons[b] & 0x80) ? 1.0f : 0.0f;
    DWORD a = s.rgdwPOV[0];
    bool c = (a & 0xFFFF) != 0xFFFF;
    padv[P_DUP] = c && (a > 27000 || a < 9000); padv[P_DDOWN] = c && a > 9000 && a < 27000;
    padv[P_DLEFT] = c && a > 18000 && a < 36000; padv[P_DRIGHT] = c && a > 0 && a < 18000;
    pad_axes(s.lX / 1000.0f, -s.lY / 1000.0f, s.lZ / 1000.0f, -s.lRz / 1000.0f);
}

// chooses and reads this instance's controller: XInput pads first, then DirectInput
static void pad_update(bool focused) {
    memset(padv_main, 0, sizeof(padv_main));
    int xi_conn[4], xi_nconn = 0;
    LONG bits = xi_present;
    for (int i = 0; i < 4; i++) if (bits >> i & 1) xi_conn[xi_nconn++] = i;
    int total = xi_nconn + (int)di_list.size();
    int k = pad_slot(total);
    if (k < 0) k = focused && total ? 0 : -1;
    bool ok = false;
    int src = 0, used = -1;
    if (k >= 0 && k < xi_nconn) {
        if (xi_get && xi_get((DWORD)xi_conn[k], &xi_state) == 0) { ok = true; used = xi_conn[k]; src = 1; pad_from_xi(xi_state); pad_family = 1; }
        else InterlockedAnd(&xi_present, ~(1L << xi_conn[k]));
    } else if (k >= 0) {
        di_open(k - xi_nconn);
        if (di_read()) { ok = true; src = 2; pad_from_di(di_state); pad_family = di_sony() ? 2 : 3; }
    }
    xi_used = used;
    pad_src = src;
    pad_live = ok;
}

static void pad_update_locked(bool focused) { EnterCriticalSection(&input_cs); pad_update(focused); LeaveCriticalSection(&input_cs); }

static float code_strength(uint32_t code, bool focused, bool mouse_ok) {
    uint32_t t = code >> 24, v = code & 0xFFFFFF;
    if (t == 5) return v < N_PADCODES ? padv[v] : 0.0f;
    if (!focused) return 0.0f;
    if (t == 1) return (GetAsyncKeyState((int)v) & 0x8000) ? 1.0f : 0.0f;
    if (t == 2) {
        if (v == 6 || v == 7) return GetTickCount() < wheel_until[v - 6] ? 1.0f : 0.0f;
        if (!mouse_ok) return 0.0f;
        static const int vk[6] = {0, VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
        if (v >= 1 && v <= 5) return (GetAsyncKeyState(vk[v]) & 0x8000) ? 1.0f : 0.0f;
    }
    return 0.0f;
}

// action engine: holds, sticks, taps, double taps and timed sequences (50 ms per step, inside the game's 7-tick windows)
struct Macro { uint32_t base, out, off; int dir, mk; bool center, hold; int src, dev; double t0; };
static const double STEP_MS = 40.0;
struct Engine {
    Macro mac, mac_next; bool mac_on, mac_pending;
    float act_v[N_ACTIONS][2]; bool act_prev[N_ACTIONS][2]; double act_last_press[N_ACTIONS][2]; bool relmod_used[N_ACTIONS];
    int clash_idx, clash_base, clash_k; double clash_t0; bool clash_prev;
    bool charge_eaten[2], l_out_prev, c_out_prev; double l_rise_ms, c_rise_ms;
    uint32_t prompt_serial_done; int prompt_pos;
};
static Engine eng_st[4];
static Engine* eng = &eng_st[0];
static double eng_ms() {
    static LARGE_INTEGER f = {};
    LARGE_INTEGER t;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)f.QuadPart;
}

static void macro_raw(const Macro& m) {
    if (!eng->mac_on) { eng->mac = m; eng->mac_on = true; }
    else { eng->mac_next = m; eng->mac_pending = true; }
}

static void macro_start(int ai, int dev, int dir, bool hold) {
    const Action& a = ACT[ai];
    Macro m = {a.base, a.out, a.off, dir, a.mk, a.center, hold, ai, dev, eng_ms()};
    if (input_log_on()) {
        if (ai == A_B_GRAB) printf("input: agarre, distancia al rival %.2f\n", psp_input_grab_dist());
        else printf("input: secuencia %s dir %d\n", a.id, dir);
    }
    if (!eng->mac_on) { eng->mac = m; eng->mac_on = true; }
    else { eng->mac_next = m; eng->mac_pending = true; }
}

static int stick_dir(bool right) {
    float best = 0.5f;
    int d = D_NONE;
    const int codes[4] = {right ? P_RSU : P_LSU, right ? P_RSD : P_LSD, right ? P_RSL : P_LSL, right ? P_RSR : P_LSR};
    for (int i = 0; i < 4; i++) if (padv[codes[i]] > best) { best = padv[codes[i]]; d = i + 1; }
    return d;
}

static float binds_strength(const uint32_t* l, bool pad, bool focused, bool mouse_ok, const std::vector<uint32_t>* consumed) {
    float m = 0.0f;
    for (int k = 0; k < N_BINDS; k++) {
        uint32_t c = l[k];
        if (!c || ((c >> 24) == 5) != pad) continue;
        if (consumed) { bool skip = false; for (uint32_t x : *consumed) if (x == c) skip = true; if (skip) continue; }
        float v = code_strength(c, focused, mouse_ok);
        if (v > m) m = v;
    }
    return m;
}

static bool mod_held(int mod, int dev, bool focused, bool mouse_ok) {
    return mod >= 0 && binds_strength(binds_of(mod, dev == 0), dev == 0, focused, mouse_ok, nullptr) >= 0.5f;
}

static int ctx_last = -1;

// beam clash: the struggle wants Circle, Cross, Square, Triangle in turn, like rolling the thumb. Only ki blast
// drives it: each press sends the next TTT_CLASH_STEPS buttons of the cycle (default 4, 40 ms each)
static const uint32_t CLASH_CYCLE[4] = {PB_CIR, PB_CRO, PB_SQU, PB_TRI};
static const uint32_t CLASH_ALL = PB_CIR | PB_CRO | PB_SQU | PB_TRI;
static int clash_steps() {
    static int n = -1;
    if (n < 0) { const char* e = getenv("TTT_CLASH_STEPS"); n = e ? atoi(e) : 4; if (n < 1) n = 1; if (n > 4) n = 4; }
    return n;
}

// command prompt of the local player (codes from 0x08999E6C as the HUD shows them): each charge press answers
// the next direction + Square (0x10..0x13 = up, down, left, right); other codes are not inputs
static bool prompt_follow() {
    int codes[12], dirs[12], nd = 0;
    uint32_t serial = 0;
    int n = psp_input_prompt_of((int)(eng - eng_st), codes, 12, &serial);
    for (int i = 0; i < n; i++) if (codes[i] >= 0x10 && codes[i] <= 0x13) dirs[nd++] = codes[i] - 0x10 + 1;
    if (!nd) return false;
    if (serial != eng->prompt_serial_done) { eng->prompt_serial_done = serial; eng->prompt_pos = 0; }
    Macro m = {0, PB_SQU, 0, dirs[eng->prompt_pos % nd], MK_TAP, true, false, A_B_CHARGE, 0, eng_ms()};
    eng->prompt_pos++;
    if (input_log_on()) printf("input: prompt respondido, dir %d\n", m.dir);
    static int pl_logs = 0;
    if (eng != eng_st && pl_logs < 10) { pl_logs++; printf("input: jugador %d responde el prompt, dir %d\n", (int)(eng - eng_st) + 1, m.dir); }
    macro_raw(m);
    return true;
}

static bool prompt_dirs() {
    int codes[12]; uint32_t serial = 0;
    int n = psp_input_prompt_of((int)(eng - eng_st), codes, 12, &serial);
    for (int i = 0; i < n && i < 12; i++) if (codes[i] >= 0x10 && codes[i] <= 0x13) return true;
    return false;
}

// transformation wheel (SZ, Graphics option): Z or R3 held opens it; Z+click/2/3/1 or the left stick pick left, up, right,
// down; the mouse wheel or L1/R1 change the page (transformations, fusions). Off: the original commands
extern volatile int tw_ready, tw_pages, tw_act[3][4], tw_shown;
int input_wheel_on() { const char* e = getenv("TTT_TR_WHEEL"); return e && *e && atoi(e) ? 1 : 0; }
static int tw_open_dev = -1, tw_page_cur = 0;
int input_wheel_open() { return tw_open_dev >= 0 ? 1 : 0; }
int input_wheel_page() { return tw_page_cur; }
int input_wheel_dev() { return tw_open_dev; }
uint32_t input_wheel_code(int pos) {   // control of a wheel position (left, up, right, down) for the device that opened it
    static const uint32_t K[4] = {C_MOUSE(1), C_KEY('2'), C_KEY('3'), C_KEY('1')}, P[4] = {C_PAD(P_LSL), C_PAD(P_LSU), C_PAD(P_LSR), C_PAD(P_LSD)};
    return pos < 0 || pos > 3 ? 0 : tw_open_dev ? K[pos] : P[pos];
}
static bool tw_skip(int a, int d, bool zk, bool r3) {
    if (a == A_B_TRANSF || (a >= A_S_FU1 && a <= A_S_DETR) || zk || r3) return true;   // wheel held: nothing else answers
    if (d == 1) return zk && (a == A_B_HIT || a == A_B_GRAB || a == A_B_PLEFT || a == A_B_PRIGHT);
    return r3 && (a == A_B_UP || a == A_B_DOWN || a == A_B_LEFT || a == A_B_RIGHT || a == A_B_TARGET || a == A_B_RISE || a == A_B_RISEF || a == A_B_FALL || a == A_B_FALLF);
}
static void tw_input(bool zk, bool r3, bool focused, bool mouse_ok, bool clash) {
    static bool kp[4], wp[2], lp[2];
    static int dp = D_NONE;
    const uint32_t KEYS[4] = {C_MOUSE(1), C_KEY('2'), C_KEY('3'), C_KEY('1')};
    bool k[4], w[2], l[2];
    for (int i = 0; i < 4; i++) k[i] = code_strength(KEYS[i], focused, mouse_ok) >= 0.5f;
    w[0] = code_strength(C_MOUSE(6), focused, mouse_ok) >= 0.5f; w[1] = code_strength(C_MOUSE(7), focused, mouse_ok) >= 0.5f;
    l[0] = padv[P_L1] >= 0.5f; l[1] = padv[P_R1] >= 0.5f;
    int dir = stick_dir(false);
    int dev = zk ? 1 : r3 ? 0 : -1;
    if (dev >= 0 && tw_open_dev < 0) {
        tw_page_cur = 0;
        for (int i = 0; i < 4; i++) kp[i] = k[i];
        wp[0] = w[0]; wp[1] = w[1]; lp[0] = l[0]; lp[1] = l[1]; dp = dir;
        printf("rueda: abierta (%s)\n", dev ? "teclado" : "mando");
    }
    if (dev < 0 && tw_open_dev >= 0) printf("rueda: cerrada\n");
    tw_open_dev = dev;
    if (dev < 0) return;
    int step = dev ? (w[0] && !wp[0] ? 1 : w[1] && !wp[1] ? -1 : 0) : (l[0] && !lp[0] ? 1 : l[1] && !lp[1] ? -1 : 0);
    if (step && tw_ready && tw_shown) {
        int g = tw_page_cur;
        for (int n = 0; n < 3; n++) { g = (g + step + 3) % 3; if (tw_pages & (1 << g)) break; }
        if (g != tw_page_cur) { tw_page_cur = g; printf("rueda: pagina %d\n", g); }
    }
    int pick = -1;
    if (dev) { for (int i = 0; i < 4; i++) if (k[i] && !kp[i]) pick = i; }
    else if (dir != dp && dir != D_NONE) pick = dir == D_LEFT ? 0 : dir == D_UP ? 1 : dir == D_RIGHT ? 2 : 3;
    for (int i = 0; i < 4; i++) kp[i] = k[i];
    wp[0] = w[0]; wp[1] = w[1]; lp[0] = l[0]; lp[1] = l[1]; dp = dir;
    if (pick < 0 || !tw_ready || !tw_shown || clash) return;
    int a = tw_act[tw_page_cur][pick];
    static const int TR[4] = {A_S_TR1, A_S_TR2, A_S_TR3, A_S_DETR};
    int act = a >= 0 && a <= 3 ? TR[a] : a >= 10 && a <= 12 ? A_S_FU1 + (a - 10) : -1;
    printf("rueda: pagina %d posicion %d -> %s\n", tw_page_cur, pick, act >= 0 ? ACT[act].id : "vacia");
    if (act >= 0) macro_start(act, dev, ACT[act].dir, ACT[act].mk == MK_TAP);
}

static void engine(int ctx, bool focused, bool mouse_ok, uint32_t* b, float* sx, float* sy) {
    std::vector<uint32_t> consumed[2];
    for (int a = 0; a < N_ACTIONS; a++) {
        if (TAB_CTX[ACT[a].tab] != ctx) continue;
        for (int d = 0; d < 2; d++) {
            int mod = d == 0 ? ACT[a].mod_pad : ACT[a].mod_kb;
            if (mod < 0 || (d == 0 ? ACT[a].k_pad : ACT[a].k_kb) == K_NONE || !mod_held(mod, d, focused, mouse_ok)) continue;
            const uint32_t* l = binds_of(a, d == 0);
            for (int k = 0; k < N_BINDS; k++) if (l[k]) consumed[d].push_back(l[k]);
        }
    }
    double now = eng_ms();
    uint32_t r = 0;
    float up = 0, down = 0, left = 0, right = 0;
    bool pad_charge = false, kb_active = false, pad_active = false, kb_combo_block = false;
    bool clash = ctx == CTX_BATTLE && psp_input_clash();
    if (clash && !eng->clash_prev) { eng->clash_idx = 0; eng->clash_k = 0; }
    eng->clash_prev = clash;
    bool wheel = ctx == CTX_BATTLE && eng == eng_st && input_wheel_on() && tw_ready;
    bool tw_zk = wheel && binds_strength(binds_of(A_B_TRANSF, false), false, focused, mouse_ok, nullptr) >= 0.5f;
    bool tw_r3 = wheel && !tw_zk && binds_strength(binds_of(A_B_TRANSF, true), true, focused, mouse_ok, nullptr) >= 0.5f;
    if (eng == eng_st) tw_input(tw_zk, tw_r3, focused, mouse_ok, clash);
    for (int a = 0; a < N_ACTIONS; a++) {
        const Action& A = ACT[a];
        if (TAB_CTX[A.tab] != ctx) { eng->act_prev[a][0] = eng->act_prev[a][1] = false; eng->act_v[a][0] = eng->act_v[a][1] = 0; continue; }
        for (int d = 0; d < 2; d++) {
            int kind = d == 0 ? A.k_pad : A.k_kb, mod = d == 0 ? A.mod_pad : A.mod_kb;
            if (kind == K_NONE || (wheel && tw_skip(a, d, tw_zk, tw_r3))) { eng->act_v[a][d] = 0; eng->act_prev[a][d] = false; continue; }
            bool modok = mod < 0 || mod_held(mod, d, focused, mouse_ok);
            float v = modok ? binds_strength(binds_of(a, d == 0), d == 0, focused, mouse_ok, mod < 0 ? &consumed[d] : nullptr) : 0.0f;
            eng->act_v[a][d] = v;
            bool on = v >= 0.5f, edge = on && !eng->act_prev[a][d], rel = !on && eng->act_prev[a][d];
            eng->act_prev[a][d] = on;
            if (on) { if (d == 0) pad_active = true; else kb_active = true; }
            if (edge && mod >= 0) eng->relmod_used[mod] = true;
            switch (kind) {
                case K_STICK:
                    if (A.dir == D_UP) up = v > up ? v : up;
                    else if (A.dir == D_DOWN) down = v > down ? v : down;
                    else if (A.dir == D_LEFT) left = v > left ? v : left;
                    else right = v > right ? v : right;
                    break;
                case K_HOLD:
                    if (a == A_B_BLAST && clash) {
                        if (edge) {
                            int st = eng->clash_k ? (int)((now - eng->clash_t0) / STEP_MS) : 0;
                            eng->clash_base = eng->clash_k && st < eng->clash_k ? (eng->clash_base + st + 1) & 3 : eng->clash_idx;
                            eng->clash_k = clash_steps();
                            eng->clash_idx = (eng->clash_base + eng->clash_k) & 3;
                            eng->clash_t0 = now;
                        }
                    } else if (v >= 0.25f) r |= A.out;
                    if (d == 1 && v >= 0.25f && (a == A_B_BLAST || a == A_B_GUARD || a == A_B_RISE || a == A_B_FALL)) kb_combo_block = true;
                    if (a == A_B_CHARGE && edge && ctx == CTX_BATTLE && prompt_follow()) eng->charge_eaten[d] = true;
                    if (a == A_B_CHARGE && !on) eng->charge_eaten[d] = false;
                    if (d == 0 && a == A_B_CHARGE && v >= 0.25f && !eng->charge_eaten[0]) pad_charge = true;
                    break;
                case K_TAP: if (edge) macro_start(a, d, A.dir, A.mk == MK_TAP); break;
                case K_TAPLS: if (edge) macro_start(a, d, stick_dir(false), false); break;
                case K_DOUBLE:
                    if (edge) {
                        if (now - eng->act_last_press[a][d] < 300.0) { macro_start(a, d, A.dir, false); eng->act_last_press[a][d] = 0; }
                        else eng->act_last_press[a][d] = now;
                    }
                    break;
                case K_TAPREL:
                    if (edge) eng->relmod_used[a] = false;
                    if (rel && !eng->relmod_used[a]) macro_start(a, d, A.dir, false);
                    break;
            }
        }
    }
    if (pad_active) pad_press_n = pad_press_n + 1;
    if (eng != eng_st) {}
    else if (pad_active) { if (input_mode != 1) printf("input: modo mando\n"); input_mode = 1; }
    else if (kb_active) { if (input_mode != 0) printf("input: modo teclado\n"); input_mode = 0; }
    float x = right - left, y = down - up;
    if (ctx == CTX_MENU) {
        if (up >= 0.5f) r |= PB_UP;
        if (down >= 0.5f) r |= PB_DOWN;
        if (left >= 0.5f) r |= PB_LEFT;
        if (right >= 0.5f) r |= PB_RIGHT;
    }
    // title screen and its demo battle only take Start: Accept also presses it there
    if (icons_title())
        for (int d = 0; d < 2; d++)
            if (binds_strength(binds_of(A_M_ACCEPT, d == 0), d == 0, focused, mouse_ok, nullptr) >= 0.5f) r |= PB_START;
    if (clash) {
        r &= ~CLASH_ALL;
        int st = eng->clash_k ? (int)((now - eng->clash_t0) / STEP_MS) : 0;
        if (eng->clash_k && st < eng->clash_k) r |= CLASH_CYCLE[(eng->clash_base + st) & 3];
        else eng->clash_k = 0;
    }
    // keyboard: charge held plus ki blast, guard or up/down must not become the L combos
    if (ctx == CTX_BATTLE && kb_combo_block && !pad_charge && eng->act_v[A_B_CHARGE][1] >= 0.5f) r &= ~PB_L;
    // charge used for a prompt follow-up: no L while it stays held (unless the other device charges)
    bool l_pad = eng->act_v[A_B_CHARGE][0] >= 0.25f && !eng->charge_eaten[0], l_kb = eng->act_v[A_B_CHARGE][1] >= 0.25f && !eng->charge_eaten[1];
    if (ctx == CTX_BATTLE && (eng->charge_eaten[0] || eng->charge_eaten[1]) && !l_pad && !l_kb) r &= ~PB_L;
    // chargeable attack follow-up is answered only with the charge button: stick + Square no longer reaches it
    if (ctx == CTX_BATTLE && x * x + y * y >= 0.25f && prompt_dirs()) r &= ~PB_SQU;
    int rd = pad_charge ? stick_dir(true) : D_NONE;
    if (eng->mac_on) {
        int step = (int)((now - eng->mac.t0) / STEP_MS);
        if (eng->mac.hold && step >= 2 && eng->act_prev[eng->mac.src][eng->mac.dev]) { step = 2; eng->mac.t0 = now - 2 * STEP_MS; }
        if (step >= 4) {
            eng->mac_on = false;
            if (eng->mac_pending) { eng->mac = eng->mac_next; eng->mac.t0 = now; eng->mac_on = true; eng->mac_pending = false; step = 0; }
        }
        if (eng->mac_on) {
            uint32_t o = eng->mac.base;
            if (eng->mac.mk == MK_TAP && (step == 1 || step == 2)) o |= eng->mac.out;
            if (eng->mac.mk == MK_TWICE && (step == 0 || step == 2)) o |= eng->mac.out;
            if (eng->mac.mk == MK_HOLD) o |= eng->mac.out;
            if (eng->mac.mk == MK_TWICE) r &= ~eng->mac.out;
            r = (r & ~eng->mac.off) | o;
            if (eng->mac.dir != D_NONE) rd = eng->mac.dir;
            else if (eng->mac.center) { x = y = 0; rd = D_NONE; }
        }
    }
    // a new L press within the double-tap window would become the emote: it waits (sequences are exempt)
    bool lnow = (r & PB_L) != 0;
    if (ctx == CTX_BATTLE && lnow && !eng->l_out_prev && !eng->mac_on) {
        if (now - eng->l_rise_ms < 280.0) { r &= ~PB_L; lnow = false; }
        else eng->l_rise_ms = now;
    } else if (lnow && !eng->l_out_prev) eng->l_rise_ms = now;
    eng->l_out_prev = lnow;
    // keyboard: the dash key tapped twice would be the game's grab (Cross twice); the grab has its own keys
    bool cnow = (r & PB_CRO) != 0, ckb = eng->act_v[A_B_DASH][1] >= 0.5f && eng->act_v[A_B_DASH][0] < 0.5f;
    if (ctx == CTX_BATTLE && cnow && !eng->c_out_prev && !eng->mac_on && ckb) {
        if (now - eng->c_rise_ms < 280.0) { r &= ~PB_CRO; cnow = false; }
        else eng->c_rise_ms = now;
    } else if (cnow && !eng->c_out_prev) eng->c_rise_ms = now;
    eng->c_out_prev = cnow;
    if (rd == D_UP) { x = 0; y = -1; }
    else if (rd == D_DOWN) { x = 0; y = 1; }
    else if (rd == D_LEFT) { x = -1; y = 0; }
    else if (rd == D_RIGHT) { x = 1; y = 0; }
    *b = r; *sx = x; *sy = y;
}

// keyboard special whose key combination is still held (chargeable specials fire on release), -1 if none
static int input_held_action(int k = 0) {
    const Engine& e = eng_st[k & 3];
    return e.mac_on && e.mac.hold && e.mac.dev == 1 && e.act_prev[e.mac.src][1] ? e.mac.src : -1;
}

// Start (battle pause) held on this instance's own device, for leaving a network battle
int psp_input_cam(int k) {
    if (k < 0 || k > 3) return 0;
    const Engine& e = eng_st[k];
    int r = 0;
    for (int d = 0; d < 2; d++) {
        if (e.act_v[A_S_CAML][d] >= 0.5f) r |= 1;
        if (e.act_v[A_S_CAMR][d] >= 0.5f) r |= 2;
        if (e.act_v[A_S_CAMD][d] >= 0.5f && e.act_v[A_B_CHARGE][d] < 0.25f) r |= 4;
    }
    return r;
}
int input_start_held() { const Engine& e = eng_st[0]; return e.act_v[A_B_PAUSE][0] >= 0.5f || e.act_v[A_B_PAUSE][1] >= 0.5f; }

// charge held by the player of this instance and not answering a prompt (charge list, item 17)
int input_charge_held() {
    const Engine& e = eng_st[0];
    return (e.act_v[A_B_CHARGE][0] >= 0.25f && !e.charge_eaten[0]) || (e.act_v[A_B_CHARGE][1] >= 0.25f && !e.charge_eaten[1]) ? 1 : 0;
}

// Shift, Ctrl and similar keys held or tapped as game controls open the sticky/filter/toggle keys prompts:
// their shortcuts are turned off while the game runs (only when the feature itself is off) and restored at exit
static STICKYKEYS sk_saved = {sizeof(STICKYKEYS), 0};
static TOGGLEKEYS tk_saved = {sizeof(TOGGLEKEYS), 0};
static FILTERKEYS fk_saved = {sizeof(FILTERKEYS), 0};
static bool shortcuts_saved = false;

static void input_shortcuts_restore() {
    if (!shortcuts_saved) return;
    SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &sk_saved, 0);
    SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tk_saved, 0);
    SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fk_saved, 0);
    shortcuts_saved = false;
}

static void input_shortcuts_off() {
    if (!patch_keys_shortcuts.val || shortcuts_saved) return;
    SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &sk_saved, 0);
    SystemParametersInfo(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tk_saved, 0);
    SystemParametersInfo(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &fk_saved, 0);
    shortcuts_saved = true;
    atexit(input_shortcuts_restore);
    STICKYKEYS sk = sk_saved;
    TOGGLEKEYS tk = tk_saved;
    FILTERKEYS fk = fk_saved;
    if (!(sk.dwFlags & SKF_STICKYKEYSON)) { sk.dwFlags &= ~(SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY); SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &sk, 0); }
    if (!(tk.dwFlags & TKF_TOGGLEKEYSON)) { tk.dwFlags &= ~(TKF_HOTKEYACTIVE | TKF_CONFIRMHOTKEY); SystemParametersInfo(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &tk, 0); }
    if (!(fk.dwFlags & FKF_FILTERKEYSON)) { fk.dwFlags &= ~(FKF_HOTKEYACTIVE | FKF_CONFIRMHOTKEY); SystemParametersInfo(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &fk, 0); }
    printf("input: atajos de teclas especiales de Windows desactivados mientras corre el juego\n");
}

static void input_init() {
    cfg_cargar();
    input_shortcuts_off();
    InitializeCriticalSection(&input_cs);
    inst_claim();
    xi_init();
    if (xi_get) {
        LONG bits = 0;
        XiState e;
        for (int i = 0; i < 4; i++) if (xi_get((DWORD)i, &e) == 0) bits |= 1 << i;
        xi_present = bits;
    }
    di_enumerate();
    input_devices_changed = false;
    HANDLE h = CreateThread(nullptr, 0, input_dev_thread, nullptr, 0, nullptr);
    if (h) { SetThreadPriority(h, THREAD_PRIORITY_BELOW_NORMAL); CloseHandle(h); }
}

static void icons_update(int ctx);

// local multiplayer (item 20): devices 0 keyboard and mouse, 1..4 XInput slots, 5 the DirectInput pad; the first
// device pressed becomes player 1, the next new one player 2 and so on; until player 1 claims one, he reads them all
enum { DEV_KB, DEV_XI0, DEV_DI = 5, N_DEVS };
static float dev_padv[N_DEVS][N_PADCODES];
static bool dev_ok[N_DEVS];
static int dev_player[N_DEVS] = {-1, -1, -1, -1, -1, -1};
static int dev_family[N_DEVS];
static int player_pads = 0;
static uint32_t loc_b[4];
static uint8_t loc_x[4] = {128, 128, 128, 128}, loc_y[4] = {128, 128, 128, 128};
static float padv_none[N_PADCODES];
// device of each local player for his button icons and rumble: mode 0 keyboard and mouse, 1 controller; pad family
static volatile int pl_mode[4] = {0, 0, 0, 0}, pl_family[4] = {1, 1, 1, 1};

// join mode (multiplayer > Local): up to 4 devices join by pressing in menus; players = devices that joined
static volatile int local_join = 0, local_manual = 0;

static int local_players() {
    if (local_join) return 4;
    int n = patch_local_players.val;
    return n < 1 ? 1 : n > 4 ? 4 : n;
}

static int local_active() { return local_join ? (player_pads < 1 ? 1 : player_pads) : local_players(); }

void psp_local_join(int on) {
    EnterCriticalSection(&input_cs);
    if (local_join != on) {
        for (int d = 0; d < N_DEVS; d++) dev_player[d] = -1;
        player_pads = 0;
        local_join = on;
        local_manual = 0;
        printf("input: modo unirse %s\n", on ? "activado (cada dispositivo se une al pulsar un boton en los menus)" : "desactivado");
    }
    LeaveCriticalSection(&input_cs);
}

static void local_devices_read(bool focused) {
    LONG bits = xi_present;
    for (int s = 0; s < 4; s++) {
        int d = DEV_XI0 + s;
        dev_ok[d] = false;
        memset(dev_padv[d], 0, sizeof(dev_padv[d]));
        XiState st;
        if (!(bits >> s & 1) || !xi_get || xi_get((DWORD)s, &st) != 0) continue;
        padv = dev_padv[d]; pad_from_xi(st);
        dev_ok[d] = true; dev_family[d] = 1;
    }
    dev_ok[DEV_DI] = false;
    memset(dev_padv[DEV_DI], 0, sizeof(dev_padv[DEV_DI]));
    if (!di_list.empty()) {
        di_open(0);
        if (di_read()) { padv = dev_padv[DEV_DI]; pad_from_di(di_state); dev_ok[DEV_DI] = true; dev_family[DEV_DI] = di_sony() ? 2 : 3; }
    }
    padv = padv_main;
    dev_ok[DEV_KB] = focused;
}

// two screens: L1+R1+L3 (Xbox LB+RB+LS) held on any controller
bool input_dual_combo() {
    for (int d = DEV_XI0; d < N_DEVS; d++)
        if (dev_ok[d] && dev_padv[d][P_L1] >= 0.5f && dev_padv[d][P_R1] >= 0.5f && dev_padv[d][P_L3] >= 0.5f) return true;
    return padv_main[P_L1] >= 0.5f && padv_main[P_R1] >= 0.5f && padv_main[P_L3] >= 0.5f;
}

static bool dev_pressed(int d, bool mouse_ok) {
    if (d == DEV_KB) {
        for (int vk = 8; vk < 256; vk++) {
            if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2) { if (!mouse_ok) continue; }
            else if (vk == VK_F1 || vk == VK_F12 || vk == VK_F11 || vk == VK_F10 || vk == VK_F9 || vk == VK_F8 || vk == VK_F7) continue;
            if (GetAsyncKeyState(vk) & 0x8000) return true;
        }
        return false;
    }
    for (int i = 0; i < N_PADCODES; i++) if (dev_padv[d][i] >= 0.5f) return true;
    return false;
}

static void local_claim(bool mouse_ok, int ctx) {
    if (local_join && (ctx != 0 || local_manual)) return;
    static const char* names[N_DEVS] = {"teclado y raton", "mando xinput 1", "mando xinput 2", "mando xinput 3", "mando xinput 4", "mando directinput"};
    for (int d = 0; d < N_DEVS; d++) {
        if (!dev_ok[d] || dev_player[d] >= 0 || player_pads >= local_players() || !dev_pressed(d, mouse_ok)) continue;
        dev_player[d] = player_pads++;
        printf("input: jugador %d usa %s\n", dev_player[d] + 1, names[d]);
    }
}

// player k's own pad (the first pad he claimed) and whether the keyboard is his
static int local_pad_dev(int k) {
    for (int d = DEV_XI0; d < N_DEVS; d++) if (dev_player[d] == k && dev_ok[d]) return d;
    return -1;
}

// local select screen (multiplayer > Local): the screen assigns devices to rows; player index = row until committed
static void local_recount() {
    player_pads = 0;
    for (int d = 0; d < N_DEVS; d++) if (dev_player[d] + 1 > player_pads) player_pads = dev_player[d] + 1;
}

void psp_local_manual(int on) { EnterCriticalSection(&input_cs); local_manual = on; LeaveCriticalSection(&input_cs); }

int psp_local_free_pressed() {
    int r = -1;
    EnterCriticalSection(&input_cs);
    for (int d = 0; d < N_DEVS && r < 0; d++) if (local_join && dev_ok[d] && dev_player[d] < 0 && dev_pressed(d, false)) r = d;
    LeaveCriticalSection(&input_cs);
    return r;
}

void psp_local_assign(int d, int k) {
    EnterCriticalSection(&input_cs);
    if (d >= 0 && d < N_DEVS) { dev_player[d] = k; local_recount(); loc_b[k & 3] = 0; }
    LeaveCriticalSection(&input_cs);
}

void psp_local_release(int k) {
    EnterCriticalSection(&input_cs);
    for (int d = 0; d < N_DEVS; d++) if (dev_player[d] == k) dev_player[d] = -1;
    local_recount();
    LeaveCriticalSection(&input_cs);
}

// map[k] = new index of player k (or -1 to drop him)
void psp_local_remap(const int* map) {
    EnterCriticalSection(&input_cs);
    for (int d = 0; d < N_DEVS; d++) if (dev_player[d] >= 0) dev_player[d] = map[dev_player[d]];
    local_recount();
    for (auto& e : eng_st) e.mac_on = e.mac_pending = false;
    for (int k = 0; k < 4; k++) loc_b[k] = 0;
    LeaveCriticalSection(&input_cs);
}

// device of player k (-1 none) and its family (0 keyboard, 1 Xbox, 2 PlayStation, 3 generic)
int psp_local_dev(int k, int* family) {
    int r = -1;
    EnterCriticalSection(&input_cs);
    for (int d = 0; d < N_DEVS && r < 0; d++) if (dev_player[d] == k) r = d;
    if (family) *family = r <= DEV_KB ? 0 : dev_family[r];
    LeaveCriticalSection(&input_cs);
    return r;
}

// Character Reference 3D viewer: right stick / W A S D orbit, L1 / R1 or the wheel zoom, d-pad / arrows (bits 1 up, 2 down, 4 left, 8 right),
// L2 / R2 or Q / E (bits 16, 32)
float psp_input_rotate() {   // 3D select: right stick X, Q left, E right
    EnterCriticalSection(&input_cs);
    bool f = ventana && GetForegroundWindow() == ventana;
    float v = padv_main[P_RSL] - padv_main[P_RSR];
    if (f && (GetAsyncKeyState('Q') & 0x8000)) v += 1.0f;
    if (f && (GetAsyncKeyState('E') & 0x8000)) v -= 1.0f;
    LeaveCriticalSection(&input_cs);
    return fabsf(v) < 0.15f ? 0.0f : v;
}
void psp_input_viewer(float* ax, float* ay, float* zoom, int* wheel, int* wasd, int* dirs) {
    EnterCriticalSection(&input_cs);
    bool f = ventana && GetForegroundWindow() == ventana;
    auto key = [&](int vk) { return f && (GetAsyncKeyState(vk) & 0x8000) != 0; };
    int k = (key('W') ? 1 : 0) | (key('S') ? 2 : 0) | (key('A') ? 4 : 0) | (key('D') ? 8 : 0);
    *ax = padv_main[P_RSL] - padv_main[P_RSR] + ((k & 8) ? 1.0f : 0.0f) - ((k & 4) ? 1.0f : 0.0f);
    *ay = padv_main[P_RSD] - padv_main[P_RSU] + ((k & 1) ? 1.0f : 0.0f) - ((k & 2) ? 1.0f : 0.0f);   // pad Y inverted (his request), keys as they were
    *zoom = padv_main[P_R1] - padv_main[P_L1];
    *wheel = (int)InterlockedExchange(&wheel_acc, 0);
    *wasd = k;
    *dirs = (padv_main[P_DUP] > 0.5f || key(VK_UP) ? 1 : 0) | (padv_main[P_DDOWN] > 0.5f || key(VK_DOWN) ? 2 : 0) |
            (padv_main[P_DLEFT] > 0.5f || key(VK_LEFT) ? 4 : 0) | (padv_main[P_DRIGHT] > 0.5f || key(VK_RIGHT) ? 8 : 0) |
            (padv_main[P_L2] > 0.5f || key('Q') ? 16 : 0) | (padv_main[P_R2] > 0.5f || key('E') ? 32 : 0);
    LeaveCriticalSection(&input_cs);
}

bool psp_input_pad() { return pl_mode[0] == 1; }

int psp_input_local(int k, uint32_t* b, uint8_t* x, uint8_t* y) {
    int n = local_active();
    if (n < 2 || k < 1 || k >= n) return n;
    *b = loc_b[k]; *x = loc_x[k]; *y = loc_y[k];
    return n;
}

static uint8_t stick_byte(float v) { int i = 128 + (int)(v * 127.0f); return (uint8_t)(i < 0 ? 0 : i > 255 ? 255 : i); }

// keyboard and mouse only with focus; controllers also without it (patch pad_background)
static void input_read(uint32_t* b, uint8_t* sx, uint8_t* sy, bool focused) {
    EnterCriticalSection(&input_cs);
    pad_update(focused);
    int ctx = psp_input_context();
    if (ctx != ctx_last) {
        static const char* names[3] = {"menus", "combate", "historia"};
        printf("input: contexto %s\n", names[ctx]);
        ctx_last = ctx;
        for (auto& e : eng_st) e.mac_on = e.mac_pending = false;
    }
    // TTT_KEYS_BG=1 (tests with several instances): keyboard and mouse also without focus, every instance gets them
    static int keys_bg = -1;
    if (keys_bg < 0) {
        const char* e = getenv("TTT_KEYS_BG");
        keys_bg = e && *e == '1';
        if (keys_bg) printf("input: teclado y raton sin foco (TTT_KEYS_BG)\n");
    }
    bool kf = focused || keys_bg;
    float x, y;
    bool mouse = kf && (keys_bg || cursor_in(ventana));
    // the click that gives the window the focus is not a game input: mouse buttons ignored until released (TTT_FOCUS_CLICK=0 restores)
    static bool was_kf = true, mouse_hold = false;
    static int fc_on = -1;
    if (fc_on < 0) { const char* e = getenv("TTT_FOCUS_CLICK"); fc_on = !(e && *e == '0'); }
    bool any_btn = ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000) != 0;
    if (fc_on && kf && !was_kf && any_btn) { mouse_hold = true; static int logs = 0; if (logs++ < 20) printf("input: clic de foco ignorado\n"); }
    if (mouse_hold && !any_btn) mouse_hold = false;
    was_kf = kf;
    if (mouse_hold) mouse = false;
    int n = local_players();
    if (n < 2) { engine(ctx, kf, mouse, b, &x, &y); pl_mode[0] = input_mode; pl_family[0] = pad_family; }
    else {
        local_devices_read(kf);
        local_claim(mouse, ctx);
        for (int k = 0; k < n; k++) {
            eng = &eng_st[k];
            bool kb = dev_player[DEV_KB] == k || (k == 0 && player_pads == 0);
            int pd = local_pad_dev(k);
            padv = pd >= 0 ? dev_padv[pd] : k == 0 && player_pads == 0 ? padv_main : padv_none;
            if (k == 0 && pd >= 0) pad_family = dev_family[pd];
            if (k == 0 && player_pads == 0) { pl_mode[0] = input_mode; pl_family[0] = pad_family; }
            else {
                if (pd >= 0) pl_family[k] = dev_family[pd];
                if (pd >= 0 && !kb) pl_mode[k] = 1;
                else if (kb && pd < 0) pl_mode[k] = 0;
                else if (pd >= 0 && dev_pressed(pd, false)) pl_mode[k] = 1;
                else if (kb && dev_pressed(DEV_KB, mouse)) pl_mode[k] = 0;
            }
            float lx, ly;
            uint32_t lb;
            engine(ctx, kf && kb, mouse && kb, &lb, &lx, &ly);
            if (k == 0) { *b = lb; x = lx; y = ly; }
            else { loc_b[k] = lb; loc_x[k] = stick_byte(lx); loc_y[k] = stick_byte(ly); }
        }
        eng = &eng_st[0];
        padv = padv_main;
    }
    icons_update(ctx);
    LeaveCriticalSection(&input_cs);
    int ix = 128 + (int)(x * 127.0f), iy = 128 + (int)(y * 127.0f);
    *sx = (uint8_t)(ix < 0 ? 0 : ix > 255 ? 255 : ix);
    *sy = (uint8_t)(iy < 0 ? 0 : iy > 255 ? 255 : iy);
}

static void code_name(uint32_t code, wchar_t* s, int n, bool brief = false) {
    uint32_t t = code >> 24, v = code & 0xFFFFFF;
    s[0] = 0;
    if (t == 1) {
        switch (v) {
            case VK_UP: wcsncpy(s, L"Up arrow", n); return;
            case VK_DOWN: wcsncpy(s, L"Down arrow", n); return;
            case VK_LEFT: wcsncpy(s, L"Left arrow", n); return;
            case VK_RIGHT: wcsncpy(s, L"Right arrow", n); return;
            case VK_LSHIFT: wcsncpy(s, brief ? L"Shift" : L"Left Shift", n); return;
            case VK_RSHIFT: wcsncpy(s, L"Right Shift", n); return;
            case VK_LCONTROL: wcsncpy(s, brief ? L"Ctrl" : L"Left Ctrl", n); return;
            case VK_RCONTROL: wcsncpy(s, L"Right Ctrl", n); return;
            case VK_LMENU: wcsncpy(s, L"Left Alt", n); return;
            case VK_RMENU: wcsncpy(s, L"Right Alt", n); return;
            case VK_RETURN: wcsncpy(s, L"Enter", n); return;
            case VK_SPACE: wcsncpy(s, L"Space", n); return;
            case VK_TAB: wcsncpy(s, L"Tab", n); return;
            case VK_BACK: wcsncpy(s, L"Backspace", n); return;
            case VK_ESCAPE: wcsncpy(s, L"Esc", n); return;
        }
        UINT sc = MapVirtualKeyW(v, MAPVK_VK_TO_VSC);
        bool ext = (v >= VK_PRIOR && v <= VK_HELP) || v == VK_DIVIDE || v == VK_NUMLOCK;
        if (!GetKeyNameTextW((LONG)((sc << 16) | (ext ? 1 << 24 : 0)), s, n)) swprintf(s, n, L"Key %u", v);
        return;
    }
    if (t == 2) {
        static const wchar_t* r[8] = {L"?", L"Left click", L"Right click", L"Wheel click", L"Side button 1", L"Side button 2", L"Wheel up", L"Wheel down"};
        wcsncpy(s, v < 8 ? r[v] : L"?", n);
        return;
    }
    if (t == 5 && v < N_PADCODES) {
        static const wchar_t* xb[N_PADCODES] = {L"A", L"B", L"X", L"Y", L"LB", L"RB", L"LT", L"RT", L"LS", L"RS", L"View", L"Menu",
            L"D-pad up", L"D-pad down", L"D-pad left", L"D-pad right", L"Left stick up", L"Left stick down", L"Left stick left", L"Left stick right",
            L"Right stick up", L"Right stick down", L"Right stick left", L"Right stick right"};
        static const wchar_t* ps[12] = {L"Cross", L"Circle", L"Square", L"Triangle", L"L1", L"R1", L"L2", L"R2", L"L3", L"R3", L"Share", L"Options"};
        wcsncpy(s, pad_family == 2 && v < 12 ? ps[v] : xb[v], n);
        return;
    }
    wcsncpy(s, L"?", n);
}
