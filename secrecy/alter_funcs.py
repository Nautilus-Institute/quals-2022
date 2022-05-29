import sys, os
import struct

BLOCK_SIZE = 16

elffile = open(sys.argv[1], "r+b")

#get the section offset
elffile.seek(0x28, os.SEEK_SET)
section_offset = struct.unpack("<Q", elffile.read(8))[0]

#get section table details
elffile.seek(0x3a, os.SEEK_SET)
(e_shentsize, e_shnum, e_shstrndx) = struct.unpack("<HHH", elffile.read(6))

#go through each section
elffile.seek(section_offset, os.SEEK_SET)

#read all sections into a table
sections = []
for i in range(0, e_shnum):
	(sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize) = struct.unpack("<IIQQQQIIQQ", elffile.read(0x40))
	sections.append({"sh_name": sh_name, "sh_type": sh_type, "sh_flags": sh_flags, "sh_addr": sh_addr, "sh_offset": sh_offset, "sh_size": sh_size, "sh_link": sh_link, "sh_info": sh_info, "sh_addralign": sh_addralign, "sh_entsize": sh_entsize})

#get the section strings
elffile.seek(sections[e_shstrndx]["sh_offset"], os.SEEK_SET)
shstr = elffile.read(sections[e_shstrndx]["sh_size"])

#find .strtab for normal strings
for entry in sections:
	if (entry["sh_type"] == 3):
		nulloffset = shstr.find(0, entry["sh_name"])
		if shstr[entry["sh_name"]:nulloffset] == b".strtab":
			elffile.seek(entry["sh_offset"], os.SEEK_SET)
			strtab = elffile.read(entry["sh_size"])
			break

#find .symtab section
for entry in sections:
	if entry["sh_type"] == 2:
		break

#seek to the symbol table
Altered = False
for i in range(0, int(entry["sh_size"] / 0x18)):
	elffile.seek(entry["sh_offset"] + (i * 0x18), os.SEEK_SET)
	(st_name, st_info, st_other, st_shndx, st_value, st_size) = struct.unpack("<IBBHQQ", elffile.read(0x18))
	st_type = st_info & 0xf
	st_bind = st_info >> 4

	if st_type == 2:
		#check if the section name for this symbol is .text.enc.
		nulloffset = shstr.find(0, sections[st_shndx]["sh_name"])
		if not shstr[sections[st_shndx]["sh_name"]:nulloffset].startswith(b".text.enc."):
			continue

		#add crc
		new_st_size = st_size + 4

		#add enough that we are claiming the function is exactly a full block, when we encrypt we will set the size to be -1
		#for padding
		padding = BLOCK_SIZE - (new_st_size % BLOCK_SIZE)
		new_st_size += padding

		if not Altered:
			print(f"Altering {sys.argv[1]}:")
			Altered = True

		elffile.seek(entry["sh_offset"] + (i * 0x18) + 0x10, os.SEEK_SET)
		elffile.write(struct.pack("<Q", new_st_size))

		nulloffset = strtab.find(0, st_name)
		func_name = strtab[st_name:nulloffset].decode("utf-8")
		print(f"\tFunction {func_name} size {hex(st_size)} altered to {hex(new_st_size)}")

		#see if we need to alter the size of this section
		if new_st_size + st_value > sections[st_shndx]["sh_size"]:
			new_st_size += st_value
			nullbyte = shstr.find(0, sections[st_shndx]["sh_name"])
			section_name = shstr[sections[st_shndx]["sh_name"]:nullbyte].decode("utf-8")
			print(f"\tAltering section {section_name} size {hex(sections[st_shndx]['sh_size'])} to {hex(new_st_size)}")

			#alter the size
			elffile.seek(section_offset + (st_shndx * 0x40) + 0x20, os.SEEK_SET)
			elffile.write(struct.pack("<Q", new_st_size))

elffile.close()