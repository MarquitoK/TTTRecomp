import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mips, vfpu


class EmisorError(Exception):
    pass


VIGILAR_SP = True   # diagnostico: avisa si una funcion no restaura sp al volver
AFTER = set()       # psp_after(pc) right after the instruction (also in a delay slot)


def R(n):
    return '0u' if n == 0 else 'c.r[%d]' % n


def S(n):
    return '0' if n == 0 else '(int32_t)c.r[%d]' % n


def W(n, e):
    return None if n == 0 else 'c.r[%d] = %s;' % (n, e)


def F(n):
    return 'c.f[%d].f' % n


def FU(n):
    return 'c.f[%d].u' % n


def FI(n):
    return 'c.f[%d].i' % n


def dir_mem(i):
    return '%s + %d' % (R(i.rs), i.simm) if i.rs else '%du' % (i.simm & 0xFFFFFFFF)


def cuerpo(i):
    n = i.name
    if n == 'nop':
        return []
    if n in ('add', 'addu'):
        return [W(i.rd, '%s + %s' % (R(i.rs), R(i.rt)))]
    if n in ('sub', 'subu'):
        return [W(i.rd, '%s - %s' % (R(i.rs), R(i.rt)))]
    if n == 'and':
        return [W(i.rd, '%s & %s' % (R(i.rs), R(i.rt)))]
    if n == 'or':
        return [W(i.rd, '%s | %s' % (R(i.rs), R(i.rt)))]
    if n == 'xor':
        return [W(i.rd, '%s ^ %s' % (R(i.rs), R(i.rt)))]
    if n == 'nor':
        return [W(i.rd, '~(%s | %s)' % (R(i.rs), R(i.rt)))]
    if n == 'slt':
        return [W(i.rd, '(uint32_t)(%s < %s)' % (S(i.rs), S(i.rt)))]
    if n == 'sltu':
        return [W(i.rd, '(uint32_t)(%s < %s)' % (R(i.rs), R(i.rt)))]
    if n == 'max':
        return [W(i.rd, '(uint32_t)(%s > %s ? %s : %s)' % (S(i.rs), S(i.rt), S(i.rs), S(i.rt)))]
    if n == 'min':
        return [W(i.rd, '(uint32_t)(%s < %s ? %s : %s)' % (S(i.rs), S(i.rt), S(i.rs), S(i.rt)))]
    if n == 'movz':
        return ['if (%s == 0) { %s }' % (R(i.rt), W(i.rd, R(i.rs)) or '')] if i.rd else []
    if n == 'movn':
        return ['if (%s != 0) { %s }' % (R(i.rt), W(i.rd, R(i.rs)) or '')] if i.rd else []
    if n == 'sll':
        return [W(i.rd, '%s << %d' % (R(i.rt), i.sa))]
    if n == 'srl':
        return [W(i.rd, '%s >> %d' % (R(i.rt), i.sa))]
    if n == 'sra':
        return [W(i.rd, '(uint32_t)(%s >> %d)' % (S(i.rt), i.sa))]
    if n == 'rotr':
        return [W(i.rd, 'psp_rotr(%s, %d)' % (R(i.rt), i.sa))]
    if n == 'sllv':
        return [W(i.rd, '%s << (%s & 31)' % (R(i.rt), R(i.rs)))]
    if n == 'srlv':
        return [W(i.rd, '%s >> (%s & 31)' % (R(i.rt), R(i.rs)))]
    if n == 'srav':
        return [W(i.rd, '(uint32_t)(%s >> (%s & 31))' % (S(i.rt), R(i.rs)))]
    if n == 'rotrv':
        return [W(i.rd, 'psp_rotr(%s, %s & 31)' % (R(i.rt), R(i.rs)))]
    if n == 'clz':
        return [W(i.rd, 'psp_clz(%s)' % R(i.rs))]
    if n == 'clo':
        return [W(i.rd, 'psp_clz(~%s)' % R(i.rs))]
    if n == 'mult':
        return ['psp_mult(c, %s, %s);' % (S(i.rs), S(i.rt))]
    if n == 'multu':
        return ['psp_multu(c, %s, %s);' % (R(i.rs), R(i.rt))]
    if n == 'div':
        return ['psp_div(c, %s, %s);' % (S(i.rs), S(i.rt))]
    if n == 'divu':
        return ['psp_divu(c, %s, %s);' % (R(i.rs), R(i.rt))]
    if n in ('madd', 'maddu', 'msub', 'msubu'):
        return ['psp_%s(c, %s, %s);' % (n, S(i.rs) if n in ('madd', 'msub') else R(i.rs),
                                        S(i.rt) if n in ('madd', 'msub') else R(i.rt))]
    if n == 'mfhi':
        return [W(i.rd, 'c.hi')]
    if n == 'mflo':
        return [W(i.rd, 'c.lo')]
    if n == 'mthi':
        return ['c.hi = %s;' % R(i.rs)]
    if n == 'mtlo':
        return ['c.lo = %s;' % R(i.rs)]
    if n in ('addi', 'addiu'):
        return [W(i.rt, '%s + %du' % (R(i.rs), i.simm & 0xFFFFFFFF))]
    if n == 'slti':
        return [W(i.rt, '(uint32_t)(%s < %d)' % (S(i.rs), i.simm))]
    if n == 'sltiu':
        return [W(i.rt, '(uint32_t)(%s < %du)' % (R(i.rs), i.simm & 0xFFFFFFFF))]
    if n == 'andi':
        return [W(i.rt, '%s & %du' % (R(i.rs), i.imm))]
    if n == 'ori':
        return [W(i.rt, '%s | %du' % (R(i.rs), i.imm))]
    if n == 'xori':
        return [W(i.rt, '%s ^ %du' % (R(i.rs), i.imm))]
    if n == 'lui':
        return [W(i.rt, '%du' % ((i.imm << 16) & 0xFFFFFFFF))]
    if n == 'ext':
        return [W(i.rt, 'psp_ext(%s, %d, %d)' % (R(i.rs), i.pos, i.size + 1))]
    if n == 'ins':
        return [W(i.rt, 'psp_ins(%s, %s, %d, %d)' % (R(i.rt), R(i.rs), i.pos, i.size + 1 - i.pos))]
    if n == 'seb':
        return [W(i.rd, '(uint32_t)(int8_t)%s' % R(i.rt))]
    if n == 'seh':
        return [W(i.rd, '(uint32_t)(int16_t)%s' % R(i.rt))]
    if n == 'wsbh':
        return [W(i.rd, 'psp_wsbh(%s)' % R(i.rt))]
    if n == 'wsbw':
        return [W(i.rd, 'psp_wsbw(%s)' % R(i.rt))]
    if n == 'bitrev':
        return [W(i.rd, 'psp_bitrev(%s)' % R(i.rt))]
    if n == 'lb':
        return [W(i.rt, '(uint32_t)(int8_t)psp_l8(%s)' % dir_mem(i))]
    if n == 'lbu':
        return [W(i.rt, '(uint32_t)psp_l8(%s)' % dir_mem(i))]
    if n == 'lh':
        return [W(i.rt, '(uint32_t)(int16_t)psp_l16(%s)' % dir_mem(i))]
    if n == 'lhu':
        return [W(i.rt, '(uint32_t)psp_l16(%s)' % dir_mem(i))]
    if n == 'lw':
        return [W(i.rt, 'psp_l32(%s)' % dir_mem(i))]
    if n == 'lwl':
        return [W(i.rt, 'psp_lwl(%s, %s)' % (R(i.rt), dir_mem(i)))]
    if n == 'lwr':
        return [W(i.rt, 'psp_lwr(%s, %s)' % (R(i.rt), dir_mem(i)))]
    if n == 'sb':
        return ['psp_s8(%s, %s);' % (dir_mem(i), R(i.rt))]
    if n == 'sh':
        return ['psp_s16(%s, %s);' % (dir_mem(i), R(i.rt))]
    if n == 'sw':
        return ['psp_s32(%s, %s);' % (dir_mem(i), R(i.rt))]
    if n == 'swl':
        return ['psp_swl(%s, %s);' % (dir_mem(i), R(i.rt))]
    if n == 'swr':
        return ['psp_swr(%s, %s);' % (dir_mem(i), R(i.rt))]
    if n in ('cache', 'sync'):
        return []
    if n == 'break':
        return ['psp_break(c, %du);' % i.code]
    if n == 'syscall':
        return ['psp_syscall(c, %du);' % i.code]
    if n == 'mfc1':
        return [W(i.rt, FU(i.fs))]
    if n == 'mtc1':
        return ['%s = %s;' % (FU(i.fs), R(i.rt))]
    if n == 'cfc1':
        return [W(i.rt, 'psp_cfc1(c, %d)' % i.fs)]
    if n == 'ctc1':
        return ['psp_ctc1(c, %d, %s);' % (i.fs, R(i.rt))]
    if n == 'lwc1':
        return ['%s = psp_l32(%s);' % (FU(i.ft), dir_mem(i))]
    if n == 'swc1':
        return ['psp_s32(%s, %s);' % (dir_mem(i), FU(i.ft))]
    if n == 'add.s':
        return ['%s = %s + %s;' % (F(i.fd), F(i.fs), F(i.ft))]
    if n == 'sub.s':
        return ['%s = %s - %s;' % (F(i.fd), F(i.fs), F(i.ft))]
    if n == 'mul.s':
        return ['%s = %s * %s;' % (F(i.fd), F(i.fs), F(i.ft))]
    if n == 'div.s':
        return ['%s = %s / %s;' % (F(i.fd), F(i.fs), F(i.ft))]
    if n == 'sqrt.s':
        return ['%s = psp_sqrt(%s);' % (F(i.fd), F(i.fs))]
    if n == 'abs.s':
        return ['%s = %s & 0x7FFFFFFFu;' % (FU(i.fd), FU(i.fs))]
    if n == 'neg.s':
        return ['%s = %s ^ 0x80000000u;' % (FU(i.fd), FU(i.fs))]
    if n == 'mov.s':
        return ['%s = %s;' % (FU(i.fd), FU(i.fs))]
    if n == 'cvt.s.w':
        return ['%s = (float)%s;' % (F(i.fd), FI(i.fs))]
    if n in ('trunc.w.s', 'floor.w.s', 'ceil.w.s', 'round.w.s', 'cvt.w.s'):
        modo = {'trunc.w.s': 'T', 'floor.w.s': 'F', 'ceil.w.s': 'C', 'round.w.s': 'R', 'cvt.w.s': 'N'}[n]
        return ['%s = psp_fcvt_%s(%s);' % (FI(i.fd), modo, F(i.fs))]
    if n.startswith('c.') and n.endswith('.s'):
        return ['c.fcc = psp_cmp_%s(%s, %s);' % (n[2:-2], F(i.fs), F(i.ft))]
    if n in ('mfc0', 'mtc0', 'mfic', 'mtic'):
        return ['// %s sin efecto' % n]
    raise EmisorError('instruccion sin emisor: %s en 0x%08X' % (n, i.pc))


def condicion(i):
    n = i.name
    if n in ('beq', 'beql'):
        return '%s == %s' % (R(i.rs), R(i.rt))
    if n in ('bne', 'bnel'):
        return '%s != %s' % (R(i.rs), R(i.rt))
    if n in ('blez', 'blezl'):
        return '%s <= 0' % S(i.rs)
    if n in ('bgtz', 'bgtzl'):
        return '%s > 0' % S(i.rs)
    if n in ('bltz', 'bltzl', 'bltzal', 'bltzall'):
        return '%s < 0' % S(i.rs)
    if n in ('bgez', 'bgezl', 'bgezal', 'bgezall'):
        return '%s >= 0' % S(i.rs)
    if n in ('bc1t', 'bc1tl'):
        return 'c.fcc'
    if n in ('bc1f', 'bc1fl'):
        return '!c.fcc'
    if n in ('bvt', 'bvtl'):
        return 'psp_vfpu_cc(c, %d)' % i.vbit
    if n in ('bvf', 'bvfl'):
        return '!psp_vfpu_cc(c, %d)' % i.vbit
    raise EmisorError('rama sin condicion: %s' % n)


class Funcion:
    def __init__(self, img, ini, conocidas, nombre_fn=None, hle=None, ganchos=(), sondas=(), medios=(), stores=False):
        self.img = img
        self.stores = stores
        self.medios = medios or ()
        self.ini = ini
        self.conocidas = conocidas
        self.hle = hle or {}
        self.ganchos = ganchos or ()
        self.sondas = sondas or ()
        self.nombre_fn = nombre_fn or (lambda a: 'f_%08X' % a)
        self.instr = {}
        self.etiquetas = set()
        self.retardo = set()
        self.tablas = {}
        self.retornos = set()
        self.vfpu = 0
        self._recorrer()

    def _recorrer(self):
        img = self.img
        pend = [self.ini]
        visto = set()
        while pend:
            pc = pend.pop()
            while img.es_codigo(pc) and pc not in visto:
                visto.add(pc)
                w = img.u32(pc)
                ins = mips.decode(w, pc)
                if ins is None and mips.is_vfpu(w):
                    self.vfpu += 1
                    self.instr[pc] = None
                    pc += 4
                    continue
                self.instr[pc] = ins
                if ins is None:
                    pc += 4
                    continue
                n = ins.name
                if n in mips.BRANCHES:
                    t = ins.branch_target
                    self.etiquetas.add(t)
                    self.retardo.add(pc + 4)
                    if img.es_codigo(t):
                        pend.append(t)
                        if n in mips.LINKING:      # rama enlazada usada como llamada interna
                            self.retornos.add(pc + 8)
                            self.etiquetas.add(pc + 8)
                    pc += 4
                    continue
                if n in ('jal', 'jalr'):
                    self.retardo.add(pc + 4)
                    pc += 4
                    continue
                if n == 'j':
                    self.retardo.add(pc + 4)
                    self.etiquetas.add(ins.tgt)
                    if img.es_codigo(ins.tgt):
                        pend.append(ins.tgt)
                    self.instr.setdefault(pc + 4, mips.decode(img.u32(pc + 4) or 0, pc + 4))
                    visto.add(pc + 4)
                    break
                if n == 'jr':
                    self.retardo.add(pc + 4)
                    self.instr[pc + 4] = mips.decode(img.u32(pc + 4) or 0, pc + 4)
                    visto.add(pc + 4)
                    if ins.rs != 31:
                        jt = self.img.tabla(pc, ins.rs)
                        if jt:
                            self.tablas[pc] = jt
                            for t in jt[1]:
                                self.etiquetas.add(t)
                                pend.append(t)
                    break
                pc += 4
        self.fin = max(visto) + 4 if visto else self.ini

    def emitir(self):
        orden = [pc for pc in sorted(self.instr) if pc not in self.retardo]
        entrada_retardo = self.ini in self.retardo
        if entrada_retardo:
            self.etiquetas.add(self.ini + 4)
        else:
            self.etiquetas.add(self.ini)
        prev = None
        for pc in orden:
            if prev is not None and pc != prev[0] and prev[1]:
                self.etiquetas.add(pc)
            prev = self._avance(pc)
        out = ['void %s(Ctx& c) {' % self.nombre_fn(self.ini),
               '    c.fn = 0x%08Xu; c.llamadas++;' % self.ini,
               '    psp_fn_count(0x%08Xu, c.r[31]);' % self.ini]
        if VIGILAR_SP:
            out.append('    uint32_t sp0 = c.r[29];')
        if self.ini in self.ganchos:
            out.append('    psp_hook(0x%08Xu, c);' % self.ini)
        if entrada_retardo:
            ins = self.instr.get(self.ini)
            out += ['    ' + l for l in (cuerpo_limpio(ins) if ins else [])]
            out.append('    goto L_%08X;' % (self.ini + 4))
        elif orden and orden[0] != self.ini:
            out.append('    goto L_%08X;' % self.ini)
        prev = None
        for pc in orden:
            if prev is not None and pc != prev[0] and prev[1]:
                out.append('    goto L_%08X;' % pc)
            if pc in self.etiquetas:
                out.append('L_%08X:;' % pc)
            out += ['    ' + l for l in self._instr(pc)]
            prev = self._avance(pc)
        out.append('}')
        return out

    def _avance(self, pc):
        ins = self.instr[pc]
        corta = ins is not None and ins.name in ('j', 'jr')
        salta = ins is not None and (ins.name in mips.BRANCHES or ins.name in ('j', 'jr', 'jal', 'jalr'))
        return (pc + (8 if salta else 4), not corta)

    def _ret(self, pc):
        # instruccion del hueco de retardo, se emite como parte del salto
        ins = self.instr.get(pc + 4)
        if ins is None:
            w = self.img.u32(pc + 4)
            if w is not None and mips.is_vfpu(w):
                lin = vfpu.emitir(w, pc + 4)
                return self._medio(pc + 4, lin) if lin is not None else ['PSP_VFX(0x%08Xu, 0x%08Xu);' % (pc + 4, w)]
            return []
        if ins.name in mips.BRANCHES or ins.name in ('j', 'jr', 'jal', 'jalr'):
            return ['psp_fault("hueco de retardo con salto", 0x%08Xu);' % (pc + 4)]
        return self._medio(pc + 4, cuerpo_limpio(ins)) + self._after(pc + 4)

    def _after(self, pc):
        return ['psp_after(0x%08Xu, c);' % pc] if pc in AFTER else []

    # guardado a medio camino entre la memoria y el valor nuevo (acumuladores de efectos a 60)
    def _medio(self, pc, lineas):
        if self.stores:
            lineas = [_store_site(pc, l) for l in lineas]
        if pc not in self.medios:
            return lineas
        out, n = [], 0
        for l in lineas:
            m = re.match(r'(\s*)psp_s32\((.+), (c\.[fv]\[\d+\]\.u)\);$', l) or re.match(r'(\s*)psp_s32\(([^,]+), PSP_ST\(4, 0x[0-9A-F]+u, [^,]+, (c\.[fv]\[\d+\]\.u)\)\);$', l)
            if m:
                l = '%spsp_s32(%s, psp_half(0x%08Xu, %s, %s));' % (m.group(1), m.group(2), pc, m.group(2), m.group(3))
                n += 1
            out.append(l)
        if not n:
            raise EmisorError('medio sin guardado en 0x%08X' % pc)
        return out

    def _instr(self, pc):
        pre = ['psp_probe(0x%08Xu, c);' % pc] if pc in self.sondas else []
        return pre + self._instr_base(pc)

    def _instr_base(self, pc):
        ins = self.instr[pc]
        if ins is None:
            w = self.img.u32(pc) or 0
            lin = vfpu.emitir(w, pc) if mips.is_vfpu(w) else None
            return self._medio(pc, lin) if lin is not None else ['PSP_VFX(0x%08Xu, 0x%08Xu);' % (pc, w)]
        n = ins.name
        if n not in mips.BRANCHES and n not in ('jal', 'jalr', 'j', 'jr'):
            return self._medio(pc, cuerpo_limpio(ins)) + self._after(pc)
        ret = self._ret(pc)
        if n in mips.BRANCHES:
            cond = condicion(ins)
            pre = ['c.r[31] = 0x%08Xu;' % (pc + 8)] if n in mips.LINKING else []
            dst = 'L_%08X' % ins.branch_target if self.img.es_codigo(ins.branch_target) else None
            salto = ['goto %s;' % dst] if dst else ['psp_jump(c, 0x%08Xu); return;' % ins.branch_target]
            if n in mips.LIKELY:
                return pre + ['if (%s) {' % cond] + ['    ' + l for l in ret + salto] + ['}']
            return pre + ['{ bool tk = (%s);' % cond] + ['  ' + l for l in ret] + \
                   ['  if (tk) { %s }' % ' '.join(salto), '}']
        if n == 'jal':
            if ins.tgt in self.hle:
                llam = ['psp_hle(c, %d);' % self.hle[ins.tgt]]
            elif ins.tgt in self.conocidas:
                llam = ['%s(c);' % self.nombre_fn(ins.tgt)]
            else:
                llam = ['psp_call(c, 0x%08Xu);' % ins.tgt]
            return ['c.r[31] = 0x%08Xu;' % (pc + 8)] + ret + llam
        if n == 'jalr':
            pre = ['uint32_t tgt = %s;' % R(ins.rs)]
            if ins.rd:
                pre.append('c.r[%d] = 0x%08Xu;' % (ins.rd, pc + 8))
            return ['{'] + ['    ' + l for l in pre + ret + ['psp_call(c, tgt);']] + ['}']
        if n == 'j':
            if self.img.es_codigo(ins.tgt) and ins.tgt in self.instr:
                return ret + ['goto L_%08X;' % ins.tgt]
            if ins.tgt in self.conocidas:
                return ret + ['%s(c); return;' % self.nombre_fn(ins.tgt)]
            return ret + ['psp_jump(c, 0x%08Xu); return;' % ins.tgt]
        if n == 'jr':
            if ins.rs == 31:
                fin = ['if (c.r[29] != sp0) psp_sp_bad(0x%08Xu, sp0, c);' % self.ini] if VIGILAR_SP else []
                vuelta = []
                if self.retornos:
                    vuelta = ['switch (c.r[31]) {']
                    for t in sorted(self.retornos):
                        vuelta.append('case 0x%08Xu: goto L_%08X;' % (t, t))
                    vuelta.append('}')
                return ret + vuelta + fin + ['return;']
            pre = ['uint32_t tgt = %s;' % R(ins.rs)]
            jt = self.tablas.get(pc)
            if jt:
                cuerpo_sw = ['switch (tgt) {']
                for t in sorted(set(jt[1])):
                    cuerpo_sw.append('case 0x%08Xu: goto L_%08X;' % (t, t))
                cuerpo_sw += ['default: psp_jump(c, tgt); return;', '}']
                return ['{'] + ['    ' + l for l in pre + ret + cuerpo_sw] + ['}']
            return ['{'] + ['    ' + l for l in pre + ret + ['psp_jump(c, tgt); return;']] + ['}']
        raise EmisorError('salto sin emisor: %s en 0x%08X' % (n, pc))


STORE_RE = re.compile(r'^(\s*)psp_s(8|16|32)\(([^,]+), (.+)\);$')


def _store_site(pc, l):
    # main-image store sites except stack slots go through PSP_ST (step census and 60 fps store rules)
    m = STORE_RE.match(l)
    if not m or m.group(3).startswith('c.r[29]'):
        return l
    sz = {'8': 1, '16': 2, '32': 4}[m.group(2)]
    return '%spsp_s%s(%s, PSP_ST(%d, 0x%08Xu, %s, %s));' % (m.group(1), m.group(2), m.group(3), sz, pc, m.group(3), m.group(4))


def cuerpo_limpio(ins):
    return [l for l in cuerpo(ins) if l]


CABECERA = '#include "psp.h"\n\n'


def emitir_unidad(img, direcciones, conocidas, nombre_fn=None, hle=None, ganchos=(), sondas=()):
    fns = [Funcion(img, a, conocidas, nombre_fn, hle, ganchos, sondas) for a in sorted(direcciones)]
    out = [CABECERA]
    nf = nombre_fn or (lambda a: 'f_%08X' % a)
    for f in fns:
        out.append('void %s(Ctx& c);' % nf(f.ini))
    out.append('')
    for f in fns:
        out += f.emitir()
        out.append('')
    return '\n'.join(out), fns
