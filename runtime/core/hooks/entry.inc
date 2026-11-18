void psp_hook(uint32_t dir, Ctx& c) {
    if (dir == 0x08A3B28Cu) mpm_box(c);
    if (dir == 0x08A2F7CCu) box_note(c.r[4], c.r[5]);
    if (dir == 0x0883A5A4u || dir == 0x0883A6E8u || dir == 0x0883A430u) {   // music: stop, fade, play (track a1)
        static int n = 0;
        if (n++ < 40) printf("musica: %s canal %u%s desde 0x%08X, banco %d\n", dir == 0x0883A5A4u ? "detener" : dir == 0x0883A6E8u ? "fundido" : "reproducir", c.r[5],
                             dir == 0x0883A430u ? (" id " + std::to_string(c.r[7])).c_str() : "", c.r[31] - 8, psp_bank_current() >= 0 ? BANKS[psp_bank_current()].indice : -1);
    }
    if (dir == 0x089B1A58u) { anim_vb = psp_l32(0x08A78E14u); events_note(c.r[4]); events_rumble(c, c.r[4]); events_fx(c, c.r[4]); }
    if (dir == 0x089D8148u || dir == 0x089D82A0u) prompt_note(c, dir, lmp_hk < 0 ? 0 : lmp_hk);
    // TTT_SCREENS=1: traces the screen manager of the name overlay
    static int screens = -1;
    if (screens < 0) { const char* e = getenv("TTT_SCREENS"); screens = e ? atoi(e) : 0; }
    if (screens && dir == 0x08A8A074u)
        printf("pantalla -> %u (desde 0x%08X)\n", c.r[5], c.r[31]);
    if (screens && dir == 0x08A8BCE4u)
        printf("keyboard: estado %u (desde 0x%08X)\n", c.r[5], c.r[31]);
    if (screens && dir == 0x08A2F7CCu)
        printf("caja de mensaje: id %u estructura 0x%08X (desde 0x%08X)\n", psp_l32(c.r[5] + 4), c.r[5], c.r[31]);
    if (screens && dir == 0x08A3B28Cu)
        printf("caja si/no: id %u opciones %u/%u (desde 0x%08X)\n", psp_l32(c.r[5] + 4), psp_l32(c.r[5] + 8), psp_l32(c.r[5] + 12), c.r[31]);
    if (screens) fflush(stdout);
    if (dir == 0x08839204u || dir == 0x0883963Cu || dir == 0x08839268u || dir == 0x0883956Cu || dir == 0x08839618u) {
        static int n = 0;
        static uint32_t last = 0;
        uint32_t target = c.r[4];
        uint32_t key = dir ^ psp_l32(target + 0x69C) ^ (psp_l8(target + 0x600) << 8);
        if (n < 60 && (dir != 0x0883963Cu || key != last)) {
            n++; last = key;
            printf("gestor guardado: f_%08X(obj 0x%08X) estado[0x69C]=%u flag[0x600]=%u resultado[0x6B8]=0x%08X modo[0x30]=%u (ra 0x%08X)\n",
                   dir, target, psp_l32(target + 0x69C), psp_l8(target + 0x600), psp_l32(target + 0x6B8), psp_l32(target + 0x30), c.r[31]);
        }
    }
    if (dir == 0x0885E664u) {                 // per-triangle filter of the height query
        static uint64_t n = 0;
        if (n < 6) {
            uint32_t idx = psp_l32(c.r[4] + 24), tab = psp_l32(0x08B61238u), base = psp_l32(0x08A7E220u);
            uint32_t tri = base + psp_l32(tab + 16) * 4 + idx * 32;
            float nrm[4]; for (int i = 0; i < 4; i++) { uint32_t u = psp_l32(tri + 16 + i * 4); memcpy(&nrm[i], &u, 4); }
            float bb[6]; for (int i = 0; i < 6; i++) { uint32_t u = psp_l32(c.r[4] + i * 4); memcpy(&bb[i], &u, 4); }
            uint32_t vb = base + psp_l32(tab + 20) * 4;
            float vv[3][3];
            for (int k = 0; k < 3; k++) for (int i = 0; i < 3; i++) { uint32_t u = psp_l32(vb + psp_l32(tri + 4 + k * 4) * 16 + i * 4); memcpy(&vv[k][i], &u, 4); }
            printf("filtro suelo: hoja 0x%08X AABB min (%.1f %.1f %.1f) max (%.1f %.1f %.1f) | tri %u idx (%u %u %u) v0 (%.1f %.1f %.1f) v1 (%.1f %.1f %.1f) v2 (%.1f %.1f %.1f) normal (%.3f %.3f %.3f) d %.3f\n",
                   c.r[4], bb[0], bb[1], bb[2], bb[3], bb[4], bb[5], idx, psp_l32(tri + 4), psp_l32(tri + 8), psp_l32(tri + 12),
                   vv[0][0], vv[0][1], vv[0][2], vv[1][0], vv[1][1], vv[1][2], vv[2][0], vv[2][1], vv[2][2], nrm[0], nrm[1], nrm[2], nrm[3]);
            uint32_t box = c.r[5];
            float q[6]; for (int i = 0; i < 6; i++) { uint32_t u = psp_l32(box + i * 4); memcpy(&q[i], &u, 4); }
            printf("   caja consulta min (%.1f %.1f %.1f) max (%.1f %.1f %.1f) | tabla 0x%08X caras +%u vertices +%u base 0x%08X\n",
                   q[0], q[1], q[2], q[3], q[4], q[5], tab, psp_l32(tab + 16), psp_l32(tab + 20), base);
        }
        if (++n % 20000 == 0) printf("filtro suelo: %llu llamadas\n", (unsigned long long)n);
    }
    if (dir == 0x0885E520u) {                 // geometric test: passed the normal filter
        static uint64_t n = 0;
        if (n < 8) {
            float v[16], q[4]; uint32_t a1 = c.r[5], a0 = c.r[4];
            for (int i = 0; i < 16; i++) { uint32_t u = psp_l32(a1 + i * 4); memcpy(&v[i], &u, 4); }
            for (int i = 0; i < 4; i++) { uint32_t u = psp_l32(a0 + i * 4); memcpy(&q[i], &u, 4); }
            printf("test suelo: consulta (%.2f %.2f %.2f) f12 %.2f mejor %.2f | v0 (%.2f %.2f %.2f) v1 (%.2f %.2f %.2f) v2 (%.2f %.2f %.2f) n (%.3f %.3f %.3f) d %.2f\n",
                   q[0], q[1], q[2], c.f[12].f, q[1], v[0], v[1], v[2], v[4], v[5], v[6], v[8], v[9], v[10], v[12], v[13], v[14], v[15]);
        }
        if (++n % 20000 == 0) printf("test suelo: %llu llamadas\n", (unsigned long long)n);
    }
    if (dir == 0x089B29ECu) {                 // bone walk
        static int n = 0, traj = 0;
        if (traj++ == 0) {                        // stage collision grid
            uint32_t scene = psp_l32(0x08A7E678u);
            uint32_t n = psp_l32(scene + 0x4C), cells = psp_l32(scene + 0x50);
            printf("escenario 0x%08X: %u celdas en 0x%08X, [0x30] 0x%08X, [0x5C] 0x%08X\n",
                   scene, n, cells, psp_l32(scene + 0x30), psp_l32(scene + 0x5C));
            uint32_t empty_n = 0, tot = 0;
            for (uint32_t k = 0; k < n && k < 65536; k++) { uint32_t cnt = psp_l32(cells + k * 64); if (!cnt) empty_n++; tot += cnt; }
            printf("  celdas vacias %u de %u, total en contadores %u\n", empty_n, n, tot);
            for (uint32_t k = 0; k < n && k < 4; k++) {
                printf("  celda %u:", k);
                for (int w = 0; w < 8; w++) printf(" %08X", psp_l32(cells + k * 64 + w * 4));
                printf("\n");
            }
        }
        if (n++ < 4) {
            uint32_t target = c.r[4];
            uint32_t flow = psp_l32(target + 64);
            printf("bones: obj 0x%08X ra 0x%08X modo=%u flujo=0x%08X tabla=0x%08X\n",
                   target, c.r[31], psp_l32(target), flow, psp_l32(target + 3500));
            printf("  lista 0x08B61178:");
            for (int i = 0; i < 16; i++) printf(" %08X", psp_l32(0x08B61178u + 4 * i));
            printf("\n");
            uint32_t p = flow;
            int k = 0, motivo = 0;
            for (; k < 1024; k++) {
                uint32_t tam = psp_l32(p);
                uint32_t pop = psp_l16(p + 4), fin = psp_l16(p + 6), idx = psp_l16(p + 10);
                if (k < 8)
                    printf("  [%3d] 0x%08X tam=%u pop=%u fin=%u hueso=%u obj=0x%08X\n",
                           k, p, tam, pop, fin, idx, psp_l32(target + 3500 + idx * 4));
                if (fin) { motivo = 1; break; }
                if (!tam) { motivo = 2; break; }
                p += tam;
            }
            printf("bones: %d entradas, %s\n", k,
                   motivo == 1 ? "termina bien" : motivo == 2 ? "tam=0, NO AVANZA" : "tope de 1024 sin fin");
        }
    }
    if (dir == 0x089C34E8u) {                 // load_overlay(index): a0 carries the PACKFILE entry
        int idx = (int)(int16_t)(c.r[4] & 0xFFFF);
        psp_bank(idx);
        static int n = 0;
        if (n++ < 20) printf("banco de overlay -> entrada %d (%s)\n", idx,
                             psp_bank_current() >= 0 ? BANKS[psp_bank_current()].nombre : "SIN BANCO");
    }
}

void psp_fault(const char* que, uint32_t v) {
    static int n = 0;
    if (n < 40) fprintf(stderr, "%s: 0x%08X\n", que, v);
    n++;
}

const char* psp_import_name(int id) {
    if (id < 0 || id >= IMPORTS_N) return "?";
    return IMPORTS[id].nombre[0] ? IMPORTS[id].nombre : IMPORTS[id].lib;
}

int psp_imports_n() { return IMPORTS_N; }
