#!/usr/bin/env python3
# Unifies identical menu, interface and text assets into assets/Shared, listed in assets/shared.txt.
# Scope: Main interface and texts, Menus, Battle_UI and Story_Mode/UI. Audio only inside menu paks.
# A file put back at its old path wins over the shared copy.
#   (default)  report of what would be unified, in unify_report.txt; touches nothing
#   --apply    one copy of each group goes to assets/Shared/<top>/<menu path or Common>/<category>/<name>, the rest is removed
#   --undo     puts a full copy back at every path and removes assets/Shared and shared.txt
import argparse, collections, filecmp, hashlib, os, re, shutil, time

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
AUDIO = ('.wav', '.unk', '.vagp', '.pphd', '.at3')
LANG = re.compile(r'_(ja|en|es|fr|de|it|dr)(?=[/_.]|$)')
NUM = re.compile(r'^(\d+)_')


def in_scope(rel):
    p = rel.split('/')
    if p[0] == 'Shared':
        return len(p) > 2
    p = p[1:]
    low = '/'.join(p).lower()
    audio = low.endswith(AUDIO) or any(s.endswith(('.vagp', '.pphd')) for s in low.split('/'))
    if audio:
        return p[0] == 'Menus' and any(s.endswith('.pak') for s in low.split('/')[:-1])
    if p[0] in ('Menus', 'Battle_UI'):
        return True
    if p[0] == 'Story_Mode':
        return len(p) > 1 and p[1] == 'UI'
    if p[0] == 'Main':
        return '/3_interface' in '/'.join(p) or '.txt1' in low
    return False


def ext_of(name):
    n = name.lower()
    for e in ('.atex.png', '.rhg.png'):
        if n.endswith(e):
            return e
    return os.path.splitext(n)[1]


def child_id(name):
    m = NUM.match(name)
    return ('#' + str(int(m.group(1))) + ext_of(name)) if m else name.lower()


class Hashes:
    def __init__(self):
        self.c = {}

    def get(self, path):
        st = os.stat(path)
        k = (path, st.st_size, st.st_mtime_ns)
        h = self.c.get(k)
        if h is None:
            d = hashlib.blake2b(digest_size=16)
            with open(path, 'rb') as f:
                for b in iter(lambda: f.read(1 << 20), b''):
                    d.update(b)
            h = self.c[k] = d.hexdigest()
        return h


def entries(assets):
    out, anc = set(), set()
    for line in open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8'):
        c = line.rstrip('\r\n').split('\t')
        if line.startswith('#') or len(c) < 7:
            continue
        p = 'Packfile/' + c[6]
        out.add(p.lower())
        if p.lower().endswith('.pak'):
            out.add(p[:-4].lower())
        d = p.lower()
        while '/' in d:
            d = d.rsplit('/', 1)[0]
            anc.add(d)
    return out, anc


def scan(assets):
    files, dirs = {}, []
    for top in ('Packfile', 'Shared'):
        base = os.path.join(assets, top)
        for dp, dn, fn in os.walk(base):
            rel = os.path.relpath(dp, assets).replace('\\', '/')
            if rel == 'Packfile':
                dn[:] = [d for d in dn if d in ('Menus', 'Story_Mode', 'Main', 'Battle_UI')]
            elif rel == 'Packfile/Story_Mode':
                dn[:] = [d for d in dn if d not in ('Characters', 'Objects')]
            if rel.count('/') >= 2 or (top == 'Packfile' and rel.count('/') >= 1):
                dirs.append(rel)
            for n in fn:
                r = rel + '/' + n
                if in_scope(r):
                    files[r] = os.path.getsize(os.path.join(assets, r))
    return files, dirs


def overlaps(a, b):
    return a == b or a.startswith(b + '/') or b.startswith(a + '/')


def find_groups(assets, hashes, ents):
    files, dirs = scan(assets)
    fh = {r: hashes.get(os.path.join(assets, r)) for r, s in files.items()}
    children = collections.defaultdict(list)
    for r in list(files) + dirs:
        children[r.rsplit('/', 1)[0]].append(r)
    dsig, dbytes = {}, {}
    for d in sorted(dirs, key=lambda x: -x.count('/')):
        if not in_scope(d) or d.lower() in ents or (d.startswith('Shared/') and d.count('/') < 3):
            continue
        parts, total, ok = [], 0, True
        for c in sorted(children[d], key=lambda x: child_id(x.rsplit('/', 1)[-1])):
            name = child_id(c.rsplit('/', 1)[-1])
            if c in dsig:
                parts.append('D' + name + dsig[c]); total += dbytes[c]
            elif c in files:
                parts.append('F' + name + fh[c]); total += files[c]
            else:
                ok = False
                break
        if ok and total:
            dsig[d] = hashlib.blake2b('|'.join(parts).encode(), digest_size=16).hexdigest()
            dbytes[d] = total
    by = collections.defaultdict(list)
    for d, h in dsig.items():
        by[h].append(d)
    cand = sorted((v for v in by.values() if len(v) > 1), key=lambda v: -dbytes[v[0]])
    taken, chosen = [], []
    for v in cand:
        v = sorted(d for d in v if not any(overlaps(d, t) for t in taken))
        if len(v) > 1:
            chosen.append(('D', dbytes[v[0]], v))
            taken += v
    if chosen:
        return chosen
    byf = collections.defaultdict(list)
    for r, h in fh.items():
        if files[r]:
            byf[h].append(r)
    return [('F', files[v[0]], sorted(v)) for v in byf.values() if len(v) > 1]


def split_ext(n):
    e = ext_of(n)
    return (n[:-len(e)], e) if e else (n, '')


def clean(n):
    s, e = split_ext(NUM.sub('', n))
    return LANG.sub('', s) or s


# where a copy lives: the cleaned path of its manifest entry (Menus/Training/menu_training_en.pak -> Training/menu_training)
def entry_loc(p, exact):
    parts = p.split('/')
    i = max((k for k in range(2, len(parts)) if '/'.join(parts[:k]).lower() in exact), default=2)
    return [clean(x) for x in parts[2:i]]


# Shared/<top>/<location>/<category>/<unit>: one menu -> its own path; several -> their common path + Common
def pick_name(v, used, exact, kind, roots):
    src = next((p for p in v if p.startswith('Shared/')), v[0])
    parts = src.split('/')
    top = parts[1]
    locs = []
    for p in v:
        if p.startswith('Shared/'):
            r = next((r for r in roots if p.lower() == r.lower() or p.lower().startswith(r.lower() + '/')), None)
            locs.append(roots[r][1] if r else [])
            if r and p.lower() == src.lower():
                parts = ['Shared', top, roots[r][2]] + p.split('/')[len(r.split('/')) - 1:]
        else:
            locs.append(entry_loc(p, exact))
    if src.startswith('Shared/'):
        cat = parts[2]
        inner = parts[3:]
    else:
        i = max((k for k in range(2, len(parts)) if '/'.join(parts[:k]).lower() in exact), default=2)
        inner = parts[i:]
        cat = clean(inner[0]) if len(inner) > 1 or kind == 'D' else 'General'
    last = parts[-1]
    s, e = split_ext(NUM.sub('', last))
    if re.match(r'^\d*(@\d+x)?$', s):
        s = clean(parts[-2]) + '_' + (s or NUM.sub('', last))
    s = LANG.sub('', s) or s
    langs = set(m.group(1) for p in v for m in [LANG.search(p)] if m)
    if len(langs) == 1 and next(iter(langs)) in ('en', 'es'):   # only the port's languages name a group (the port looks Shared up by these names)
        s += '_' + langs.pop()
    low = [[x.lower() for x in l] for l in locs]
    if all(l == low[0] for l in low):
        loc = locs[0]
    else:
        n = 0
        while all(len(l) > n and l[n] == low[0][n] for l in low):
            n += 1
        loc = locs[0][:n] + ['Common']
    pre = '/'.join([top] + loc + [cat])
    name, k = '%s/%s%s' % (pre, s, e), 2
    while name.lower() in used:
        name = '%s/%s_%d%s' % (pre, s, k, e)
        k += 1
    used.add(name.lower())
    roots['Shared/' + name] = (top, loc, cat)
    return name


def same(a, b):
    if os.path.isdir(a) != os.path.isdir(b):
        return False
    if not os.path.isdir(a):
        return filecmp.cmp(a, b, shallow=False)
    ka = {child_id(n): n for n in os.listdir(a)}
    kb = {child_id(n): n for n in os.listdir(b)}
    return ka.keys() == kb.keys() and all(same(os.path.join(a, ka[k]), os.path.join(b, kb[k])) for k in ka)


# antivirus or indexer scans hold freshly written files for a moment: retry for up to 20 s before giving up
def retry(fn, *a):
    for i in range(40):
        try:
            return fn(*a)
        except PermissionError:
            if i == 39:
                raise
            time.sleep(0.5)


def remove(path):
    if os.path.isdir(path):
        retry(shutil.rmtree, path)
    else:
        retry(os.remove, path)


def read_links(assets):
    links = []
    path = os.path.join(assets, 'shared.txt')
    if os.path.isfile(path):
        for line in open(path, encoding='utf-8'):
            if line.startswith('#') or '\t' not in line:
                continue
            k, t = line.rstrip('\r\n').split('\t')
            links.append((k if k.startswith('Shared/') else 'Packfile/' + k, 'Shared/' + t))
    return links


def write_links(assets, links):
    with open(os.path.join(assets, 'shared.txt'), 'w', encoding='utf-8', newline='\n') as o:
        o.write('# Packfile or Shared path\tassets/Shared path\n')
        for k, t in links:
            o.write('%s\t%s\n' % (k[len('Packfile/'):] if k.startswith('Packfile/') else k, t[len('Shared/'):]))


def plan_pass(assets, hashes, ents, used, dry, roots):
    exact, anc = ents
    groups = find_groups(assets, hashes, exact | anc)
    done = []
    for kind, size, v in groups:
        canon = next((p for p in v if p in roots), None)
        src = canon or next((p for p in v if p.startswith('Shared/')), v[0])
        bad = [p for p in v if p != src and not same(os.path.join(assets, src), os.path.join(assets, p))]
        if bad:
            print('not identical, skipped: %s vs %s' % (src, bad[0]))
            continue
        if canon:
            used.add(canon[len('Shared/'):].lower())
        name = canon or 'Shared/' + pick_name(v, used, exact, kind, roots)
        if not dry:
            if src.lower() != name.lower():
                dst = os.path.join(assets, name)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                try:
                    retry(os.replace, os.path.join(assets, src), dst)
                except PermissionError as ex:
                    print('locked, group left as it is: %s (%s)' % (src, ex))
                    continue
            kept = [src]
            for p in v:
                if p == src:
                    continue
                try:
                    if os.path.exists(os.path.join(assets, p)):
                        remove(os.path.join(assets, p))
                    kept.append(p)
                except PermissionError as ex:
                    print('locked, copy kept at its path: %s (%s)' % (p, ex))
            v = kept
            if len(v) < 2 and src.lower() == name.lower():
                continue
        done.append((kind, size, v, name))
    return done


def unify(assets, out, dry):
    hashes, ents, used, roots = Hashes(), entries(assets), set(), {}
    links, plan = [], []
    try:
        while True:
            done = plan_pass(assets, hashes, ents, used, dry, roots)
            if not done:
                break
            for kind, size, v, name in done:
                plan.append((kind, size, v, name))
                for p in v:
                    if p.lower() != name.lower():
                        links.append((p, name))
            if dry:
                break
    finally:
        # links of every group already moved are written even if a later step fails, so --undo always works
        if not dry:
            write_links(assets, links)
    saved = sum(s * (len(v) - 1) for _, s, v, _ in plan)
    with open(out, 'w', encoding='utf-8', newline='\n') as o:
        o.write('# %d groups, %.1f MB repeated%s\n' % (len(plan), saved / 1048576, ' (first pass only)' if dry else ''))
        for kind, s, v, name in plan:
            o.write('%s\t%d\t%d\t%s\t%s\n' % (kind, len(v), s * (len(v) - 1), name, '\t'.join(v)))
    print('%s: %d groups, %.1f MB repeated%s; report in %s' % ('applied' if not dry else 'plan', len(plan), saved / 1048576,
                                                              ' (first pass only)' if dry else '', out))


def resolver(assets, links):
    lk = {k.lower(): t for k, t in links}
    kids = collections.defaultdict(list)
    for k, t in links:
        kids[k.rsplit('/', 1)[0].lower()].append((k.rsplit('/', 1)[1], t))

    def real(p):
        for _ in range(8):
            parts = p.split('/')
            hit = False
            for i in range(len(parts), 1, -1):
                t = lk.get('/'.join(parts[:i]).lower())
                if t:
                    p = t + ('/' + '/'.join(parts[i:]) if i < len(parts) else '')
                    hit = True
                    break
            if not hit:
                return p
        return p

    def materialize(virtual, dst):
        src = os.path.join(assets, real(virtual))
        if os.path.isdir(src):
            os.makedirs(dst, exist_ok=True)
            names = set(os.listdir(src)) | set(n for n, _ in kids.get(virtual.lower(), []))
            names |= set(n for n, _ in kids.get(real(virtual).lower(), []))
            for n in names:
                materialize(virtual + '/' + n, os.path.join(dst, n))
        else:
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
    return materialize


def undo(assets):
    if not os.path.isfile(os.path.join(assets, 'shared.txt')):
        raise SystemExit('missing assets/shared.txt: nothing to undo')
    links = read_links(assets)
    materialize = resolver(assets, links)
    n = 0
    for k, t in links:
        if k.startswith('Packfile/') and not os.path.exists(os.path.join(assets, k)):
            materialize(k, os.path.join(assets, k))
            n += 1
    if os.path.isdir(os.path.join(assets, 'Shared')):
        retry(shutil.rmtree, os.path.join(assets, 'Shared'))
    os.remove(os.path.join(assets, 'shared.txt'))
    print('undo: %d paths restored' % n)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--out', default=os.path.join(BASE, 'unify_report.txt'))
    g = ap.add_mutually_exclusive_group()
    g.add_argument('--apply', action='store_true')
    g.add_argument('--undo', action='store_true')
    a = ap.parse_args()
    if not os.path.isdir(os.path.join(a.assets, 'Packfile')):
        raise SystemExit('missing %s/Packfile' % a.assets)
    if a.undo:
        return undo(a.assets)
    if os.path.exists(os.path.join(a.assets, 'shared.txt')) or os.path.exists(os.path.join(a.assets, 'Shared')):
        raise SystemExit('assets are already unified; run --undo first')
    unify(a.assets, a.out, not a.apply)


if __name__ == '__main__':
    main()
