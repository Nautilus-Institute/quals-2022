import lzma
import struct
import sys
from PIL import Image

def Convert(Filename):
	data = open(Filename,"rb").read()
	outdata = lzma.compress(data,format=lzma.FORMAT_ALONE)
	outdata = outdata[0:5] + struct.pack("<Q", len(data)) + outdata[5+8:]
	open(Filename + ".lzma","wb").write(outdata)
	return outdata

if __name__ == "__main__":
	Convert(sys.argv[1])