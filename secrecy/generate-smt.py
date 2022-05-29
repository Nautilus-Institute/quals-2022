import sys, os
import lightning
import subprocess
import struct

TableBytes = [0x90, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x47, 0x74, 0x12, 0xc3, 0x69, 0x18, 0x96]
EndCRC = 0x3115cda3

#StartCRC is based on GetCRC(0, Payload[0:7]) to save calculation time
StartCRC = 0x5749f971

def GenerateSMT(TableBytes):
	lightning.GenerateCRCTable()

	f = open("solver.smt","w")

	#write the CRC functions out
	f.write("(define-fun GetCRCEntry ((entry (_ BitVec 32))) (_ BitVec 32)\n")
	for i in range(0, 0xfe):
		f.write("\t(ite (= entry #x%08x) #x%08x\n" % (i, lightning.crc_table[i]))
	f.write("\t(ite (= entry #x%08x) #x%08x #x%08x\n" % (0xfe, lightning.crc_table[0xfe], lightning.crc_table[0xff]))
	f.write("\t" + ")"*50 + "\n")
	f.write("\t" + ")"*50 + "\n")
	f.write("\t" + ")"*50 + "\n")
	f.write("\t" + ")"*50 + "\n")
	f.write("\t" + ")"*50 + "\n")
	f.write("\t" + ")"*5 + "\n")
	f.write(")\n")

	f.write("""
(define-fun GetCRC ((CRC (_ BitVec 32)) (CurByte (_ BitVec 32))) (_ BitVec 32)
	(bvxor (GetCRCEntry (bvand (bvxor CRC CurByte) #x000000ff)) (bvlshr CRC #x00000008))
)\n""")

	f.write("""
(declare-const StartCRC (_ BitVec 32))
(declare-const EndCRC (_ BitVec 32))
(assert (= StartCRC #x%08x))
(assert (= EndCRC #x%08x))

(declare-const TableStartCRC (_ BitVec 32))
(declare-const TableEndCRC (_ BitVec 32))\n\n""" % (StartCRC, EndCRC))

	f.write("""(assert (not (= (bvand TableStartCRC #xf0000000) #x00000000)))
(assert (not (= (bvand TableEndCRC #xf0000000) #x00000000)))\n\n""");
	f.write("""(assert (not (= (bvand TableStartCRC #xff000000) #x00000000)))
(assert (not (= (bvand TableEndCRC #xff000000) #x00000000)))\n\n""");
	f.write("""(assert (not (= (bvand TableStartCRC #x00ff0000) #x00000000)))
(assert (not (= (bvand TableEndCRC #x00ff0000) #x00000000)))\n\n""");
	f.write("""(assert (not (= (bvand TableStartCRC #x0000ff00) #x00000000)))
(assert (not (= (bvand TableEndCRC #x0000ff00) #x00000000)))\n\n""");
	f.write("""(assert (not (= (bvand TableStartCRC #x000000ff) #x00000000)))
(assert (not (= (bvand TableEndCRC #x000000ff) #x00000000)))\n\n""");

	f.write("""(assert (not (= (bvand TableStartCRC #xff000000) #xff000000)))
(assert (not (= (bvand TableEndCRC #xff000000) #xff000000)))\n\n""");
	f.write("""(assert (not (= (bvand TableStartCRC #x00ff0000) #x00ff0000)))
(assert (not (= (bvand TableEndCRC #x00ff0000) #x00ff0000)))\n\n""");
	f.write("""(assert (not (= (bvand TableStartCRC #x0000ff00) #x0000ff00)))
(assert (not (= (bvand TableEndCRC #x0000ff00) #x0000ff00)))\n\n""");
	f.write("""(assert (not (= (bvand TableStartCRC #x000000ff) #x000000ff)))
(assert (not (= (bvand TableEndCRC #x000000ff) #x000000ff)))\n\n""");

	for i in range(0, 16):
		f.write("(declare-const PayloadByte%02d (_ BitVec 32))\n" % (i))

	for i in range(0, 16):
		f.write("(declare-const TableByte%02d (_ BitVec 32))\n" % (i))

	for i in range(7, 16):
		f.write("(declare-const NewCRC%02d (_ BitVec 32))\n" % (i))

	for i in range(0, 31):
		f.write("(declare-const TableCRC%02d (_ BitVec 32))\n" % (i))

	f.write("""
(assert (= PayloadByte00 #x0000005e))
(assert (= PayloadByte01 #x00000093))
(assert (= PayloadByte02 #x00000056))
(assert (= PayloadByte03 #x00000092))
(assert (= PayloadByte04 #x0000000f))
(assert (= PayloadByte05 #x00000005))
(assert (= PayloadByte06 #x000000c3))\n\n""")

	for i in range(0, len(TableBytes)):
		f.write("(assert (= TableByte%02d #x%08x))\n" % (i, TableBytes[i]));

	f.write("\n(assert (= NewCRC07 (GetCRC StartCRC PayloadByte07)))\n")

	for i in range(8, 15):
		f.write("(assert (= NewCRC%02d (GetCRC NewCRC%02d PayloadByte%02d)))\n" % (i, i-1, i))
	f.write("(assert (= EndCRC (GetCRC NewCRC14 PayloadByte15)))\n\n")

	f.write("(assert (= TableCRC00 (GetCRC TableStartCRC TableByte00)))\n")

	for i in range(1, 16):
		f.write("(assert (= TableCRC%02d (GetCRC TableCRC%02d TableByte%02d)))\n" % (i, i-1, i))

	for i in range(16, 31):
		f.write("(assert (= TableCRC%02d (GetCRC TableCRC%02d PayloadByte%02d)))\n" % (i, i-1, i-16))

	f.write("""(assert (= TableEndCRC (GetCRC TableCRC30 PayloadByte15)))

(check-sat)
(get-model)""")
	f.close()

if __name__ == "__main__":
	GenerateSMT(TableBytes)
	output = subprocess.check_output(["z3", "solver.smt"])
	open("solver.output","wb").write(output)

	if output[0:3] == b"sat":
		output = output.split(b"\n")
		Values = [-1]*4
		for i in range(0, len(output)):
			if b"TableStartCRC" in output[i]:
				(FuncStart, FuncSize) = struct.unpack("<QQ", bytes(TableBytes))
				Values[0] = int(output[i+1][0:-1].decode("utf-8").strip()[2:], 16)
				Values[1] = FuncStart
				Values[2] = FuncSize
				if -1 not in Values:
					break

			elif b"TableEndCRC" in output[i]:
				Values[3] = int(output[i+1][0:-1].decode("utf-8").strip()[2:], 16)
				if -1 not in Values:
					break

		print("EncTableStartingCRC = 0x%08x" % (Values[0]))
		print("EncTableEndingCRC = 0x%08x" % (Values[3]))
		print("CongratsFuncStart = 0x%08x" % (Values[1]))
		print("CongratsFuncSize = 0x%x" % (Values[2]))
	else:
		print("Not sat")