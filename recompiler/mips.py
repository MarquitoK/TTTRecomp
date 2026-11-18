import struct

GPR = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
       's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']
COND = ['f', 'un', 'eq', 'ueq', 'olt', 'ult', 'ole', 'ule', 'sf', 'ngle', 'seq', 'ngl', 'lt', 'nge', 'le', 'ngt']

SPECIAL = {0x20: 'add', 0x21: 'addu', 0x22: 'sub', 0x23: 'subu', 0x24: 'and', 0x25: 'or', 0x26: 'xor',
           0x27: 'nor', 0x2A: 'slt', 0x2B: 'sltu', 0x2C: 'max', 0x2D: 'min', 0x0A: 'movz', 0x0B: 'movn',
           0x08: 'jr', 0x09: 'jalr', 0x0C: 'syscall', 0x0D: 'break', 0x0F: 'sync',
           0x10: 'mfhi', 0x11: 'mthi', 0x12: 'mflo', 0x13: 'mtlo', 0x16: 'clz', 0x17: 'clo',
           0x18: 'mult', 0x19: 'multu', 0x1A: 'div', 0x1B: 'divu', 0x1C: 'madd', 0x1D: 'maddu',
           0x2E: 'msub', 0x2F: 'msubu'}
REGIMM = {0: 'bltz', 1: 'bgez', 2: 'bltzl', 3: 'bgezl', 0x10: 'bltzal', 0x11: 'bgezal',
          0x12: 'bltzall', 0x13: 'bgezall'}
IMM_OPS = {8: 'addi', 9: 'addiu', 0xA: 'slti', 0xB: 'sltiu', 0xC: 'andi', 0xD: 'ori', 0xE: 'xori'}
LOADS = {0x20: 'lb', 0x21: 'lh', 0x22: 'lwl', 0x23: 'lw', 0x24: 'lbu', 0x25: 'lhu', 0x26: 'lwr'}
STORES = {0x28: 'sb', 0x29: 'sh', 0x2A: 'swl', 0x2B: 'sw', 0x2E: 'swr'}
FS_OPS = {0: 'add.s', 1: 'sub.s', 2: 'mul.s', 3: 'div.s', 4: 'sqrt.s', 5: 'abs.s', 6: 'mov.s', 7: 'neg.s',
          0xC: 'round.w.s', 0xD: 'trunc.w.s', 0xE: 'ceil.w.s', 0xF: 'floor.w.s', 0x24: 'cvt.w.s'}
BITS = {2: 'wsbh', 3: 'wsbw', 0x10: 'seb', 0x14: 'bitrev', 0x18: 'seh'}

BRANCHES = set(REGIMM.values()) | {'beq', 'bne', 'beql', 'bnel', 'blez', 'bgtz', 'blezl', 'bgtzl',
                                   'bc1f', 'bc1t', 'bc1fl', 'bc1tl', 'bvf', 'bvt', 'bvfl', 'bvtl'}
LIKELY = {'beql', 'bnel', 'blezl', 'bgtzl', 'bltzl', 'bgezl', 'bltzall', 'bgezall', 'bc1fl', 'bc1tl',
          'bvfl', 'bvtl'}
LINKING = {'bltzal', 'bgezal', 'bltzall', 'bgezall'}
FLOAT_DST = set(FS_OPS.values()) | {'cvt.s.w'}


class Ins:
    __slots__ = ('name', 'pc', 'w', 'rs', 'rt', 'rd', 'sa', 'imm', 'simm', 'fs', 'ft', 'fd', 'tgt', 'code', 'pos', 'size')

    def __init__(self, name, pc, w):
        self.name, self.pc, self.w = name, pc, w
        self.rs = (w >> 21) & 31
        self.rt = (w >> 16) & 31
        self.rd = (w >> 11) & 31
        self.sa = (w >> 6) & 31
        self.imm = w & 0xFFFF
        self.simm = self.imm - 0x10000 if self.imm & 0x8000 else self.imm
        self.fs, self.ft, self.fd = self.rd, self.rt, self.sa
        self.tgt = ((pc + 4) & 0xF0000000) | ((w & 0x3FFFFFF) << 2)
        self.code = (w >> 6) & 0xFFFFF
        self.pos = self.sa
        self.size = (w >> 11) & 31

    @property
    def vbit(self):
        return (self.w >> 18) & 7

    @property
    def branch_target(self):
        return self.pc + 4 + self.simm * 4

    def __repr__(self):
        return '%08x %08x %s' % (self.pc, self.w, self.name)


VFPU_OPS = {0x12, 0x18, 0x19, 0x1B, 0x32, 0x34, 0x35, 0x36, 0x37, 0x3A, 0x3C, 0x3D, 0x3E, 0x3F}


def is_vfpu(w):
    return (w >> 26) in VFPU_OPS


def decode(w, pc=0):
    op = w >> 26
    rs, rt, rd, sa, fn = (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31, (w >> 6) & 31, w & 0x3F
    name = None
    if op == 0:
        if fn == 2:
            name = 'rotr' if rs == 1 else 'srl' if rs == 0 else None
        elif fn == 6:
            name = 'rotrv' if sa == 1 else 'srlv' if sa == 0 else None
        elif fn == 0:
            name = 'sll' if rs == 0 else None
        elif fn == 3:
            name = 'sra' if rs == 0 else None
        elif fn == 4:
            name = 'sllv' if sa == 0 else None
        elif fn == 7:
            name = 'srav' if sa == 0 else None
        else:
            name = SPECIAL.get(fn)
    elif op == 1:
        name = REGIMM.get(rt)
    elif op == 2:
        name = 'j'
    elif op == 3:
        name = 'jal'
    elif op == 4:
        name = 'beq'
    elif op == 5:
        name = 'bne'
    elif op == 6 and rt == 0:
        name = 'blez'
    elif op == 7 and rt == 0:
        name = 'bgtz'
    elif op in IMM_OPS:
        name = IMM_OPS[op]
    elif op == 0xF and rs == 0:
        name = 'lui'
    elif op == 0x12 and rs == 8:
        name = ['bvf', 'bvt', 'bvfl', 'bvtl'][rt & 3]
    elif op == 0x10:
        name = 'mfc0' if rs == 0 else 'mtc0' if rs == 4 else None
    elif op == 0x11:
        if rs == 0:
            name = 'mfc1'
        elif rs == 2:
            name = 'cfc1'
        elif rs == 4:
            name = 'mtc1'
        elif rs == 6:
            name = 'ctc1'
        elif rs == 8:
            name = ['bc1f', 'bc1t', 'bc1fl', 'bc1tl'][rt & 3]
        elif rs == 0x10:
            name = 'c.%s.s' % COND[fn & 15] if 0x30 <= fn <= 0x3F else FS_OPS.get(fn)
        elif rs == 0x14 and fn == 0x20:
            name = 'cvt.s.w'
    elif op == 0x14:
        name = 'beql'
    elif op == 0x15:
        name = 'bnel'
    elif op == 0x16 and rt == 0:
        name = 'blezl'
    elif op == 0x17 and rt == 0:
        name = 'bgtzl'
    elif op == 0x1C:
        name = 'mfic' if fn == 0x24 else 'mtic' if fn == 0x26 else None
    elif op == 0x1F:
        if fn == 0:
            name = 'ext'
        elif fn == 4:
            name = 'ins'
        elif fn == 0x20 and rs == 0:
            name = BITS.get((w >> 6) & 31)
    elif op in LOADS:
        name = LOADS[op]
    elif op in STORES:
        name = STORES[op]
    elif op == 0x2F:
        name = 'cache'
    elif op == 0x31:
        name = 'lwc1'
    elif op == 0x39:
        name = 'swc1'
    if name is None or not _valid(name, rs, rt, rd, sa, fn):
        return None
    return Ins(name, pc, w)


ZERO = {}
for _n in ('add', 'addu', 'sub', 'subu', 'and', 'or', 'xor', 'nor', 'slt', 'sltu', 'max', 'min', 'movz', 'movn'):
    ZERO[_n] = ('sa',)
ZERO['jr'] = ('rt', 'rd', 'sa')
ZERO['jalr'] = ('rt', 'sa')
ZERO['sync'] = ('rs', 'rt', 'rd', 'sa')
ZERO['mfhi'] = ZERO['mflo'] = ('rs', 'rt', 'sa')
ZERO['mthi'] = ZERO['mtlo'] = ('rt', 'rd', 'sa')
ZERO['clz'] = ZERO['clo'] = ('rt', 'sa')
for _n in ('mult', 'multu', 'div', 'divu', 'madd', 'maddu', 'msub', 'msubu'):
    ZERO[_n] = ('rd', 'sa')
for _n in ('blez', 'bgtz', 'blezl', 'bgtzl'):
    ZERO[_n] = ('rt',)
for _n in ('mfc0', 'mtc0', 'mfc1', 'cfc1', 'mtc1', 'ctc1'):
    ZERO[_n] = ('sa', 'fn')
for _n in ('sqrt.s', 'abs.s', 'mov.s', 'neg.s', 'round.w.s', 'trunc.w.s', 'ceil.w.s', 'floor.w.s',
           'cvt.w.s', 'cvt.s.w'):
    ZERO[_n] = ('rt',)
for _c in COND:
    ZERO['c.%s.s' % _c] = ('sa',)
ZERO['mfic'] = ZERO['mtic'] = ('rs', 'sa')


def _valid(name, rs, rt, rd, sa, fn):
    z = ZERO.get(name)
    if not z:
        return True
    v = dict(rs=rs, rt=rt, rd=rd, sa=sa, fn=fn)
    return all(v[f] == 0 for f in z)


def words(data, off, n):
    return struct.unpack_from('<%dI' % n, data, off)
