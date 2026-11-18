#!/usr/bin/env python3
# Personajes, extras, objetos de historia y mapas sobre assets/ (nunca el ISO):
#   --listar                              tipo, clave, nombre, indice y partes de cada unidad del manifiesto
#   --exportar CLAVE PARTE DESTINO [--pak] copia una parte como carpeta editable o la arma como .pak
#   --importar CLAVE PARTE ORIGEN          reemplaza esa parte en assets con una carpeta o un .pak (destructivo)
#   --mod CARPETA                          deja un mod instalado como assets: paks abiertos, texturas en png, audio en wav
import argparse, os, re, shutil, struct, subprocess, sys, collections, time
T_INICIO = time.perf_counter()

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import open_paks as ap
import bt3_open

LISTAS = os.path.join(BASE, 'tools', 'lists')
VOCES = 126

_EXTRA_RAW = [
    (1390, "191 Gold Dragon",             []),
    (1391, "192 Ghost Gotenks",           []),
    (1392, "193 Ghost Bu",                []),
    (1393, "194 Rock 1",                  []),
    (1394, "195 Rock 2",                  []),
    (1395, "196 Trunk Pencil",            []),
    (1396, "197 Goku Reflection",         []),
    (1397, "198 Android 16 Hand",         []),
    (1398, "199 Big Hand",                []),
    (1399, "200 Chocolate",               []),
    (1400, "201 Inflated Cell",           [1401, 1402]),
    (1403, "204 Android 17",              []),
    (1404, "205 Android 18",              []),
    (1405, "206 Burter",                  []),
    (1406, "207 Jeice",                   [1424]),
    (1407, "208 Piccolo",                 [1408]),
    (1409, "210 Gohan",                   []),
    (1410, "211 Goku Base - Fusion",      [1411, 1412, 1427]),
    (1413, "214 Vegeta Base - Fusion",    [1414]),
    (1415, "216 Goten Base - Fusion",     [1416]),
    (1417, "218 Goten SS - Fusion",       [1418]),
    (1419, "220 Kid Trunks Base - Fusion",[1420]),
    (1421, "222 Kid Trunks SS - Fusion",  [1422]),
    (1423, "224 Vegeta Scouter",          []),
    (1425, "226 Kid Gohan",               []),
    (1426, "227 Krilin",                  []),
]

_STORY_RAW = [
    (1060, "110 Goku Base",         []),
    (1061, "111 Goku SS",           []),
    (1062, "112 Goku SS2",          []),
    (1063, "113 Goku SS3",          []),
    (1064, "114 Kid Gohan",         []),
    (1065, "115 Teen Gohan Base",   []),
    (1066, "116 Teen Gohan SS",     []),
    (1067, "117 Teen Gohan SS2",    []),
    (1068, "118 Gohan Base",        []),
    (1069, "119 Gohan SS",          []),
    (1070, "120 Gohan SS2",         []),
    (1071, "121 Ultimate Gohan",    []),
    (1072, "122 Piccolo",           []),
    (1073, "123 Krilin",            []),
    (1074, "124 Yamcha",            []),
    (1075, "125 Tenshinhan",        []),
    (1076, "126 Chaos",             []),
    (1077, "127 Vegeta Scouter",    []),
    (1078, "128 Vegeta Base",       [1079]),
    (1080, "130 Vegeta SS",         [1081]),
    (1082, "132 Vegeta SS2",        []),
    (1083, "133 Majin Vegeta",      []),
    (1084, "134 Kid Trunks Base",   []),
    (1085, "135 Kid Trunks SS",     []),
    (1086, "136 Trunks Sword Base", []),
    (1087, "137 Trunks Sword SS",   []),
    (1088, "138 Trunks Base",       []),
    (1089, "139 Trunks SS",         []),
    (1090, "140 Videl",             []),
    (1091, "141 Goten Base",        []),
    (1092, "142 Goten SS",          []),
    (1093, "143 Gotenks SS",        []),
    (1094, "144 Gotenks SS3",       []),
    (1095, "145 Super Gogeta",      []),
    (1096, "146 Super Vegito",      []),
    (1097, "147 Bardock",           []),
    (1098, "148 Raditz",            []),
    (1099, "149 Saibaiman",         [1100, 1101, 1102]),
    (1103, "153 Nappa",             []),
    (1104, "154 Cui",               []),
    (1105, "155 Zarbon",            []),
    (1106, "156 Zarbon Transformed",[]),
    (1107, "157 Dodoria",           []),
    (1108, "158 Captain Ginyu",     []),
    (1109, "159 Recome",            []),
    (1110, "160 Guldo",             []),
    (1111, "161 Recome",            []),
    (1112, "162 Guldo",             []),
    (1113, "163 Freezer F1",        []),
    (1114, "164 Freezer F2",        []),
    (1115, "165 Freezer F3",        []),
    (1116, "166 Freezer FF",        []),
    (1117, "167 Freezer Full Power",[]),
    (1118, "168 Android 16",        []),
    (1119, "169 Android 17",        []),
    (1120, "170 Android 18",        []),
    (1121, "171 Android 19",        []),
    (1122, "172 Dr Gero",           []),
    (1123, "173 Cell Jr.",          [1124, 1125, 1126]),
    (1127, "177 Cell F2",           []),
    (1128, "178 Cell F2",           []),
    (1129, "179 Cell Perfect Form", []),
    (1130, "180 Cell Perfect",      []),
    (1131, "181 Dabura",            []),
    (1132, "182 Majin Bu",          []),
    (1133, "183 Super Bu",          []),
    (1134, "184 Super Bu Gohan",    []),
    (1135, "185 Kid Bu",            []),
    (1136, "186 Broly SSL",         []),
    (1137, "187 Freezer Soldier",   [1138, 1139, 1140]),
]

_OBJECTS_RAW = [
    (1021, "71 Turtle"),   (1022, "72 Karin"),       (1023, "73 House 1"),
    (1024, "74 House 2"),  (1025, "75 House 3"),      (1026, "76 House 1 Ruin"),
    (1027, "77 House 4"),  (1028, "78 House 5"),      (1029, "79 House 5 Ruin"),
    (1030, "80 Guru"),     (1031, "81 House 6"),      (1032, "82 Dragon Ball 1"),
    (1033, "83 Dragon Ball 2"), (1034, "84 Dragon Ball 3"), (1035, "85 Dragon Ball 4"),
    (1036, "86 Dragon Ball 5"), (1037, "87 Dragon Ball 6"), (1038, "88 Dragon Ball 7"),
    (1039, "89 Capsule 1"), (1040, "90 Sphere Ship"), (1041, "91 Capsule 2"),
    (1042, "92 Capsule 3"), (1043, "93 Big Dragon Ball 1"), (1044, "94 Big Dragon Ball 2"),
    (1045, "95 Big Dragon Ball 3"), (1046, "96 House 7"), (1047, "97 House 8"),
    (1048, "98 House 9"),  (1049, "99 House 10"),     (1050, "100 Bulma"),
    (1051, "101 Empty 1"), (1052, "102 Dende"),       (1053, "103 Empty 2"),
    (1054, "104 Mr. Satan"),(1055, "105 Empty 3"),    (1056, "106 Shin"),
    (1057, "107 Empty 4"), (1058, "108 Nail"),        (1059, "109 Empty 5"),
]


def manifiesto(assets):
    filas = {}
    for linea in open(os.path.join(assets, 'manifiesto.txt'), encoding='utf-8'):
        if linea.startswith('#'):
            continue
        c = linea.rstrip('\r\n').split('\t')
        if len(c) >= 7:
            filas[int(c[0])] = c[6]
    return filas


def leer_lista(nombre):
    salida = []
    ruta = os.path.join(LISTAS, nombre)
    if not os.path.isfile(ruta):
        return salida
    for linea in open(ruta, encoding='utf-8-sig'):
        if ':' not in linea or linea.lstrip().startswith('#'):
            continue
        n, v = [x.strip() for x in linea.split(':', 1)]
        nums = [int(x) for x in re.findall(r'\d+', v)]
        if n and nums:
            salida.append((n, nums))
    return salida


# unidad: tipo, clave, nombre y partes {id: [indices del manifiesto]}
def unidades(assets):
    m = manifiesto(assets)
    salida = []
    def agregar(tipo, clave, nombre, partes):
        partes = [(k, [i for i in v if i in m]) for k, v in partes]
        partes = [(k, v) for k, v in partes if v]
        if partes:
            salida.append({'tipo': tipo, 'clave': clave, 'nombre': nombre, 'partes': partes, 'm': m})
    for n, nums in leer_lista('LISTA_CHARS_TTT.txt'):
        b = nums[0]
        partes = [('color_%d' % (k + 1), [b + k]) for k in range(4)] + [('sfx', [b + 4])]
        if len(nums) > 1 and nums[1]:
            partes.append(('voces', list(range(nums[1], nums[1] + (nums[2] if len(nums) > 2 else VOCES)))))
        agregar('personajes', 'p%d' % b, n, partes)
    for tipo, tabla in (('extra', _EXTRA_RAW), ('historia', _STORY_RAW)):
        for fn, n, extra in tabla:
            ids = [fn] + extra
            agregar(tipo, '%s%d' % (tipo[0], fn), n, [('pak', ids)] if len(ids) == 1 else [('color_%d' % (k + 1), [x]) for k, x in enumerate(ids)])
    for fn, n in _OBJECTS_RAW:
        agregar('objetos', 'o%d' % fn, n, [('pak', [fn])])
    for n, nums in leer_lista('LISTA_MAPS_TTT.txt'):
        agregar('mapas', 'm%d' % nums[0], n, [('mapa', [nums[0]]), ('sfx', [nums[1] if len(nums) > 1 else nums[0] + 1])])
    return salida


def buscar(assets, clave):
    for u in unidades(assets):
        if u['clave'] == clave:
            return u
    raise SystemExit('no existe %s' % clave)


def base_de(rel):
    return os.path.basename(rel)[:-4]


def en_disco(raiz, rel):
    pak, carpeta = os.path.join(raiz, rel), os.path.join(raiz, rel[:-4])
    if os.path.isfile(pak):
        return pak, False
    if os.path.isdir(carpeta):
        return carpeta, True
    return None, False


def borrar(ruta):
    if os.path.isdir(ruta):
        shutil.rmtree(ruta)
    elif os.path.isfile(ruta):
        os.remove(ruta)


# el pak importado se abre en su carpeta y desaparece; sin plantilla se abre con nombres genericos
def abrir_cerrado(raiz, rel, ctx):
    ruta = os.path.join(raiz, rel)
    with open(ruta, 'rb') as f:
        d = f.read()
    r = ap.analizar(d)
    if not r:
        raise SystemExit('el archivo no es un pak valido')
    nombre = ap.plantilla_de(rel, r['n'], ctx['reglas'], ctx['porcuenta'])
    plant = ctx['plantillas'].get(nombre, {}) if nombre and nombre not in ('-', '!') else {}
    carpeta = ap.abrir(ruta, d, r, plant) or ap.abrir(ruta, d, r, {})
    if not carpeta:
        raise SystemExit('no se pudo abrir el pak')
    os.remove(ruta)
    ap.abrir_anidados(carpeta, rel, ctx)
    ap.abrir_tex_pendientes(carpeta)


def partes(u):
    return [k for k, _ in u['partes']]


def rel_de(u, parte):
    for k, v in u['partes']:
        if k == parte and k != 'voces':
            return u['m'][v[0]]
    return None


def voces_de(u):
    for k, v in u['partes']:
        if k == 'voces':
            return [u['m'][i] for i in v]
    return []


def voz_real(raiz, rel):
    for ext in ('.wav', '.unk'):
        r = os.path.join(raiz, os.path.splitext(rel)[0] + ext)
        if os.path.isfile(r):
            return r
    return None


def contexto():
    plantillas = ap.leer_plantillas(os.path.join(BASE, 'tools', 'lists', 'paks'))
    porcuenta = collections.defaultdict(list)
    for p in plantillas:
        porcuenta[len(plantillas[p])].append(p)
    return {'reglas': ap.leer_reglas(os.path.join(BASE, 'tools', 'lists', 'REGLAS_PAK.txt')), 'porcuenta': porcuenta, 'plantillas': plantillas, 'sin': collections.Counter()}


def exportar_parte(assets, u, parte, destino, como_pak):
    raiz = os.path.join(assets, 'Packfile')
    if parte == 'voces':
        borrar(destino)
        os.makedirs(destino, exist_ok=True)
        n = 0
        for rel in voces_de(u):
            r = voz_real(raiz, rel)
            if r:
                shutil.copyfile(r, os.path.join(destino, os.path.basename(r)))
                n += 1
        print('exportado: %d voces' % n)
        return
    rel = rel_de(u, parte)
    if not rel:
        raise SystemExit('parte desconocida: %s' % parte)
    origen, abierto = en_disco(raiz, rel)
    if not origen:
        raise SystemExit('falta en assets: %s' % rel)
    if como_pak:
        os.makedirs(os.path.dirname(os.path.abspath(destino)), exist_ok=True)
        if abierto:
            with open(destino, 'wb') as o:
                o.write(ap.empaquetar(origen))
        else:
            shutil.copyfile(origen, destino)
    else:
        borrar(destino)
        if abierto:
            shutil.copytree(origen, destino)
        else:
            os.makedirs(destino, exist_ok=True)
            shutil.copyfile(origen, os.path.join(destino, os.path.basename(origen)))
    print('exportado: %s' % rel)


def importar_parte(assets, u, parte, origen):
    raiz = os.path.join(assets, 'Packfile')
    if not os.path.exists(origen):
        raise SystemExit('no existe %s' % origen)
    if parte == 'voces':
        if not os.path.isdir(origen):
            raise SystemExit('las voces se importan como carpeta')
        n = importar_voces(assets, raiz, u, {os.path.splitext(x)[0].lower(): os.path.join(origen, x) for x in os.listdir(origen) if x.lower().endswith(('.wav', '.unk'))})
        print('importado: %d voces' % n)
        return
    rel = rel_de(u, parte)
    if not rel:
        raise SystemExit('parte desconocida: %s' % parte)
    borrar(os.path.join(raiz, rel))
    borrar(os.path.join(raiz, rel[:-4]))
    if os.path.isdir(origen):
        shutil.copytree(origen, os.path.join(raiz, rel[:-4]))
    else:
        os.makedirs(os.path.dirname(os.path.join(raiz, rel)), exist_ok=True)
        shutil.copyfile(origen, os.path.join(raiz, rel))
        abrir_cerrado(raiz, rel, contexto())
    print('importado: %s' % rel)


# copia cada voz por su nombre de linea; lo que no llega se deja como estaba
def importar_voces(assets, raiz, u, fuentes):
    n, crudos = 0, False
    for rel in voces_de(u):
        base = os.path.splitext(os.path.basename(rel))[0].lower()
        f = fuentes.get(base)
        if not f:
            continue
        for ext in ('.wav', '.unk'):
            viejo = os.path.join(raiz, os.path.splitext(rel)[0] + ext)
            if os.path.isfile(viejo):
                os.remove(viejo)
        ext = os.path.splitext(f)[1].lower()
        destino = os.path.join(raiz, os.path.splitext(rel)[0] + ext)
        os.makedirs(os.path.dirname(destino), exist_ok=True)
        shutil.copyfile(f, destino)
        crudos |= ext == '.unk'
        n += 1
    if crudos and voces_de(u):
        subprocess.call([sys.executable, os.path.join(BASE, 'tools', 'expose_at3.py'), '--assets', assets, '--solo', os.path.dirname(voces_de(u)[0]) + '/'])
    return n


def archivos_de(raiz):
    salida = []
    for dirpath, _, nombres in os.walk(raiz):
        for x in nombres:
            salida.append(os.path.relpath(os.path.join(dirpath, x), raiz).replace('\\', '/'))
    return sorted(salida)


# clave con la que un archivo de mod tapa uno de assets (mod_clave de disco.h)
def clave_mod(nombre, es_dir, txt1):
    n = nombre.lower()
    m = re.match(r'\d+', n)
    if m:
        if txt1:
            if not es_dir and n.endswith('.txt') and re.match(r'\d+-[0-9a-f]+', n):
                return '#%d' % int(m.group())
        elif not es_dir or n.endswith(ap.CONTENEDORES):
            return '#%d' % int(m.group())
    if not es_dir:
        s = ap.png_suelto(nombre)
        if s:
            return s[0].lower()
    return n


def entradas(d):
    return {x: os.path.isdir(os.path.join(d, x)) for x in os.listdir(d) if x.lower() not in ap.IGNORAR and not x.lower().endswith('.vfx') and not x.lower().startswith('011_textures_dmg.')} if d and os.path.isdir(d) else {}


# archivos que ve el juego en una carpeta de assets con el mod encima (listar_dir de disco.h)
def vista(base, mod, rel=''):
    t1 = rel.lower().endswith('.txt1')
    eb, em = entradas(base), entradas(mod)
    tapan = {clave_mod(x, d, t1) for x, d in em.items()}
    salida = set()
    for x, d in eb.items():
        if clave_mod(x, d, t1) not in tapan:
            salida |= vista(os.path.join(base, x), None, rel + '/' + x) if d else {(rel + '/' + x).lower()}
    for x, d in em.items():
        salida |= vista(os.path.join(base, x) if base else None, os.path.join(mod, x), rel + '/' + x) if d else {(rel + '/' + x).lower()}
    return salida


# abierto solo si el juego ve exactamente lo abierto: nada de assets se cuela por numeros sin tapar
def exacto(base, abierto):
    return not os.path.isdir(abierto) or vista(base, abierto) == vista(None, abierto)


# pak del manifiesto dentro de un mod: se abre como en assets si assets lo tiene abierto y el resultado es exacto
def abrir_pak_mod(raiz, rel, ent, base, ctx):
    ruta = os.path.join(raiz, rel)
    carpeta_base = os.path.join(base, ent[:-4])
    if not os.path.isdir(carpeta_base):
        return 0, []
    with open(ruta, 'rb') as f:
        d = f.read()
    r = ap.analizar(d)
    if not r:
        return 0, [rel + ' no es un pak valido']
    nombre = ap.plantilla_de(ent, r['n'], ctx['reglas'], ctx['porcuenta'])
    plant = ctx['plantillas'].get(nombre, {}) if nombre and nombre not in ('-', '!') else {}
    carpeta = os.path.join(os.path.dirname(ruta), os.path.basename(ent)[:-4])
    tmp = carpeta + '.abriendo'
    borrar(tmp)
    if not (ap.abrir(ruta, d, r, plant, carpeta=tmp) or ap.abrir(ruta, d, r, {}, carpeta=tmp)):
        borrar(tmp)
        return 0, [rel + ' no se pudo abrir']
    _, problemas = ap.abrir_anidados(tmp, ent, ctx)
    ap.abrir_tex_pendientes(tmp)
    if not exacto(carpeta_base, tmp):
        borrar(tmp)
        return 0, [rel + ' queda como .pak: sus partes no coinciden con las de assets']
    if os.path.isdir(os.path.join(carpeta, '1_p_dmg')):   # damaged costume of this colour: kept next to the opened pak
        os.replace(os.path.join(carpeta, '1_p_dmg'), os.path.join(tmp, '1_p_dmg'))
    borrar(carpeta)
    os.replace(tmp, carpeta)
    os.remove(ruta)
    return 1, problemas


# contenedor suelto dentro de una carpeta del mod: se abre aparte y solo se deja si es exacto
def abrir_suelto_mod(raiz, frel, ent, base, ctx, tmp):
    ruta = os.path.join(raiz, frel)
    padre, nombre = os.path.split(ruta)
    padre_base = os.path.join(base, os.path.dirname(frel))
    borrar(tmp)
    os.makedirs(tmp)
    shutil.copyfile(ruta, os.path.join(tmp, nombre))
    prestados = []
    if nombre.lower().endswith('.vagp'):
        for origen in (padre, padre_base):
            ps = [x for x in entradas(origen) if x.lower().endswith('.pphd')]
            if ps:
                for x in ps:
                    shutil.copyfile(os.path.join(origen, x), os.path.join(tmp, x))
                    prestados.append(x)
                break
    ent_dir = ent[:-4]
    sub = os.path.dirname(frel)
    sub = ent + sub[len(ent_dir):] if sub.lower().startswith(ent_dir.lower()) else sub
    a, problemas = ap.abrir_anidados(tmp, sub, ctx)
    for x in prestados:
        borrar(os.path.join(tmp, x))
    hecho = sum(a.values()) and not os.path.isfile(os.path.join(tmp, nombre))
    if hecho and not exacto(os.path.join(padre_base, nombre), os.path.join(tmp, nombre)):
        problemas.append(frel + ' queda cerrado: sus partes no coinciden con las de assets')
        hecho = False
    if hecho:
        os.remove(ruta)
        for x in os.listdir(tmp):
            os.replace(os.path.join(tmp, x), os.path.join(padre, x))
    borrar(tmp)
    return (sum(a.values()) if hecho else 0), problemas


def pngs_en(raiz):
    return set(x for x in archivos_de(raiz) if x.lower().endswith('.png'))


def leer_tamanos(assets):
    t = {}
    ruta = os.path.join(assets, 'tamanos_texturas.txt')
    if os.path.isfile(ruta):
        for linea in open(ruta, encoding='utf-8'):
            c = linea.rstrip('\r\n').split(' ', 2)
            if len(c) == 3 and c[0].isdigit() and c[1].isdigit():
                t[c[2].lower()] = (int(c[0]), int(c[1]))
    return t


# una textura del pak con otro tamano que el original lleva @1x: el juego usa el tamano del pak, no el registrado
def fijar_tamanos(raiz, nuevos, tamanos):
    n = 0
    for rel in sorted(nuevos):
        carpeta, x = os.path.split(rel)
        if '@' in x:
            continue
        if carpeta.lower().endswith(ap.TEXTURAS):
            num, _ = ap.png_de_textura(x)
            if num is None:
                continue
            clave = carpeta + '#%d' % num
        else:
            s = ap.png_suelto(x)
            if not s:
                continue
            clave = (carpeta + '/' if carpeta else '') + s[0] + '#1'
        orig = tamanos.get(clave.lower())
        ruta = os.path.join(raiz, rel)
        with open(ruta, 'rb') as f:
            cab = f.read(24)
        if not orig or cab[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack('>II', cab[16:24]) == orig:
            continue
        os.replace(ruta, ruta[:-4] + '@1x.png')
        n += 1
    return n


# carpetas (relativas a Packfile) de los archivos que cambiaron: solo ahi hay trabajo
def zonas_de(raiz, lista):
    zonas = set()
    for linea in open(lista, encoding='utf-8'):
        r = linea.strip().replace('\\', '/')
        if r.lower().startswith('packfile/'):
            zonas.add(os.path.dirname(r[9:]))
    zonas = sorted(z for z in zonas if os.path.isdir(os.path.join(raiz, z)))
    return [z for i, z in enumerate(zonas) if not any(z == y or z.startswith(y + '/') for y in zonas[:i])]


def archivos_en(raiz, zonas):
    if zonas is None:
        return archivos_de(raiz)
    if '' in zonas:
        return archivos_de(raiz)
    salida = []
    for z in zonas:
        salida += [z + '/' + x for x in archivos_de(os.path.join(raiz, z))]
    return sorted(salida)


def normalizar_mod(assets, mod, solo=None):
    raiz = os.path.join(mod, 'Packfile')
    if not os.path.isdir(raiz):
        raise SystemExit('el mod no trae una carpeta Packfile')
    t = [time.perf_counter()]
    zonas = zonas_de(raiz, solo) if solo else None
    bt3 = sum(bt3_open.abrir_arbol(os.path.join(raiz, z)) for z in zonas) if zonas is not None else bt3_open.abrir_arbol(raiz)
    t.append(time.perf_counter())
    base = os.path.join(assets, 'Packfile')
    ents = {v.lower(): v for v in manifiesto(assets).values()}
    ctx = contexto()
    antes = set(x for x in archivos_en(raiz, zonas) if x.lower().endswith('.png'))
    t.append(time.perf_counter())
    paks = otros = 0
    avisos = []
    for rel in archivos_en(raiz, zonas):
        ent = ents.get(rel.lower())
        if not ent:
            continue
        if ent.lower().endswith('.pak'):
            n, p = abrir_pak_mod(raiz, rel, ent, base, ctx)
            paks += n
            avisos += p
        elif ent.lower().endswith('.txt1') and os.path.isdir(os.path.join(base, ent)):
            ruta = os.path.join(raiz, rel)
            with open(ruta, 'rb') as f:
                d = f.read()
            if ap.abrir_txt1(ruta, d):
                otros += 1
            else:
                avisos.append(rel + ' no parece txt1')
    t.append(time.perf_counter())
    tmp = os.path.join(mod, '.abriendo')
    for rel in archivos_en(raiz, zonas):
        if not rel.lower().endswith(ap.CONTENEDORES) or rel.lower() in ents:
            continue
        partes = rel.split('/')
        ent = next((ents[k] for k in ('/'.join(partes[:i]).lower() + '.pak' for i in range(len(partes) - 1, 0, -1)) if k in ents), None)
        if ent and os.path.isfile(os.path.join(raiz, rel)):
            n, p = abrir_suelto_mod(raiz, rel, ent, base, ctx, tmp)
            otros += n
            avisos += p
    t.append(time.perf_counter())
    despues = archivos_en(raiz, zonas)
    fijados = fijar_tamanos(raiz, set(x for x in despues if x.lower().endswith('.png')) - antes, leer_tamanos(assets))
    t.append(time.perf_counter())
    for x in avisos:
        print('aviso: ' + x)
    if any(x.lower().endswith('.unk') for x in despues):
        subprocess.call([sys.executable, os.path.join(BASE, 'tools', 'expose_at3.py'), '--assets', mod] + (['--solo', zonas[0]] if zonas and len(zonas) == 1 and zonas[0] else []))
    t.append(time.perf_counter())
    print('tiempos: python %.1f s, bt3 %.1f s, manifiesto %.1f s, paks %.1f s, sueltos %.1f s, tamanos %.1f s, at3 %.1f s, %s' % ((t[0] - T_INICIO,) + tuple(t[i + 1] - t[i] for i in range(6)) + ('%d carpetas' % len(zonas) if zonas is not None else 'todo el mod',)))
    print('mod: %d paks abiertos, %d contenedores abiertos, %d texturas con tamano propio, %d archivos BT3 abiertos' % (paks, otros, fijados, bt3))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--assets', default=os.path.join(BASE, 'assets'))
    p.add_argument('--listar', action='store_true')
    p.add_argument('--exportar', nargs=3, metavar=('CLAVE', 'PARTE', 'DESTINO'))
    p.add_argument('--pak', action='store_true')
    p.add_argument('--importar', nargs=3, metavar=('CLAVE', 'PARTE', 'ORIGEN'))
    p.add_argument('--mod')
    p.add_argument('--solo')
    a = p.parse_args()
    if a.listar:
        for u in unidades(a.assets):
            print('%s\t%s\t%s\t%d\t%s' % (u['tipo'], u['clave'], u['nombre'], u['partes'][0][1][0], ','.join(partes(u))))
    elif a.exportar:
        exportar_parte(a.assets, buscar(a.assets, a.exportar[0]), a.exportar[1], a.exportar[2], a.pak)
    elif a.importar:
        importar_parte(a.assets, buscar(a.assets, a.importar[0]), a.importar[1], a.importar[2])
    elif a.mod:
        normalizar_mod(a.assets, a.mod, a.solo)
    else:
        p.print_help()


if __name__ == '__main__':
    main()
