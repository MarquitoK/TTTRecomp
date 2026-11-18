#include "cpu/vfpu_exec.h"
#include "perf.h"
#include <cstdlib>
#include <cmath>
#include <cstdio>

// VFPU: 128-register bank in c.v, control in c.vctrl.
// Prefixes in vctrl[0..2] (S, T, D), conditions in vctrl[6] (CC).

void psp_vfpu(Ctx& c, uint32_t pc, uint32_t w) { vfpu_exec(c, pc, w); }

static void vfx_block(const char* label) {
    printf("vfpu: bloque %s en linea %s\n", label, psp_vfx_on ? "si" : "no");
    psp_vfx_on ^= 1;
}
static struct VfxInit { VfxInit() {
    const char* e = getenv("TTT_VFPU_INLINE");
    int v = e && *e ? atoi(e) : 1;
    psp_vfx_on = v ? 1 : 0;
    if (v == 2) perf_block_hook = vfx_block;
} } vfx_init;

bool psp_vfpu_cc(Ctx& c, int bit) {
    return (c.vctrl[CTRL_CC] >> bit) & 1;
}

void psp_vfpu_init(Ctx& c) {
    c.vctrl[CTRL_SPFX] = PFX_IDENT;
    c.vctrl[CTRL_TPFX] = PFX_IDENT;
    c.vctrl[CTRL_DPFX] = 0;
}
