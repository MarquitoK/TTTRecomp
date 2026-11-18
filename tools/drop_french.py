#!/usr/bin/env python3
# French out of the port: French entries of assets/Packfile, mods/*/Packfile and assets/cache/armado.
# A top-level PACKFILE entry is removed (the port serves it empty); a French part inside a pak becomes an empty file so the
# pak keeps its numbering; the 043_move_list_fr.txt texts are removed.
# Multi-language txt1 lose their French slot (strings and the txt1.txt languages); mods lose CharRef/fr.txt.
#   (default)  lists what would change in drop_french.txt, touches nothing
#   --apply    does it (assets without shared copies: unify_assets.py --undo first)
import argparse, os, re, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as op

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
CONT = ('.pak', '.txt1', '.vagp', '.atex', '.rhg')


def tops(assets):
    t = set()
    for line in open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8'):
        k = line.rstrip('\r\n').split('\t')
        if line.startswith('#') or len(k) < 7:
            continue
        r = k[6].replace('\\', '/').lower()
        t.add(r)
        if r.endswith('.pak'):
            t.add(r[:-4])
    return t


def numbered(dp):   # an opened pak that is not in the manifest (a mod's own menu): every entry starts with its number
    xs = [x for x in os.listdir(dp) if x.lower() not in op.IGNORAR and not x.startswith('.')]
    return len(xs) > 1 and all(re.match(r'^\d+_', x) for x in xs)


def size_of(p):
    if os.path.isfile(p):
        return os.path.getsize(p)
    return sum(os.path.getsize(os.path.join(dp, f)) for dp, dn, fn in os.walk(p) for f in fn)


def part_of(name):   # a texture opened as loose PNG (NN_x_fr.rhg.png, @2x) is the part NN_x_fr.rhg
    return re.sub(r'(@\dx)?\.png$', '', name, flags=re.I) if re.search(r'\.(rhg|atex)(@\dx)?\.png$', name, re.I) else name


def scan(root, top):
    acts, inside = [], {}
    for dp, dn, fn in os.walk(root):
        rel = os.path.relpath(dp, root).replace('\\', '/')
        rel = '' if rel == '.' else rel
        low = rel.lower()
        cur = bool(rel) and (low in top or low.endswith(CONT) or inside.get(os.path.dirname(dp), False) or numbered(dp))
        inside[dp] = cur
        keep = []
        for name in dn:
            r = rel + '/' + name if rel else name
            if not op.es_frances(name):
                keep.append(name)
                continue
            p = os.path.join(dp, name)
            acts.append(('vaciar' if cur and r.lower() not in top else 'borrar', p, r, size_of(p)))
        dn[:] = keep
        seen = set()
        for name in fn:
            if not op.es_frances(name):
                continue
            r, p = rel + '/' + name if rel else name, os.path.join(dp, name)
            if name.lower().startswith('043_move_list_') or not cur or r.lower() in top:
                acts.append(('borrar', p, r, os.path.getsize(p)))
                continue
            part = part_of(name)
            if part in seen or (part == name and not os.path.getsize(p)):
                continue
            seen.add(part)
            acts.append(('vaciar', os.path.join(dp, part), (rel + '/' + part) if rel else part, os.path.getsize(p)))
    return acts


def retry(fn, *a):   # antivirus, indexer or a preview holding a file: retry for up to 20 s
    for i in range(40):
        try:
            return fn(*a)
        except PermissionError:
            if i == 39:
                raise
            time.sleep(0.5)


def apply_one(kind, p):
    if os.path.isdir(p):
        retry(shutil.rmtree, p)
    elif kind == 'borrar':
        if os.path.isfile(p):
            retry(os.remove, p)
    else:
        retry(op.quitar_textura, p)
    if kind == 'vaciar':
        open(p, 'wb').close()


def txt1_slots(assets):   # (folder, line index, languages, strings) of every opened txt1 with a French slot
    out, path = [], os.path.join(assets, 'txt1.txt')
    if not os.path.isfile(path):
        return out, []
    lines = open(path, encoding='utf-8').read().split('\n')
    for i, l in enumerate(lines):
        c = l.split('\t')
        if l.startswith('#') or len(c) != 3 or c[1] == 'raw' or 'fr' not in c[2].split() or op.es_frances(c[0]):
            continue
        f = next((x for x in (os.path.join(assets, 'Packfile', c[0]), os.path.join(assets, 'Packfile', c[0][:-5])) if os.path.isdir(x)), None)
        out.append((f, i, c[2].split(), c[0]))
    return out, lines


def txt1_drop(f, langs):   # None: left as it is (a French-only text table has no other slot to keep)
    if langs.count('fr') != 1 or len(langs) < 2:
        return None
    t = op.leer_texto(f)
    if len(t) % len(langs):
        return None
    n, k = len(t) // len(langs), langs.index('fr')
    return t[:k * n] + t[(k + 1) * n:], n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--apply', action='store_true')
    a = ap.parse_args()
    assets = os.path.join(BASE, 'assets')
    if os.path.isfile(os.path.join(assets, 'shared.txt')):   # the list must see the same tree the apply will touch
        raise SystemExit('assets/shared.txt exists: run python3 tools/unify_assets.py --undo first')
    top = tops(assets)
    roots = [('assets', os.path.join(assets, 'Packfile'))]
    mods = os.path.join(BASE, 'mods')
    if os.path.isdir(mods):
        roots += [('mods/' + m, os.path.join(mods, m, 'Packfile')) for m in sorted(os.listdir(mods)) if os.path.isdir(os.path.join(mods, m, 'Packfile'))]
    rows, tot = [], {}
    for label, root in roots:
        for kind, p, r, sz in scan(root, top):
            rows.append((label, kind, p, r, sz))
            t = tot.setdefault((label, kind), [0, 0])
            t[0] += 1
            t[1] += sz
    cache = os.path.join(assets, 'cache', 'armado')
    if os.path.isdir(cache):
        for name in sorted(os.listdir(cache)):
            if op.es_frances(name):
                p = os.path.join(cache, name)
                rows.append(('cache', 'borrar', p, name, os.path.getsize(p)))
                t = tot.setdefault(('cache', 'borrar'), [0, 0])
                t[0] += 1
                t[1] += os.path.getsize(p)
    for m in (os.listdir(mods) if os.path.isdir(mods) else []):
        p = os.path.join(mods, m, 'CharRef', 'fr.txt')
        if os.path.isfile(p):
            rows.append(('mods/' + m, 'borrar', p, 'CharRef/fr.txt', os.path.getsize(p)))
            tot.setdefault(('mods/' + m, 'borrar'), [0, 0])[0] += 1
    slots, tlines = txt1_slots(assets)
    bad = [s[3] for s in slots if not s[0] or txt1_drop(s[0], s[2]) is None]
    slots = [s for s in slots if s[0] and txt1_drop(s[0], s[2])]
    strings = sum(txt1_drop(s[0], s[2])[1] for s in slots if s[0] and txt1_drop(s[0], s[2]))
    with open(os.path.join(BASE, 'drop_french.txt'), 'w', encoding='utf-8', newline='\n') as o:
        for label, kind, p, r, sz in rows:
            o.write('%s\t%s\t%s\t%d\n' % (kind, label, r, sz))
    for (label, kind), (n, sz) in sorted(tot.items()):
        print('frances: %s: %s %d, %.1f MB' % (label, kind, n, sz / 1048576))
    print('frances: %d en total, %.1f MB' % (len(rows), sum(r[4] for r in rows) / 1048576))
    print('frances: txt1 con ranura francesa %d, %d cadenas francesas%s' % (len(slots), strings, '; se dejan (solo frances o sin patron): ' + ' '.join(bad) if bad else ''))
    if not a.apply:
        print('frances: solo lista (drop_french.txt); --apply lo aplica')
        return
    failed = []
    for label, kind, p, r, sz in rows:
        try:
            apply_one(kind, p)
        except OSError as ex:
            failed.append('%s: %s' % (r, ex))
    for f, i, langs, rel in slots:
        r = txt1_drop(f, langs) if f else None
        if not r:
            continue
        op.escribir_texto(f, r[0])
        tlines[i] = '%s\t%s\t%s' % (rel, tlines[i].split('\t')[1], ' '.join(x for x in langs if x != 'fr'))
    if slots:
        with open(os.path.join(assets, 'txt1.txt'), 'w', encoding='utf-8', newline='\n') as o:
            o.write('\n'.join(tlines))
    for x in failed:
        print('frances: no se pudo: ' + x)
    print('frances: aplicado%s' % (', %d fallos (vuelve a ejecutar --apply con el juego y el explorador cerrados)' % len(failed) if failed else ''))
    if failed:
        sys.exit(1)


if __name__ == '__main__':
    main()
