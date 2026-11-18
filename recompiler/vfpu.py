import struct

# indice real dentro del banco de 128: matriz*4 + columna + fila*32
def regs_vector(vreg, n):
    mtx = (vreg >> 2) & 7
    col = vreg & 3
    if n == 1:
        fila, trans = (vreg >> 5) & 3, 0
    elif n == 3:
        fila, trans = (vreg >> 6) & 1, (vreg >> 5) & 1
    else:
        fila, trans = (vreg >> 5) & 2, (vreg >> 5) & 1
    out = []
    for i in range(n):
        idx = mtx * 4
        if trans:
            idx += ((fila + i) & 3) + col * 32
        else:
            idx += col + ((fila + i) & 3) * 32
        out.append(idx)
    return out


def regs_matriz(vreg, n):
    mtx = (vreg >> 2) & 7
    col = vreg & 3
    fila = (vreg >> 6) & 1 if n == 3 else (vreg >> 5) & 2
    trans = (vreg >> 5) & 1
    out = [0] * (n * n)
    for i in range(n):
        for j in range(n):
            idx = mtx * 4
            if trans:
                idx += ((fila + i) & 3) + ((col + j) & 3) * 32
            else:
                idx += ((col + j) & 3) + ((fila + i) & 3) * 32
            out[j * n + i] = idx
    return out


VOFF = [regs_vector(i, 1)[0] for i in range(128)]


def campos(w):
    # lv.s/sv.s: los dos bits bajos son bits 5-6 del registro; lv.q/sv.q: solo el bit 0, el bit 1 es wb (writeback)
    op = w >> 26
    bajos = (w & 1) if op in (0x36, 0x3E) else (w & 3)
    return dict(vt=((w >> 16) & 0x1F) | (bajos << 5),
                rs=(w >> 21) & 0x1F,
                rt=(w >> 16) & 0x1F,
                off=(w & 0xFFFC) - 0x10000 if (w & 0x8000) else (w & 0xFFFC),
                imm8=w & 0xFF)


def grupo(w):
    op = w >> 26
    if op in (0x32, 0x36, 0x3A, 0x3E):
        return (op, -1)
    if op == 0x12:
        return (op, (w >> 21) & 31)
    return (op, (w >> 23) & 7)


def emitir(w, pc):
    # devuelve lineas de C++ o None si la instruccion todavia no esta cubierta
    op = w >> 26
    c = campos(w)
    if op in (0x32, 0x36, 0x3A, 0x3E):
        n = 4 if op in (0x36, 0x3E) else 1
        regs = regs_vector(c['vt'], n)
        dir_ = ('c.r[%d] + %d' % (c['rs'], c['off'])) if c['rs'] else ('%du' % (c['off'] & 0xFFFFFFFF))
        out = ['{ uint32_t d = %s;' % dir_]
        for i, r in enumerate(regs):
            if op in (0x32, 0x36):
                out.append('  c.v[%d].u = psp_l32(d + %d);' % (r, 4 * i))
            else:
                out.append('  psp_s32(d + %d, c.v[%d].u);' % (4 * i, r))
        out.append('}')
        return out
    if op == 0x12:
        imm = c['imm8']
        if c['rs'] == 3:
            if imm < 128:
                return ['c.r[%d] = c.v[%d].u;' % (c['rt'], VOFF[imm])] if c['rt'] else []
            return ['c.r[%d] = c.vctrl[%d];' % (c['rt'], imm - 128)] if c['rt'] and imm - 128 < 16 else []
        if c['rs'] == 7:
            if imm < 128:
                return ['c.v[%d].u = %s;' % (VOFF[imm], 'c.r[%d]' % c['rt'] if c['rt'] else '0u')]
            if imm - 128 < 16:
                return ['c.vctrl[%d] = %s;' % (imm - 128, 'c.r[%d]' % c['rt'] if c['rt'] else '0u')]
            return []
    return None


def ejecutar(cpu, w, pc):
    # version de referencia del mismo subconjunto
    op = w >> 26
    c = campos(w)
    if op in (0x32, 0x36, 0x3A, 0x3E):
        n = 4 if op in (0x36, 0x3E) else 1
        d = (cpu.r[c['rs']] + c['off']) & 0xFFFFFFFF
        for i, r in enumerate(regs_vector(c['vt'], n)):
            if op in (0x32, 0x36):
                cpu.v[r] = cpu.mem.l32((d + 4 * i) & 0xFFFFFFFF)
            else:
                cpu.mem.s32((d + 4 * i) & 0xFFFFFFFF, cpu.v[r])
        return True
    if op == 0x12:
        imm = c['imm8']
        if c['rs'] == 3:
            if c['rt']:
                cpu.r[c['rt']] = cpu.v[VOFF[imm]] if imm < 128 else (cpu.vctrl[imm - 128] if imm - 128 < 16 else 0)
            return True
        if c['rs'] == 7:
            val = cpu.r[c['rt']]
            if imm < 128:
                cpu.v[VOFF[imm]] = val
            elif imm - 128 < 16:
                cpu.vctrl[imm - 128] = val
            return True
    return False
