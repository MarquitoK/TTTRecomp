#!/usr/bin/env python3
# Diagnostico para el asistente: que trae el ISO (SYSDIR, USRDIR, PARAM.SFO) y de donde salen game/EBOOT.OLD y game/overlays. No cambia nada.
import hashlib, os, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import extract_assets as ea

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
SECTOR = 2048


def tipo(b):
    if b[:4] == b'\x7fELF':
        return 'ELF'
    if b[:4] == b'~PSP':
        return 'PSP cifrado'
    if b[:4] == b'\x00PSF':
        return 'PSF'
    if b[:4] == b'PacK':
        return 'PacK'
    return b[:8].hex()


def sha(b):
    return hashlib.sha1(b).hexdigest()[:16]


def sfo(d):
    if d[:4] != b'\x00PSF':
        return {}
    kt, dt, n = struct.unpack_from('<III', d, 8)
    out = {}
    for i in range(n):
        ko, fmt, ln, mx, do = struct.unpack_from('<HHIII', d, 20 + i * 16)
        k = d[kt + ko:d.index(b'\0', kt + ko)].decode('ascii', 'replace')
        v = d[dt + do:dt + do + ln]
        out[k] = v.rstrip(b'\0').decode('utf-8', 'replace') if fmt != 0x404 else struct.unpack('<I', v[:4])[0]
    return out


def main():
    iso = sys.argv[1] if len(sys.argv) > 1 else os.path.join(BASE, 'game', 'game.iso')
    print('iso: %s (%d bytes)' % (iso, os.path.getsize(iso)))
    f = open(iso, 'rb')
    arch = ea.iso_listar(f)
    leidos = {}
    for k in sorted(arch):
        u = k.upper()
        if not (u.startswith('PSP_GAME/SYSDIR') or u.startswith('PSP_GAME/USRDIR') or u.startswith('PSP_GAME/PARAM')):
            continue
        lba, tam = arch[k]
        f.seek(lba * SECTOR)
        cab = f.read(min(tam, 64))
        extra = ''
        if tam <= 8 << 20:
            f.seek(lba * SECTOR)
            d = f.read(tam)
            leidos[u] = d
            extra = ' sha1 %s' % sha(d)
        print('  %-40s %10d  %s%s' % (k, tam, tipo(cab), extra))
    p = leidos.get('PSP_GAME/PARAM.SFO')
    if p:
        s = sfo(p)
        print('PARAM.SFO: DISC_ID %s, DISC_VERSION %s, TITLE %s' % (s.get('DISC_ID'), s.get('DISC_VERSION'), s.get('TITLE')))
    print('archivos del port en game/:')
    g = os.path.join(BASE, 'game')
    for fn in sorted(os.listdir(g)):
        if fn.upper().startswith('EBOOT'):
            d = open(os.path.join(g, fn), 'rb').read()
            igual = [k for k, v in leidos.items() if v == d]
            print('  %-24s %10d  %s sha1 %s%s' % (fn, len(d), tipo(d), sha(d), '  = ' + ', '.join(igual) if igual else ''))
    clave = next((k for k in arch if k.upper().endswith('USRDIR/PACKFILE.BIN')), None)
    if not clave:
        print('sin PACKFILE.BIN')
        return
    pk_lba, pk_tam = arch[clave]
    sd_e, n_e = ea.leer_eboot(os.path.join(g, 'EBOOT.OLD'))
    n, sd, ent, _ = ea.leer_indice(f, pk_lba, pk_tam, sd_e)
    print('PACKFILE: %d entradas, datos en el sector %d (EBOOT: %s)' % (n, sd, sd_e))
    od = os.path.join(g, 'overlays')
    for fn in sorted(os.listdir(od)) if os.path.isdir(od) else []:
        if not fn.lower().endswith('.elf'):
            continue
        try:
            i = int(fn.split('_')[0])
        except ValueError:
            print('  %s: nombre sin numero de entrada' % fn)
            continue
        mine = open(os.path.join(od, fn), 'rb').read()
        s, t, alt, _ = ent[i]
        f.seek((pk_lba + sd + s) * SECTOR)
        raw = f.read(t)
        how = 'igual a la entrada' if raw == mine else ''
        if not how:
            for off in (0, 4, 8, 16):
                try:
                    if zlib.decompress(raw[off:]) == mine:
                        how = 'zlib desde +%d' % off
                        break
                except Exception:
                    pass
        if not how:
            k = raw.find(mine[:64])
            how = 'contenido dentro de la entrada en +%d' % k if k >= 0 else 'distinta (cab %s, alt %s)' % (tipo(raw[:8]), alt)
        print('  %-18s entrada %5d: %8d bytes, %s' % (fn, i, t, how))


main()
