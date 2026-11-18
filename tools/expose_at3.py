#!/usr/bin/env python3
# Convierte las entradas ATRAC3 de assets/Packfile en wav sueltos y borra el .unk original.
# Por defecto usa at3_a_wav, que lleva el mismo decodificador del runtime y se compila solo con g++.
# La musica sale estereo a 44100 y las voces mono a 22050, que es su frecuencia real: el juego lee
# la salida de una voz como una corriente mono, y eso es lo que la lleva a 44100 al reproducirla.
#   --decodificador <ruta>   usa vgmstream-cli o ffmpeg en vez de at3_a_wav
#   --rehacer                vuelve a convertir aunque ya exista el wav
#   --solo <patron>          limita a las rutas que contengan ese texto
# Las entradas que no llegan a una trama son huecos sin sonido: se borran. Para habilitar una,
# basta dejar un wav con ese mismo nombre y el runtime la sirve como corriente normal.
import argparse, os, shutil, struct, subprocess, tempfile

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
AQUI = os.path.dirname(os.path.abspath(__file__))
TRAMA_MONO, TRAMA_EST = 152, 304
RATE_VOZ, RATE_NATIVO = 22050, 44100


# toda trama ATRAC3 empieza con los mismos 6 bits. Tras la ultima trama buena hay relleno
# (ceros o una trama cortada) que no forma parte del sonido: se cuentan solo las del principio
def tramas_buenas(d):
    n = len(d) // TRAMA_MONO
    k = 0
    while k < n and d[k * TRAMA_MONO] >> 2 == 0x28:
        k += 1
    return k


def util_at3(d, trama):
    return tramas_buenas(d) * TRAMA_MONO // trama * trama


def es_hueco(d):
    return not d or (len(d) < TRAMA_MONO and d[0] >> 2 == 0x28)


# primera trama de 152 que no empieza como ATRAC3, para saber si el fallo es de cola o de contenido
def donde_falla(d):
    n = len(d) // TRAMA_MONO
    for k in range(n):
        if d[k * TRAMA_MONO] >> 2 != 0x28:
            return 'trama %d de %d mala en 0x%X: %s' % (k, n, k * TRAMA_MONO, d[k * TRAMA_MONO:k * TRAMA_MONO + 8].hex())
    return 'las %d tramas buenas, sobran %d bytes: %s' % (n, len(d) - n * TRAMA_MONO, d[n * TRAMA_MONO:n * TRAMA_MONO + 8].hex())


def wav_valido(ruta):
    try:
        with open(ruta, 'rb') as f:
            c = f.read(12)
        return len(c) == 12 and c[:4] == b'RIFF' and c[8:] == b'WAVE' and os.path.getsize(ruta) > 64
    except OSError:
        return False


def es_musica(rel):
    return rel.replace('\\', '/').startswith('Music/')


def envolver(crudo, canales, trama, rate):
    n = len(crudo) // trama * trama
    crudo = crudo[:n]
    extra = struct.pack('<HIHHHH', 1, 2048, 0, 0, 1, 0)
    fmt = struct.pack('<HHIIHHH', 0x0270, canales, rate, trama * rate // 1024, trama, 0, len(extra)) + extra
    cuerpo = (b'WAVE' + b'fmt ' + struct.pack('<I', len(fmt)) + fmt
              + b'fact' + struct.pack('<I', 4) + struct.pack('<I', n // trama * 1024)
              + b'data' + struct.pack('<I', len(crudo)) + crudo)
    return b'RIFF' + struct.pack('<I', len(cuerpo)) + cuerpo


# el decodificador propio vive junto a este script y se compila la primera vez.
# g++ en Windows agrega .exe cuando el nombre de salida no lleva extension
def exe_propio():
    for n in ('at3_to_wav', 'at3_to_wav.exe'):
        p = os.path.join(AQUI, n)
        if os.path.isfile(p):
            return p
    return None


def compilar_propio():
    fuente = os.path.join(AQUI, 'at3_to_wav.cpp')
    if not os.path.isfile(fuente):
        raise SystemExit('falta %s; ponlo junto a este script' % fuente)
    exe = exe_propio()
    if exe and os.path.getmtime(exe) >= os.path.getmtime(fuente):
        return exe
    print('compilando at3_to_wav')
    r = subprocess.run(['g++', '-O2', '-std=c++17', '-w', '-I../runtime', '-o', 'at3_to_wav', 'at3_to_wav.cpp'],
                       cwd=AQUI, capture_output=True)
    exe = exe_propio()
    if not exe:
        raise SystemExit('no pude compilar at3_to_wav:\n' + (r.stdout + r.stderr).decode('utf-8', 'replace')[:400])
    return exe


def buscar_decodificador(ruta):
    if not ruta:
        return compilar_propio()
    p = ruta if os.path.isfile(ruta) else shutil.which(ruta)
    if not p and os.path.isfile(ruta + '.exe'):
        p = ruta + '.exe'
    if not p:
        raise SystemExit('no existe el decodificador %s' % ruta)
    return p


def decodificar(dec, ruta, d, musica, salida):
    nombre = os.path.basename(dec).lower()
    tmp = None
    try:
        if nombre.startswith('at3_to_wav'):
            orden = [dec, ruta, salida, 'musica' if musica else 'voz']
        else:
            canales, trama, rate = (2, TRAMA_EST, RATE_NATIVO) if musica else (1, TRAMA_MONO, RATE_VOZ)
            tmp = os.path.join(tempfile.gettempdir(), 'ttt_at3_%d.at3' % os.getpid())
            with open(tmp, 'wb') as f:
                f.write(envolver(d, canales, trama, rate))
            if 'ffmpeg' in nombre:
                orden = [dec, '-y', '-v', 'error', '-i', tmp, '-c:a', 'pcm_s16le', salida]
            else:
                orden = [dec, '-o', salida, tmp]
        try:
            r = subprocess.run(orden, capture_output=True)
        except OSError as e:
            return str(e)[:160]
    finally:
        if tmp and os.path.exists(tmp):
            os.remove(tmp)
    if not os.path.isfile(salida) or os.path.getsize(salida) < 64:
        return (r.stdout + r.stderr).decode('utf-8', 'replace').strip()[:160] or 'sin salida'
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--decodificador', default=None)
    ap.add_argument('--rehacer', action='store_true')
    ap.add_argument('--solo', default=None)
    a = ap.parse_args()

    dec = buscar_decodificador(a.decodificador)
    print('decodificador: %s' % dec)
    raiz = os.path.join(a.assets, 'Packfile')
    if not os.path.isdir(raiz):
        raise SystemExit('no existe %s' % raiz)

    hechos = fallos = saltados = descartados = huecos = recortados = 0
    for carpeta, _, archivos in os.walk(raiz):
        for nombre in sorted(archivos):
            if not nombre.lower().endswith('.unk'):
                continue
            ruta = os.path.join(carpeta, nombre)
            rel = os.path.relpath(ruta, raiz).replace('\\', '/')
            if a.solo and a.solo not in rel:
                continue
            wav = os.path.splitext(ruta)[0] + '.wav'
            # si ya hay un wav bueno, el crudo sobra: se borra igual
            if os.path.isfile(wav) and not a.rehacer:
                if wav_valido(wav):
                    os.remove(ruta)
                    saltados += 1
                else:
                    print('aviso: %s tiene un wav ilegible, se rehace' % rel)
                    os.remove(wav)
            if os.path.isfile(wav) and not a.rehacer:
                continue
            with open(ruta, 'rb') as f:
                d = f.read()
            musica = es_musica(rel)
            trama = TRAMA_EST if musica else TRAMA_MONO
            util = util_at3(d, trama)
            # un hueco no tiene nada que exponer: se borra y su ausencia pasa a ser el hueco
            if not util:
                if es_hueco(d):
                    os.remove(ruta)
                    huecos += 1
                elif musica or d[0] >> 2 == 0x28:
                    print('descartado: %s %d bytes, %s' % (rel, len(d), donde_falla(d)))
                    descartados += 1
                continue
            fuente = ruta
            # una cola de menos de una trama ya la ignora el decodificador; solo se recorta el relleno real
            if len(d) - util >= TRAMA_MONO:
                recortados += 1
                print('recortado: %s %d tramas de %d, sobran %d de %d bytes' % (rel, util // trama, trama, len(d) - util, len(d)))
                fuente = os.path.join(tempfile.gettempdir(), 'ttt_at3_%d.unk' % os.getpid())
                with open(fuente, 'wb') as f:
                    f.write(d[:util])
            err = decodificar(dec, fuente, d if fuente == ruta else d[:util], musica, wav)
            if fuente != ruta:
                os.remove(fuente)
            if err:
                print('fallo: %s: %s' % (rel, err))
                fallos += 1
                if fallos >= 5 and not hechos:
                    raise SystemExit('el decodificador no produce wav; revisa la ruta')
                continue
            os.remove(ruta)
            hechos += 1
    print('at3: %d convertidos (%d recortados), %d ya estaban, %d huecos vacios, %d descartados, %d fallos'
          % (hechos, recortados, saltados, huecos, descartados, fallos))


if __name__ == '__main__':
    main()
