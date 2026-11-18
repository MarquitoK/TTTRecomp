import os, sys, json, argparse, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mips, emitter
from analyzer import Imagen, BANCO_VA, BANCO_FIN
from game_data import GAME, section, addresses

PER_UNIT = 350


def load_game(game):
    g = {k: set(addresses(section('recomp.txt', k, game))) for k in ('hooks', 'wrappers', 'halves', 'probes', 'after')}
    g['conv60'] = addresses(section('recomp.txt', 'conv60', game))
    g['ovl_wrappers'] = set((l.split()[0], int(w, 16)) for l in section('recomp.txt', 'ovl_wrappers', game) for w in l.split()[1:])
    g['free_classes'] = [w for l in section('recomp.txt', 'free_classes', game) for w in l.split()]
    g['legacy_env'] = [tuple(l.split()) for l in section('recomp.txt', 'legacy_env', game)]
    for p in g['legacy_env']:
        if len(p) != 2: raise SystemExit('recomp.txt [legacy_env]: expected "OLD NEW": %s' % ' '.join(p))
    return g


def nid_names():
    p = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'psp_nids.json')
    if not os.path.exists(p):
        return {}
    d = json.load(open(p))
    out, loose = {}, {}
    for lib, tab in d.items():
        if not isinstance(tab, dict):
            continue
        for k, v in tab.items():
            try:
                nid = int(k, 16) & 0xFFFFFFFF
            except (ValueError, TypeError):
                continue
            out[(lib, nid)] = v
            loose.setdefault(nid, v)
    out['_loose'] = loose
    return out


def hle_table(main_img):
    nid = nid_names()
    hle, rows = {}, []
    for lib in main_img.e.imports():
        for f in lib['funcs']:
            name = nid.get((lib['lib'], f['nid'])) or nid['_loose'].get(f['nid'], '')
            hle[f['stub']] = len(rows)
            rows.append((lib['lib'], f['nid'], name, f['stub']))
    return hle, rows


def write(path, text):
    # unchanged files are not rewritten so make does not rebuild them
    if os.path.exists(path) and open(path, encoding='utf-8').read() == text:
        return
    with open(path, 'w', encoding='utf-8') as f:
        f.write(text)


def game_data_cpp(g):
    conv = g['conv60'] or [0]
    t = ['#include <cstdint>',
         'extern const uint32_t GAME_CONV60[] = {%s};' % ', '.join('0x%08Xu' % a for a in conv),
         'extern const int GAME_CONV60_N = %d;' % len(g['conv60']),
         'extern const char* const GAME_FREE_CLASSES = "%s";' % ','.join(g['free_classes']),
         'extern const char* const GAME_LEGACY_ENV[][2] = {']
    t += ['    {"%s", "%s"},' % p for p in g['legacy_env'] or [('', '')]]
    t += ['};', 'extern const int GAME_LEGACY_ENV_N = %d;' % len(g['legacy_env'])]
    return '\n'.join(t) + '\n'


def generate(eboot, overlays_dir, map_json, out, game=GAME, log=print):
    os.makedirs(out, exist_ok=True)
    g = load_game(game)
    HOOKS, WRAPPERS, HALVES, PROBES = g['hooks'], g['wrappers'], g['halves'], g['probes']
    emitter.AFTER = g['after']
    doc = json.load(open(map_json))
    main_img = Imagen(eboot, 'principal')
    hle, hle_rows = hle_table(main_img)
    log('%d import stubs' % len(hle))

    # .60 copies from patch_60fps.py: differing functions are emitted twice and psp_fps60() picks per call
    def without60(path, *a):
        return Imagen(path + '.60', *a) if os.path.exists(path + '.60') else None
    banks = [('principal', main_img, doc['imagenes'][0], 0)]
    imgs30 = {'principal': without60(eboot, 'principal')}
    if overlays_dir:
        for e in doc['imagenes'][1:]:
            p = os.path.join(overlays_dir, e['nombre'] + '.elf')
            if os.path.exists(p):
                banks.append((e['nombre'], Imagen(p, e['nombre'], e['indice_packfile']), e,
                              e['indice_packfile']))
                imgs30[e['nombre']] = without60(p, e['nombre'], e['indice_packfile'])

    units = []
    summary = []
    for name, img, entry, idx in banks:
        pref = 'f' if name == 'principal' else 'b%d' % idx
        nf = (lambda a, _p=pref: '%s_%08X' % (_p, a))
        halves = HALVES if pref == 'f' else ()
        addrs = [int(f['a'], 16) for f in entry['funciones'] if int(f['a'], 16) not in hle]
        known = set(addrs)
        t0 = time.time()
        stores = pref == 'f'
        fns = [emitter.Funcion(img, a, known, nf, hle, HOOKS, PROBES, halves, stores) for a in sorted(addrs)]
        img30 = imgs30.get(name)
        only30 = set()
        if img30:   # targets that exist only in the original (patch_60fps moved the animation setter)
            for f in list(fns):
                if any(img30.u32(x) != img.u32(x) for x in f.instr):
                    h = emitter.Funcion(img30, f.ini, known, nf, hle, HOOKS, PROBES)
                    for i in h.instr.values():
                        if i and i.name == 'jal' and i.tgt not in known and i.tgt not in hle and img30.es_codigo(i.tgt):
                            only30.add(i.tgt)
            for x in sorted(only30):
                known.add(x)
                addrs.append(x)
                fns.append(emitter.Funcion(img30, x, known, nf, hle, HOOKS, PROBES, halves, stores))
            fns.sort(key=lambda f: f.ini)
        decl = ['void %s(Ctx& c);' % nf(f.ini) for f in fns]
        nvf = sum(f.vfpu for f in fns)
        chunks = [fns[i:i + PER_UNIT] for i in range(0, len(fns), PER_UNIT)]
        hdr = os.path.join(out, 'decl_%s.h' % pref)
        write(hdr, '#pragma once\n#include "psp.h"\n#include "cpu/vfpu_exec.h"\nuint32_t psp_half(uint32_t pc, uint32_t dir, uint32_t v);\nint psp_fps60();\n' + '\n'.join(decl) + '\n')
        dual = 0
        for k, ch in enumerate(chunks):
            body = ['#include "decl_%s.h"' % pref, '']
            for f in ch:
                lines = f.emitir()
                nm, ent = nf(f.ini), 0
                if img30 and f.ini not in only30 and any(img30.u32(a) != img.u32(a) for a in f.instr):
                    l30 = emitter.Funcion(img30, f.ini, known, nf, hle, HOOKS, PROBES, halves, stores).emitir()
                    lines[0] = 'static void %s_60(Ctx& c) {' % nm
                    l30[0] = 'static void %s_30(Ctx& c) {' % nm
                    lines += [''] + l30 + ['void %s(Ctx& c) { if (psp_fps60()) %s_60(c); else %s_30(c); }' % (nm, nm, nm)]
                    ent = len(lines) - 1
                    dual += 1
                if (pref == 'f' and f.ini in WRAPPERS) or (name, f.ini) in g['ovl_wrappers']:
                    lines[ent] = lines[ent].replace('void %s(' % nm, 'static void %s_orig(' % nm, 1)
                    lines += ['void %s(Ctx& c) { psp_wrap(0x%08Xu, c, %s_orig); }' % (nm, f.ini, nm)]
                body += lines
                body.append('')
            fn = '%s_%02d.cpp' % (pref, k)
            write(os.path.join(out, fn), '\n'.join(body))
            units.append(fn)
        summary.append((name, pref, len(fns), nvf, sorted(addrs)))
        log('  %-14s %5d functions, %5d VFPU, %2d units, %d dual 30/60, %d only 30, %.1fs'
            % (name, len(fns), nvf, len(chunks), dual, len(only30), time.time() - t0))

    t = ['#include "psp.h"']
    for name, pref, n, nvf, addrs in summary:
        t.append('#include "decl_%s.h"' % pref)
    t.append('')
    for name, pref, n, nvf, addrs in summary:
        t.append('static const Entry TAB_%s[] = {' % pref)
        for a in addrs:
            t.append('    {0x%08Xu, %s_%08X},' % (a, pref, a))
        t.append('};')
    t.append('')
    t.append('const Bank BANKS[] = {')
    for name, pref, n, nvf, addrs in summary:
        idx = -1 if name == 'principal' else next(b[3] for b in banks if b[0] == name)
        t.append('    {"%s", %d, TAB_%s, %d},' % (name, idx, pref, len(addrs)))
    t.append('};')
    t.append('const int BANKS_N = %d;' % len(summary))
    t.append('')
    t.append('const Import IMPORTS[] = {')
    for lib, nid, name, stub in hle_rows:
        t.append('    {"%s", 0x%08Xu, "%s", 0x%08Xu},' % (lib, nid, name, stub))
    t.append('};')
    t.append('const int IMPORTS_N = %d;' % len(hle_rows))
    write(os.path.join(out, 'tablas.cpp'), '\n'.join(t) + '\n')
    units.append('tablas.cpp')
    write(os.path.join(out, 'game_data.cpp'), game_data_cpp(g))
    units.append('game_data.cpp')

    mk = ['CXX ?= g++',
          'LIBS ?= $(if $(filter Windows_NT,$(OS)),-lgdi32 -lwinmm -lcomdlg32 -lshell32 -lole32 -luuid -lws2_32 -lwinhttp -lbcrypt,)', 'CXXFLAGS ?= -O0 -std=c++17 -fno-strict-aliasing -w -I../../runtime -I.',
          'RTFLAGS ?= -O2 -std=c++17 -fno-strict-aliasing -w -I../../runtime -I.',
          'BASE = ' + ' '.join(u[:-4] + '.o' for u in units) + ' hooks.o vfpu.o threads.o ge.o gpu.o window.o udp.o online.o $(JUICE)',
          'JUICE = $(patsubst ../../runtime/net/juice/%.c,j_%.o,$(wildcard ../../runtime/net/juice/*.c))',
          'OBJS = $(BASE) main.o',
          '', 'all: ttt tttrecomp', '', 'ttt: $(OBJS)', '\t$(CXX) $(OBJS) -o ttt $(LIBS)',
          'HEADERS = ../../runtime/psp.h ../../runtime/perf.h ../../runtime/gpu/gl.h ../../runtime/gpu/gpu_gl.h ../../runtime/audio/sound.h ../../runtime/audio/atrac3.h ../../runtime/save/savedata.h ../../runtime/save/crypto.h ../../runtime/input/input.h ../../runtime/input/rumble.h ../../runtime/input/icons.h ../../runtime/ui/menu.h ../../runtime/disc/disc.h ../../runtime/disc/bt3.h ../../runtime/disc/roster_def.h ../../runtime/disc/ags.h ../../runtime/gpu/image.h ../../runtime/disc/mods.h ../../runtime/ui/mods_ui.h ../../runtime/ui/theme.h ../../runtime/ui/mod_creator.h ../../runtime/ui/character_editor.h ../../runtime/ui/graphics.h ../../runtime/ui/tpal_editor.h ../../runtime/net/net.h ../../runtime/net/online.h $(wildcard ../../runtime/patches/*.h)',
          '', 'hooks.o vfpu.o threads.o ge.o gpu.o window.o main.o boot.o: $(HEADERS)',
          '', 'hooks.o: $(wildcard ../../runtime/core/hooks/*.inc)',
          '', 'boot.o: $(wildcard ../../runtime/core/boot/*.inc)',
          '', 'hooks.o: ../../runtime/core/hooks.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'window.o: ../../runtime/ui/window.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'gpu.o: ../../runtime/gpu/gpu.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'ge.o: ../../runtime/gpu/ge.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'threads.o: ../../runtime/core/threads.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'udp.o: ../../runtime/net/udp.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'online.o: ../../runtime/net/online.cpp $(wildcard ../../runtime/net/juice/*.h)', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'j_%.o: ../../runtime/net/juice/%.c $(wildcard ../../runtime/net/juice/*.h)', '\t$(CXX) -x c -O2 -w -DJUICE_STATIC -DNO_SERVER -I../../runtime/net/juice -c $< -o $@',
          '', 'vfpu.o: ../../runtime/cpu/vfpu.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'main.o: ../../runtime/core/main.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'boot.o: ../../runtime/core/boot.cpp', '\t$(CXX) $(RTFLAGS) -c $< -o $@',
          '', 'tttrecomp: $(BASE) boot.o', '\t$(CXX) $(BASE) boot.o -o tttrecomp $(LIBS)',
          '', '%.o: %.cpp', '\t$(CXX) $(CXXFLAGS) -c $< -o $@',
          '', 'clean:', '\trm -f *.o ttt', '']
    write(os.path.join(out, 'Makefile'), '\n'.join(mk))
    log('%d translation units in %s' % (len(units), out))
    return summary


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description='Emits the C++ of the whole program')
    ap.add_argument('--eboot', default=os.path.join(root, 'game', 'EBOOT.OLD'))
    ap.add_argument('--overlays', default=os.path.join(root, 'game', 'overlays'))
    ap.add_argument('--mapa', '--map', dest='mapa', default=os.path.join(root, 'generated', 'programa.json'))
    ap.add_argument('--game', default=GAME)
    ap.add_argument('-o', '--out', default=os.path.join(root, 'generated', 'src'))
    a = ap.parse_args()
    generate(a.eboot, a.overlays, a.mapa, a.out, a.game)


if __name__ == '__main__':
    main()
