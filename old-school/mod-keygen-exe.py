import sys, os
import struct

data = open("keygen.exe", "rb").read()

#trigger vb3 decomp cheat message
data = data[0:0x6ca] + bytes([0]) + data[0x6cb:]

#find the form data
FormName = b"FRMKEYGE.FRM"
formpos = data.find(b"\x03\x20\x81\x80")
formpos = data.find(FormName, formpos)
data = data[0:formpos] + b"NUL" + b"\x00"*(len(FormName) - 3) + data[formpos+len(FormName):]

#remove the names of fields at the end
NamePos = 0
for i in range(len(data) - 1, 0, -1):
	if data[i] == 0:
		NamePos = i+1
		break

"""
#replace each field name with null bytes
while(NamePos < len(data)):
	CurNameLen = data[NamePos]
	data = data[0:NamePos + 1] + b"\x00"*CurNameLen + data[NamePos+1+CurNameLen:]
	NamePos += CurNameLen + 1
"""

data = data[0:NamePos]

#get rid of the data at the end that is 0 bytes
for i in range(NamePos-1, 0, -1):
	if data[i] != 0:
		#found non null data, pad to 4 bytes
		if i % 4:
			i += 4 - (i % 4)
		data = data[0:i]
		break

hash = 0
for i in range(0, len(data), 2):
	val = struct.unpack("<H", data[i:i+2])[0]
	hash ^= val

data = data + struct.pack("<H", hash)
open("keygen1.exe","wb").write(data)
