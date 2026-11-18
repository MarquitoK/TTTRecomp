#!/usr/bin/env python3
# Colours of the 70 original characters in assets/: keeps only the colours the original game has.
#   python3 tools/colors.py            check: what would be removed
#   python3 tools/colors.py --original removes the extra colours
import argparse, os, shutil, struct

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
MODEL_INI, MODEL_STEP = 1428, 5
REC, REC_N, REC_COUNT = 60, 161, 6
GROUP = [255, 255, 255, 0, 1, 2, 3, 255, 255, 255, 255, 255, 255, 4, 5, 6, 7, 8, 9, 10, 255, 11, 255, 12, 255, 13, 14, 15, 16, 17, 255, 255, 255, 255, 18, 19, 20, 21, 255, 24, 25, 26, 27, 255, 22, 23, 29, 30, 255, 31, 32, 255, 34, 33, 255, 255, 28,
         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 35, 255, 255, 255, 36, 255, 37, 38, 255, 40, 41, 42, 39, 43, 44, 47, 46, 45, 48, 49, 50, 51, 52, 255, 255, 255, 69, 53, 54, 55, 56, 57, 59, 60, 61, 62, 58, 255,
         63, 64, 255, 65, 255, 66, 67] + [255] * 15 + [68] + [255] * 27
# BT3 id -> colours in the original game (Goku family and Gohan kid 3; Saibaiman, Freezer soldier and Cell Jr 4; the rest 2)
ORIGINAL = {s: 2 for s in range(REC_N) if GROUP[s] != 255}
ORIGINAL.update({3: 3, 4: 3, 5: 3, 6: 3, 13: 3, 79: 4, 99: 4, 109: 4})


def manifest(assets):
    rutas = {}
    with open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8') as f:
        for l in f:
            if l.startswith('#'):
                continue
            p = l.rstrip('\r\n').split('\t')
            if len(p) >= 7:
                rutas[int(p[0])] = p[6]
    return rutas


def color_count_file(assets, rutas):
    pk = os.path.join(assets, 'Packfile')
    for ruta in rutas.values():
        if not ruta.lower().endswith('system_en.pak'):
            continue
        for d in (os.path.join(pk, ruta), os.path.join(pk, ruta[:-4])):
            g = os.path.join(d, '2_general')
            if os.path.isdir(g):
                for n in sorted(os.listdir(g)):
                    if n.startswith('037'):
                        return open(os.path.join(g, n), 'rb').read()
        f = os.path.join(pk, ruta)
        if os.path.isfile(f):
            d = open(f, 'rb').read()
            n = struct.unpack_from('>I', d, 0)[0]
            offs = struct.unpack_from('>%dI' % (n + 1), d, 4)
            for i in range(n):
                if offs[i + 1] - offs[i] >= 4 + REC * REC_N and i >= 30:
                    e = d[offs[i]:offs[i + 1]]
                    if all(e[4 + REC * s + REC_COUNT] in (1, 2, 3, 4) for s in range(REC_N)):
                        return e
    return None


def all_four(assets, rutas=None):
    rutas = rutas or manifest(assets)
    d = color_count_file(assets, rutas)
    return d is not None and all(d[4 + REC * s + REC_COUNT] == 4 for s in ORIGINAL)


def extras(assets, rutas):
    pk = os.path.join(assets, 'Packfile')
    out = []
    for s, keep in sorted(ORIGINAL.items()):
        for j in range(keep, 4):
            ruta = rutas.get(MODEL_INI + MODEL_STEP * GROUP[s] + j)
            if not ruta:
                continue
            for r in (ruta, ruta[:-4] if ruta.lower().endswith('.pak') else None):
                if r and os.path.exists(os.path.join(pk, r)):
                    out.append(r)
    return out


def original(assets, apply):
    rutas = manifest(assets)
    if not all_four(assets, rutas):
        print('colors: the original characters do not all have 4 colours; nothing to do')
        return 0
    lst, removed = extras(assets, rutas), 0
    for r in lst:
        src = os.path.join(assets, 'Packfile', r)
        if apply:
            if os.path.isdir(src):
                shutil.rmtree(src)
            elif os.path.isfile(src):
                os.remove(src)
            removed += 1
        else:
            print('  ' + r)
    print('colors: %d extra colours %s' % (len(lst), 'removed' if apply else 'would be removed (run with --original)'))
    return removed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--original', action='store_true')
    a = ap.parse_args()
    original(a.assets, a.original)


if __name__ == '__main__':
    main()
