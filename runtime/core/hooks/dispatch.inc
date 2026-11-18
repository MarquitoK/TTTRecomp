Fn psp_resolve(uint32_t dir) {
    if (resolve_hash()) {
        const Cell* e = peek(dir);
        return (e && e->imp < 0) ? e->fn : nullptr;
    }
    if (dir >= PSP_BANK_VA && dir < PSP_BANK_END) {
        if (active_bank < 0) return nullptr;
        return lookup(BANKS[active_bank], dir);
    }
    return lookup(BANKS[0], dir);
}

void mpm_native(Ctx& c, uint32_t dir);

void psp_call(Ctx& c, uint32_t dir) {
    PERF_N(PC_JALR, 1);
    if ((dir & 0xFFFFFF00u) == 0x0BFF0000u) { mpm_native(c, dir); return; }
    int imp = -1;
    Fn f = nullptr;
    {
        PERF(PZ_RES);
        if (resolve_hash()) {
            const Cell* e = peek(dir);
            if (e) { imp = e->imp; f = e->fn; }
        } else {
            for (int i = 0; i < IMPORTS_N; i++)
                if (IMPORTS[i].stub == dir) { imp = i; break; }
            if (imp < 0) f = psp_resolve(dir);
        }
    }
    if (imp >= 0) { PERF_N(PC_IMPORT, 1); psp_hle(c, imp); return; }
    if (psp_jalr_trace > 0) { psp_jalr_trace--; printf("  jalr -> 0x%08X (a0 0x%08X a1 0x%08X, desde 0x%08X)\n", dir, c.r[4], c.r[5], c.r[31]); }
    watch_call(dir, c);
    jalr_census(dir, c);
    if (psp_cov_on) psp_coverage_note(f ? COV_JALR : COV_CALL_UNRES, dir, c.r[31] - 8);
    if (!f) {
        psp_unresolved++;
        static int n = 0;
        if (n++ < 20)
            fprintf(stderr, "llamada no resuelta 0x%08X desde el hilo %d (ra 0x%08X, a0 0x%08X, a1 0x%08X)\n",
                    dir, psp_thread_current(), c.r[31], c.r[4], c.r[5]);
        c.r[2] = 0;
        return;
    }
    f(c);
}

void psp_jump(Ctx& c, uint32_t dir) {
    Fn f;
    { PERF(PZ_RES); f = psp_resolve(dir); }
    if (!f) { psp_unresolved++; if (psp_cov_on) psp_coverage_note(COV_JUMP_UNRES, dir, c.fn); psp_fault("salto no resuelto", dir); return; }
    f(c);
}

void psp_syscall(Ctx& c, uint32_t code) { (void)c; psp_fault("syscall", code); }
void psp_break(Ctx& c, uint32_t code) { (void)c; psp_fault("break", code); }

static const char* GPR_N[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"};

static void bh_tw_style(Ctx& c);
void psp_probe(uint32_t pc, Ctx& c) {
    if (pc == 0x08832530u && c.r[31] == 0x089F23FCu) bh_tw_style(c);   // technique name text (BT3 HUD)
    if (text_probe(pc, c)) return;
    static uint32_t seen_calls[64];
    static int times[64];
    static int nv = 0;
    int k = -1;
    for (int i = 0; i < nv; i++) if (seen_calls[i] == pc) { k = i; break; }
    if (k < 0) { if (nv >= 64) return; k = nv++; seen_calls[k] = pc; times[k] = 0; }
    if (times[k]++ >= 12) return;
    printf("sonda 0x%08X paso %d hilo %d\n", pc, times[k], psp_thread_current());
    for (int i = 0; i < 32; i += 8) {
        printf("   ");
        for (int j = 0; j < 8; j++) printf("%s=%08X ", GPR_N[i + j], c.r[i + j]);
        printf("\n");
    }
    uint32_t a0 = c.r[4];
    if (a0 >= 0x08800000u && a0 < 0x0A000000u) {          // bytes at a0 as floats, 4 per line
        int n32 = (pc == 0x089B2BECu) ? 16 : 32;
        for (int i = 0; i < n32; i += 4) {
            float f[4];
            for (int j = 0; j < 4; j++) { uint32_t u = psp_l32(a0 + (i + j) * 4); memcpy(&f[j], &u, 4); }
            printf("   [a0+%3d] %10.4f %10.4f %10.4f %10.4f   %08X %08X %08X %08X\n", i * 4, f[0], f[1], f[2], f[3],
                   psp_l32(a0 + i * 4), psp_l32(a0 + i * 4 + 4), psp_l32(a0 + i * 4 + 8), psp_l32(a0 + i * 4 + 12));
        }
    }
}

void psp_sp_bad(uint32_t dir, uint32_t sp0, Ctx& c) {
    static uint32_t seen_calls[80];
    static int nv = 0;
    for (int i = 0; i < nv; i++) if (seen_calls[i] == dir) return;
    if (nv >= 80) return;
    seen_calls[nv++] = dir;
    printf("sp mal: f_%08X entra con 0x%08X sale con 0x%08X (delta %d) ra 0x%08X hilo %d\n",
           dir, sp0, c.r[29], (int)(c.r[29] - sp0), c.r[31], psp_thread_current());
}

