#!/usr/bin/env python3
# Raises the size limits listed in games/ttt/limits.txt. Usage: patch_limits.py [--character KB] [--map KB] [--sfx KB] [--sfx-map KB] [--undo]
import struct, sys, os, shutil
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'recompiler'))
from game_data import sections, option, flag

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game')
EB = os.path.join(D, 'EBOOT.OLD')
VADDR, FOFF = 0x08804040, 0x1018


def off(va): return va - VADDR + FOFF


if flag(sys.argv, '--undo', ['--deshacer']):
    if os.path.exists(EB + '.limites'):
        shutil.copy2(EB + '.limites', EB)
        print('restored', EB)
    raise SystemExit

groups = []
for head, lines in sections('limits.txt'):
    name, kb = head[0], int(head[1])
    v = int(option(sys.argv, '--' + name.replace('_', '-'), ['--' + a for a in head[2:]], kb)) * 1024
    groups.append((name, v, [tuple(int(w, 16) for w in l.split()) for l in lines]))

if not os.path.exists(EB + '.limites'): shutil.copy2(EB, EB + '.limites')
orig = bytearray(open(EB + '.limites', 'rb').read())
d = bytearray(orig)


# lui/addiu pair: hi = high + 1 when low is negative
def pair(lui_va, addiu_va, v, name):
    lo = v & 0xFFFF
    hi = (v >> 16) + (1 if lo & 0x8000 else 0)
    for va, field in ((lui_va, hi), (addiu_va, lo)):
        o = off(va)
        w = struct.unpack_from('<I', orig, o)[0]
        if (w >> 26) not in (0x0F, 0x09): raise SystemExit('no lui/addiu at 0x%08X: %08X' % (va, w))
        struct.pack_into('<I', d, o, (w & 0xFFFF0000) | field)
    print('0x%08X/0x%08X  %s = 0x%X (%d KB)' % (lui_va, addiu_va, name, v, v // 1024))


for name, v, pairs in groups:
    for a, b in pairs: pair(a, b, v, name)

open(EB, 'wb').write(d)
print('EBOOT patched; regenerate the port (analyzer.py + generator.py + make)')
