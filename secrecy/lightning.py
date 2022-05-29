import sys, os
import socket
import time
import binascii
import struct
from Crypto.Cipher import AES

def GetKeyData(s, Identity):
	KeyData = [-1]*16

	skip_keydata = os.environ.get("SKIP_KEYDATA", "DONOTSKIP")
	if skip_keydata != "DONOTSKIP":
		key = open("keydata","rb").read(16)
		iv = [0]*16
		cipher = AES.new(key, AES.MODE_CBC, bytes(iv))
		Identity += b"\x00"*(16-len(Identity))
		KeyData = cipher.decrypt(Identity)
		return KeyData

	#find the first value as we need to look for invalid state due to needing 15 bytes
	IV = [0]*16

	#now find the rest of the end block
	for x in range(0, 16):
		for z in range(0, x):
			IV[15-z] = KeyData[15-z] ^ (x+1)
		for i in range(0, 0x100):
			IV[15-x] = i
			IVBytes = (b"0"*32 + binascii.b2a_hex(bytes(IV)))[-32:]

			s.send(Identity + b"\n")
			s.send(IVBytes + b"\n")
			time.sleep(0.01)

			data = b""
			while(data.find(b"Input your identity:") == -1):
				result = s.recv(1024)
				if len(result) == 0:
					print("Connection error")
					sys.exit(0)
				data += result

			if ((not x) and (data.find(b"Invalid verification state") != -1)) or \
				(x and (data.find(b"Invalid verification length") != -1)):
				KeyData[15-x] = i ^ (x+1)
				print("Found KeyData[%d]: %02x" % (15-x,KeyData[15-x]))
				break

	return KeyData

def GenerateCRCTable():
	global crc_table
	crc_table = [0]*0x100

	for i in range(0, 0x100):
		x = i
		for j in range(0, 8):
			if (x & 1) == 0:
				x = (x >> 1) ^ 0x93a23c59
			else:
				x >>= 1
		crc_table[i] = x

def GetCRC(CRC, Data):
	global crc_table

	crc_ret = CRC
	for i in range(0, len(Data)):
		table_entry = (crc_ret ^ Data[i]) & 0xff
		crc_ret = crc_table[table_entry] ^ (crc_ret >> 8)

	return crc_ret

def GenerateCheckBuffer(Identity):
	CheckBuffer = [0]*32

	for x in range(0, len(CheckBuffer)):
		CheckBuffer[x] = x
		CRC = GetCRC(0, CheckBuffer)
		CRC = GetCRC(CRC, list(Identity))
		CheckBuffer[x] = CRC & 0xff

	return CheckBuffer

def SumCheckBuffer(CheckBuffer):
	#see what check buffer sums to
	Total = 0
	for Entry in CheckBuffer:
		Total += Entry
	Total &= 0x1ff
	return Total

def XorData(CheckBuffer, Pos, Data):
	#we always xor 16 bytes of data
	DataStruct = struct.unpack(">QQ", bytes(Data))
	BufferStruct1, BufferStruct2 = struct.unpack(">QQ", bytes(CheckBuffer[Pos:Pos+16]))
	BufferStruct1 ^= DataStruct[0]
	BufferStruct2 ^= DataStruct[1]
	BufferData = struct.pack(">QQ", BufferStruct1, BufferStruct2)
	CheckBuffer[Pos:Pos+16] = list(BufferData)
	return CheckBuffer

def RolData(CheckBuffer,Pos,RolLen,RolBits):
	RolLen += 1
	RolBits += 1

	#rotate a number of bits across a number of bytes at a specific position
	RolData = CheckBuffer[Pos:Pos+RolLen]
	if RolLen > 8:
		RolData = [0]*(16-len(RolData)) + RolData
		RolData1, RolData2 = struct.unpack(">QQ", bytes(RolData))

		Temp = RolData1
		RolData1 <<= RolBits
		RolData1 |= (RolData2 >> (64 - RolBits))
		RolData1 &= 0xffffffffffffffff

		Temp >>= 64-RolBits-((16-RolLen)*8)

		RolData2 <<= RolBits
		RolData2 |= Temp
		RolData2 &= 0xffffffffffffffff

		RolData = list(struct.pack(">QQ", RolData1, RolData2))
		RolData = RolData[16-RolLen:]

	elif RolLen > 4:
		RolData = [0]*(8-RolLen) + RolData
		RolData = struct.unpack(">Q", bytes(RolData))[0]
		Temp = RolData
		RolData <<= RolBits
		Temp >>= 64-RolBits-((8-RolLen)*8)
		RolData = (RolData | Temp) & 0xffffffffffffffff
		RolData = list(struct.pack(">Q", RolData))
		RolData = RolData[8-RolLen:]
	elif RolLen == 4:
		RolData = struct.unpack(">I", bytes(RolData))[0]
		Temp = RolData
		RolData <<= RolBits
		Temp >>= 32-RolBits
		RolData = (RolData | Temp) & 0xffffffff
		RolData = list(struct.pack(">I", RolData))
	else:
		FirstByte = RolData[0]
		for i in range(0, RolLen - 1):
			RolData[i] = ((RolData[i] << RolBits) | (RolData[i+1] >> (8-RolBits))) & 0xff
		RolData[RolLen-1] = ((RolData[RolLen-1] << RolBits) | (FirstByte >> (8-RolBits))) & 0xff
	CheckBuffer[Pos:Pos+RolLen] = RolData
	return CheckBuffer

def GetCommandBits(CheckBuffer, Identity):
	#we need to generate a block of command bits that gives us a 0, we can only move
	#forward through the buffer of 32 bytes and must xor in the full buffer

	#start our command bits
	CommandBits = ""

	#do a brute force to find a rol/xor combo that decreases the number we need to offset for
	LargestCombo = {"startpos": -1, "rolbytes": 0, "rolbits": 0, "xorpos": -1, "Sum": -1, "Buffer": []}
	SmallestCombo = {"startpos": -1, "rolbytes": 0, "rolbits": 0, "xorpos": -1, "Sum": 0x200, "Buffer": []}

	Found = False
	for PtrIndex in range(0, len(CheckBuffer) - len(Identity)):
		#rotate loop
		for rollen in range(-1, 16):
			for rolbits in range(-1, 8):
				for xorpos in range(PtrIndex, len(CheckBuffer) - len(Identity)):
					TempCheckBuffer = list(CheckBuffer)

					#do rotation
					if (rollen >= 0) and (rolbits >= 0):
						TempCheckBuffer = RolData(TempCheckBuffer, PtrIndex, rollen, rolbits)

					#xor data in
					TempCheckBuffer = XorData(TempCheckBuffer, PtrIndex, Identity)

					#check
					BufferSum = SumCheckBuffer(TempCheckBuffer)
					if BufferSum < SmallestCombo["Sum"]:
						SmallestCombo = {"startpos": PtrIndex, "rolbytes": rollen, "rolbits": rolbits, "xorpos": xorpos, "Sum": BufferSum, "Buffer": TempCheckBuffer}

					if (BufferSum > LargestCombo["Sum"]):
						LargestCombo = {"startpos": PtrIndex, "rolbytes": rollen, "rolbits": rolbits, "xorpos": xorpos, "Sum": BufferSum, "Buffer": TempCheckBuffer}

					if BufferSum == 0:
						Found = True
						break

				if Found:
					break
			if Found:
				break
		if Found:
			break

	#see if we need to attempt to use and to shrink our space
	if ((0x200 - LargestCombo["Sum"]) > 16) and (SmallestCombo["Sum"] > 16):
		#take our smallest combo and see if we can and anything to get closer
		PtrIndex = SmallestCombo["startpos"]
		SmallAndIndex = [-1, -1, SmallestCombo["Sum"]]
		Found = False
		for i in range(PtrIndex, len(SmallestCombo["Buffer"])):
			for x in range(0, 16):
				TempCheckBuffer = list(SmallestCombo["Buffer"])
				TempCheckBuffer[i] = (TempCheckBuffer[i] & x) & 0xff
				BufferSum = SumCheckBuffer(TempCheckBuffer)
				if (BufferSum == 0) or (BufferSum < SmallAndIndex[2]):
					SmallAndIndex = [i, x, BufferSum, TempCheckBuffer]
					if BufferSum == 0:
						Found = True
						break
				elif (0x200 - BufferSum < SmallAndIndex[2]):
					SmallAndIndex = [i, x, 0x200 - BufferSum, TempCheckBuffer]
			if Found:
				break

		PtrIndex = LargestCombo["startpos"]
		LargeAndIndex = [-1, -1, LargestCombo["Sum"]]
		Found = False
		for i in range(PtrIndex, len(LargestCombo["Buffer"])):
			for x in range(0, 16):
				TempCheckBuffer = list(LargestCombo["Buffer"])
				TempCheckBuffer[i] = (TempCheckBuffer[i] & x) & 0xff
				BufferSum = SumCheckBuffer(TempCheckBuffer)
				if (BufferSum == 0) or (BufferSum < LargeAndIndex[2]):
					LargeAndIndex = [i, x, BufferSum, TempCheckBuffer]
					if BufferSum == 0:
						Found = True
						break
				elif (0x200 - BufferSum < LargeAndIndex[2]):
					LargeAndIndex = [i, x, 0x200 - BufferSum, TempCheckBuffer]
			if Found:
				break

		AndCmd = SmallAndIndex
		GoodCombo = SmallestCombo
		if (SmallAndIndex[0] == -1) or ((LargeAndIndex[0] != -1) and (SmallAndIndex[2] > LargeAndIndex[2])):
			AndCmd = LargeAndIndex
			GoodCombo = LargestCombo
			AndCmd[2] = 0x200 - AndCmd[2]
	else:
		#choose large or small to use
		GoodCombo = LargestCombo
		if 0x200 - LargestCombo["Sum"] > SmallestCombo["Sum"]:
			GoodCombo = SmallestCombo
		AndCmd = [-1, 0, 0, []]

	#now setup the command bits as and alters which we go with

	#move forward if we need to adjust the start position
	if GoodCombo["startpos"]:
		#keep incrementing until we hit startpos
		CurPos = 0
		while(CurPos < GoodCombo["startpos"]):
			PosInc = GoodCombo["startpos"] - CurPos
			if PosInc > 7:
				PosInc = 7
			CommandBits += "010" + ("0000" + bin(PosInc)[2:])[-4:]
			CurPos += PosInc

	if GoodCombo["rolbytes"] >= 0:
		CommandBits += "011" + ("000" + bin(GoodCombo["rolbits"])[2:])[-3:] + ("0000" + bin(GoodCombo["rolbytes"])[2:])[-4:]

	if GoodCombo["xorpos"] > GoodCombo["startpos"]:
		CommandBits += "010" + ("0000" + bin(GoodCombo["xorpos"] - GoodCombo["startpos"] - 1)[2:])[-4:]

	#add in xor of 16 bytes
	CommandBits += "0011111"

	#alter the actual check buffer now
	CheckBuffer = GoodCombo["Buffer"]

	#if we have an and to do then handle it
	PtrIndex = GoodCombo["startpos"]
	if AndCmd[0] != -1:
		#if we need to adjust position then do so
		if AndCmd[0] > 0:
			#if there is an adjustment to the pointer then adjust it
			while(PtrIndex < AndCmd[0]):
				PtrOffset = AndCmd[0] - PtrIndex
				if PtrOffset > 16:
					PtrOffset = 16
				CommandBits += "010" + ("0000" + bin(PtrOffset - 1)[2:])[-4:]
				PtrIndex += PtrOffset

		#add in the and command
		CommandBits += "101" + ("0000" + bin(AndCmd[1])[2:])[-4:]
		CheckBuffer = AndCmd[3]

	BufferSum = SumCheckBuffer(CheckBuffer)

	#now do adds or subs until we zero out
	if BufferSum < 0x80:
		#subtract to get closer
		while(BufferSum):
			Adjustment = BufferSum
			if Adjustment > 8:
				Adjustment = 8

			#if no room on the current byte move forward in the buffer
			if CheckBuffer[PtrIndex] + Adjustment >= 0x100:
				Adjustment = 0xff - CheckBuffer[PtrIndex]
				if Adjustment == 0:
					#find a byte to adjust
					Adjusted = False
					for x in range(PtrIndex + 1, len(CheckBuffer)):
						if CheckBuffer[x] > 0x8:
							x = x - PtrIndex
							while(x):
								Adjustment = x
								if Adjustment > 7:
									Adjustment = 7
								CommandBits += "010" + ("0000" + bin(Adjustment)[2:])[-4:]
								PtrIndex += Adjustment
								x -= Adjustment
							Adjusted = True
							break

					#if we failed to locate a place then exit the while loop
					if not Adjusted:
						break
					continue

			#sub instruction
			CommandBits += "110" + ("000" + bin(Adjustment - 1)[2:])[-3:]
			CheckBuffer[PtrIndex] -= Adjustment
			BufferSum -= Adjustment

	elif BufferSum > 0x80:
		#add to get closer
		while(BufferSum & 0x1ff):
			Adjustment = 0x200 - BufferSum
			if Adjustment > 8:
				Adjustment = 8

			#if no room on the current byte move forward in the buffer
			if CheckBuffer[PtrIndex] + Adjustment >= 0x100:
				Adjustment = 0xff - CheckBuffer[PtrIndex]
				if Adjustment == 0:
					#find a byte to adjust
					Adjusted = False
					for x in range(PtrIndex + 1, len(CheckBuffer)):
						if CheckBuffer[x] < 0xf8:
							x = x - PtrIndex
							while(x):
								Adjustment = x
								if Adjustment > 7:
									Adjustment = 7
								CommandBits += "010" + ("0000" + bin(Adjustment)[2:])[-4:]
								PtrIndex += Adjustment
								x -= Adjustment
							Adjusted = True
							break

					#if we failed to locate a place then exit the while loop
					if not Adjusted:
						break
					continue

			#add instruction
			CommandBits += "100" + ("000" + bin(Adjustment - 1)[2:])[-3:]
			CheckBuffer[PtrIndex] += Adjustment
			BufferSum += Adjustment

	#now fill up the bits as far as we can

	#do add/sub back and forth with incrementing values until we
	#run out of room
	Counter = 0
	AddSubFlag = 0
	while (len(CommandBits) + 12 + 3) <= 8*14:
		if AddSubFlag:
			#add first then sub
			Func1 = "100"
			Func2 = "110"
		else:
			#sub then add
			Func1 = "110"
			Func2 = "100"

		CommandBits += Func1 + ("000" + bin(Counter)[2:])[-3:]
		CommandBits += Func2 + ("000" + bin(Counter)[2:])[-3:]
		Counter += 1
		AddSubFlag ^= 1

	#keep adding back to back not's
	while(len(CommandBits) + 6 + 3) <= 8*14:
		CommandBits += "111111"

	#end command
	CommandBits += "000"

	BitsLeft = (8*14) - len(CommandBits)

	BufferSum = SumCheckBuffer(CheckBuffer)
	if (BufferSum != 0) or (len(CommandBits) > (8 * 14)):
		return -1, []

	CommandBytes = []
	CommandBits = CommandBits + "0"*(8*14 - len(CommandBits))
	Sum = 0
	for i in range(0, len(CommandBits), 8):
		CommandBytes.append(int(CommandBits[i:i+8], 2))
		Sum += CommandBytes[-1]

	#get last byte so sum is 0
	CommandBytes.append((0x100 - Sum) & 0xff)

	return (BitsLeft, CommandBytes)

if __name__ == "__main__":
	GenerateCRCTable()

	host = os.environ.get("HOST", "127.0.0.1")
	port = os.environ.get("PORT", 10000)
	ticket = os.environ.get("TICKET", "ticket{SternBowline8663n22:QL_vWf9X_8lCLEbjQlB3b6vIdKnnybYKGSGFQJcklwa5eHWN}")
	s = socket.create_connection((host, port))

	time.sleep(0.02)

	s.sendall((ticket + '\n').encode("utf-8"))

	s.recv(1024)

	Identity = b"Lightning"
	KeyData = GetKeyData(s, Identity)

	#generate the check buffer
	CheckBuffer = GenerateCheckBuffer(Identity)

	#if identity isn't long enough then make it long enough
	CmdIdentity = Identity
	if len(CmdIdentity) < 16:
		CmdIdentity = CmdIdentity + b"\x00"*(16-len(CmdIdentity))
	CmdIdentity = CmdIdentity[0:16]

	#figure out command bits
	BitsLeft, CmdBytes = GetCommandBits(CheckBuffer, CmdIdentity)

	#add our last byte for padding
	CmdBytes.append(1)

	#we need to xor the whole string into the buffer
	IV = list(KeyData)
	for i in range(0, len(IV)):
		IV[i] ^= CmdBytes[i]

	IVBytes = (b"0"*32 + binascii.b2a_hex(bytes(IV)))[-32:]

	s.send(Identity + b"\n")
	s.send(IVBytes + b"\n")
	time.sleep(1)

	Result = s.recv(4096).decode("utf-8")
	print(Result)
	if "Congratulations" in Result:
		print("Identity: ", Identity.decode("utf-8"))
		print("Valid key: ", IVBytes.decode("utf-8"))
