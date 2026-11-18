#!/usr/bin/env python3
# Coverage corpus report (TTT_COVERAGE). Usage: coverage.py [corpus] [--missing IMAGE] [--callers ADDR] [--callees ADDR] [--merge A B ... -o OUT]
import os, sys, json, bisect
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'recompiler'))
from analyzer import Imagen
from game_data import section, addresses

DEFAULT = os.path.join(ROOT, 'coverage', 'corpus.tsv')
BANK_VA, BANK_END = 0x08A814A0, 0x08AEFB70


def load(path):
    runs, rows = 0, {}
    for l in open(path, encoding='utf-8'):
        l = l.rstrip('\r\n')
        if l.startswith('#runs\t'): runs = int(l[6:]); continue
        p = l.split('\t')
        if len(p) < 6 or p[0] == 'kind': continue
        k = tuple(p[:4])
        n, r = rows.get(k, (0, 0, ''))[:2]
        rows[k] = (n + int(p[4]), r + int(p[5]), p[6] if len(p) > 6 else '')
    return runs, rows


def save(path, runs, rows):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('#runs\t%d\nkind\toverlay\taddr\tsite\tcount\truns\tname\n' % runs)
        for k, v in sorted(rows.items(), key=lambda x: -x[1][0]):
            f.write('\t'.join(k) + '\t%d\t%d\t%s\n' % v)


def program():
    doc = json.load(open(os.path.join(ROOT, 'generated', 'programa.json')))
    stubs = {f['stub'] for lib in Imagen(os.path.join(ROOT, 'game', 'EBOOT.OLD'), 'principal').e.imports() for f in lib['funcs']}
    fns = {img['nombre']: sorted((int(f['a'], 16), int(f['fin'], 16)) for f in img['funciones']) for img in doc['imagenes']}
    fns['principal'] = [x for x in fns['principal'] if x[0] not in stubs]
    return fns, doc


def image_of(addr, overlay):
    return overlay if BANK_VA <= addr < BANK_END and overlay != '-' else 'principal'


class Code:
    def __init__(self, doc):
        self.doc, self.imgs = doc, {}

    def word(self, image, va):
        if image not in self.imgs:
            if image == 'principal': p = os.path.join(ROOT, 'game', 'EBOOT.OLD')
            else: p = os.path.join(ROOT, 'game', 'overlays', image + '.elf')
            self.imgs[image] = Imagen(p, image) if os.path.exists(p) else None
        img = self.imgs[image]
        try: return img.u32(va) if img else None
        except Exception: return None

    def kind(self, image, site):
        w = self.word(image, site)
        if w is None: return '?'
        if w >> 26 == 3: return 'jal'
        if w >> 26 == 0 and w & 0x3F == 9: return 'jalr'
        return 'other'


def owner(fns, addr):
    i = bisect.bisect_right(fns, (addr, 0xFFFFFFFF)) - 1
    return fns[i][0] if i >= 0 and fns[i][0] <= addr < fns[i][1] else None


def report(path):
    runs, rows = load(path)
    fns, doc = program()
    code = Code(doc)
    hooks = set(addresses(section('recomp.txt', 'hooks')))
    wrappers = set(addresses(section('recomp.txt', 'wrappers')))
    by = {}
    for k, v in rows.items(): by.setdefault(k[0], []).append((k, v))
    print('corpus %s: %d runs, %d rows' % (path, runs, len(rows)))
    print('  ' + ', '.join('%s %d' % (k, len(by.get(k, []))) for k in ('fn', 'hle', 'jalr', 'unresolved_call', 'unresolved_jump')))
    ran = {}
    for (kind, ov, a, s), v in by.get('fn', []):
        a = int(a, 16)
        ran.setdefault(image_of(a, ov), {}).setdefault(a, 0)
        ran[image_of(a, ov)][a] += v[0]
    print('\nfunctions run per image:')
    for img, lst in fns.items():
        r = ran.get(img, {})
        print('  %-12s %5d / %5d  %5.1f %%' % (img, len(r), len(lst), 100.0 * len(r) / max(1, len(lst))))
    main = ran.get('principal', {})
    print('\nhooks run %d / %d, wrappers run %d / %d' % (len(hooks & set(main)), len(hooks), len(wrappers & set(main)), len(wrappers)))
    hle = {}
    for (kind, ov, a, s), v in by.get('hle', []):
        n = hle.setdefault(v[2] or a, [0, 0]); n[0] += v[0]; n[1] += 1
    print('\nHLE imports served: %d' % len(hle))
    for name, (n, sites) in sorted(hle.items(), key=lambda x: -x[1][0])[:25]:
        print('  %-50s %12d  %3d sites' % (name, n, sites))
    starts = {img: {x for x, _ in lst} for img, lst in fns.items()}
    missed = [(k, v) for k, v in by.get('jalr', []) if k[3] != '-' and int(k[2], 16) not in starts.get(image_of(int(k[2], 16), k[1]), ())
              and code.kind(image_of(int(k[3], 16), k[1]), int(k[3], 16)) == 'jal']
    print('\nstatic calls to functions the analyzer missed: %d' % len(missed))
    for (kind, ov, a, s), v in sorted(missed, key=lambda x: -x[1][0])[:40]:
        print('  %s -> %s  %-10s x%d' % (s, a, ov, v[0]))
    for kind in ('unresolved_call', 'unresolved_jump'):
        lst = by.get(kind, [])
        print('\n%s: %d' % (kind, len(lst)))
        for (k, ov, a, s), v in sorted(lst, key=lambda x: -x[1][0])[:40]:
            print('  target %s  site %s  %-10s x%d in %d runs' % (a, s, ov, v[0], v[1]))
    tg = {}
    for (kind, ov, a, s), v in by.get('jalr', []):
        t = tg.setdefault((a, image_of(int(a, 16), ov)), [0, 0]); t[0] += v[0]; t[1] += 1
    print('\nindirect targets: %d (top 25)' % len(tg))
    for (a, img), (n, sites) in sorted(tg.items(), key=lambda x: -x[1][0])[:25]:
        print('  %s %-10s %12d  %3d sites' % (a, img, n, sites))


def missing(path, image):
    runs, rows = load(path)
    fns, doc = program()
    if image not in fns: raise SystemExit('unknown image %s (available: %s)' % (image, ', '.join(fns)))
    ran = {int(k[2], 16) for k in rows if k[0] == 'fn' and image_of(int(k[2], 16), k[1]) == image}
    for a, b in fns[image]:
        if a not in ran: print('0x%08X  %5d instr' % (a, (b - a) // 4))


def callers(path, target):
    runs, rows = load(path)
    fns, doc = program()
    agg = {}
    for (kind, ov, a, s), v in rows.items():
        if kind in ('fn', 'jalr', 'hle') and int(a, 16) == target and s != '-':
            si = int(s, 16)
            o = owner(fns.get(image_of(si, ov), []), si)
            k = (s, '0x%08X' % o if o else '?', kind, ov)
            agg[k] = agg.get(k, 0) + v[0]
    for (s, o, kind, ov), n in sorted(agg.items(), key=lambda x: -x[1]):
        print('  site %s in %s  %-4s %-10s x%d' % (s, o, kind, ov, n))


def callees(path, fn_addr):
    runs, rows = load(path)
    fns, doc = program()
    agg = {}
    for (kind, ov, a, s), v in rows.items():
        if s == '-' or kind == 'unresolved_jump': continue
        si = int(s, 16)
        if owner(fns.get(image_of(si, ov), []), si) != fn_addr: continue
        k = (a, kind, v[2])
        agg[k] = agg.get(k, 0) + v[0]
    for (a, kind, name), n in sorted(agg.items(), key=lambda x: -x[1]):
        print('  %s %-15s %-10d %s' % (a, kind, n, name))


def main():
    a = sys.argv[1:]
    if '--merge' in a:
        i = a.index('--merge')
        out = a[a.index('-o') + 1] if '-o' in a else DEFAULT
        ins = [x for x in a[i + 1:] if x != '-o' and x != out]
        runs, rows = 0, {}
        for p in ins:
            r, rw = load(p)
            runs += r
            for k, v in rw.items():
                n, rn, nm = rows.get(k, (0, 0, ''))
                rows[k] = (n + v[0], rn + v[1], nm or v[2])
        save(out, runs, rows)
        print('merged %d corpora: %d runs, %d rows -> %s' % (len(ins), runs, len(rows), out))
        return
    opts = {k: a[a.index(k) + 1] for k in ('--missing', '--callers', '--callees') if k in a}
    path = next((x for i, x in enumerate(a) if not x.startswith('--') and (i == 0 or a[i - 1] not in opts)), DEFAULT)
    if not os.path.exists(path): raise SystemExit('no corpus at %s (run the game with TTT_COVERAGE=1)' % path)
    if '--missing' in opts: missing(path, opts['--missing'])
    elif '--callers' in opts: callers(path, int(opts['--callers'], 16))
    elif '--callees' in opts: callees(path, int(opts['--callees'], 16))
    else: report(path)


if __name__ == '__main__':
    main()
