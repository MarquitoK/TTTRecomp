#!/usr/bin/env python3
# Proposes 60 fps store rules from step censuses (TTT_STEPCENSUS). Usage: step_sites.py [census.csv] | --compare A.csv B.csv [--write]
# | --probe step_probe.csv [--write]: judges the rules of fps60_sites.txt with a TTT_STEP_PROBE run
# | --mark A.csv B.csv: sites specific to the Ctrl+F7 marked window, 30 vs 60
import os, sys, json, bisect
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'recompiler'))
from game_data import GAME, section, addresses

DEFAULT = os.path.join(ROOT, 'generated', 'src', 'step_census.csv')
OUT = os.path.join(GAME, 'fps60_sites.txt')
MIN_FRAMES, CHAIN, MODE, FLOAT = 20, 0.8, 0.9, 0.9
BATTLE = (0x08804040, 0x089C0000)   # fixed 30 Hz battle engine (EBOOT_TTT 24.2); above it the engine scales by the step


def functions():
    doc = json.load(open(os.path.join(ROOT, 'generated', 'programa.json')))
    return sorted((int(f['a'], 16), int(f['fin'], 16)) for f in doc['imagenes'][0]['funciones'])


def owner(fns, a):
    i = bisect.bisect_right(fns, (a, 0xFFFFFFFF)) - 1
    return fns[i] if i >= 0 and fns[i][0] <= a < fns[i][1] else None


def dual(fns):
    p = os.path.join(ROOT, 'game', 'EBOOT.OLD')
    if not os.path.exists(p + '.60'): return set()
    a, b = open(p, 'rb').read(), open(p + '.60', 'rb').read()
    off = lambda va: va - 0x8804040 + 0x1018
    return {f for f in fns if a[off(f[0]):off(f[1])] != b[off(f[0]):off(f[1])]}


def load(path):
    ticks, rows = 0, []
    for l in open(path):
        l = l.strip()
        if l.startswith('#ticks,'):
            h = l[7:].split(',')
            ticks, load.hz = int(h[0]), int(h[1]) if len(h) > 1 else 30
            load.marked = int(h[2]) if len(h) > 2 else 0
            continue
        if not l or l.startswith('pc,'): continue
        p = l.split(',')
        r = dict(pc=int(p[0], 16), size=int(p[1]), count=int(p[2]), frames=int(p[3]), per=int(p[4]), chain=int(p[5]), zero=int(p[6]),
                 fplaus=int(p[7]), fmode=float(p[8]), fmode_n=int(p[9]), fother=int(p[10]), imode=int(p[11]), imode_n=int(p[12]),
                 iother=int(p[13]), lo=int(p[14], 16), hi=int(p[15], 16),
                 fabs=float(p[16]) if len(p) > 16 else 0.0, iabs=float(p[17]) if len(p) > 17 else 0.0)
        if len(p) > 22: r.update(mcount=int(p[18]), mzero=int(p[19]), mframes=int(p[20]), mabs=float(p[21]), mchain=int(p[22]))
        rows.append(r)
    return ticks, rows


def classify(r):
    moved = r['count'] - r['zero']
    if r['frames'] < MIN_FRAMES or moved < MIN_FRAMES or r['chain'] < CHAIN * r['count']: return None
    if r['size'] == 4 and r['fplaus'] >= FLOAT * moved and r['imode_n'] < MODE * moved: return 'f'
    if r['imode_n'] >= MODE * moved:
        if r['imode'] == 1: return 'u'
        if r['imode'] == -1: return 'd'
        if abs(r['imode']) >= 2 and r['fplaus'] < FLOAT * moved: return 'j'
    return None


def is_float(r):
    moved = r['count'] - r['zero']
    return r['size'] == 4 and moved and r['fplaus'] >= FLOAT * moved and r['imode_n'] < MODE * moved


def compare(p1, p2, write, everywhere):
    t1, r1 = load(p1); h1 = load.hz
    t2, r2 = load(p2); h2 = load.hz
    if {h1, h2} != {30, 60}: raise SystemExit('need one census at 30 fps and one at 60 fps (got %d and %d)' % (h1, h2))
    (t30, a30), (t60, a60) = ((t1, r1), (t2, r2)) if h1 == 30 else ((t2, r2), (t1, r1))
    b60 = {r['pc']: r for r in a60}
    fns = functions()
    handled = dual(fns)
    wrapped = set(addresses(section('recomp.txt', 'wrappers')))
    groups = {'accelerated': [], 'scaled': [], 'unclear': []}
    skipped = 0
    for r in a30:
        q = b60.get(r['pc'])
        if not q or r['frames'] < MIN_FRAMES or r['count'] - r['zero'] < 30 or q['count'] - q['zero'] < 30: continue
        if r['chain'] < CHAIN * r['count']: continue
        f = owner(fns, r['pc'])
        if f in handled or (f and f[0] in wrapped): skipped += 1; continue
        fl = is_float(r)
        if not everywhere and not BATTLE[0] <= r['pc'] < BATTLE[1]: continue
        m30, m60 = (r['fabs'], q['fabs']) if fl else (r['iabs'], q['iabs'])
        if m30 <= 0: continue
        # change per second: mean change per moving store x moving stores per second
        ratio = (m60 * (q['count'] - q['zero']) / t60 * 60) / (m30 * (r['count'] - r['zero']) / t30 * 30)
        r.update(fn=f[0] if f else 0, ratio=ratio, kind=classify(r) or ('f' if fl else '?'), ticks60=q['frames'])
        groups['accelerated' if ratio >= 1.6 else 'scaled' if 0.7 <= ratio <= 1.4 else 'unclear'].append(r)
    print('census 30 fps: %d ticks, %d sites; 60 fps: %d ticks, %d sites' % (t30, len(a30), t60, len(a60)))
    print('accumulator sites in both%s: accelerated %d, right speed %d, unclear %d; already handled %d' % (' (battle engine)' if not everywhere else '',
        len(groups['accelerated']), len(groups['scaled']), len(groups['unclear']), skipped))
    acc = sorted(groups['accelerated'], key=lambda r: -r['frames'])
    by_fn = {}
    for r in acc: by_fn.setdefault(r['fn'], []).append(r)
    print('\naccelerated by function (%d functions, top 60 sites by ticks):' % len(by_fn))
    for r in acc[:60]:
        print('  0x%08X %s fn 0x%08X  ticks %5d/%5d  change per second 30->60 x%.2f  addr 0x%08X..0x%08X' % (
            r['pc'], r['kind'], r['fn'], r['frames'], r['ticks60'], r['ratio'], r['lo'], r['hi']))
    print('\nkinds: ' + ', '.join('%s %d' % (k, sum(1 for r in acc if r['kind'] == k)) for k in 'fudj?'))
    if write:
        with open(OUT, 'w', newline='\n') as f:
            f.write('# 60 fps store rules (BT3-Recomp kinds): f float accumulator, u up-counter, d countdown, j integer step\n')
            f.write('# sites that change twice as fast per second at 60 fps; from tools/step_sites.py --compare, comment a line to drop it\n[sites]\n')
            for r in sorted(acc, key=lambda r: r['pc']):
                if r['kind'] != '?': f.write('0x%08X %s  # fn 0x%08X x%.2f\n' % (r['pc'], r['kind'], r['fn'], r['ratio']))
        print('\nwritten %s' % OUT)


VERDICTS = ('accumulates', 'recomputes', 'pointer', 'cursor', 'undecided', 'untested')


def verdict(t):
    if t is None or t['stores'] == 0: return 'untested'
    if 2 * t['pointer'] >= t['stores']: return 'pointer'
    if 4 * t['same'] >= t['stores']: return 'cursor'
    if t['acc'] >= 3 and t['acc'] >= 3 * t['rec']: return 'accumulates'
    if t['rec'] >= 3 and t['rec'] > t['acc']: return 'recomputes'
    return 'untested' if t['probes'] == 0 else 'undecided'


def probe(path, write):
    stats = {}
    for l in open(path):
        p = l.strip().split(',')
        if len(p) < 9 or not p[0].startswith('0x'): continue
        stats[int(p[0], 16)] = dict(zip(('stores', 'same', 'pointer', 'probes', 'acc', 'rec', 'other'), map(int, p[2:9])))
    rules = []
    for l in open(OUT):
        l = l.strip()
        if l.startswith('# 0x'): l = l[2:]
        s = l.split('#')[0].split()
        if len(s) >= 2 and s[0].startswith('0x') and s[1] in ('f', 'u', 'd', 'j'):
            c = l.split('#', 1)[1].strip() if '#' in l else ''
            if c.split()[-1:] and c.split()[-1] in VERDICTS: c = c.rsplit(None, 1)[0] if ' ' in c else ''
            rules.append((int(s[0], 16), s[1], c))
    by = {}
    for pc, k, c in rules: by.setdefault(verdict(stats.get(pc)), []).append((pc, k, c))
    order = VERDICTS
    print('rules in %s: %d; probe file %s: %d sites' % (os.path.basename(OUT), len(rules), path, len(stats)))
    print(', '.join('%s %d' % (v, len(by.get(v, []))) for v in order))
    for v in order[:5]:
        lst = by.get(v, [])
        print('\n[%s] %d (kinds %s)' % (v, len(lst), ' '.join('%s %d' % (k, sum(1 for r in lst if r[1] == k)) for k in 'fudj')))
        for pc, k, c in sorted(lst, key=lambda r: -stats[r[0]]['stores'])[:15]:
            t = stats[pc]
            print('  0x%08X %s  stores %7d  same tick %6d  pointer %6d  probes %4d  acc %4d  rec %4d  other %4d  %s' % (
                pc, k, t['stores'], t['same'], t['pointer'], t['probes'], t['acc'], t['rec'], t['other'], c))
    if write:
        with open(OUT, 'w', newline='\n') as f:
            f.write('# 60 fps store rules (BT3-Recomp kinds): f float accumulator, u up-counter, d countdown, j integer step\n')
            f.write('# judged by a TTT_STEP_PROBE run (step_sites.py --probe); only accumulators are active\n[sites]\n')
            for pc, k, c in sorted(rules):
                v = verdict(stats.get(pc))
                f.write('%s0x%08X %s  # %s%s\n' % ('' if v == 'accumulates' else '# ', pc, k, c + ' ' if c else '', v))
        print('\nwritten %s' % OUT)


def mark(p1, p2):
    t1, r1 = load(p1); h1, m1 = load.hz, load.marked
    t2, r2 = load(p2); h2, m2 = load.hz, load.marked
    if {h1, h2} != {30, 60}: raise SystemExit('need one census at 30 fps and one at 60 fps (got %d and %d)' % (h1, h2))
    (t30, m30, a30), (t60, m60, a60) = ((t1, m1, r1), (t2, m2, r2)) if h1 == 30 else ((t2, m2, r2), (t1, m1, r1))
    if not m30 or not m60: raise SystemExit('a census has no marked ticks (Ctrl+F7 during the battle)')
    b60 = {r['pc']: r for r in a60}
    fns = functions()
    handled = dual(fns)
    wrapped = set(addresses(section('recomp.txt', 'wrappers')))
    spec = lambda r, t, m: (r['mcount'] - r['mzero']) / m >= 3 * max(r['count'] - r['zero'] - r['mcount'] + r['mzero'], 0.5) / max(t - m, 1)
    rows = []
    for r in a30:
        q = b60.get(r['pc'])
        if not q or 'mchain' not in r or 'mchain' not in q or r['mchain'] < 10 or q['mchain'] < 10: continue
        if not spec(r, t30, m30) or not spec(q, t60, m60) or r['mabs'] <= 0: continue
        ratio = (q['mabs'] * q['mchain'] / m60 * 60) / (r['mabs'] * r['mchain'] / m30 * 30)
        f = owner(fns, r['pc'])
        r.update(fn=f[0] if f else 0, ratio=ratio, kind=classify(r) or ('f' if is_float(r) else '?'),
                 done='dual' if f in handled else 'wrapped' if f and f[0] in wrapped else '')
        rows.append(r)
    print('census 30 fps: %d ticks, %d marked; 60 fps: %d ticks, %d marked' % (t30, m30, t60, m60))
    # the two windows never hold the same amount of action per second: the median of the solid accumulators calibrates it
    solid = sorted(r['ratio'] for r in rows if r['mchain'] >= 0.8 * (r['mcount'] - r['mzero']))
    k = solid[len(solid) // 2] if len(solid) >= 5 else 1.0
    for r in rows: r['ratio'] /= k
    print('window factor %.2f (median of %d accumulators with 80%%+ chained stores), ratios below are relative to it' % (k, len(solid)))
    rows = [r for r in rows if r['mchain'] >= 0.5 * (r['mcount'] - r['mzero'])]
    grp = lambda x: 'accelerated' if x >= 1.6 else 'right speed' if 0.7 <= x <= 1.4 else 'unclear'
    print('sites specific to the marked window with 50%%+ chained stores: %d (%s)' % (len(rows), ', '.join('%s %d' % (g, sum(1 for r in rows if grp(r['ratio']) == g))
                                                                            for g in ('accelerated', 'right speed', 'unclear'))))
    by = {}
    for r in rows: by.setdefault(r['fn'], []).append(r)
    print('\nfunctions (sites, accelerated):')
    for fn, lst in sorted(by.items(), key=lambda it: -sum(1 for r in it[1] if r['ratio'] >= 1.6))[:40]:
        print('  0x%08X  %3d sites  %3d accelerated%s' % (fn, len(lst), sum(1 for r in lst if r['ratio'] >= 1.6), '  (%s)' % lst[0]['done'] if lst[0]['done'] else ''))
    print('\nsites by change per second 30->60 (chained stores only):')
    for r in sorted(rows, key=lambda r: -r['ratio'])[:80]:
        print('  0x%08X %s fn 0x%08X  x%.2f  marked ticks %5d/%5d  chained %3d%%  per tick %3d  addr 0x%08X..0x%08X %s' % (
            r['pc'], r['kind'], r['fn'], r['ratio'], r['mframes'], b60[r['pc']]['mframes'], 100 * r['mchain'] // max(1, r['mcount'] - r['mzero']),
            r['per'], r['lo'], r['hi'], r['done']))


def main():
    a = sys.argv[1:]
    if '--mark' in a:
        i = a.index('--mark')
        return mark(a[i + 1], a[i + 2])
    if '--probe' in a:
        return probe(a[a.index('--probe') + 1], '--write' in a)
    if '--compare' in a:
        i = a.index('--compare')
        return compare(a[i + 1], a[i + 2], '--write' in a, '--all' in a)
    path = next((x for x in a if not x.startswith('--')), DEFAULT)
    if not os.path.exists(path): raise SystemExit('no census at %s (run a battle at 30 fps with TTT_STEPCENSUS=1)' % path)
    ticks, rows = load(path)
    fns = functions()
    handled = dual(fns)
    wrapped = set(addresses(section('recomp.txt', 'wrappers')))
    out, skipped = {}, {}
    for r in rows:
        k = classify(r)
        if not k: continue
        f = owner(fns, r['pc'])
        why = 'dual' if f in handled else 'wrapped' if f and f[0] in wrapped else None
        if why: skipped.setdefault(why, []).append(r); continue
        r['fn'] = f[0] if f else 0
        out.setdefault(k, []).append(r)
    print('census %s: %d battle ticks, %d store sites' % (path, ticks, len(rows)))
    print('proposed: ' + ', '.join('%s %d' % (k, len(out.get(k, []))) for k in 'fudj') +
          '; already handled: ' + ', '.join('%s %d' % (k, len(v)) for k, v in skipped.items()))
    for k in 'fudj':
        lst = sorted(out.get(k, []), key=lambda r: -r['frames'])
        print('\n[%s] %d sites (top 25 by ticks)' % (k, len(lst)))
        for r in lst[:25]:
            print('  0x%08X fn 0x%08X  ticks %6d  per tick %3d  chain %3d%%  %s  addr 0x%08X..0x%08X' % (
                r['pc'], r['fn'], r['frames'], r['per'], 100 * r['chain'] // max(1, r['count']),
                ('delta %g' % r['fmode']) if k == 'f' else ('step %d' % r['imode']), r['lo'], r['hi']))
    if '--write' in a:
        with open(OUT, 'w', newline='\n') as f:
            f.write('# 60 fps store rules (BT3-Recomp kinds): f float accumulator, u up-counter, d countdown, j integer step,\n')
            f.write('# h per-tick rate, D duration; proposed by tools/step_sites.py, review before use\n[sites]\n')
            for k in 'fudj':
                for r in sorted(out.get(k, []), key=lambda r: r['pc']):
                    f.write('0x%08X %s  # fn 0x%08X ticks %d\n' % (r['pc'], k, r['fn'], r['frames']))
        print('\nwritten %s' % OUT)


if __name__ == '__main__':
    main()
