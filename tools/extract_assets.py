#!/usr/bin/env python3
# Extrae el ISO de DBZ Tenkaichi Tag Team a assets/ con rutas descriptivas y un manifiesto.
#   --solo-manifiesto   escribe el manifiesto y el informe de nombres sin extraer datos
#   --verificar         reconstruye el PACKFILE desde assets/ y lo compara con el del ISO
import argparse, os, re, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as op

SECTOR = 2048
CABECERA = b''
CLAVE_SECTOR, CLAVE_ALT = 0x4B636150, 0xAAAAAAAA
BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
VOZ_INI, VOZ_LINEAS, VOZ_GRUPOS = 4442, 126, 70
MODELO_INI, MODELO_PASO = 1428, 5
NUEVO_MODELO_INI, NUEVO_VOZ_INI, VANILLA = 14098, 14553, 14097
# secciones donde la posicion importa para el modding: prefijo con el orden del juego, en base 0
NUMERADAS = {'Extra_Characters', 'Maps', 'Maps/SFX', 'Story_Mode/Characters', 'Story_Mode/HUDs',
             'Story_Mode/Objects', 'Story_Mode/Renders', 'Story_Mode/Title_Backgrounds'}


# ISO 9660
def iso_listar(f):
    f.seek(16 * SECTOR)
    pvd = f.read(SECTOR)
    if pvd[1:6] != b'CD001':
        raise SystemExit('el archivo no es un ISO 9660')
    raiz = pvd[156:190]
    salida = {}

    def recorrer(lba, tam, ruta):
        f.seek(lba * SECTOR)
        datos = f.read(tam)
        p = 0
        while p < len(datos):
            n = datos[p]
            if n == 0:
                p = (p // SECTOR + 1) * SECTOR
                continue
            r = datos[p:p + n]
            ext, sz, banderas, ln = struct.unpack_from('<I', r, 2)[0], struct.unpack_from('<I', r, 10)[0], r[25], r[32]
            nombre = r[33:33 + ln]
            p += n
            if nombre in (b'\x00', b'\x01'):
                continue
            nombre = nombre.decode('ascii', 'replace').split(';')[0]
            completo = ruta + '/' + nombre
            if banderas & 2:
                recorrer(ext, sz, completo)
            else:
                salida[completo.lstrip('/')] = (ext, sz)

    recorrer(struct.unpack_from('<I', raiz, 2)[0], struct.unpack_from('<I', raiz, 10)[0], '')
    return salida


# listas de nombres
def leer_lista(ruta):
    nombres, seccion = {}, ''
    with open(ruta, encoding='utf-8', errors='replace') as f:
        for linea in f:
            linea = linea.strip()
            if not linea:
                continue
            m = re.match(r'(\d+)_[0-9A-Fa-f]+\.unk:\s*(.+)$', linea)
            if m:
                nombres[int(m.group(1))] = (seccion, m.group(2).strip())
            elif linea.endswith(':'):
                seccion = linea[:-1].replace('\\', '/').strip('/')
    return nombres


def leer_voces(ruta):
    lineas = {}
    with open(ruta, encoding='utf-8', errors='replace') as f:
        for linea in f:
            m = re.match(r'(\d+)-[0-9A-Fa-f]+\.unk:\s*(.+)$', linea.strip())
            if m:
                lineas[int(m.group(1)) - 1] = m.group(2).strip()
    return lineas


def leer_personajes(ruta):
    grupos = {}
    if ruta and os.path.exists(ruta):
        with open(ruta, encoding='utf-8', errors='replace') as f:
            for linea in f:
                m = re.match(r'(\d+)\s*:\s*(.+)$', linea.strip())
                if m:
                    grupos[int(m.group(1))] = m.group(2).strip().replace('\\', '/').strip('/')
    return grupos


def limpiar_nombre(i, nombre):
    if re.fullmatch(r'%d_%X(\.[^.]+)?' % (i, i), nombre, re.I):
        ext = os.path.splitext(nombre)[1]
        return 'sin_nombre_%d%s' % (i, ext)
    return re.sub(r'^%d_' % i, '', nombre)


# rutas de cada entrada (indice en base 1)
def construir_rutas(n, lista, voces, personajes, avisos):
    rutas, usadas = {}, set()
    posicion = {}
    for sec in NUMERADAS:
        ids = sorted(i for i, (c, _) in lista.items() if c == sec)
        ancho = max(2, len(str(len(ids) - 1)))
        for p, i in enumerate(ids):
            posicion[i] = '%0*d_' % (ancho, p)

    def carpeta_grupo(k):
        if k in personajes:
            return personajes[k]
        if k < VOZ_GRUPOS and (MODELO_INI + k * MODELO_PASO) in lista:
            return lista[MODELO_INI + k * MODELO_PASO][0]
        return 'Characters/Nuevo_%03d' % k

    for i in range(1, n + 1):
        if MODELO_INI <= i < MODELO_INI + VOZ_GRUPOS * MODELO_PASO:
            k, j = divmod(i - MODELO_INI, MODELO_PASO)
            ruta = carpeta_grupo(k) + '/' + ('sfx.pak' if j == 4 else 'color_%d.pak' % (j + 1))
        elif i in lista:
            carpeta, nombre = lista[i]
            ruta = (carpeta + '/' if carpeta else '') + posicion.get(i, '') + limpiar_nombre(i, nombre)
        elif VOZ_INI <= i < VOZ_INI + VOZ_GRUPOS * VOZ_LINEAS:
            k, j = divmod(i - VOZ_INI, VOZ_LINEAS)
            ruta = carpeta_grupo(k) + '/Voices/' + voces.get(j, '%03d_linea.unk' % (j + 1))
        elif i >= NUEVO_MODELO_INI and i < NUEVO_VOZ_INI:
            k, j = divmod(i - NUEVO_MODELO_INI, MODELO_PASO)
            k += VOZ_GRUPOS
            ruta = carpeta_grupo(k) + '/' + ('sfx.pak' if j == 4 else 'color_%d.pak' % (j + 1))
        elif i >= NUEVO_VOZ_INI:
            k, j = divmod(i - NUEVO_VOZ_INI, VOZ_LINEAS)
            k += VOZ_GRUPOS
            ruta = carpeta_grupo(k) + '/Voices/' + voces.get(j, '%03d_linea.unk' % (j + 1))
        else:
            ruta = 'Sin_Nombre/%d.bin' % i
            avisos.append('sin nombre en la lista: entrada %d' % i)
        ruta = re.sub(r'[<>:"|?*]', '_', ruta)
        if ruta.lower() in usadas:
            base, ext = os.path.splitext(ruta)
            nueva = '%s_%d%s' % (base, i, ext)
            avisos.append('ruta repetida: entrada %d %s -> %s' % (i, ruta, nueva))
            ruta = nueva
        usadas.add(ruta.lower())
        rutas[i] = ruta
    return rutas


# PACKFILE
def leer_indice(f, pk_lba, pk_tam, forzar):
    f.seek(pk_lba * SECTOR)
    cab = f.read(16)
    global CABECERA
    CABECERA = cab
    if cab[:4] != b'PacK':
        raise SystemExit('PACKFILE sin cabecera PacK')
    n = struct.unpack_from('<I', cab, 4)[0]
    f.seek(pk_lba * SECTOR)
    indice = f.read((n + 1) * 16)
    entradas = {}
    for i in range(1, n + 1):
        a, b, c, d = struct.unpack_from('<4I', indice, i * 16)
        alt = c ^ CLAVE_ALT
        entradas[i] = (a ^ CLAVE_SECTOR, b ^ (i - 1), None if c == (CLAVE_SECTOR ^ CLAVE_ALT) else alt, d)
    if forzar is not None:
        return n, forzar, entradas, indice
    minimo = -(-((n + 1) * 16) // SECTOR)
    s0 = min(entradas.values(), key=lambda e: (e[0], -e[1]))
    for cand in (minimo, minimo + 1, minimo + 2):
        fin = max((s + cand) * SECTOR + t for s, t, _, _ in entradas.values())
        f.seek((pk_lba + cand + s0[0]) * SECTOR)
        cabeza = f.read(64)
        print('  candidato %d: fin de datos %d frente a %d del disco, primeros bytes %s' % (cand, fin, pk_tam, cabeza[:8].hex()))
        if any(cabeza):
            return n, cand, entradas, indice
    raise SystemExit('no pude deducir el sector de datos; usa --sector-datos')


# el EBOOT que usa el runtime lleva el sector de datos y el conteo del cargador (0x0882FBF8 y 0x0882FBFC)
def leer_eboot(ruta):
    if not ruta or not os.path.exists(ruta):
        return None, None
    d = open(ruta, 'rb').read()
    if d[:4] != b'\x7fELF':
        return None, None
    w1, w2 = struct.unpack_from('<2I', d, 0x0882FBF8 - 0x08804040 + 0x1018)
    if (w1 >> 16) != 0x3417 or (w2 >> 16) != 0x341E:
        return None, None
    return w1 & 0xFFFF, w2 & 0xFFFF


# compara lo que hay en assets (archivo suelto o carpeta de pak) con el PACKFILE del ISO
def comparar(f, pk_lba, sd, n, entradas, rutas, dir_pk):
    import importlib.util
    spec = importlib.util.spec_from_file_location('open_paks', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'open_paks.py'))
    ap = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(ap)
    iguales, distintos, faltan = 0, [], 0
    for i in range(1, n + 1):
        s, t, _, _ = entradas[i]
        f.seek((pk_lba + sd + s) * SECTOR)
        orig = f.read(t)
        if len(orig) < t:
            orig += b'\x00' * (t - len(orig))
        ruta = os.path.join(dir_pk, rutas[i])
        carpeta = os.path.splitext(ruta)[0]
        if os.path.isfile(ruta):
            with open(ruta, 'rb') as o:
                nuevo = o.read()
        elif os.path.isdir(ruta):
            nuevo = ap.armar(ruta)
        elif os.path.isdir(carpeta):
            nuevo = ap.empaquetar(carpeta)
        else:
            faltan += 1
            continue
        if nuevo == orig or (ruta.endswith('.txt1') and ap.mismo_txt1(orig, nuevo)):
            iguales += 1
            continue
        ra, rb = ap.analizar(orig), ap.analizar(nuevo)
        detalle = ''
        if ra and rb and ra['n'] == rb['n']:
            malas = [k + 1 for k in range(ra['n']) if ap.sin_ceros_finales(orig[ra['limites'][k]:ra['limites'][k + 1]]) != ap.sin_ceros_finales(nuevo[rb['limites'][k]:rb['limites'][k + 1]])]
            if not malas:
                iguales += 1
                continue
            detalle = 'entradas distintas %d (primeras %s)' % (len(malas), malas[:5]) if malas else 'solo cambian los desplazamientos'
        distintos.append('%s: %d bytes frente a %d. %s' % (rutas[i], len(nuevo), len(orig), detalle))
    print('comparacion con el ISO: %d iguales, %d distintos, %d sin archivo' % (iguales, len(distintos), faltan))
    for x in distintos[:40]:
        print('  %s' % x)
    return len(distintos)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--iso', default=os.path.join(BASE, 'game', 'game.iso'))
    ap.add_argument('--lista', default=os.path.join(BASE, 'tools', 'lists', 'LISTA_PACKFILE.txt'))
    ap.add_argument('--voces', default=os.path.join(BASE, 'tools', 'lists', 'LISTA_VO_TTT.txt'))
    ap.add_argument('--personajes', default=os.path.join(BASE, 'tools', 'lists', 'LISTA_PERSONAJES.txt'))
    ap.add_argument('--salida', default=os.path.join(BASE, 'assets'))
    ap.add_argument('--eboot', default=os.path.join(BASE, 'game', 'EBOOT.OLD'))
    ap.add_argument('--sector-datos', type=lambda v: int(v, 0))
    ap.add_argument('--solo-manifiesto', action='store_true')
    ap.add_argument('--verificar', action='store_true')
    ap.add_argument('--comparar', action='store_true')
    ap.add_argument('--reparar-tamanos', action='store_true')
    a = ap.parse_args()

    lista, voces, personajes = leer_lista(a.lista), leer_voces(a.voces), leer_personajes(a.personajes)
    print('lista: %d nombres, %d lineas de voz, %d grupos de personaje renombrados' % (len(lista), len(voces), len(personajes)))
    f = open(a.iso, 'rb')
    archivos = iso_listar(f)
    clave_pk = next((k for k in archivos if k.upper().endswith('USRDIR/PACKFILE.BIN')), None)
    if not clave_pk:
        raise SystemExit('no encuentro PSP_GAME/USRDIR/PACKFILE.BIN en el ISO')
    pk_lba, pk_tam = archivos[clave_pk]
    sd_eboot, n_eboot = leer_eboot(a.eboot)
    forzar = a.sector_datos if a.sector_datos is not None else sd_eboot
    if sd_eboot is not None:
        print('EBOOT: sector de datos %d, %d entradas' % (sd_eboot, n_eboot))
    n, sd, entradas, indice = leer_indice(f, pk_lba, pk_tam, forzar)
    print('PACKFILE: sector %d, %d bytes, %d entradas, datos en el sector %d' % (pk_lba, pk_tam, n, sd))
    if a.reparar_tamanos:
        return reparar_tamanos(f, pk_lba, sd, entradas, a.salida)
    if n_eboot is not None and n != n_eboot:
        print('aviso: el indice tiene %d entradas y el EBOOT espera %d' % (n, n_eboot))
    fuera = sum(1 for s, t, _, _ in entradas.values() if (sd + s) * SECTOR + t > pk_tam)
    if fuera:
        print('aviso: %d entradas terminan despues del tamano que declara el disco; se leen igual por sectores' % fuera)

    avisos = []
    rutas = construir_rutas(n, lista, voces, personajes, avisos)
    dir_pk = os.path.join(a.salida, 'Packfile')
    if a.comparar:
        comparar(f, pk_lba, sd, n, entradas, rutas, dir_pk)
        return
    os.makedirs(a.salida, exist_ok=True)
    migrar(a.salida, dir_pk, rutas)

    with open(os.path.join(a.salida, 'manifiesto.txt'), 'w', encoding='utf-8', newline='\n') as m:
        m.write('# packfile entradas %d sector_datos %d tamano %d cabecera %s\n' % (n, sd, pk_tam, CABECERA.hex()))
        m.write('# clave base 0\n')
        m.write('# indice\tsector\ttamano\talterno\tcola\tcrc32\truta\n')
        total = 0
        for i in range(1, n + 1):
            s, t, alt, cola = entradas[i]
            crc = 0
            if not a.solo_manifiesto and not op.es_frances(rutas[i]):   # French is not extracted
                f.seek((pk_lba + sd + s) * SECTOR)
                datos = f.read(t)
                if len(datos) < t:
                    avisos.append('entrada %d: el ISO se acaba antes, faltan %d bytes rellenados con ceros' % (i, t - len(datos)))
                    datos += b'\x00' * (t - len(datos))
                crc = zlib.crc32(datos)
                destino = os.path.join(dir_pk, rutas[i])
                os.makedirs(os.path.dirname(destino), exist_ok=True)
                with open(destino, 'wb') as o:
                    o.write(datos)
                total += t
                if i % 1000 == 0:
                    print('  %d/%d entradas, %.1f MB' % (i, n, total / 1048576))
            m.write('%d\t%d\t%d\t%s\t%08X\t%08X\t%s\n' % (i, s, t, '-' if alt is None else str(alt), cola, crc, rutas[i]))

    otros = 0
    if not a.solo_manifiesto:
        for ruta, (lba, tam) in sorted(archivos.items()):
            if ruta == clave_pk:
                continue
            destino = os.path.join(a.salida, 'Disco', ruta)
            os.makedirs(os.path.dirname(destino), exist_ok=True)
            f.seek(lba * SECTOR)
            with open(destino, 'wb') as o:
                restante = tam
                while restante:
                    bloque = f.read(min(restante, 16 * 1048576))
                    o.write(bloque)
                    restante -= len(bloque)
            otros += 1
    with open(os.path.join(a.salida, 'avisos_nombres.txt'), 'w', encoding='utf-8', newline='\n') as w:
        w.write('\n'.join(avisos) + ('\n' if avisos else ''))
    print('extraidas %d entradas y %d archivos del disco; %d avisos de nombres en avisos_nombres.txt' % (0 if a.solo_manifiesto else n, otros, len(avisos)))

    if a.verificar and not a.solo_manifiesto:
        verificar(f, pk_lba, pk_tam, n, sd, entradas, indice, rutas, dir_pk)


# si el manifiesto anterior tenia otras rutas, mueve lo ya extraido (archivo o carpeta de pak abierto)
# assets extraidos con el tamano decodificado con el indice base 1: completa o recorta cada archivo suelto intacto
def reparar_tamanos(f, pk_lba, sd, entradas, salida):
    man = os.path.join(salida, 'manifiesto.txt')
    lineas = open(man, encoding='utf-8').read().split('\n')
    if any(l.startswith('# clave base 0') for l in lineas):
        print('el manifiesto ya es de una extraccion corregida, no hay nada que reparar')
        return
    completados = recortados = cambiados = 0
    for l in lineas:
        c = l.split('\t')
        if l.startswith('#') or len(c) < 7:
            continue
        i, t_viejo = int(c[0]), int(c[2])
        s, t = entradas[i][0], entradas[i][1]
        ruta = os.path.join(salida, 'Packfile', c[6])
        if t == t_viejo or not os.path.isfile(ruta):
            continue
        tam = os.path.getsize(ruta)
        if tam != t_viejo:
            cambiados += 1
            continue
        if t > tam:
            f.seek((pk_lba + sd + s) * SECTOR + tam)
            with open(ruta, 'ab') as o:
                o.write(f.read(t - tam))
            completados += 1
        else:
            with open(ruta, 'r+b') as o:
                o.truncate(t)
            recortados += 1
    print('reparados: %d archivos completados, %d recortados; %d con otro tamano (editados) sin tocar' % (completados, recortados, cambiados))

def migrar(salida, dir_pk, rutas):
    viejo = os.path.join(salida, 'manifiesto.txt')
    if not os.path.exists(viejo):
        return
    movidos = 0
    for linea in open(viejo, encoding='utf-8'):
        if linea.startswith('#'):
            continue
        campos = linea.rstrip('\r\n').split('\t')
        if len(campos) < 7:
            continue
        i, antes = int(campos[0]), campos[6]
        despues = rutas.get(i)
        if not despues or antes == despues:
            continue
        for a_, d_ in ((antes, despues), (os.path.splitext(antes)[0], os.path.splitext(despues)[0])):
            src, dst = os.path.join(dir_pk, a_), os.path.join(dir_pk, d_)
            if os.path.exists(src) and not os.path.exists(dst):
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                os.replace(src, dst)
                movidos += 1
                padre = os.path.dirname(src)
                while padre != dir_pk and os.path.isdir(padre) and not os.listdir(padre):
                    os.rmdir(padre)
                    padre = os.path.dirname(padre)
    if movidos:
        print('migracion: %d archivos o carpetas movidos a sus rutas nuevas' % movidos)


# reconstruye el PACKFILE en memoria por tramos y lo compara byte a byte con el original
def verificar(f, pk_lba, pk_tam, n, sd, entradas, indice, rutas, dir_pk):
    idx = bytearray(indice[:16])
    for i in range(1, n + 1):
        s, t, alt, cola = entradas[i]
        idx += struct.pack('<4I', s ^ CLAVE_SECTOR, t ^ (i - 1), (CLAVE_SECTOR ^ CLAVE_ALT) if alt is None else alt ^ CLAVE_ALT, cola)
    f.seek(pk_lba * SECTOR)
    fallos_indice = sum(1 for x, y in zip(idx, f.read(len(idx))) if x != y)
    fallos_datos, relleno_no_cero = 0, 0
    ocupado = []
    for i in range(1, n + 1):
        s, t, _, _ = entradas[i]
        if op.es_frances(rutas[i]):
            ocupado.append((s * SECTOR, s * SECTOR + t))
            continue
        with open(os.path.join(dir_pk, rutas[i]), 'rb') as a:
            local = a.read()
        f.seek((pk_lba + sd + s) * SECTOR)
        if local != f.read(t):
            fallos_datos += 1
        ocupado.append((s * SECTOR, s * SECTOR + t))
    ocupado.sort()
    pos = 0
    for ini, fin in ocupado:
        if ini > pos:
            f.seek(pk_lba * SECTOR + sd * SECTOR + pos)
            relleno_no_cero += sum(1 for b in f.read(ini - pos) if b)
        pos = max(pos, fin)
    print('verificacion: indice %d bytes distintos, %d entradas distintas, %d bytes de relleno no nulos entre entradas' % (fallos_indice, fallos_datos, relleno_no_cero))
    print('verificacion: %s' % ('IDENTICO' if not (fallos_indice or fallos_datos or relleno_no_cero) else 'CON DIFERENCIAS'))


if __name__ == '__main__':
    main()
