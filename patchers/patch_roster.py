#!/usr/bin/env python3
# Character select classes sized for every menu number (0..0xA6) and the map select for 126 maps, as Los ijue30s'
# patch_select/patch_sala/patch_stage; the rest is data read at run time (core/hooks/roster.inc, stages.inc). Usage: patch_roster.py [--undo]; then a full regeneration
import struct, sys, os, shutil

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game')
EB = os.path.join(ROOT, 'EBOOT.OLD')
OVL = os.path.join(ROOT, 'overlays')
OVERLAYS = ['24_18', '32_20', '48_30', '56_38', '65_41']
VADDR, FOFF = 0x08804040, 0x1018
OV_VA, OV_OFF = 0x08A814A0, 0x54
N = 0xA7
MEM_OPS = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x28, 0x29, 0x2A, 0x2B, 0x2E, 0x31, 0x39}
SP = 29


def off(va): return va - VADDR + FOFF


def classes(n):
    b = 0x1A8
    c = b + 4 * n + 0x14
    d = c + 4 * n + 0x8
    tail = d + 4 * n
    pak = tail + 4
    pantalla = dict(name='pantalla', lo=0x089F706C, hi=0x089F8FA0, size=pak + 4 * (5 + n),
                    remap=dict([(0x74 + 4 * k, pak + 4 * k) for k in range(6)] +
                               [(0x2C4 + 4 * k, c - 0x14 + 4 * k) for k in range(6)] +
                               [(0x3F4, d - 8), (0x3F8, d - 4), (0x3FC, d), (0x518, tail)]),
                    counts=[(a, 0xDD, (tail - 0x1A4) // 4) for a in (0x089F7100, 0x089F7174, 0x089F87D0, 0x089F88F8)] +
                           [(a, 0x4C, 5 + n) for a in (0x089F70E4, 0x089F71E4)] +
                           [(a, 0x47, n) for a in (0x089F7448, 0x089F8604, 0x089F8D10, 0x089F8F64)])
    g = 0x1D0
    grilla = dict(name='grilla', lo=0x089F8FA0, hi=0x089FA978, size=g + 4 * n,
                  remap=dict([(0x84 + 4 * k, 0x1C0 + 4 * k) for k in range(5)] + [(0x1B0, g + 0x47 * 4), (0x1B4, g + 0x48 * 4)]),
                  counts=[(a, 0x4D, 4 + n) for a in (0x089F9030, 0x089F90A4, 0x089F956C, 0x089F984C, 0x089F9EB4)] +
                         [(0x089F9B70, 0x47, n)])
    ce = 0x9C + 4 * n
    caja = dict(name='caja', lo=0x089FA978, hi=0x089FC124, size=ce + 0x10,
                remap={0x1C4 + 4 * k: ce + 4 * k for k in range(4)},
                counts=[(a, 0x52, 8 + n) for a in (0x089FA9E8, 0x089FAA6C, 0x089FB5F8)] +
                       [(0x089FA9F4, 0x1C4, ce), (0x089FBB68, 0x51, 8 + n), (0x089FB2B0, 0x47, n)])
    forma = dict(name='forma', lo=0x089FEF78, hi=0x08A01100, size=0x84 + 4 * n, remap={},
                 counts=[(a, 0x49, 2 + n) for a in (0x089FEFE8, 0x089FF05C, 0x089FF848)] + [(0x089FF6F4, 0x47, n)])
    zone = 0x74 + 4 * (9 + n)
    arrays = zone + 4 * 19
    sala_size = zone + 4 * (19 + 3 * n + 1)

    def sala_remap(o):
        if o == 0x1B0: return zone - 4
        if 0x1B4 <= o < 0x200: return o + (zone - 0x1B4)
        for k, a in enumerate((0x200, 0x31C, 0x438)):
            if o == a: return arrays + n * 4 * k
        if o == 0x554: return arrays + n * 12
        return None
    sala = dict(name='sala', lo=0x08A01050, hi=0x08A03C0C, size=sala_size, remap=sala_remap,
                counts=[(0x08A010A4, 0x50, 9 + n), (0x08A011A4, 0x50, 9 + n)] +
                       [(a, 0xE9, 19 + 3 * n + 1) for a in (0x08A010C0, 0x08A01134, 0x08A016B8)] +
                       [(0x08A01AE0, 0x47, n), (0x08A02950, 0x47, n)])
    return [pantalla, grilla, caja, forma, sala]


# map select (Los ijue30s' patch_stage): its three per-cell arrays (thumbnails 0x8C, big renders 0xC0, names 0xEC) move to
# the object's tail, one word per cell for 126 maps + Random; counts and states are answered at run time (core/hooks/stages.inc)
ST_CELLS = 127
ST_ARRAYS = (0x8C, 0xC0, 0xEC)
ST_SIZE = 0x13C + 4 * ST_CELLS * len(ST_ARRAYS)


def patch_stage(d):
    tail = {o: 0x13C + 4 * ST_CELLS * k for k, o in enumerate(ST_ARRAYS)}
    moved = 0
    for va in range(0x08A03A00, 0x08A05E24, 4):
        w = struct.unpack_from('<I', d, off(va))[0]
        if w >> 26 not in MEM_OPS or (w >> 21) & 31 == SP or (w & 0xFFFF) not in tail: continue
        struct.pack_into('<I', d, off(va), (w & 0xFFFF0000) | tail[w & 0xFFFF])
        moved += 1
    if moved != 27: raise SystemExit('map select: 27 array accesses expected, %d found' % moved)
    print('map select: 0x%X bytes, %d accesses moved' % (ST_SIZE, moved))


# immediates outside the classes: battle id ranges (patch_battle) and the loops over every number (patch_unlock)
EXTRA = [(0x089C60F8, 0xF, 6), (0x089C619C, 0xF, 6), (0x0886FEF4, 0x4D, 0x50), (0x089CEE90, 0x46, N), (0x0885745C, 0x46, N)]
CTORS = [('pantalla', 0x089F708C, 0x51C), ('grilla', 0x089F8FB8, 0x1C0), ('caja', 0x089FA978, 0x1D4),
         ('forma', 0x089FEF78, 0x1A0), ('sala', 0x08A01050, 0x558), ('stage', 0x08A03C0C, 0x13C)]


def set_imm(d, o, va, old, new):
    w = struct.unpack_from('<I', d, o)[0]
    if w & 0xFFFF != old: raise SystemExit('0x%08X: immediate 0x%X expected, word %08X' % (va, old, w))
    struct.pack_into('<I', d, o, (w & 0xFFFF0000) | (new & 0xFFFF))


def patch_main(d):
    sizes = {}
    for cls in classes(N):
        moved = 0
        rm = cls['remap']
        get = rm if callable(rm) else rm.get
        for va in range(cls['lo'], cls['hi'], 4):
            w = struct.unpack_from('<I', d, off(va))[0]
            if w >> 26 not in MEM_OPS or (w >> 21) & 31 == SP or w & 0x8000: continue
            new = get(w & 0xFFFF)
            if new is None: continue
            struct.pack_into('<I', d, off(va), (w & 0xFFFF0000) | new)
            moved += 1
        for va, old, new in cls['counts']: set_imm(d, off(va), va, old, new)
        sizes[cls['name']] = cls['size']
        print('%s: 0x%X bytes, %d accesses moved, %d counts' % (cls['name'], cls['size'], moved, len(cls['counts'])))
    for va, old, new in EXTRA: set_imm(d, off(va), va, old, new)
    patch_stage(d)
    sizes['stage'] = ST_SIZE
    return sizes


def patch_overlay(path, sizes):
    d = bytearray(open(path, 'rb').read())
    n = 0
    for name, ctor, old in CTORS:
        jal = struct.pack('<I', 0x0C000000 | ((ctor >> 2) & 0x3FFFFFF))
        c = d.find(jal)
        while c != -1:
            for j in range(c - 4, max(0, c - 0x40) - 1, -4):
                w = struct.unpack_from('<I', d, j)[0]
                if w >> 16 == 0x3404 and w & 0xFFFF >= old:
                    struct.pack_into('<I', d, j, 0x34040000 | sizes[name])
                    n += 1
                    break
            else:
                raise SystemExit('%s: no size before the %s constructor at 0x%08X' % (os.path.basename(path), name, c - OV_OFF + OV_VA))
            c = d.find(jal, c + 4)
    open(path, 'wb').write(d)
    print('%s: %d allocations' % (os.path.basename(path), n))
    return n


def backup(p):
    if not os.path.exists(p + '.roster'): shutil.copy2(p, p + '.roster')
    return bytearray(open(p + '.roster', 'rb').read())


main = [EB] + [EB + '.60'] * os.path.exists(EB + '.60')
ovls = [os.path.join(OVL, o + '.elf') for o in OVERLAYS]
ovls += [o + '.60' for o in ovls if os.path.exists(o + '.60')]
if '--undo' in sys.argv:
    for p in main + ovls:
        if os.path.exists(p + '.roster'):
            shutil.copy2(p + '.roster', p)
            print('restored', p)
    raise SystemExit
for p in main:
    d = backup(p)
    sizes = patch_main(d)
    open(p, 'wb').write(d)
total = 0
for p in ovls:
    if not os.path.exists(p): raise SystemExit('missing ' + p)
    d = backup(p)
    open(p, 'wb').write(d)
    total += patch_overlay(p, sizes)
if not total: raise SystemExit('no character select constructor found in the overlays')
print('EBOOT and overlays patched for menu numbers up to 0x%X and %d maps; regenerate the port (analyzer.py + generator.py + make)' % (N - 1, ST_CELLS - 1))
