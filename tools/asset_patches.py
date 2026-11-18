#!/usr/bin/env python3
# Porter asset patches: deltas against the user's own extracted entries (only edited bytes are stored).
#   --make NAME PAK... [--name N=section/file]   builds games/ttt/asset_patches/NAME.txt from edited paks
#   --apply | --check                            applies every patch to assets/Packfile (after open_paks --abrir)
import shutil
import argparse, collections, glob, hashlib, json, os, re, struct, sys

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
PATCH_DIR = os.path.join(BASE, 'games', 'ttt', 'asset_patches')
SECTOR = 2048
KEY = 16
NUM = re.compile(r'^(\d+)_')


def digest(d):
    return hashlib.blake2b(d, digest_size=16).hexdigest()


def split_pak(d):
    n = struct.unpack_from('>I', d, 0)[0]
    offs = struct.unpack_from('>%dI' % (n + 1), d, 4)
    return [d[offs[i]:offs[i + 1]] for i in range(n)]


def read_manifest(assets):
    head, rows = None, {}
    for line in open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8'):
        if line.startswith('# packfile'):
            head = line.split()
            continue
        if line.startswith('#'):
            continue
        f = line.rstrip('\r\n').split('\t')
        if len(f) >= 7:
            rows[f[6]] = (int(f[1]), int(f[2]))
    if not head:
        raise SystemExit('manifiesto.txt has no packfile header')
    return int(head[head.index('sector_datos') + 1]), rows


# only entries that stay as raw files after opening can be copy sources (textures, texts and containers are converted)
def raw_source(d):
    import open_paks as op
    if not d or d[:4] in (b'RHG\x00', b'VAGp') or op.analizar_atex(d) or op.analizar(d):
        return False
    return not (d[:2] == b'\x64\x00' and op.analizar_txt1(d))


def iso_entry(iso, sd, sector, size):
    from extract_assets import iso_listar
    with open(iso, 'rb') as f:
        files = iso_listar(f)
        key = next((k for k in files if k.upper().endswith('USRDIR/PACKFILE.BIN')), None)
        if not key:
            raise SystemExit('PACKFILE.BIN not found in the ISO')
        f.seek((files[key][0] + sd + sector) * SECTOR)
        return f.read(size)


def delta(target, sources):
    pool, owner, index = bytearray(), [], {}
    for num, d in sources:
        start = len(pool)
        pool += d
        owner.append((start, start + len(d), num))
        for p in range(start, start + len(d) - KEY + 1, 4):
            k = bytes(pool[p:p + KEY])
            if not any(k):
                continue
            lst = index.setdefault(k, [])
            if len(lst) < 32:
                lst.append(p)

    def span(p):
        for s, e, num in owner:
            if s <= p < e:
                return s, e, num

    ops, lit, i, n, last = [], bytearray(), 0, len(target), None

    def flush():
        for k in range(0, len(lit), 64):
            ops.append('D ' + lit[k:k + 64].hex())
        lit.clear()

    while i < n:
        if i + KEY <= n and not any(target[i:i + KEY]):
            j = i
            while j < n and target[j] == 0:
                j += 1
            flush()
            ops.append('Z %d' % (j - i))
            i = j
            continue
        best = None
        cands = ([last + len(lit)] if last is not None else []) + (index.get(bytes(target[i:i + KEY]), []) if i + KEY <= n else [])
        for p in cands:
            if p >= len(pool):
                continue
            s, e, num = span(p)
            L = 0
            while i + L < n and p + L < e and pool[p + L] == target[i + L]:
                L += 1
            if L >= KEY and (not best or L > best[0]):
                best = (L, num, p - s, p)
        if best:
            flush()
            ops.append('C %d %d %d' % (best[1], best[2], best[0]))
            i += best[0]
            last = best[3] + best[0]
        else:
            lit.append(target[i])
            i += 1
    flush()
    return ops


def rebuild(ops, sources):
    out = bytearray()
    for op in ops:
        k = op.split()
        if k[0] == 'C':
            num, off, L = int(k[1]), int(k[2]), int(k[3])
            out += sources[num][off:off + L]
        elif k[0] == 'Z':
            out += bytes(int(k[1]))
        else:
            out += bytes.fromhex(k[1])
    return bytes(out)


def make(a):
    assets = os.path.join(BASE, 'assets')
    sd, rows = read_manifest(assets)
    names = dict((int(x.split('=', 1)[0]), x.split('=', 1)[1]) for x in a.name)
    out = ['# asset patch %s' % a.make]
    import open_paks as op
    for path in a.paks:
        if op.es_frances(os.path.basename(path)):
            print('%s: French, skipped' % path)
            continue
        base = NUM.sub('', os.path.basename(path))
        keys = [k for k in rows if k.rsplit('/', 1)[-1].lower() == base.lower()]
        if len(keys) != 1:
            raise SystemExit('%s: %d manifest matches for %s' % (path, len(keys), base))
        orig = split_pak(iso_entry(a.iso, sd, *rows[keys[0]]))
        edit = split_pak(open(path, 'rb').read())
        if len(edit) < len(orig):
            raise SystemExit('%s: fewer entries than the original' % path)
        src = [(i + 1, d) for i, d in enumerate(orig) if raw_source(d) and (i >= len(edit) or edit[i] == d)]
        srcmap = dict((i + 1, d) for i, d in enumerate(orig))
        out.append('pak %s' % keys[0])
        changed = lits = 0
        for i, d in enumerate(edit):
            num = i + 1
            if i < len(orig) and d == orig[i]:
                continue
            if i >= len(orig) and num not in names:
                raise SystemExit('entry %d is new: give its name with --name %d=section/%03d_name.ext' % (num, num, num))
            ops = delta(d, src)
            if rebuild(ops, srcmap) != d:
                raise SystemExit('entry %d: delta does not rebuild' % num)
            used = sorted(set(int(o.split()[1]) for o in ops if o[0] == 'C'))
            kind = 'new ' + names[num] if i >= len(orig) else 'mod %s' % digest(orig[i])
            out.append('entry %d %s %d %s' % (num, kind, len(d), digest(d)))
            out += ['need %d %s' % (u, digest(srcmap[u])) for u in used]
            out += ops
            lit = sum(len(o) // 2 - 1 for o in ops if o[0] == 'D')
            lits += lit
            changed += 1
            print('  %s entry %d: %d bytes, %d ops, %d literal bytes%s' % (base, num, len(d), len(ops), lit, ' (new)' if i >= len(orig) else ''))
        print('%s: %d entries changed, %d literal bytes' % (keys[0], changed, lits))
    for path in a.shared:
        if not op.es_frances(os.path.basename(path)):
            out += shared_entries(a, sd, rows, path)
    os.makedirs(PATCH_DIR, exist_ok=True)
    dest = os.path.join(PATCH_DIR, a.make + '.txt')
    with open(dest, 'w', encoding='utf-8', newline='\n') as w:
        w.write('\n'.join(out) + '\n')
    print('written %s' % os.path.relpath(dest, BASE).replace('\\', '/'))


# an edited nested pak kept once in assets/Shared (char_select...): one entry in every top pak that holds it (assets/shared.txt)
def shared_entries(a, sd, rows, path):
    edit = open(path, 'rb').read()
    base = NUM.sub('', os.path.basename(path)).lower()
    links = []
    for line in open(os.path.join(BASE, 'assets', 'shared.txt'), encoding='utf-8'):
        if line.startswith('#') or '\t' not in line:
            continue
        k, t = line.rstrip('\r\n').split('\t')
        links.append((k, t.rsplit('/', 1)[-1].lower()))
    names = [base]
    links = [(k, t) for k, t in links if not k.startswith('Shared/')]
    hits = [k for k, t in links if t in names] or [k for k, t in links if NUM.sub('', k.rsplit('/', 1)[-1]).lower() in names]
    if not hits:
        raise SystemExit('%s: no shared.txt line points to %s' % (path, ' or '.join(names)))
    folders = dict((k[:-4].lower(), k) for k in rows if k.lower().endswith('.pak'))
    folders.update((k.lower(), k) for k in rows if k.lower().endswith('.pak'))
    out, done = [], 0
    for k in hits:
        parent, last = k.rsplit('/', 1)
        m = NUM.match(last)
        top = folders.get(parent.lower())
        if not m or not top:
            print('  %s: not an entry of a PACKFILE pak, skipped' % k)
            continue
        num = int(m.group(1))
        orig = split_pak(iso_entry(a.iso, sd, *rows[top]))
        if num > len(orig):
            print('  %s: entry %d beyond %d, skipped' % (k, num, len(orig)))
            continue
        o = orig[num - 1]
        out.append('pak %s' % top)
        if o == edit:
            print('  %s entry %d: unchanged' % (top, num))
            continue
        ops = delta(edit, [(num, o)])
        if rebuild(ops, {num: o}) != edit:
            raise SystemExit('%s entry %d: delta does not rebuild' % (top, num))
        out.append('entry %d mod %s %d %s' % (num, digest(o), len(edit), digest(edit)))
        out.append('need %d %s' % (num, digest(o)))
        out += ops
        lit = sum(len(x) // 2 - 1 for x in ops if x[0] == 'D')
        print('  %s entry %d (%s): %d bytes, %d ops, %d literal bytes' % (top, num, os.path.basename(path), len(edit), len(ops), lit))
        done += 1
    print('%s: %d PACKFILE entries changed' % (os.path.basename(path), done))
    return out


def parse(path):
    paks, cur, ent = [], None, None
    for line in open(path, encoding='utf-8'):
        k = line.split()
        if not k or k[0].startswith('#'):
            continue
        if k[0] == 'pak':
            cur = {'path': k[1], 'entries': []}
            paks.append(cur)
        elif k[0] == 'entry':
            ent = {'num': int(k[1]), 'kind': k[2], 'arg': k[3], 'size': int(k[4]), 'hash': k[5], 'need': {}, 'ops': []}
            cur['entries'].append(ent)
        elif k[0] == 'need':
            ent['need'][int(k[1])] = k[2]
        else:
            ent['ops'].append(line.strip())
    return paks


def is_txt1(path):
    return os.path.isdir(path) and path.lower().endswith('.txt1')


def entry_files(folder):
    files = {}
    for dp, dn, fn in os.walk(folder):
        for x in dn + fn:
            m = NUM.match(x)
            if m:
                files.setdefault(int(m.group(1)), os.path.join(dp, x))
    return files


_ctx = None


def pak_ctx():
    global _ctx
    if _ctx is None:
        import open_paks as op
        pl = op.leer_plantillas(os.path.join(BASE, 'tools', 'lists', 'paks'))
        pc = collections.defaultdict(list)
        for k in pl:
            pc[len(pl[k])].append(k)
        _ctx = {'reglas': op.leer_reglas(os.path.join(BASE, 'tools', 'lists', 'REGLAS_PAK.txt')), 'porcuenta': pc, 'plantillas': pl, 'sin': collections.Counter()}
    return _ctx


# a nested pak written as one raw entry is opened again as a folder, so its textures stay exposed as PNG (button icons)
def reopen(path, d, sub):
    import open_paks as op
    r = op.analizar(d)
    if not r:
        return False
    ctx = pak_ctx()
    nom = op.plantilla_de(sub, r['n'], ctx['reglas'], ctx['porcuenta'])
    stem = NUM.sub('', os.path.splitext(os.path.basename(path))[0])
    if not nom and stem in ctx['plantillas']:
        nom = stem
    if not nom or nom in ('!', '-'):
        return False
    tmp = path + '.abriendo'
    os.replace(path, tmp)
    done = op.abrir(path, d, r, ctx['plantillas'].get(nom, {}), carpeta=path)
    if not done or not op.mismo_contenido(op.empaquetar(done), d):
        if done:
            op.borrar_carpeta(done)
        os.replace(tmp, path)
        return False
    os.remove(tmp)
    op.abrir_anidados(done, sub, ctx)
    return True


# tamanos_texturas.txt keeps the size a texture had when first seen; a patched texture defines a new game size, so its lines go
def sizes_drop(dests):
    path = os.path.join(BASE, 'assets', 'tamanos_texturas.txt')
    if not dests or not os.path.isfile(path):
        return 0
    norm = lambda k: '/'.join(x[:-4] if x.endswith('.pak') else x for x in k.replace('\\', '/').lower().split('/'))
    want = set('/'.join(norm(d).split('/')[-2:]) for d in dests)
    keep, n = [], 0
    for line in open(path, encoding='utf-8'):
        k = line.rstrip('\r\n').split(' ', 2)
        if len(k) == 3 and '#' in k[2] and '/'.join(norm(k[2].rsplit('#', 1)[0]).split('/')[-2:]) in want:
            n += 1
            continue
        keep.append(line)
    if n:
        with open(path, 'w', encoding='utf-8', newline='') as w:
            w.writelines(keep)
    return n


def apply(write, iso=None, force=False):
    patches = sorted(glob.glob(os.path.join(PATCH_DIR, '*.txt')))
    root = os.path.join(BASE, 'assets', 'Packfile')
    import open_paks as op
    done = skipped = bad = reopened = french = 0
    state_path = os.path.join(PATCH_DIR, 'applied.json')
    state = json.load(open(state_path)) if os.path.isfile(state_path) else {}
    originals = {}
    texs = []

    def original(path, num):
        # original entry from the ISO when the one on disk was already patched
        if path not in originals:
            originals[path] = None
            if iso and os.path.isfile(iso):
                try:
                    sd, rows = read_manifest(os.path.join(BASE, 'assets'))
                    originals[path] = split_pak(iso_entry(iso, sd, *rows[path]))
                except Exception as ex:
                    print('%s: original not readable from the ISO (%s)' % (path, ex))
        o = originals[path]
        return o[num - 1] if o and num <= len(o) else None
    for pf in patches:
        name = os.path.basename(pf)[:-4]
        for pak in parse(pf):
            if op.es_frances(pak['path']):
                continue
            folder = os.path.join(root, pak['path'][:-4] if pak['path'].lower().endswith('.pak') else pak['path'])
            if not os.path.isdir(folder):
                print('%s: %s is not an opened folder, skipped' % (name, pak['path']))
                bad += 1
                continue
            files = entry_files(folder)
            for e in pak['entries']:
                label = '%s: %s entry %d' % (name, pak['path'], e['num'])
                srcs, ok = {}, True
                for num, h in e['need'].items():
                    p = files.get(num)
                    d = open(p, 'rb').read() if p and os.path.isfile(p) else op.empaquetar_txt1(p) if p and is_txt1(p) else None
                    if d is None or digest(d) != h:
                        d = original(pak['path'], num)
                    if d is None or digest(d) != h:
                        print('%s: source entry %d differs from the expected original, skipped' % (label, num))
                        ok = False
                        break
                    srcs[num] = d
                if not ok:
                    bad += 1
                    continue
                dest = os.path.join(folder, e['arg']) if e['kind'] == 'new' else files.get(e['num'])
                if dest and re.search(r'\.(rhg|atex)(@2x)?\.png$', dest, re.I):
                    dest = re.sub(r'(@2x)?\.png$', '', dest, flags=re.I)
                if dest and op.es_frances(os.path.basename(dest)):   # French part: an empty file keeps the numbering
                    if write and (not os.path.isfile(dest) or os.path.getsize(dest)):
                        op.quitar_textura(dest)
                        open(dest, 'wb').close()
                    french += 1
                    continue
                # a texture opened by --abrir-tex (loose png or folder of png) is compared and rewritten in that form
                if dest and dest.lower().endswith(('.rhg', '.atex')) and not os.path.isfile(dest):
                    import open_paks as op
                    d = rebuild(e['ops'], srcs)
                    if len(d) != e['size'] or digest(d) != e['hash']:
                        print('%s: result does not match, skipped' % label)
                        bad += 1
                        continue
                    key = '%s#%d' % (pak['path'], e['num'])
                    cur = op.empaquetar_tex(dest) if op.pngs_de(dest) else None
                    texs.append(os.path.relpath(dest, root))
                    if cur is not None and op.mismo_tex(dest, cur, d):
                        state[key] = e['hash']
                        skipped += 1
                        continue
                    if write:
                        if not op.abrir_tex(dest, d):
                            op.quitar_textura(dest)
                            with open(dest, 'wb') as o:
                                o.write(d)
                        state[key] = e['hash']
                    done += 1
                    continue
                # an opened txt1 (folder with _txt1.bin) is compared repacked and reopened with the patched text
                if dest and is_txt1(dest):
                    d = rebuild(e['ops'], srcs)
                    if len(d) != e['size'] or digest(d) != e['hash']:
                        print('%s: result does not match, skipped' % label)
                        bad += 1
                        continue
                    key = '%s#%d' % (pak['path'], e['num'])
                    if op.mismo_txt1(op.empaquetar_txt1(dest), d):
                        state[key] = e['hash']
                        skipped += 1
                        continue
                    if key in state and not force:
                        print('%s: text was edited after patching, kept (--force replaces it)' % label)
                        skipped += 1
                        continue
                    if write:
                        if not op.abrir_txt1(dest, d):
                            print('%s: patched text could not be opened, skipped' % label)
                            bad += 1
                            continue
                        state[key] = e['hash']
                    done += 1
                    continue
                # an opened nested pak (folder NN_name.pak) is replaced by the patched pak as one raw entry
                opened = bool(dest) and os.path.isdir(dest) and dest.lower().endswith('.pak')
                if not dest or (os.path.isdir(dest) and not opened):
                    print('%s: target missing or opened as a folder, skipped' % label)
                    bad += 1
                    continue
                sub = pak['path'] + '/' + os.path.relpath(dest, folder).replace('\\', '/')
                nested = dest.lower().endswith('.pak')
                key = '%s#%d' % (pak['path'], e['num'])
                cur = open(dest, 'rb').read() if os.path.isfile(dest) else None
                if cur is not None and digest(cur) == e['hash']:
                    state[key] = e['hash']
                    skipped += 1
                    if write and nested and reopen(dest, cur, sub):
                        reopened += 1
                    continue
                if opened:
                    import open_paks as op
                    d = rebuild(e['ops'], srcs)
                    if len(d) == e['size'] and digest(d) == e['hash'] and op.mismo_contenido(op.empaquetar(dest), d):
                        state[key] = e['hash']
                        skipped += 1
                        continue
                ours = cur is not None and state.get(key) == digest(cur)
                if e['kind'] == 'mod' and not opened and (cur is None or digest(cur) != e['arg']) and not ours and not force:
                    print('%s: target was edited, skipped (--force replaces it)' % label)
                    bad += 1
                    continue
                d = rebuild(e['ops'], srcs)
                if len(d) != e['size'] or digest(d) != e['hash']:
                    print('%s: result does not match, skipped' % label)
                    bad += 1
                    continue
                if write:
                    if opened:
                        shutil.rmtree(dest)
                    os.makedirs(os.path.dirname(dest), exist_ok=True)
                    with open(dest, 'wb') as o:
                        o.write(d)
                    state[key] = e['hash']
                    if nested and reopen(dest, d, sub):
                        reopened += 1
                done += 1
    if write:
        with open(state_path, 'w', encoding='utf-8') as w:
            json.dump(state, w, indent=0, sort_keys=True)
        n = sizes_drop(texs)
        if n:
            print('asset patches: %d texture sizes of patched textures renewed' % n)
    print('asset patches: %d files, %d applied%s, %d already applied, %d failed, %d nested paks reopened, %d French left empty' % (len(patches), done, '' if write else ' (check only)', skipped, bad, reopened, french))
    return bad


# French entries of the patch files keep only their entry line with an empty size: --apply leaves them as empty files
def strip_french():
    import open_paks as op
    root = os.path.join(BASE, 'assets', 'Packfile')
    for pf in sorted(glob.glob(os.path.join(PATCH_DIR, '*.txt'))):
        lines = open(pf, encoding='utf-8').read().split('\n')
        out, fr_pak, skip, files, n = [], False, False, {}, 0
        for line in lines:
            k = line.split()
            if k and k[0] == 'pak':
                fr_pak, skip = op.es_frances(k[1]), False
                folder = os.path.join(root, k[1][:-4] if k[1].lower().endswith('.pak') else k[1])
                files = entry_files(folder) if os.path.isdir(folder) else {}
                if fr_pak:
                    n += 1
                    continue
            elif fr_pak:
                continue
            elif k and k[0] == 'entry':
                name = k[3] if k[2] == 'new' else os.path.basename(files.get(int(k[1]), ''))
                skip = bool(name) and op.es_frances(os.path.basename(name))
                if skip:
                    out.append('entry %s %s %s 0 %s' % (k[1], k[2], k[3], digest(b'')))
                    n += 1
                    continue
            elif skip:
                continue
            out.append(line)
        with open(pf, 'w', encoding='utf-8', newline='\n') as w:
            w.write('\n'.join(out))
        print('%s: %d French blocks or entries left empty' % (os.path.basename(pf), n))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--make')
    ap.add_argument('paks', nargs='*')
    ap.add_argument('--name', action='append', default=[])
    ap.add_argument('--shared', nargs='+', default=[])
    ap.add_argument('--iso', default=os.path.join(BASE, 'game', 'game.iso'))
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--strip-french', action='store_true')
    a = ap.parse_args()
    if a.strip_french:
        strip_french()
    elif a.make:
        make(a)
    elif a.apply or a.check:
        sys.exit(1 if apply(a.apply, a.iso, a.force) else 0)
    else:
        ap.print_help()


if __name__ == '__main__':
    main()
