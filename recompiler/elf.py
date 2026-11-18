import struct

PT_LOAD = 1
SHT_PROGBITS, SHT_NOBITS = 1, 8
SHF_EXEC = 4


class Elf:
    def __init__(self, path):
        self.path = path
        self.d = open(path, 'rb').read()
        d = self.d
        if d[:4] != b'\x7fELF':
            raise ValueError('%s no es un ELF' % path)
        self.e_type, self.e_mach = struct.unpack_from('<HH', d, 16)
        self.entry, phoff, shoff, self.flags = struct.unpack_from('<IIII', d, 24)
        phnum = struct.unpack_from('<H', d, 0x2C)[0]
        shnum = struct.unpack_from('<H', d, 0x30)[0]
        self.ph = []
        for i in range(phnum):
            t, off, va, pa, fsz, msz, fl, al = struct.unpack_from('<8I', d, phoff + 32 * i)
            self.ph.append(dict(type=t, off=off, vaddr=va, paddr=pa, filesz=fsz, memsz=msz, flags=fl))
        self.sh = []
        for i in range(shnum):
            nm, t, fl, ad, off, sz, lk, inf, al, es = struct.unpack_from('<10I', d, shoff + 40 * i)
            self.sh.append(dict(idx=i, type=t, flags=fl, addr=ad, off=off, size=sz))
        self.load = [p for p in self.ph if p['type'] == PT_LOAD and p['filesz']]

    def off(self, va):
        for p in self.load:
            if p['vaddr'] <= va < p['vaddr'] + p['filesz']:
                return p['off'] + va - p['vaddr']
        return None

    def u32(self, va):
        o = self.off(va)
        return None if o is None else struct.unpack_from('<I', self.d, o)[0]

    def u8(self, va):
        o = self.off(va)
        return None if o is None else self.d[o]

    def bytes(self, va, n):
        o = self.off(va)
        return None if o is None else self.d[o:o + n]

    def code_ranges(self):
        # secciones ejecutables; si el ELF no las trae (overlays), todo el segmento cargable
        out = [(s['addr'], s['addr'] + s['size']) for s in self.sh
               if s['type'] == SHT_PROGBITS and s['size'] and (s['flags'] & SHF_EXEC)]
        if not out:
            out = [(p['vaddr'], p['vaddr'] + p['filesz']) for p in self.load]
        return sorted(out)

    def data_ranges(self):
        return sorted((s['addr'], s['addr'] + s['size']) for s in self.sh
                      if s['type'] == SHT_PROGBITS and s['size'] and not (s['flags'] & SHF_EXEC))

    def segments(self):
        return [(p['vaddr'], p['off'], p['filesz'], p['memsz']) for p in self.load]

    def cstr(self, va):
        o = self.off(va)
        if o is None:
            return ''
        return self.d[o:self.d.index(b'\0', o)].decode('ascii', 'replace')

    def modinfo(self):
        o = self.ph[0]['paddr'] & 0x7FFFFFFF
        if o + 0x34 > len(self.d):
            return None
        attr, ver = struct.unpack_from('<HH', self.d, o)
        nombre = self.d[o + 4:o + 32].split(b'\0')[0].decode('ascii', 'replace')
        gp, ent, ent_fin, imp, imp_fin = struct.unpack_from('<5I', self.d, o + 32)
        return dict(nombre=nombre, gp=gp, exports=(ent, ent_fin), imports=(imp, imp_fin))

    def imports(self):
        mi = self.modinfo()
        if not mi:
            return []
        a, fin = mi['imports']
        out = []
        while a < fin:
            nm_ptr, ver, attr, tam, nvar = struct.unpack_from('<IHHBB', self.d, self.off(a))
            nfun = struct.unpack_from('<H', self.d, self.off(a) + 10)[0]
            nid_tab, stub_tab = struct.unpack_from('<II', self.d, self.off(a) + 12)
            lib = self.cstr(nm_ptr)
            fns = []
            for i in range(nfun):
                nid = self.u32(nid_tab + 4 * i)
                fns.append(dict(nid=nid, stub=stub_tab + 8 * i))
            out.append(dict(lib=lib, version=ver, attr=attr, funcs=fns, nvar=nvar))
            a += tam * 4
        return out
