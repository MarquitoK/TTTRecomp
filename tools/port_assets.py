#!/usr/bin/env python3
# Builds assets/ from game/game.iso in one go: extraction, paks, texts, asset patches, textures, audio and shared copies.
import argparse, os, subprocess, sys

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
TOOLS = os.path.join(BASE, 'tools')
STEPS = [
    ('extraction', ['extract_assets.py']),
    ('paks and texts', ['open_paks.py', '--abrir']),
    ('move lists', ['move_lists.py', '--apply']),
    ('asset patches', ['asset_patches.py', '--apply']),
    ('textures', ['open_paks.py', '--abrir-tex']),
    ('audio', ['expose_at3.py']),
    ('French out', ['drop_french.py', '--apply']),
    ('shared copies', ['unify_assets.py', '--apply']),
]


def ask_colors(assets, choice):
    sys.path.insert(0, TOOLS)
    import colors
    if not colors.all_four(assets):
        return
    if not choice and sys.stdin.isatty() and sys.stdout.isatty():
        print('All 70 original characters have 4 colours.\n  1 = extract all colours (default)\n  2 = only the original colours')
        choice = 'original' if input('Choice [1]: ').strip() == '2' else 'all'
    if choice == 'original':
        colors.original(assets, True)
    else:
        print('colors: all colours kept')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--colors', choices=['all', 'original'])
    a = ap.parse_args()
    assets = os.path.join(BASE, 'assets')
    if not os.path.isfile(os.path.join(BASE, 'game', 'game.iso')):
        raise SystemExit('missing game/game.iso')
    if os.path.isdir(os.path.join(assets, 'Packfile')):
        raise SystemExit('assets/Packfile already exists; move or delete assets/ first')
    ok_mark = os.path.join(assets, '.port_assets_ok')
    for k, (name, cmd) in enumerate(STEPS, 1):
        print('== %d/%d %s' % (k, len(STEPS), name), flush=True)
        if subprocess.call([sys.executable, os.path.join(TOOLS, cmd[0])] + cmd[1:], cwd=BASE):
            raise SystemExit('step "%s" failed; fix it and run the remaining steps by hand' % name)
        if k == 1:
            ask_colors(assets, a.colors)
    sidecars = sum(1 for dp, dn, fn in os.walk(os.path.join(assets, 'Packfile')) for n in fn if n == '_txt1.bin')
    index = os.path.join(assets, 'txt1.txt')
    lines = [l for l in open(index, encoding='utf-8') if not l.startswith('#')] if os.path.isfile(index) else []
    print('== done: %d txt1 in txt1.txt (%d raw), %d _txt1.bin left' % (len(lines), sum('\traw\t' in l for l in lines), sidecars))
    open(ok_mark, 'w').close()


if __name__ == '__main__':
    main()
