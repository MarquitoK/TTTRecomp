import argparse, os, shutil

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def targets(assets):
    t = set()
    for linea in open(os.path.join(assets, 'shared.txt'), encoding='utf-8'):
        if linea.startswith('#') or '\t' not in linea:
            continue
        t.add(linea.rstrip('\r\n').split('\t')[1])
    return t


# old layout Menus/<category>/<name> -> the group's path now (same rule as psp_shared_rel in disc.h)
def indice(ts):
    ix = {}
    for t in sorted(ts):
        p = t.split('/')
        if len(p) < 3:
            continue
        ix.setdefault('/'.join([p[0], p[-2], p[-1]]).lower(), t)
        ix.setdefault('/'.join([p[0], p[-2]]).lower(), '/'.join(p[:-1]))
    return ix


def nuevo(rel, ts, ix):
    low = rel.lower()
    if any(low == t.lower() or low.startswith(t.lower() + '/') for t in ts):
        return None
    p = rel.split('/')
    for n in (3, 2):
        if len(p) > n - 1:
            t = ix.get('/'.join(p[:n]).lower())
            if t:
                return '/'.join([t] + p[n:])
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--mods', default=os.path.join(BASE, 'mods'))
    ap.add_argument('--apply', action='store_true')
    a = ap.parse_args()
    if not os.path.isfile(os.path.join(a.assets, 'shared.txt')):
        raise SystemExit('shared: primero python3 tools/unify_assets.py --apply')
    ts = targets(a.assets)
    ix = indice(ts)
    total, sin = 0, 0
    for m in sorted(os.listdir(a.mods)) if os.path.isdir(a.mods) else []:
        raiz = os.path.join(a.mods, m, 'Shared')
        if not os.path.isdir(raiz):
            continue
        mov, falta = [], []
        for dp, _, fn in os.walk(raiz):
            for x in fn:
                rel = os.path.relpath(os.path.join(dp, x), raiz).replace('\\', '/')
                n = nuevo(rel, ts, ix)
                if n:
                    mov.append((rel, n))
                elif not any(rel.lower().startswith(t.lower() + '/') or rel.lower() == t.lower() for t in ts):
                    falta.append(rel)
        print('shared: %s: %d archivos a mover, %d sin grupo' % (m, len(mov), len(falta)))
        for rel, n in mov[:5]:
            print('shared:   %s -> %s' % (rel, n))
        for rel in falta[:5]:
            print('shared:   sin grupo: %s' % rel)
        total += len(mov)
        sin += len(falta)
        if not a.apply:
            continue
        for rel, n in mov:
            dst = os.path.join(raiz, n)
            if os.path.exists(dst):
                print('shared:   ya existe, se deja: %s' % n)
                continue
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.move(os.path.join(raiz, rel), dst)
        for dp, dn, fn in sorted(os.walk(raiz, topdown=False), key=lambda x: -len(x[0])):
            if dp != raiz and not os.listdir(dp):
                os.rmdir(dp)
    print('shared: %d archivos %s, %d sin grupo' % (total, 'movidos' if a.apply else 'por mover', sin))


if __name__ == '__main__':
    main()
