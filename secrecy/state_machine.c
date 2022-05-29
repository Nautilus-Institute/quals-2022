#include <stdio.h>
#include "secrecy.h"

//i'm abusing shit to force specific distance
typedef struct DataStruct {
	uint8_t PaddingBuffer[0x48];
	uint8_t Data[AES_BLOCKLEN*2];
} DataStruct;
DataStruct CheckBuffer __attribute__((section(".state_machine.bss.1")));
#define CheckBuffer CheckBuffer.Data

uint8_t DataBuffer[AES_BLOCKLEN] __attribute__((section(".state_machine.bss")));
uint8_t CommandBuffer[AES_BLOCKLEN - 2] __attribute__((section(".state_machine.bss")));
int64_t BitOffset __attribute__((section(".state_machine.bss")));
int64_t DataOffset __attribute__((section(".state_machine.bss")));
int64_t CheckOffset __attribute__((section(".state_machine.bss")));

ENCRYPT(InitStateMachine)
{
	//Param1 - Command Buffer
	//Param2 - Pointer to original inputted identity
	//Param3 - Length of original identity
	uint64_t gen_crc;

	BitOffset = 0;
	DataOffset = 0;
	CheckOffset = 0;

	CallFunc(ENC_Memcpy, CommandBuffer, Param1, (void *)(sizeof(CommandBuffer)), 0);
	CallFunc(ENC_Memcpy, DataBuffer, Param2, (void *)(sizeof(DataBuffer)), 0);
	CallFunc(ENC_Memset, CheckBuffer, 0, (void *)(sizeof(CheckBuffer)), 0);

	//loop through and setup the check buffer
	for(int i = 0; i < sizeof(CheckBuffer); i++) {
		CheckBuffer[i] = i;
		gen_crc = CallFunc(ENC_crc32, 0, CheckBuffer, (void *)(sizeof(CheckBuffer)), 0);
		gen_crc = CallFunc(ENC_crc32, (void *)gen_crc, Param2, Param3, 0);
		CheckBuffer[i] = gen_crc & 0xff;
	}
	return 0;
}

ENCRYPT(GetBits)
{
	uint16_t Bits;
	uint64_t BitCount = (uint64_t)Param1;

	//get bits from the buffer
	Bits = __builtin_bswap16(*(uint16_t *)&CommandBuffer[BitOffset / 8]);
	Bits >>= (16 - (BitOffset % 8) - BitCount);
	Bits &= ((1 << BitCount) - 1);

	//increment our offset
	BitOffset += BitCount;
	if(BitOffset > (sizeof(CommandBuffer) * 8))
		return INVALID_STATE;

	return Bits;
}

ENCRYPT(RunStateMachine)
{
	int Run = 1;
	int64_t Ret;

	Ret = CallFunc(ENC_InitStateMachine, Param1, Param2, Param3, 0);
	if(Ret != 0)
		return Ret;

	while(Run) {
		//figure out the command
		Ret = CallFunc(ENC_GetBits, (void *)3, 0, 0, 0);
		if(Ret < 0)
			return Ret;

		switch(Ret) {
			case 0:		//end
				Run = 0;
				Ret = 1;
				break;

			case 1:		//xor data into check buffer
				Ret = CallFunc(ENC_XorData, 0, 0, 0, 0);
				break;

			case 2:		//increment check pointer
				Ret = CallFunc(ENC_IncPointer, 0, 0, 0, 0);
				break;

			case 3:		//rotate check data left
				Ret = CallFunc(ENC_Rol, 0, 0, 0, 0);
				break;

			case 4:		//add value to data in check buffer
				Ret = CallFunc(ENC_Add, 0, 0, 0, 0);
				break;

			case 5:		//and value to data in check buffer
				Ret = CallFunc(ENC_And, 0, 0, 0, 0);
				break;

			case 6:		//sub value from data in check buffer
				Ret = CallFunc(ENC_Sub, 0, 0, 0, 0);
				break;

			case 7:		//bit flip byte in check buffer
				Ret = CallFunc(ENC_BitFlip, 0, 0, 0, 0);
				break;
		}

		if(Ret != 1)
			return Ret;
	}

	//done running, we must be sitting at the end of the data buffer
	if(DataOffset != sizeof(DataBuffer))
		return INVALID_STATE;

	//see if we hit all the command bytes, we must be in the 14th byte
	if(BitOffset < (8*13))
		return INVALID_STATE;

	//see if we pass the hash validation
	Ret = 0;
	for(int i = 0; i < sizeof(CheckBuffer); i++) {
		Ret += CheckBuffer[i];
	}

	//return good or bad
	return ((Ret & 0x1ff) == 0);
}

ENCRYPT(IncPointer)
{
	int8_t Bits;

	//get 4 bits
	Bits = CallFunc(ENC_GetBits, (void *)4, 0, 0, 0);
	if(Bits < 0)
		return Bits;

	//convert and adjust the pointer
	Bits = (int8_t)(Bits << 4) >> 4;
	CheckOffset += Bits;
	if(CheckOffset >= (int64_t)sizeof(CheckBuffer))
		return INVALID_STATE;

	return 1;
}

ENCRYPT(XorData)
{
	int8_t Bits;
	int64_t TempCheckOffset = CheckOffset;

	//get 4 bits
	Bits = CallFunc(ENC_GetBits, (void *)4, 0, 0, 0);
	if(Bits < 0)
		return Bits;

	//xor bytes into check buffer
	Bits++;
	while(Bits) {
		if(DataOffset >= sizeof(DataBuffer))
			return INVALID_STATE;

		if(TempCheckOffset >= (int64_t)sizeof(CheckBuffer))
			return INVALID_STATE;
		
		CheckBuffer[TempCheckOffset] ^= DataBuffer[DataOffset];
		TempCheckOffset++;
		DataOffset++;
		Bits--;
	}

	return 1;
}

ENCRYPT(Rol)
{
	int8_t RolBits;
	int8_t RolBytes;
	uint8_t FirstByte;
	uint16_t CurBytes;
	int i;

	//3 bits for number of bits to rotate by
	//4 bits for how many bytes of data to rotate

	//get 3 bits
	RolBits = CallFunc(ENC_GetBits, (void *)3, 0, 0, 0);
	if(RolBits < 0)
		return RolBits;

	//get 4 bytes
	RolBytes = CallFunc(ENC_GetBits, (void *)4, 0, 0, 0);
	if(RolBytes < 0)
		return RolBytes;

	//increment by 1
	RolBits++;
	RolBytes++;

	//make sure rolbytes doesn't go past the end of the check buffer
	if(RolBytes + CheckOffset >= sizeof(CheckBuffer))
		return INVALID_STATE;

	//rotate the needed number of bytes by the specified number of bits
	FirstByte = CheckBuffer[CheckOffset];
	for(i = 0; i <= RolBytes - 2; i++) {
		CurBytes = __builtin_bswap16(*(uint16_t *)&CheckBuffer[CheckOffset + i]);
		CheckBuffer[CheckOffset + i] = (CurBytes << RolBits) >> 8;
	}

	//now fix up the last byte
	CurBytes = ((uint16_t)CheckBuffer[CheckOffset + i] << 8) | FirstByte;
	CheckBuffer[CheckOffset + i] = (CurBytes << RolBits) >> 8;
	return 1;
}

ENCRYPT(Add)
{
	int8_t Bits;

	//get 3 bits
	Bits = CallFunc(ENC_GetBits, (void *)3, 0, 0, 0);
	if(Bits < 0)
		return Bits;

	CheckBuffer[CheckOffset] += Bits + 1;
	return 1;
}

ENCRYPT(Sub)
{
	int8_t Bits;

	//get 3 bits
	Bits = CallFunc(ENC_GetBits, (void *)3, 0, 0, 0);
	if(Bits < 0)
		return Bits;

	CheckBuffer[CheckOffset] -= (Bits + 1);
	return 1;
}

ENCRYPT(And)
{
	int8_t Bits;

	//get 4 bits
	Bits = CallFunc(ENC_GetBits, (void *)4, 0, 0, 0);
	if(Bits < 0)
		return Bits;

	CheckBuffer[CheckOffset] &= Bits;
	return 1;
}

ENCRYPT(BitFlip)
{
	CheckBuffer[CheckOffset] = ~CheckBuffer[CheckOffset];
	return 1;
}