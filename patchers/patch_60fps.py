#!/usr/bin/env python3
# 60 fps layers from games/ttt/fps60_layers.txt. Usage: patch_60fps.py [--layers a,b] [--undo] [--status]
import struct, sys, os, shutil
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'recompiler'))
from game_data import sections, option, flag

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game')


def off_eboot(va): return va - 0x8804040 + 0x1018


def off_overlay(d, va):
    phoff = struct.unpack_from('<I', d, 28)[0]
    phnum = struct.unpack_from('<H', d, 44)[0]
    for i in range(phnum):
        t, o, v, p, fsz = struct.unpack_from('<IIIII', d, phoff + i * 32)
        if t == 1 and v <= va < v + fsz: return va - v + o
    raise SystemExit('address %08X not found in the overlay' % va)


def load_layers():
    layers, order, alias = {}, [], {}
    for head, lines in sections('fps60_layers.txt'):
        name = head[0]
        order.append(name)
        for a in head[1:]: alias[a] = name
        rows = []
        for l in lines:
            code, desc = l.split('|', 1)
            w = code.split()
            k = w.index('>')
            rows.append((os.path.join(D, w[0]), int(w[1], 16), [int(x, 16) for x in w[2:k]], [int(x, 16) for x in w[k + 1:]], desc.strip()))
        layers[name] = rows
    return layers, order, alias


LAYERS, ORDER, ALIAS = load_layers()


def read(path): return bytearray(open(path, 'rb').read())


def words(d, o, n): return [struct.unpack_from('<I', d, o + 4 * i)[0] for i in range(n)]


def offset(path, d, va): return off_eboot(va) if os.path.basename(path) == 'EBOOT.OLD' else off_overlay(d, va)


def apply(layer, undo):
    for path, va, old, new, desc in LAYERS[layer]:
        d = read(path)
        o = offset(path, d, va)
        w = words(d, o, len(old))
        src, dst = (new, old) if undo else (old, new)
        if w == dst: print('%s: already %s (%s)' % (layer, 'undone' if undo else 'applied', desc)); continue
        if w != src: raise SystemExit('%s: at %08X expected %s and found %s' % (os.path.basename(path), va, ' '.join('%08X' % x for x in src), ' '.join('%08X' % x for x in w)))
        if not os.path.exists(path + '.60'): shutil.copy2(path, path + '.60')
        for i, x in enumerate(dst): struct.pack_into('<I', d, o + 4 * i, x)
        open(path, 'wb').write(d)
        print('%s: %s (%s)' % (layer, 'undone' if undo else 'applied', desc))


def status():
    for layer in ORDER:
        n = 0
        for path, va, old, new, desc in LAYERS[layer]:
            d = read(path)
            n += words(d, offset(path, d, va), len(new)) == new
        print('%s: %d/%d applied' % (layer, n, len(LAYERS[layer])))


args = sys.argv[1:]
if flag(args, '--status', ['--estado']): status(); sys.exit()
undo = flag(args, '--undo', ['--deshacer'])
chosen = ORDER
sel = option(args, '--layers', ['--capas'])
if sel is not None: chosen = [ALIAS.get(c, c) for c in sel.split(',')]
for c in chosen:
    if c not in LAYERS: raise SystemExit('unknown layer: %s (available: %s)' % (c, ', '.join(ORDER)))
for c in (reversed(chosen) if undo else chosen): apply(c, undo)
