import os, sys, struct, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mips, vfpu

M32 = 0xFFFFFFFF
PAG = 16
PAGTAM = 1 << PAG


class Memoria:
    def __init__(self, mascara=0x0FFFFFFF):
        self.pag = {}
        self.mascara = mascara

    def _p(self, a):
        a &= self.mascara
        p = a >> PAG
        b = self.pag.get(p)
        if b is None:
            b = bytearray(PAGTAM)
            self.pag[p] = b
        return b, a & (PAGTAM - 1)

    def escribir(self, a, datos):
        for i, v in enumerate(datos):
            b, o = self._p(a + i)
            b[o] = v

    def l8(self, a):
        b, o = self._p(a)
        return b[o]

    def l16(self, a):
        return self.l8(a) | (self.l8(a + 1) << 8)

    def l32(self, a):
        return self.l16(a) | (self.l16(a + 2) << 16)

    def s8(self, a, v):
        b, o = self._p(a)
        b[o] = v & 0xFF

    def s16(self, a, v):
        self.s8(a, v)
        self.s8(a + 1, v >> 8)

    def s32(self, a, v):
        self.s16(a, v)
        self.s16(a + 2, v >> 16)

    def volcado(self):
        return {p: bytes(b) for p, b in sorted(self.pag.items()) if any(b)}


class Alto(Exception):
    pass


def s32(v):
    v &= M32
    return v - 0x100000000 if v & 0x80000000 else v


def f2u(x):
    try:
        return struct.unpack('<I', struct.pack('<f', x))[0]
    except OverflowError:
        return 0xFF800000 if x < 0 else 0x7F800000


def u2f(u):
    return struct.unpack('<f', struct.pack('<I', u & M32))[0]


def sat(x):
    if x != x:
        return 0x7FFFFFFF
    if x >= 2147483648.0:
        return 0x7FFFFFFF
    if x < -2147483648.0:
        return 0x80000000
    return int(x) & M32


CMP = {
    'f': lambda a, b: False,
    'un': lambda a, b: a != a or b != b,
    'eq': lambda a, b: not (a != a or b != b) and a == b,
    'ueq': lambda a, b: (a != a or b != b) or a == b,
    'olt': lambda a, b: not (a != a or b != b) and a < b,
    'ult': lambda a, b: (a != a or b != b) or a < b,
    'ole': lambda a, b: not (a != a or b != b) and a <= b,
    'ule': lambda a, b: (a != a or b != b) or a <= b,
}
for _a, _b in (('sf', 'f'), ('ngle', 'un'), ('seq', 'eq'), ('ngl', 'ueq'),
               ('lt', 'olt'), ('nge', 'ult'), ('le', 'ole'), ('ngt', 'ule')):
    CMP[_a] = CMP[_b]


class Cpu:
    def __init__(self, mem):
        self.r = [0] * 32
        self.f = [0] * 32
        self.v = [0] * 128
        self.vctrl = [0] * 16
        self.hi = self.lo = 0
        self.fcc = False
        self.fcr31 = 0
        self.mem = mem
        self.pc = 0
        self.pasos = 0
        self.limite = 20000000
        self.hle = {}

    def leer(self, pc):
        return self.mem.l32(pc)

    def correr(self, ini, ra=0xDEADBEE0):
        self.r[31] = ra
        self.pc = ini
        while True:
            if self.pc == ra:
                return
            self.paso()

    def paso(self):
        self.pasos += 1
        if self.pasos > self.limite:
            raise Alto('limite de pasos')
        pc = self.pc
        w = self.leer(pc)
        if mips.is_vfpu(w) and mips.decode(w, pc) is None:
            if not vfpu.ejecutar(self, w, pc):
                raise Alto('VFPU sin cubrir 0x%08X en 0x%08X' % (w, pc))
            self.pc = pc + 4
            return
        ins = mips.decode(w, pc)
        if ins is None:
            raise Alto('instruccion invalida 0x%08X en 0x%08X' % (w, pc))
        n = ins.name
        if n in mips.BRANCHES:
            tomada = self.cond(ins)
            if n in mips.LINKING:
                self.set(31, pc + 8)
            if n in mips.LIKELY:
                if tomada:
                    self.ejecutar(self.decodificar(pc + 4))
                    self.pc = ins.branch_target
                else:
                    self.pc = pc + 8
            else:
                self.ejecutar(self.decodificar(pc + 4))
                self.pc = ins.branch_target if tomada else pc + 8
            return
        if n == 'j':
            self.ejecutar(self.decodificar(pc + 4))
            self.pc = ins.tgt
            return
        if n == 'jal':
            self.set(31, pc + 8)
            self.ejecutar(self.decodificar(pc + 4))
            self.pc = ins.tgt
            return
        if n == 'jalr':
            t = self.r[ins.rs]
            self.set(ins.rd, pc + 8)
            self.ejecutar(self.decodificar(pc + 4))
            self.pc = t
            return
        if n == 'jr':
            t = self.r[ins.rs]
            self.ejecutar(self.decodificar(pc + 4))
            self.pc = t
            return
        self.ejecutar(ins)
        self.pc = pc + 4

    def decodificar(self, pc):
        w = self.leer(pc)
        if mips.is_vfpu(w):
            raise Alto('VFPU en hueco de retardo 0x%08X' % pc)
        ins = mips.decode(w, pc)
        if ins is None:
            raise Alto('hueco de retardo invalido en 0x%08X' % pc)
        return ins

    def set(self, i, v):
        if i:
            self.r[i] = v & M32

    def cond(self, i):
        n = i.name
        a, b = self.r[i.rs], self.r[i.rt]
        if n in ('beq', 'beql'):
            return a == b
        if n in ('bne', 'bnel'):
            return a != b
        if n in ('blez', 'blezl'):
            return s32(a) <= 0
        if n in ('bgtz', 'bgtzl'):
            return s32(a) > 0
        if n in ('bltz', 'bltzl', 'bltzal', 'bltzall'):
            return s32(a) < 0
        if n in ('bgez', 'bgezl', 'bgezal', 'bgezall'):
            return s32(a) >= 0
        if n in ('bc1t', 'bc1tl'):
            return self.fcc
        if n in ('bc1f', 'bc1fl'):
            return not self.fcc
        if n in ('bvt', 'bvtl'):
            return bool((self.vctrl[6] >> i.vbit) & 1)
        if n in ('bvf', 'bvfl'):
            return not ((self.vctrl[6] >> i.vbit) & 1)
        raise Alto('rama %s' % n)

    def ejecutar(self, i):
        n = i.name
        r = self.r
        rs, rt, rd = i.rs, i.rt, i.rd
        if n in ('add', 'addu'):
            self.set(rd, r[rs] + r[rt])
        elif n in ('sub', 'subu'):
            self.set(rd, r[rs] - r[rt])
        elif n == 'and':
            self.set(rd, r[rs] & r[rt])
        elif n == 'or':
            self.set(rd, r[rs] | r[rt])
        elif n == 'xor':
            self.set(rd, r[rs] ^ r[rt])
        elif n == 'nor':
            self.set(rd, ~(r[rs] | r[rt]))
        elif n == 'slt':
            self.set(rd, 1 if s32(r[rs]) < s32(r[rt]) else 0)
        elif n == 'sltu':
            self.set(rd, 1 if r[rs] < r[rt] else 0)
        elif n == 'max':
            self.set(rd, r[rs] if s32(r[rs]) > s32(r[rt]) else r[rt])
        elif n == 'min':
            self.set(rd, r[rs] if s32(r[rs]) < s32(r[rt]) else r[rt])
        elif n == 'movz':
            if r[rt] == 0:
                self.set(rd, r[rs])
        elif n == 'movn':
            if r[rt] != 0:
                self.set(rd, r[rs])
        elif n == 'sll':
            self.set(rd, r[rt] << i.sa)
        elif n == 'srl':
            self.set(rd, r[rt] >> i.sa)
        elif n == 'sra':
            self.set(rd, s32(r[rt]) >> i.sa)
        elif n == 'rotr':
            self.set(rd, rot(r[rt], i.sa))
        elif n == 'sllv':
            self.set(rd, r[rt] << (r[rs] & 31))
        elif n == 'srlv':
            self.set(rd, r[rt] >> (r[rs] & 31))
        elif n == 'srav':
            self.set(rd, s32(r[rt]) >> (r[rs] & 31))
        elif n == 'rotrv':
            self.set(rd, rot(r[rt], r[rs] & 31))
        elif n == 'clz':
            self.set(rd, clz(r[rs]))
        elif n == 'clo':
            self.set(rd, clz(~r[rs] & M32))
        elif n in ('mult', 'multu'):
            a = s32(r[rs]) if n == 'mult' else r[rs]
            b = s32(r[rt]) if n == 'mult' else r[rt]
            p = (a * b) & 0xFFFFFFFFFFFFFFFF
            self.lo, self.hi = p & M32, (p >> 32) & M32
        elif n in ('div', 'divu'):
            if n == 'div':
                a, b = s32(r[rs]), s32(r[rt])
                if b == 0:
                    self.lo, self.hi = (M32 if a >= 0 else 1), a & M32
                elif a == -0x80000000 and b == -1:
                    self.lo, self.hi = 0x80000000, 0
                else:
                    q = abs(a) // abs(b)
                    if (a < 0) != (b < 0):
                        q = -q
                    self.lo, self.hi = q & M32, (a - q * b) & M32
            else:
                a, b = r[rs], r[rt]
                if b == 0:
                    self.lo, self.hi = M32, a
                else:
                    self.lo, self.hi = (a // b) & M32, (a % b) & M32
        elif n in ('madd', 'maddu', 'msub', 'msubu'):
            a = s32(r[rs]) if n in ('madd', 'msub') else r[rs]
            b = s32(r[rt]) if n in ('madd', 'msub') else r[rt]
            acc = (self.hi << 32) | self.lo
            p = a * b
            acc = (acc + p) if n in ('madd', 'maddu') else (acc - p)
            acc &= 0xFFFFFFFFFFFFFFFF
            self.hi, self.lo = (acc >> 32) & M32, acc & M32
        elif n == 'mfhi':
            self.set(rd, self.hi)
        elif n == 'mflo':
            self.set(rd, self.lo)
        elif n == 'mthi':
            self.hi = r[rs]
        elif n == 'mtlo':
            self.lo = r[rs]
        elif n in ('addi', 'addiu'):
            self.set(rt, r[rs] + i.simm)
        elif n == 'slti':
            self.set(rt, 1 if s32(r[rs]) < i.simm else 0)
        elif n == 'sltiu':
            self.set(rt, 1 if r[rs] < (i.simm & M32) else 0)
        elif n == 'andi':
            self.set(rt, r[rs] & i.imm)
        elif n == 'ori':
            self.set(rt, r[rs] | i.imm)
        elif n == 'xori':
            self.set(rt, r[rs] ^ i.imm)
        elif n == 'lui':
            self.set(rt, i.imm << 16)
        elif n == 'ext':
            self.set(rt, ext(r[rs], i.pos, i.size + 1))
        elif n == 'ins':
            self.set(rt, ins_(r[rt], r[rs], i.pos, i.size + 1 - i.pos))
        elif n == 'seb':
            self.set(rd, sext(r[rt], 8))
        elif n == 'seh':
            self.set(rd, sext(r[rt], 16))
        elif n == 'wsbh':
            v = r[rt]
            self.set(rd, ((v & 0x00FF00FF) << 8) | ((v >> 8) & 0x00FF00FF))
        elif n == 'wsbw':
            self.set(rd, wsbw(r[rt]))
        elif n == 'bitrev':
            self.set(rd, bitrev(r[rt]))
        elif n in ('lb', 'lbu', 'lh', 'lhu', 'lw', 'lwl', 'lwr', 'lwc1'):
            a = (r[rs] + i.simm) & M32
            if n == 'lb':
                self.set(rt, sext(self.mem.l8(a), 8))
            elif n == 'lbu':
                self.set(rt, self.mem.l8(a))
            elif n == 'lh':
                self.set(rt, sext(self.mem.l16(a), 16))
            elif n == 'lhu':
                self.set(rt, self.mem.l16(a))
            elif n == 'lw':
                self.set(rt, self.mem.l32(a))
            elif n == 'lwc1':
                self.f[i.ft] = self.mem.l32(a)
            elif n == 'lwl':
                sh = (a & 3) * 8
                self.set(rt, (r[rt] & (0x00FFFFFF >> sh)) | ((self.mem.l32(a & ~3) << (24 - sh)) & M32))
            else:
                sh = (a & 3) * 8
                self.set(rt, (r[rt] & ((0xFFFFFF00 << (24 - sh)) & M32)) | (self.mem.l32(a & ~3) >> sh))
        elif n in ('sb', 'sh', 'sw', 'swl', 'swr', 'swc1'):
            a = (r[rs] + i.simm) & M32
            if n == 'sb':
                self.mem.s8(a, r[rt])
            elif n == 'sh':
                self.mem.s16(a, r[rt])
            elif n == 'sw':
                self.mem.s32(a, r[rt])
            elif n == 'swc1':
                self.mem.s32(a, self.f[i.ft])
            elif n == 'swl':
                sh = (a & 3) * 8
                self.mem.s32(a & ~3, ((r[rt] >> (24 - sh)) | (self.mem.l32(a & ~3) & ((0xFFFFFF00 << sh) & M32))) & M32)
            else:
                sh = (a & 3) * 8
                self.mem.s32(a & ~3, (((r[rt] << sh) & M32) | (self.mem.l32(a & ~3) & (0x00FFFFFF >> (24 - sh)))) & M32)
        elif n in ('cache', 'sync', 'mfc0', 'mtc0', 'mfic', 'mtic'):
            if n == 'mfc0':
                self.set(rt, 0)
        elif n == 'mfc1':
            self.set(rt, self.f[i.fs])
        elif n == 'mtc1':
            self.f[i.fs] = r[rt]
        elif n == 'cfc1':
            self.set(rt, (self.fcr31 & ~0x00800000) | (0x00800000 if self.fcc else 0) if i.fs == 31 else 0)
        elif n == 'ctc1':
            if i.fs == 31:
                self.fcr31 = r[rt]
                self.fcc = bool(r[rt] & 0x00800000)
        elif n in ('add.s', 'sub.s', 'mul.s', 'div.s'):
            a, b = u2f(self.f[i.fs]), u2f(self.f[i.ft])
            try:
                if n == 'div.s':
                    if b == 0:
                        v = float('nan') if (a == 0 or a != a) else \
                            math.copysign(float('inf'), a) * math.copysign(1.0, b)
                    else:
                        v = a / b
                else:
                    v = {'add.s': a + b, 'sub.s': a - b, 'mul.s': a * b}[n]
            except ZeroDivisionError:
                v = float('nan')
            except OverflowError:
                v = math.copysign(float('inf'), a) * (math.copysign(1.0, b) if n != 'sub.s' else 1.0)
            self.f[i.fd] = f2u(v)
        elif n == 'sqrt.s':
            a = u2f(self.f[i.fs])
            self.f[i.fd] = f2u(math.sqrt(a)) if a >= 0 else f2u(float('nan'))
        elif n == 'abs.s':
            self.f[i.fd] = self.f[i.fs] & 0x7FFFFFFF
        elif n == 'neg.s':
            self.f[i.fd] = self.f[i.fs] ^ 0x80000000
        elif n == 'mov.s':
            self.f[i.fd] = self.f[i.fs]
        elif n == 'cvt.s.w':
            self.f[i.fd] = f2u(float(s32(self.f[i.fs])))
        elif n in ('trunc.w.s', 'floor.w.s', 'ceil.w.s', 'round.w.s', 'cvt.w.s'):
            a = u2f(self.f[i.fs])
            fn = {'trunc.w.s': math.trunc, 'floor.w.s': math.floor, 'ceil.w.s': math.ceil,
                  'round.w.s': redondear, 'cvt.w.s': redondear}[n]
            self.f[i.fd] = 0x7FFFFFFF if a != a or abs(a) == float('inf') else sat(fn(a))
        elif n.startswith('c.') and n.endswith('.s'):
            self.fcc = CMP[n[2:-2]](u2f(self.f[i.fs]), u2f(self.f[i.ft]))
        elif n == 'syscall':
            fn = self.hle.get(self.pc)
            raise Alto('syscall 0x%X en 0x%08X' % (i.code, i.pc)) if fn is None else fn(self)
        elif n == 'break':
            raise Alto('break en 0x%08X' % i.pc)
        else:
            raise Alto('sin implementar: %s en 0x%08X' % (n, i.pc))


def redondear(x):
    f = math.floor(x)
    d = x - f
    if d > 0.5:
        return f + 1
    if d < 0.5:
        return f
    return f if f % 2 == 0 else f + 1


def rot(v, n):
    n &= 31
    return v if n == 0 else ((v >> n) | (v << (32 - n))) & M32


def clz(v):
    v &= M32
    n = 0
    while n < 32 and not (v & 0x80000000):
        v = (v << 1) & M32
        n += 1
    return n


def ext(v, pos, size):
    return (v >> pos) & ((1 << size) - 1) if size < 32 else (v >> pos)


def ins_(dst, src, pos, size):
    if size <= 0 or size > 32:
        return dst
    m = (((1 << size) - 1) << pos) & M32
    return ((dst & ~m) | ((src << pos) & m)) & M32


def sext(v, bits):
    m = 1 << (bits - 1)
    v &= (1 << bits) - 1
    return ((v ^ m) - m) & M32


def wsbw(v):
    return ((v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000)) & M32


def bitrev(v):
    v = ((v & 0x55555555) << 1) | ((v >> 1) & 0x55555555)
    v = ((v & 0x33333333) << 2) | ((v >> 2) & 0x33333333)
    v = ((v & 0x0F0F0F0F) << 4) | ((v >> 4) & 0x0F0F0F0F)
    return wsbw(v & M32)
