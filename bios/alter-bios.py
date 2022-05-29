import sys, os
import struct
import convert_bio_data
import generate_aif
import compress_lzma

#write the cbfs header
##define CBFS_HEADER_MAGIC 0x4F524243
##define CBFS_VERSION1 0x31313131
#
#struct cbfs_header {
#    u32 magic;
#    u32 version;
#    u32 romsize;
#    u32 bootblocksize;
#    u32 align;
#    u32 offset;
#    u32 pad[2];
#} PACKED;

##define CBFS_FILE_MAGIC 0x455649484352414cLL // LARCHIVE
#
#struct cbfs_file {
#    u64 magic;
#    u32 len;
#    u32 type;
#    u32 checksum;
#    u32 offset;
#    char filename[0];
#} PACKED;

romdata = open("seabios/out/bios.bin","rb").read()
ROM_SIZE = 60*1024

CBFS_ROMSIZE = len(romdata) - ROM_SIZE
CBFS_START_LOCATION = 0x100000000 - CBFS_ROMSIZE - ROM_SIZE
CBFS_END_LOCATION = CBFS_START_LOCATION + CBFS_ROMSIZE

romdata = romdata[-ROM_SIZE:]

CBFS_HEADER_MAGIC = 0x4F524243
CBFS_VERSION1 = 0x31313131
CBFS_FILEMAGIC = 0x455649484352414c

def AddCBFSHeader(data):
	#make sure we are padded out to our rom size
	data = data + bytes([0]*((CBFS_ROMSIZE) - len(data)))
	cbfs_header = struct.pack(">IIIIIII", CBFS_HEADER_MAGIC, CBFS_VERSION1, CBFS_ROMSIZE, 0, 4, 0, 0) + struct.pack("<I", CBFS_END_LOCATION - (8*4))
	print("Pointer to header should be at %08x" % ((CBFS_END_LOCATION)))
	return data[0:-len(cbfs_header)] + cbfs_header

def AddCBFSFile(data, filename, store_filename):
	filedata = open(filename, "rb").read()
	store_filename = store_filename.encode("utf-8")
	cbfs_filedata = struct.pack("<Q", CBFS_FILEMAGIC) + struct.pack(">IIII", len(filedata), 0, 0, (6*4) + len(store_filename) + 1) + store_filename + bytes([0]) + filedata
	if len(cbfs_filedata) % 4:
		cbfs_filedata += bytes([0]*(4-(len(cbfs_filedata) % 4)))
	return data + cbfs_filedata

def CreateFlagFile():
	answer = "The Flag Is:\nDescent ][\nRules"

	#get the image and convert to aif format
	aifdata = generate_aif.getaifdata(generate_aif.txt2img(answer, FontSize=40), 42)

	#compress the data
	open("answer.aif","wb").write(aifdata)
	answer_data = compress_lzma.Convert("answer.aif")

	LFSR = 31337
	answer_encrypted = b""
	for i in range(0, len(answer_data)):
		bit = ((LFSR >> 15) ^ (LFSR >> 10) ^ (LFSR >> 8) ^ (LFSR >> 3)) & 1
		LFSR = ((LFSR << 1) | bit) & 0xffff
		answer_encrypted += bytes([(answer_data[i] ^ (LFSR & 0xff)) & 0xff])

	open("answer_encrypted.data", "wb").write(answer_encrypted)

CreateFlagFile()

#compress the loading image
compress_lzma.Convert("nautilus.bmp")

cbfsdata = b""
cbfsdata = AddCBFSFile(cbfsdata, "nautilus.bmp.lzma", "bootsplash.bmp.lzma")
cbfsdata = AddCBFSFile(cbfsdata, "font-out.bmp.lzma", "font.bmp.lzma")
cbfsdata = AddCBFSFile(cbfsdata, "font-out.dat", "font.dat")
cbfsdata = AddCBFSFile(cbfsdata, "fire.pal", "fire.pal")
cbfsdata = AddCBFSFile(cbfsdata, "bio data/names.txt", "names.txt")

names = convert_bio_data.ConvertBIOData()
for entry in names:
	cbfsdata = AddCBFSFile(cbfsdata, f"bio data/bio_{entry}-out.bmp.lzma", f"{entry.upper()}.bmp.lzma")
	cbfsdata = AddCBFSFile(cbfsdata, f"bio data/bio_{entry}-out.txt", f"{entry.upper()}.txt")

cbfsdata = AddCBFSFile(cbfsdata, "answer_encrypted.data", "flag.lzma.enc")

#if data + header is too large fail
if len(cbfsdata) + (8*4) > CBFS_ROMSIZE:
	print(f"Error, data is too large, please fix script, {len(cbfsdata) + (8*4)} bytes, max {CBFS_ROMSIZE} bytes")
	sys.exit(0)

cbfsdata = AddCBFSHeader(cbfsdata)
open("bios-nautilus.bin","wb").write(cbfsdata + romdata)