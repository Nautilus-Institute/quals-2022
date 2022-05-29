import sys, os
import struct
import binascii
from Crypto.Cipher import AES
from Crypto import Random
import lightning
import lightning_exploit

#value from generate-smt.py
EncTableStartingCRC = 0x472fd8ea
EncTableEndingCRC = 0xc6047a6b
CongratsFuncStart = 0x00003390
CongratsFuncSize = 0x961869c31274478b

funckey = bytes([0xe1, 0x6c, 0x9b, 0x4d, 0x60, 0xd2, 0xaf, 0x4a, 0x4a, 0x1f, 0xf7, 0x3f, 0xc5, 0x0d, 0x62, 0x5b])
strkey = bytes([0xcb, 0x52, 0xda, 0x05, 0xc5, 0x79, 0xd4, 0x35, 0x85, 0xd5, 0x03, 0x60, 0x75, 0x3e, 0x99, 0xb2])

crc_table = lightning.GenerateCRCTable()
crc_rev_table = lightning_exploit.Generate_CRC_Reverse_Table()

RandomData = Random.new()

def EncryptFunc(FuncInfo):
	#offset by 1 to set a single byte of padding at the end
	funcsize = FuncInfo["size"]
	funcoffset = FuncInfo["offset"]
	IV = FuncInfo["IV"]

	funcsize -= 1
	elffile.seek(funcoffset, os.SEEK_SET)
	funcdata = elffile.read(funcsize)

	#generate a CRC for the function data - 4 bytes for the crc
	funccrc = lightning.GetCRC(0, funcdata[0:-4])
	funcdata = funcdata[0:-4] + struct.pack("<I", funccrc) + b"\x01"
	print(f"Encrypting {FuncInfo['name']} with IV {binascii.b2a_hex(IV).decode('utf-8')}")

	cipher = AES.new(funckey, AES.MODE_CBC, IV)
	funcdata = cipher.encrypt(funcdata)

	elffile.seek(funcoffset, os.SEEK_SET)
	elffile.write(funcdata)
	return

def SizeCRCLookup(func, size):
	if func < 3:
		return bytes([size & 0xff]) + RandomData.read(3) + bytes([size >> 8]) + RandomData.read(3)

	#all sizes should be below 64k so look up 2 crc's values to represent each byte and return them
	CRC1 = lightning.crc_table[size & 0xff]
	CRC2 = lightning.crc_table[(size >> 8) & 0xff]
	return struct.pack("<II", CRC1, CRC2)

mapfile = open("secrecy.map", "r").read().split("\n")
elffile = open("secrecy", "r+b")
funclist = open("encrypted_funcs.h", "r").read().split("\n")

encfunclist = []
for i in range(0, len(funclist)):
	if funclist[i].startswith("ENCRYPT("):
		encfunclist.append(funclist[i][8:-1])

#read the elf header and find the sections to alter

#get the section offset
elffile.seek(0x28, os.SEEK_SET)
section_offset = struct.unpack("<Q", elffile.read(8))[0]

#get section table details
elffile.seek(0x3a, os.SEEK_SET)
(e_shentsize, e_shnum, e_shstrndx) = struct.unpack("<HHH", elffile.read(6))

#now read all sections
sections = []
elffile.seek(section_offset, os.SEEK_SET)
for i in range(0, e_shnum):
	(sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize) = struct.unpack("<IIQQQQIIQQ", elffile.read(0x40))
	sections.append({"sh_name": sh_name, "sh_type": sh_type, "sh_flags": sh_flags, "sh_addr": sh_addr, "sh_offset": sh_offset, "sh_size": sh_size, "sh_link": sh_link, "sh_info": sh_info, "sh_addralign": sh_addralign, "sh_entsize": sh_entsize})

#get the section strings
elffile.seek(sections[e_shstrndx]["sh_offset"], os.SEEK_SET)
shstr = elffile.read(sections[e_shstrndx]["sh_size"])

#find .data section
for entry in sections:
	nulloffset = shstr.find(0, entry["sh_name"])
	if shstr[entry["sh_name"]:nulloffset] == b".data":
		DataELFOffset = entry["sh_addr"] - entry["sh_offset"]
		break

FuncsToEncrypt = {}

for curmapline in range(0, len(mapfile)):
	if mapfile[curmapline].startswith(" .text.enc."):
		if len(mapfile[curmapline].split()) == 4:
			#get func data
			mapencline = mapfile[curmapline].split()
			funcoffset = int(mapencline[1], 16)
			funcsize = int(mapencline[2], 16)
			funcname = mapfile[curmapline+1].split()[1]
		else:
			#get func data
			mapencline = mapfile[curmapline+1].split()
			funcoffset = int(mapencline[0], 16)
			funcsize = int(mapencline[1], 16)
			funcname = mapfile[curmapline+2].split()[1]

		print(f"Found {funcname}: offset {hex(funcoffset)} size {hex(funcsize)}")

		#if this is the FirstDecode function then validate the offset in the header
		if funcname == "FirstDecode":
			FirstDecodeOffset = int(open("firstdecodeoffset.h","r").read().split()[2][2:], 16)
			if FirstDecodeOffset != funcoffset:
				#no match, rewrite the header and report
				open("firstdecodeoffset.h","w").write("#define FIRST_DECODE_OFFSET 0x%08x" % (funcoffset))
				print("FirstDecode offset does not match setting in firstdecodeoffset.h, file rewritten\nRerun make clean && make")
				sys.exit(-1)

		if funcsize % AES.block_size:
			print("improperly padded function, aborting")
			sys.exit(-1)

		FuncsToEncrypt[funcname] ={"name": funcname, "size": funcsize, "offset": funcoffset}

	elif mapfile[curmapline].find(".data.enc") > 0:
		#go through each entry and encrypt the strings
		lineoffset = 1
		while mapfile[curmapline+lineoffset][0:3] == "   ":
			str_offset = int(mapfile[curmapline+lineoffset].split()[0], 16) - DataELFOffset

			#read the actual length of the string
			elffile.seek(str_offset, os.SEEK_SET)
			actual_strlen = ord(elffile.read(1))
			print(f"Found string at {hex(str_offset)} of length {hex(actual_strlen)}, encrypting")

			strdata = elffile.read(actual_strlen)
			padding_byte = AES.block_size - (len(strdata) % AES.block_size)
			strdata = strdata + bytes([padding_byte] * padding_byte)
			iv = bytes([actual_strlen] * 16)
			cipher = AES.new(strkey, AES.MODE_CBC, iv)
			strdata = cipher.encrypt(strdata)

			elffile.seek(str_offset+1, os.SEEK_SET)
			elffile.write(strdata)

			#get move onto the next len/str combo
			lineoffset += 1
	elif mapfile[curmapline].find("EncFuncTableCRC") > 0:
		mapencline = mapfile[curmapline].split()
		dataoffset = int(mapencline[0], 16)
		print(f"Altering EncFuncTableCRC to {hex(EncTableEndingCRC)}")
		elffile.seek(dataoffset - DataELFOffset)
		elffile.write(struct.pack("<Q", EncTableEndingCRC))

	elif mapfile[curmapline].find("EncryptedFuncs") > 0:
		mapencline = mapfile[curmapline].split()
		dataoffset = int(mapencline[0], 16)

		EncTableCRC = 0
		for i in range(0, len(encfunclist)):
			entry = encfunclist[i]
			if entry not in FuncsToEncrypt:
				print(f"Unable to locate {entry} in list of found encrypted functions, aborting")
				sys.exit(-1)

			"""
			typedef struct EncryptFuncDataStruct {
				EncryptedFunc Func;
				uint32_t FuncLen;
				uint32_t Hash;
				uint64_t IV[2];
			} EncryptFuncDataStruct;
			"""

			#get a new IV and assign it
			IV = RandomData.read(AES.block_size)

			#get the data that will be in the table
			tabledata = struct.pack("<Q", FuncsToEncrypt[entry]["offset"])
			SizeCRCData = SizeCRCLookup(i, FuncsToEncrypt[entry]["size"] - 1)
			tabledata += SizeCRCData

			#update our CRC
			EncTableCRC = lightning.GetCRC(EncTableCRC, tabledata)

			#if this is not the last or second to last entry then add in the IV we generated
			if i < len(encfunclist) - 2:
				EncTableCRC = lightning.GetCRC(EncTableCRC, IV)
			elif i == len(encfunclist) - 2:
				#we need to alter the IV to hit a specific value
				EncTableCRC = lightning.GetCRC(EncTableCRC, IV[0:12])

				#value came from generate-smt.py
				NewIVBytes = lightning_exploit.Crc_findReverse(EncTableStartingCRC, EncTableCRC)
				print(f"Adjusting IV for {entry} to end with {binascii.b2a_hex(bytes(NewIVBytes)).decode('utf-8')}")
				IV = IV[0:12] + bytes(NewIVBytes)

				#crc in the new bytes
				EncTableCRC = lightning.GetCRC(EncTableCRC, NewIVBytes)

			elif i == len(encfunclist) - 1:
				#make sure this last entry didn't have it's data altered
				if (FuncsToEncrypt[entry]["offset"] != CongratsFuncStart) or (struct.unpack("<Q", SizeCRCData)[0] != CongratsFuncSize):
					print("Last function for encryption does not match. Re-run generate-smt.py with the following and alter value at beginning of this file")
					TableBytes = ""
					for ti in tabledata:
						TableBytes += "0x%02x, " % (ti)
					print("TableBytes = [%s]" % (TableBytes[0:-2]))

					#seek to the beginning of the function and crc the first 16 bytes
					elffile.seek(FuncsToEncrypt[entry]["offset"])
					funcdata = elffile.read(16)
					EndCRC = lightning.GetCRC(0, funcdata)
					print("EndCRC = 0x%08x" % (EndCRC))
					sys.exit(-1)

				#last entry, we need to hit 00000000
				#we need to alter the IV to hit a specific value
				EncTableCRC = lightning.GetCRC(EncTableCRC, IV[0:12])

				#value came from generate-smt.py
				NewIVBytes = lightning_exploit.Crc_findReverse(EncTableEndingCRC, EncTableCRC)
				print(f"Adjusting IV for {entry} to end with {binascii.b2a_hex(bytes(NewIVBytes)).decode('utf-8')}")
				IV = IV[0:12] + bytes(NewIVBytes)

			#now encrypt the function
			FuncsToEncrypt[entry]["IV"] = IV
			EncryptFunc(FuncsToEncrypt[entry])

			#now update the size and IV in the table
			elffile.seek(dataoffset - DataELFOffset + (i*0x20), os.SEEK_SET)
			elffile.write(RandomData.read(8))	#yes i'm a bitch, relocations will overwrite this
			elffile.write(SizeCRCData)
			elffile.write(FuncsToEncrypt[entry]["IV"])

#get the program header offset
elffile.seek(0x20, os.SEEK_SET)
section_offset = struct.unpack("<Q", elffile.read(8))[0]

#get program table details
elffile.seek(0x36, os.SEEK_SET)
(e_phentsize, e_phnum) = struct.unpack("<HH", elffile.read(4))

#go through each program header and find something marked as executable
for i in range(0, e_phnum):
	elffile.seek(section_offset + (0x38 * i), os.SEEK_SET)
	(p_type, p_flags) = struct.unpack("<II", elffile.read(8))
	if(p_type == 1) and (p_flags == 5):
		print(f"Altering header entry {i}")
		elffile.seek(section_offset + (0x38 * i) + 4, os.SEEK_SET)
		elffile.write(struct.pack("<I", 7))
		break

elffile.close()