import sys, os
import binascii

CRC_Table = []
CRC_Reverse_Table = []
crc_ = 0


POLYNOMIAL = 0xbfa28ea408cb865c28008f4d786b04a67791
POLYNOMIAL_SIZE = 144

#POLYNOMIAL = 0xbfa28ea408cb865c28008f4d786b04a6770290e96f85e0df
#POLYNOMIAL_SIZE = 192
#POLYNOMIAL = 0xa7bee02210a451e9bbd2c83a6e54b5f3
#POLYNOMIAL_SIZE = 128
#POLYNOMIAL = 0x4c11db7
#POLYNOMIAL_SIZE = 32 	

def INV(x):
	return (~x) & ((1 << POLYNOMIAL_SIZE) - 1)

def Generate_CRC_Table():
	table = {}
	POLYNOMIAL_REVERSE = 0
	for i in range(0, POLYNOMIAL_SIZE):
		POLYNOMIAL_REVERSE |= (((POLYNOMIAL & (1 << i)) >> i) << (POLYNOMIAL_SIZE - i - 1))

	for i in range(0, 256):
		crc = i
		for bit in range(0, 8):
			if crc & 1:
				crc = ((crc >> 1) ^ POLYNOMIAL_REVERSE) & ((1 << POLYNOMIAL_SIZE) - 1)
			else:
				crc = (crc >> 1) & ((1 << POLYNOMIAL_SIZE) - 1)
		table[i] = crc
	return table

def Generate_CRC_Reverse_Table():
	table = {}
	for i in range(0, 256):
		table[(CRC_Table[i] >> (POLYNOMIAL_SIZE - 8)) & 0xff] = i
	return table

def Crc_append(value):
	global crc_
	crc_ = INV(crc_);
	crc_ = (crc_ >> 8) ^ CRC_Table[value ^ (crc_ & 0xff)];
	crc_ = INV(crc_);
	return

def Crc_set(crc):
	global crc_
	crc_ = crc
	return

def Crc_findReverse(desiredCrc):
	global crc_
	crcIdx = [0] * int(POLYNOMIAL_SIZE / 8)
	patchBytes = [0] * int(POLYNOMIAL_SIZE / 8)

	iterCrc = INV(desiredCrc)
	for j in range(int(POLYNOMIAL_SIZE / 8) - 1, -1, -1):
		crcIdx[j] = CRC_Reverse_Table[iterCrc >> (POLYNOMIAL_SIZE - 8)]
		iterCrc = ((iterCrc ^ CRC_Table[crcIdx[j]]) << 8) & ((1 << POLYNOMIAL_SIZE) - 1)

	crc = INV(crc_)
	for j in range(0, int(POLYNOMIAL_SIZE / 8)):
		patchBytes[j] = (crc ^ crcIdx[j]) & 0xff
		crc = (crc >> 8) ^ CRC_Table[patchBytes[j] ^ (crc & 0xff)]

	return patchBytes

def Crc_FindMatch(Data1, desiredCrc):
	#append a character then try to find a match
	Crc_set(POLYNOMIAL)
	for i in Data1:
		Crc_append(ord(i))

	desiredCrcVal = 0
	for i in desiredCrc:
		desiredCrcVal = (desiredCrcVal << 8) | ord(i)

	Match = Crc_findReverse(desiredCrcVal)
	NewList = b""

	for Entry in Match:
		NewList += bytes([Entry])

	return NewList

	return b""

def CalcCRC(Str1, data):
	#append a character then try to find a match
	Crc_set(POLYNOMIAL)
	for entry in Str1:
		Crc_append(ord(entry))

	for entry in data:
		Crc_append(entry)

	return crc_

def GenerateBase94(data):
	output = ""
	value = 0

	if len(data) % 3:
		data = b"\x00"*(3 - len(data)%3) + data

	for i in range(0, len(data), 3):
		value = 0x1000000 | (data[i] << 16) | (data[i+1] << 8) | data[i+2]

		while(value):
			newchar = int(value % 94)
			value = int((value - newchar) / 94)
			output += chr(newchar + 0x21)

	return output

def Base94ToVal(data):
	output = []
	value = 0

	for i in range(len(data) - 1, -1, -4):
		value = int(ord(data[i]) - 0x21)
		value = (value*94) + int(ord(data[i-1]) - 0x21)
		value = (value*94) + int(ord(data[i-2]) - 0x21)
		value = (value*94) + int(ord(data[i-3]) - 0x21)

		output.insert(0, value & 0xff)
		value >>= 8
		output.insert(0, value & 0xff)
		value >>= 8
		output.insert(0, value & 0xff)
		value >>= 8
		value = 0

	return bytes(output[0:int(POLYNOMIAL_SIZE / 8)])

def Validate(punycode, data):
	global CRC_Table, CRC_Reverse_Table
	CRC_Table = Generate_CRC_Table()
	CRC_Reverse_Table = Generate_CRC_Reverse_Table()
	if len(CRC_Reverse_Table) != 256:
		print("Reverse table invalid")
		sys.exit(0)

	"""
	ret = GenerateBase94(Crc_FindMatch("Nautilus InstituteLightning", "Nautilus Institute"))
	finalcrc = CalcCRC("Nautilus InstituteLightning", Base94ToVal(ret))
	print(ret)
	print(hex(finalcrc)[2:])
	"""

	Name = punycode
	Serial = data

	finalcrc = CalcCRC("Nautilus Institute" + Name, Base94ToVal(Serial))
	finalcrc = hex(finalcrc)[2:]
	if len(finalcrc) % 2:
		finalcrc = "0" + finalcrc

	ret = binascii.a2b_hex(finalcrc)
	return (ret == b"Nautilus Institute")

if __name__ == "__main__":
	if Validate(sys.argv[1], sys.argv[2]):
		print("OK")
	else:
		print("BAD")
