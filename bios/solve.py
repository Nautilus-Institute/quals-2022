import sys
import lzma
import struct
from PIL import Image

def DecryptFlagData(data):
	DecryptValue = 31337
	outdata = b""
	for i in range(0, len(data)):
		bit = ((DecryptValue >> 15) ^ (DecryptValue >> 10) ^ (DecryptValue >> 8) ^ (DecryptValue >> 3)) & 1
		DecryptValue = ((DecryptValue << 1) | bit) & 0xffff
		outdata += bytes([data[i] ^ (DecryptValue & 0xff)])

	return outdata	

def AIFToPNG(aifdata):
	data = aifdata.split(b"\n\n\r")
	if data[0] != b"APERTURE IMAGE FORMAT (c) 1985":
		print("Not a valid file")
		sys.exit(0)

	sk = int(data[1])
	sn = 0

	i = 0
	x = 0
	y = 199
	r = 0
	rt = 0

	img = Image.new('RGB', (320, 200), "#000000")

	for c in data[2]:
		r = c-32

		if i == 1:
			i = 0
		else:
			i = 1
			
		while(r):
			if i != 0:
				if (x+r) > 320:
					for x1 in range(x, 320):
						img.putpixel((x1, y), (0xff, 0xff, 0xff))

					r = (r+x) % 320
					x = 0
					y = y - sk
				else:
					for x1 in range(x, x+r):
						img.putpixel((x1, y), (0xff, 0xff, 0xff))

					x = x + r
					r = 0
			else:
				if (x+r) > 320:
					r = (r+x) % 320
					x = 0
					y = y - sk
				else:
					x = x + r
					r = 0
			if y < 0:
				sn += 1
				y = 199 - sn

	return img


f = open("bios-nautilus.bin", "rb")
file_data = f.read()
f.close()

#find flag.lzma.enc
flag_offset = file_data.find(b"flag.lzma.enc")
flag_size = struct.unpack(">H", file_data[flag_offset-14:flag_offset-14+2])[0]
flag_data = file_data[flag_offset+14:flag_offset+14+flag_size]

flag_data = DecryptFlagData(flag_data)

#undo the size entry in the lzma header and decompress
flag_data = flag_data[0:5] + struct.pack("<Q", 0xffffffffffffffff) + flag_data[5+8:]
flag_data = lzma.decompress(flag_data,format=lzma.FORMAT_ALONE)

img = AIFToPNG(flag_data)
img.save("solution.png")

print("Answer saved to solution.png")