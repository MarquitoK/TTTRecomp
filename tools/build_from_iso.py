#!/usr/bin/env python3
# Builds the port from a PSP ISO (ULUS10537): game files, patches, C++ translation, compilation and assets.
# The setup assistant runs it and reads the lines that start with @: "@paso K N clave", "@avance a b", "@error clave texto", "@fin".
#   --iso PATH  --root DIR (install folder; the port's own folders are copied there when missing)  --colors all|original
#   --until iso|juego|parches|traducir|compilar|assets  --jobs N  --no-unlock  --compare DIR (sha1 of game/ against another install)  --force
#   --resume skips the steps whose inputs did not change (build_state.json): an update only redoes what its files touch
# ASSETS_VERSION: raise it when the extractor changes in a way that needs assets/ made again; asset patch changes alone are applied in place
import argparse, hashlib, json, os, re, shutil, struct, subprocess, sys, time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
OWN = ['recompiler', 'patchers', 'games', 'runtime', 'tools']
DISC_ID, EBOOT_SHA = 'ULUS10537', 'eb7bffa629a689df'
OVERLAYS = [7, 11, 13, 17, 21, 24, 32, 48, 56, 65, 13933]
LAYERS = 'base,anim,move,actions,actions5,float,camera,wlan,struggle,input,debris'
PATCHES = [('patch_border.py', ['--nivel', '2']), ('patch_name.py', ['--sin-aviso']), ('patch_boot.py', []), ('patch_limits.py', []), ('patch_unlock.py', []),
           ('patch_60fps.py', ['--layers', LAYERS]), ('patch_roster.py', [])]
STEPS = ['iso', 'juego', 'parches', 'traducir', 'compilar', 'assets']
ASSETS_VERSION = 1
CXXFLAGS = '-O2 -std=c++17 -fno-strict-aliasing -w -I../../runtime -I.'
SECTOR = 2048


def say(*a):
    print(*a, flush=True)


def fail(key, text):
    say('@error %s %s' % (key, text))
    sys.exit(1)


def step(k, key):
    say('@paso %d %d %s' % (k, len(STEPS), key))


def run(cmd, cwd, key, on_line=None):
    say('$ ' + ' '.join(cmd))
    p = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding='utf-8', errors='replace', bufsize=1)
    tail = []
    for line in p.stdout:
        line = line.rstrip()
        say(line)
        tail = (tail + [line])[-6:]
        if on_line:
            on_line(line)
    if p.wait():
        fail(key, (tail[-1] if tail else 'codigo %d' % p.returncode)[:300])


def tree_hash(root, seed, parts):
    h = hashlib.sha1(seed.encode())
    for part in parts:
        base = os.path.join(root, part)
        files = [base] if os.path.isfile(base) else [os.path.join(dp, f) for dp, dn, fn in os.walk(base) for f in fn if '__pycache__' not in dp]
        for f in sorted(files):
            if part.endswith('/*') or os.path.basename(f) == 'applied.json':
                continue
            h.update(os.path.relpath(f, root).replace('\\', '/').encode())
            h.update(open(f, 'rb').read())
    return h.hexdigest()


def top_files(root, d):
    p = os.path.join(root, d)
    return [os.path.join(d, n) for n in sorted(os.listdir(p)) if os.path.isfile(os.path.join(p, n))] if os.path.isdir(p) else []


def sfo_id(d):
    if d[:4] != b'\x00PSF':
        return ''
    kt, dt, n = struct.unpack_from('<III', d, 8)
    for i in range(n):
        ko, fmt, ln, mx, do = struct.unpack_from('<HHIII', d, 20 + i * 16)
        if d[kt + ko:d.index(b'\0', kt + ko)] == b'DISC_ID':
            return d[dt + do:dt + do + ln].rstrip(b'\0').decode('ascii', 'replace')
    return ''


def stage(root):
    for name in OWN:
        dst = os.path.join(root, name)
        if not os.path.isdir(dst) and os.path.abspath(dst) != os.path.join(REPO, name):
            shutil.copytree(os.path.join(REPO, name), dst)
    os.makedirs(os.path.join(root, 'game', 'overlays'), exist_ok=True)
    os.makedirs(os.path.join(root, 'generated', 'src'), exist_ok=True)


def read_iso(iso, root):
    sys.path.insert(0, os.path.join(root, 'tools'))
    import extract_assets as ea
    f = open(iso, 'rb')
    try:
        files = {k.upper(): v for k, v in ea.iso_listar(f).items()}
    except SystemExit:
        fail('iso_formato', 'el archivo no es una ISO de PSP')
    def get(name):
        if name not in files:
            return None
        lba, size = files[name]
        f.seek(lba * SECTOR)
        return f.read(size)
    disc = sfo_id(get('PSP_GAME/PARAM.SFO') or b'')
    if disc != DISC_ID:
        fail('iso_juego', 'la ISO es %s y el port necesita %s (Dragon Ball Z: Tenkaichi Tag Team, EE. UU.)' % (disc or 'desconocida', DISC_ID))
    eboot = get('PSP_GAME/SYSDIR/EBOOT.OLD')
    if eboot is None or eboot[:4] != b'\x7fELF':
        eboot = get('PSP_GAME/SYSDIR/EBOOT.BIN')
    if eboot is None or eboot[:4] != b'\x7fELF' or not hashlib.sha1(eboot).hexdigest().startswith(EBOOT_SHA):
        fail('iso_eboot', 'el EBOOT de la ISO no es el original reconocido (version 1.00)')
    pk = files.get('PSP_GAME/USRDIR/PACKFILE.BIN')
    if not pk:
        fail('iso_packfile', 'la ISO no tiene PSP_GAME/USRDIR/PACKFILE.BIN')
    w1, w2 = struct.unpack_from('<2I', eboot, 0x0882FBF8 - 0x08804040 + 0x1018)
    sd = w1 & 0xFFFF if (w1 >> 16) == 0x3417 and (w2 >> 16) == 0x341E else None
    nent, sd, ent, _ = ea.leer_indice(f, pk[0], pk[1], sd)
    ovl = {}
    for i in OVERLAYS:
        s, t, _, _ = ent[i]
        f.seek((pk[0] + sd + s) * SECTOR)
        d = f.read((t + 15) & ~15)
        if d[:4] != b'\x7fELF':
            fail('iso_overlay', 'la entrada %d del PACKFILE no es un overlay' % i)
        ovl['%d_%X.elf' % (i, i)] = d
    f.close()
    say('iso: %s, EBOOT original, PACKFILE con %d entradas, %d overlays' % (disc, nent, len(ovl)))
    return eboot, ovl


def copy_iso(iso, dst):
    if os.path.exists(dst) and os.path.samefile(iso, dst):
        return
    total, done = os.path.getsize(iso), 0
    with open(iso, 'rb') as a, open(dst + '.part', 'wb') as b:
        while True:
            c = a.read(8 << 20)
            if not c:
                break
            b.write(c)
            done += len(c)
            say('@avance %d %d' % (done >> 20, total >> 20))
    os.replace(dst + '.part', dst)


def compare(root, other):
    rows = []
    names = sorted(n for n in os.listdir(os.path.join(other, 'game', 'overlays')) if n.endswith(('.elf', '.elf.60')))
    for rel in ['game/EBOOT.OLD', 'game/EBOOT.OLD.60'] + ['game/overlays/' + n for n in names]:
        a, b = os.path.join(root, rel), os.path.join(other, rel)
        ha = hashlib.sha1(open(a, 'rb').read()).hexdigest()[:16] if os.path.exists(a) else 'falta'
        hb = hashlib.sha1(open(b, 'rb').read()).hexdigest()[:16] if os.path.exists(b) else 'falta'
        rows.append((rel, ha, hb))
        say('comparar: %-34s %s %s %s' % (rel, ha, hb, 'igual' if ha == hb else 'DISTINTO'))
    say('comparar: %d de %d iguales' % (sum(r[1] == r[2] for r in rows), len(rows)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--iso', required=True)
    ap.add_argument('--root', default=REPO)
    ap.add_argument('--colors', choices=['all', 'original'], default='all')
    ap.add_argument('--until', choices=STEPS, default='assets')
    ap.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 1))
    ap.add_argument('--compare')
    ap.add_argument('--force', action='store_true')
    ap.add_argument('--no-unlock', action='store_true')
    ap.add_argument('--resume', action='store_true')
    a = ap.parse_args()
    root, iso = os.path.abspath(a.root), os.path.abspath(a.iso)
    last = STEPS.index(a.until) + 1
    t0 = time.time()
    if not os.path.isfile(iso):
        fail('iso_falta', 'no existe ' + iso)
    if os.path.exists(os.path.join(root, 'game', 'EBOOT.OLD')) and not a.force and not a.resume:
        fail('ya_instalado', 'ya hay un juego en %s (usa --force para reconstruirlo)' % os.path.join(root, 'game'))
    stage(root)
    py = sys.executable
    state_path = os.path.join(root, 'build_state.json')
    try:
        st = json.load(open(state_path)).get('h', {}) if a.resume else {}
    except Exception:
        st = {}
    data = top_files(root, os.path.join('games', 'ttt'))
    hp = tree_hash(root, 'unlock=%d' % (not a.no_unlock), ['patchers'] + data)
    ht = tree_hash(root, hp, ['recompiler'])
    hc = tree_hash(root, ht, ['runtime', os.path.join('tools', 'launcher.cpp'), os.path.join('tools', 'launcher.rc')])
    ha = tree_hash(root, '', [os.path.join('games', 'ttt', 'asset_patches')])

    def save():
        json.dump({'h': st}, open(state_path, 'w'), indent=0)

    def skip(k, key, h):
        if st.get(key) == h:
            step(k, key)
            say('%s: sin cambios, se conserva' % key)
            say('@avance 1 1')
            return True
        return False

    if st.get('parches') == hp:
        for k, key in ((1, 'iso'), (2, 'juego'), (3, 'parches')):
            skip(k, key, hp)
    else:
        st.pop('traducir', None); st.pop('compilar', None)
        game_and_patches(a, root, iso, py, last)
        if last >= 3:
            st['parches'] = hp
            save()
    clean_backups(root)
    if last <= 3:
        return say('@fin')

    if not skip(4, 'traducir', ht):
        st.pop('compilar', None)
        translate(root, py)
        st['traducir'] = ht
        save()
    if last == 4:
        return say('@fin')

    if not skip(5, 'compilar', hc):
        build(a, root)
        st['compilar'] = hc
        save()
    if last == 5:
        return say('@fin')

    step(6, 'assets')
    ok_mark = os.path.join(root, 'assets', '.port_assets_ok')
    packfile = os.path.join(root, 'assets', 'Packfile')
    try:
        aver = int(open(ok_mark).read().strip() or 1)
    except Exception:
        aver = 1 if os.path.isfile(os.path.join(root, 'assets', 'shared.txt')) else 0   # finished before the marker existed: shared.txt is the last step's
    dev = not os.path.isdir(os.path.join(root, 'toolchain'))   # his own work folder: assets are his, never redone here
    if os.path.isdir(packfile) and (dev or aver >= ASSETS_VERSION):
        if aver and st.get('assets') != ha:
            say('assets: parches de assets nuevos, se aplican sin reextraer')
            say('@avance 0 3')
            run([py, os.path.join(root, 'tools', 'unify_assets.py'), '--undo'], root, 'assets')
            say('@avance 1 3')
            run([py, os.path.join(root, 'tools', 'asset_patches.py'), '--apply'], root, 'assets')
            say('@avance 2 3')
            run([py, os.path.join(root, 'tools', 'unify_assets.py'), '--apply'], root, 'assets')
        else:
            say('assets: ya existen, se conservan')
        st['assets'] = ha
        save()
        say('@avance 1 1')
        say('listo en %d s' % (time.time() - t0))
        return say('@fin')
    if aver and aver < ASSETS_VERSION:
        say('assets: el extractor cambio (version %d -> %d), se extraen de nuevo' % (aver, ASSETS_VERSION))
    elif os.path.isdir(os.path.join(root, 'assets')):
        say('assets: incompletos de un intento anterior, se rehacen')
    if os.path.isdir(os.path.join(root, 'assets')):
        shutil.rmtree(os.path.join(root, 'assets'))
    def assets(line):
        m = re.match(r'== (\d+)/(\d+) ', line)
        if m:
            say('@avance %d %s' % (int(m.group(1)) - 1, m.group(2)))
    run([py, os.path.join(root, 'tools', 'port_assets.py'), '--colors', a.colors], root, 'assets', assets)
    open(ok_mark, 'w').write(str(ASSETS_VERSION))
    st['assets'] = ha
    save()
    say('listo en %d s' % (time.time() - t0))
    say('@fin')


# copies the patchers keep to undo themselves: the build always starts again from the ISO, so they are not needed
BACKUPS = ('.borde', '.desbloqueo', '.limites', '.roster', '.nombre', '.inicio')


def clean_backups(root):
    n = 0
    for d in (os.path.join(root, 'game'), os.path.join(root, 'game', 'overlays')):
        for f in os.listdir(d) if os.path.isdir(d) else []:
            if f.endswith(BACKUPS):
                os.remove(os.path.join(d, f))
                n += 1
    if n:
        say('juego: %d copias de respaldo de los parches borradas' % n)


def game_and_patches(a, root, iso, py, last):
    step(1, 'iso')
    eboot, ovl = read_iso(iso, root)
    if last == 1:
        return
    step(2, 'juego')
    g = os.path.join(root, 'game')
    for n in os.listdir(os.path.join(g, 'overlays')):
        os.remove(os.path.join(g, 'overlays', n))
    for n in os.listdir(g):
        if n.startswith('EBOOT.OLD'):
            os.remove(os.path.join(g, n))
    open(os.path.join(g, 'EBOOT.OLD'), 'wb').write(eboot)
    for n, d in ovl.items():
        open(os.path.join(g, 'overlays', n), 'wb').write(d)
    copy_iso(iso, os.path.join(g, 'game.iso'))
    if last == 2:
        return

    step(3, 'parches')
    patches = [p for p in PATCHES if not (a.no_unlock and p[0] == 'patch_unlock.py')]
    for k, (script, args) in enumerate(patches):
        say('@avance %d %d' % (k, len(patches)))
        run([py, os.path.join(root, 'patchers', script)] + args, root, 'parches')
    say('@avance %d %d' % (len(patches), len(patches)))
    if a.compare:
        compare(root, os.path.abspath(a.compare))


def translate(root, py):
    say('@avance 0 2')
    run([py, os.path.join(root, 'recompiler', 'analyzer.py'), os.path.join(root, 'game', 'EBOOT.OLD'), '--overlays', os.path.join(root, 'game', 'overlays'),
         '-o', os.path.join(root, 'generated', 'programa.json')], root, 'traducir')
    say('@avance 1 2')
    run([py, os.path.join(root, 'recompiler', 'generator.py')], root, 'traducir')
    say('@avance 2 2')


def build(a, root):
    src = os.path.join(root, 'generated', 'src')
    total = len([n for n in os.listdir(src) if n.endswith(('.cpp', '.c'))]) + 30
    done = [0]
    def count(line):
        if re.search(r'\s-c\s', line):
            done[0] += 1
            say('@avance %d %d' % (min(done[0], total), total))
    make = shutil.which('make') or shutil.which('mingw32-make') or fail('compilar', 'no encuentro make ni mingw32-make')
    run([make, '-C', src, 'tttrecomp', 'CXXFLAGS=' + CXXFLAGS, '-j%d' % a.jobs], root, 'compilar', count)
    res = []
    if os.path.isfile(os.path.join(root, 'games', 'ttt', 'icons', 'tttrecomp.ico')):
        run(['windres', os.path.join('tools', 'launcher.rc'), '-O', 'coff', '-o', os.path.join('generated', 'launcher_res.o')], root, 'compilar')
        res = [os.path.join('generated', 'launcher_res.o')]
    run(['g++', '-O2', '-std=c++17', '-municode', '-mwindows', '-static', '-o', 'launcher.exe', os.path.join('tools', 'launcher.cpp')] + res + ['-lwinhttp'], root, 'compilar')
    gxx = shutil.which('g++')
    for dll in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
        p = os.path.join(os.path.dirname(gxx), dll) if gxx else ''
        if p and os.path.isfile(p):
            shutil.copy2(p, os.path.join(src, dll))
    say('@avance %d %d' % (total, total))


main()
