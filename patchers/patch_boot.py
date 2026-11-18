#!/usr/bin/env python3
# Quita los avisos de PSP del arranque (overlays/7_7.elf): frecuencia de CPU, autoguardado y "Comprobando Memory Stick"
#   --deshacer    restaura el overlay original
import struct, sys, os, shutil

D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game', 'overlays')
OV = os.path.join(D, '7_7.elf')
VADDR, FOFF = 0x08A814A0, 0x54

def off(va): return va - VADDR + FOFF

if '--deshacer' in sys.argv:
    if os.path.exists(OV + '.inicio'):
        shutil.copy2(OV + '.inicio', OV)
        print('restaurado', OV)
    raise SystemExit

if not os.path.exists(OV + '.inicio'): shutil.copy2(OV, OV + '.inicio')
orig = bytearray(open(OV + '.inicio', 'rb').read())
d = bytearray(orig)

def parche(va, esperado, nueva, nombre):
    o = off(va)
    w = struct.unpack_from('<I', orig, o)[0]
    if w != esperado: raise SystemExit('en 0x%08X esperaba %08X y hay %08X' % (va, esperado, w))
    struct.pack_into('<I', d, o, nueva)
    print('0x%08X  %08X -> %08X  %s' % (va, w, nueva, nombre))

# las cajas nunca abiertas quedan en estado -1 (cerradas), asi las esperas del juego terminan solas
parche(0x08A815EC, 0x12600007, 0x10000007, 'sin cajas de frecuencia y autoguardado')
parche(0x08A821DC, 0x0E28BDF3, 0x00000000, 'sin texto de la caja de comprobacion')
parche(0x08A821E4, 0x0E27457C, 0x00000000, 'sin abrir la caja de comprobacion')
parche(0x08A82294, 0x28840033, 0x28840000, 'sin espera minima de 51 cuadros')
parche(0x08A822A4, 0x50800004, 0x50000004, 'sin cerrar la caja de comprobacion')

open(OV, 'wb').write(d)
print('overlay de inicio parcheado')
