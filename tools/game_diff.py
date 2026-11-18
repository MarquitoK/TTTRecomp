#!/usr/bin/env python3
# Compares game/EBOOT.OLD(.60) and game/overlays of two installs word by word and prints the differing addresses.
# Usage: game_diff.py DIR_A DIR_B [--max N]
import os, struct, sys


def segments(d, name):
    if name.startswith('EBOOT'):
        return [(0x08804040, 0x1018, len(d) - 0x1018)]
    phoff, phnum = struct.unpack_from('<I', d, 28)[0], struct.unpack_from('<H', d, 44)[0]
    out = []
    for i in range(phnum):
        t, o, v, p, fsz = struct.unpack_from('<IIIII', d, phoff + i * 32)
        if t == 1:
            out.append((v, o, fsz))
    return out


def va_of(segs, off):
    for v, o, n in segs:
        if o <= off < o + n:
            return '%08X' % (v + off - o)
    return 'off+%X' % off


def main():
    a, b = sys.argv[1], sys.argv[2]
    mx = int(sys.argv[sys.argv.index('--max') + 1]) if '--max' in sys.argv else 40
    names = ['EBOOT.OLD', 'EBOOT.OLD.60'] + ['overlays/' + n for n in sorted(os.listdir(os.path.join(b, 'game', 'overlays'))) if n.endswith(('.elf', '.elf.60'))]
    for n in names:
        pa, pb = os.path.join(a, 'game', n), os.path.join(b, 'game', n)
        if not (os.path.exists(pa) and os.path.exists(pb)):
            print('%s: falta en %s' % (n, a if not os.path.exists(pa) else b))
            continue
        da, db = open(pa, 'rb').read(), open(pb, 'rb').read()
        if da == db:
            continue
        segs = segments(db, os.path.basename(n))
        diffs = [o for o in range(0, min(len(da), len(db)) - 3, 4) if da[o:o + 4] != db[o:o + 4]]
        print('%s: %d palabras distintas, tamanos %d / %d' % (n, len(diffs), len(da), len(db)))
        for o in diffs[:mx]:
            print('  %s  %08X  %08X' % (va_of(segs, o), struct.unpack_from('<I', da, o)[0], struct.unpack_from('<I', db, o)[0]))


main()
