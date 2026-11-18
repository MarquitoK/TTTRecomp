#!/usr/bin/env python3
# Restores from game/game.iso the nested paks of assets/Packfile that were emptied or left incomplete (an empty file, or an
# opened folder with fewer entries than the original), by name pattern (default: the select paks of every language menu),
# and every opened txt1 whose text file is empty or invalid (its txt1.txt line is rewritten).
#   (default)        lists what would be restored, touches nothing
#   --apply          restores (raw pak written, then opened as a folder; run open_paks.py --abrir-tex afterwards)
#   --names a b ...  base names to check (default char_select.pak map_select.pak)
# Assets must not be unified (unify_assets.py --undo first).
import argparse, os, shutil, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as op
import asset_patches as ap

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')


def retry(fn, *a):
    for i in range(40):
        try:
            return fn(*a)
        except PermissionError:
            if i == 39:
                raise
            time.sleep(0.5)


def chain(rel, rows):   # top manifest key and the 1-based nested entry numbers down to rel
    parts = rel.split('/')
    low = dict((k.lower(), k) for k in rows)
    for i in range(len(parts) - 1, 0, -1):
        cand = '/'.join(parts[:i])
        key = low.get(cand.lower()) or low.get((cand + '.pak').lower())
        if key:
            nums = []
            for x in parts[i:]:
                n = ap.NUM.match(x)
                if not n:
                    return None, None, None
                nums.append(int(n.group(1)))
            return key, nums, '/'.join(parts[i:])
    return None, None, None


def nested_bytes(iso, sd, rows, key, nums):
    d = ap.iso_entry(iso, sd, *rows[key])
    for n in nums:
        e = ap.split_pak(d)
        if n < 1 or n > len(e):
            return None
        d = e[n - 1]
    return d


def txt1_ok(p):
    try:
        return bool(op.leer_texto(p)) if os.path.isfile(op.ruta_texto(p)) else bool(op.listar_txt1(p))
    except Exception:
        return False


def txt1_forget(assets, rel):   # drops the txt1.txt lines of that container (abrir_txt1 writes a fresh one)
    path = os.path.join(assets, 'txt1.txt')
    if not os.path.isfile(path):
        return
    lines = open(path, encoding='utf-8').read().split('\n')
    keep = [l for l in lines if l.split('\t')[0].lower() != rel.lower()]
    with open(path, 'w', encoding='utf-8', newline='\n') as o:
        o.write('\n'.join(keep))
    op._indice_txt1.clear()


def main():
    a = argparse.ArgumentParser()
    a.add_argument('--apply', action='store_true')
    a.add_argument('--names', nargs='+', default=['char_select.pak', 'map_select.pak'])
    a.add_argument('--iso', default=os.path.join(BASE, 'game', 'game.iso'))
    a = a.parse_args()
    assets = os.path.join(BASE, 'assets')
    if os.path.isfile(os.path.join(assets, 'shared.txt')):
        raise SystemExit('assets/shared.txt exists: run python3 tools/unify_assets.py --undo first')
    if not os.path.isfile(a.iso):
        raise SystemExit('missing ' + a.iso)
    sd, rows = ap.read_manifest(assets)
    root = os.path.join(assets, 'Packfile')
    names = set(x.lower() for x in a.names)
    found, bad = [], 0
    for dp, dn, fn in os.walk(root):
        for x in sorted(dn + fn, key=op.clave_natural):
            if x.lower().endswith('.txt1') and x in dn:
                p = os.path.join(dp, x)
                rel = os.path.relpath(p, root).replace('\\', '/')
                if op.es_frances(rel) or txt1_ok(p):
                    continue
                key, nums, inner = chain(rel, rows)
                d = nested_bytes(a.iso, sd, rows, key, nums) if key else None
                if d is None or not op.analizar_txt1(d):
                    print('txt1 sin entrada en el ISO: ' + rel)
                    bad += 1
                    continue
                found.append((p, rel, key, nums, d, 'txt1 vacio o invalido', inner))
                print('%s: txt1 vacio o invalido' % rel)
                continue
            if ap.NUM.sub('', x).lower() not in names:
                continue
            p = os.path.join(dp, x)
            rel = os.path.relpath(p, root).replace('\\', '/')
            if op.es_frances(rel):
                continue
            key, nums, inner = chain(rel, rows)
            if not key:
                print('sin entrada en el manifiesto: ' + rel)
                bad += 1
                continue
            d = nested_bytes(a.iso, sd, rows, key, nums)
            if d is None or not op.analizar(d):
                print('no es un pak en el ISO: ' + rel)
                bad += 1
                continue
            n_iso = len(ap.split_pak(d))
            if os.path.isdir(p):   # sections may restart the numbering: only an empty folder counts as damaged
                state = 'carpeta vacia' if not ap.entry_files(p) else ''
            else:
                state = 'archivo vacio' if not os.path.getsize(p) else ''
            if state:
                found.append((p, rel, key, nums, d, state, inner))
                print('%s: %s' % (rel, state))
            if os.path.isdir(p):
                dn[:] = [y for y in dn if y != x]
    print('restaurar: %d danados, %d sin resolver' % (len(found), bad))
    if not a.apply or not found:
        if not a.apply and found:
            print('restaurar: solo lista; --apply restaura')
        return
    done = raw = 0
    for p, rel, key, nums, d, state, inner in found:
        if os.path.isdir(p):
            retry(shutil.rmtree, p)
        if rel.lower().endswith('.txt1'):
            txt1_forget(assets, rel)
            if op.abrir_txt1(p, d):
                done += 1
            else:
                raw += 1
                print('restaurar: %s no se pudo abrir como txt1' % rel)
            continue
        with open(p, 'wb') as o:
            o.write(d)
        if ap.reopen(p, d, key + '/' + inner):
            done += 1
        else:
            raw += 1
            print('restaurar: %s queda como pak sin abrir' % rel)
    print('restaurar: %d restaurados y abiertos, %d como pak; ahora python3 tools/open_paks.py --abrir-tex' % (done, raw))


if __name__ == '__main__':
    main()
