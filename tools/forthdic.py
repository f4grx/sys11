#!/usr/bin/env python3
#this tools displays the contents of the .dict section inside my forth interpreter.

import sys
import elffile
import struct

#------------------------------------------------------------------------------
#this set of classes are helper that follow the structure of the elffile library
class ElfSymbolEntry(elffile.StructBase):
    nameoffset = None #Elf32_Word(4)
    name = None #found separately
    value = None #Elf32_Addr(4)
    size = None #Elf32_Word(4)
    info = None #unsigned char(1)
    other = None #unsigned char(1)
    shndx = None #Elf32_Half(2)

    def unpack_from(self, section, offset=0):
        (self.nameoffset, self.value, self.size, self.info,
         self.other, self.shndx ) = self.coder.unpack_from(section, offset)
        return self

    def pack_into(self, section, offset=0):
        self.coder.pack_into(section, offset,
                             self.nameoffset, self.value, self.size, self.info,
                             self.other, self.shndx)
        return self

    def __repr__(self):
        return ('<{0}@{1}: nameoffset={2}, name=\'{8}\' value={3}, size={4}, info={5}, other={6}, shndx={7}>'
                .format(self.__class__.__name__, hex(id(self)), self.nameoffset, "%08X"%self.value, self.size, self.info, self.other, self.shndx, self.name))

    def findname(self, strtab):
        if self.nameoffset == 0:
            self.name = ''
        else:
            strcontents = strtab.content
            self.name = strcontents[self.nameoffset:strcontents.find(b'\0', self.nameoffset)]
        return self

#subclass for bigendian (unused)
class ElfSymbolEntryl(ElfSymbolEntry):
    coder = struct.Struct(b'<IIIBBH')

#subclass for little endian
class ElfSymbolEntryb(ElfSymbolEntry):
    coder = struct.Struct(b'>IIIBBH')

#------------------------------------------------------------------------------

try:
    elf = elffile.open(sys.argv[1])
except Exception as e:
    print("ERROR : Could not open input file '", file_input,"'")
    print(e)
    sys.exit(1)

dic = None
symbols = None
strings = None

for sec in elf.sectionHeaders:
    if sec.name == b".rodata":
        print("    .rodata found")
        dic = sec
    elif sec.name == b".symtab":
        symbols = sec
        print("    Symbol table found, %u bytes" % len(symbols.content))
    elif sec.name == b".strtab":
        print("    Strings table found")
        strings = sec

if strings == None:
    strings = elf.sectionHeaders[elf.fileHeader.shstrndx]
    print("    ! Found string table via file header, this part of the code seems buggy")

#create the list of all symbols
syms = []
if symbols != None:
    symcount = int(len(symbols.content) / 16)
    for i in range(symcount):
        s = ElfSymbolEntryb().unpack_from(symbols.content, i*16).findname(strings)
        syms.append(s)

def findsymaddr(name):
    for s in syms:
        if s.name == name:
            return s.value

    return None

def findsymname(addr):
    if addr == 0:
        return "NULL"

    for s in syms:
        if s.value == addr:
            if len(s.name) == 0: continue
            return s.name

    return "None@%04X" % addr

#------------------------------------------------------------------------------
if dic == None:
    print("No dic found")
    sys.exit(0)

dicstart = findsymaddr(b"dic_start")
dicend = findsymaddr(b"dic_end")
adrenter = findsymaddr(b"code_ENTER")
adrexit  = findsymaddr(b"RETURN")

print("ENTER=%04X EXIT=%04X" % (adrenter,adrexit))
print("start=%04X end=%04X" % (dicstart,dicend))

base = dic.addr
cnt = dic.content
length = len(cnt)
print("dic size: ", length)
for i in range(dicend-dicstart):
    if i%16==0: print("%04X: "%i, end='')
    print("%02X" % cnt[i+dicstart-base], end='')
    if i%16==15: print()
print()

ptr = dicstart

while ptr < dicend:
    print()

    print("[%04X] " % ptr, end='')
    lnk = struct.Struct(">H").unpack_from(cnt, ptr-base)
    lnk = lnk[0]
    ptr += 2
    print("link=%04X" % lnk, findsymname(lnk) )

    print("[%04X] " % ptr, end='')
    name = ""
    while cnt[ptr-base] != 0:
        name = name + chr(cnt[ptr-base])
        ptr+=1
    print("name=%s" % name)
    ptr+=1

    print("[%04X] " % ptr, end='')
    code = struct.Struct(">H").unpack_from(cnt, ptr-base)
    code = code[0]
    ptr += 2
    print("code=%04X" % code, findsymname(code) )

    if code != adrenter:
        continue

    #we have a wordlist. display words
    word = 0
    while word != adrexit and ptr < dicend:
        print("[%04X] " % ptr, end='')
        word = struct.Struct(">H").unpack_from(cnt, ptr-base)
        word = word[0]
        ptr += 2
        print("word=%04X" % word, findsymname(word) )

