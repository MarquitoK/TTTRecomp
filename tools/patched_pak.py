#!/usr/bin/env python3
# Rebuilds edited paks from their originals with an asset patch: python3 tools/patched_pak.py NAME ORIGINAL.pak...
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import asset_patches as ap


def main():
    if len(sys.argv) < 3:
        raise SystemExit('uso: patched_pak.py NOMBRE_PARCHE ORIGINAL.pak...')
    paks = ap.parse(os.path.join(ap.PATCH_DIR, sys.argv[1] + '.txt'))
    out_dir = os.path.join(ap.BASE, 'patch_in')
    os.makedirs(out_dir, exist_ok=True)
    for src in sys.argv[2:]:
        d = open(src, 'rb').read()
        h = ap.digest(d)
        done = False
        for p in paks:
            for e in p['entries']:
                if done or e['need'].get(e['num']) != h:
                    continue
                r = ap.rebuild(e['ops'], {e['num']: d})
                if len(r) != e['size'] or ap.digest(r) != e['hash']:
                    raise SystemExit('%s: el parche no reconstruye el pak' % src)
                dst = os.path.join(out_dir, os.path.basename(src))
                open(dst, 'wb').write(r)
                print('%s -> %s (%d bytes, entradas %d)' % (src, os.path.relpath(dst, ap.BASE), len(r), len(ap.split_pak(r))))
                done = True
        if not done:
            print('%s: no coincide con ningun original del parche %s' % (src, sys.argv[1]))


main()
