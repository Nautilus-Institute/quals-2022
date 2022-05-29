#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "secrecy.h"
#include "aes.h"
#include "crc.h"

uint8_t KeyData[AES_BLOCKLEN];

DEFINE_ENCRYPT_STR(NoKeyDataStr, "Unable to find key data, exiting\n");
DEFINE_ENCRYPT_STR(BadKeyDataStr, "Key data not long enough, exiting\n");
DEFINE_ENCRYPT_STR(WelcomeMsgStr, "Welcome to secrey, the key validation software for Nautilus Institute");
DEFINE_ENCRYPT_STR(KeyDataStr, "keydata");
DEFINE_ENCRYPT_STR(GetIdentityStr, "Input your identity:");
DEFINE_ENCRYPT_STR(GetVerificationStr, "Input your verification code:");
DEFINE_ENCRYPT_STR(VerificationSucceededStr, "Verification Succeeded");
DEFINE_ENCRYPT_STR(CongratsStr, "Congratulations on validating your key, please submit it to the score server for your points");

ENCRYPT(ReadKeyData)
{
	int fd;
	int readlen;

	DECRYPT_STR(KeyDataStr);
	fd = open(ENCRYPT_STR_REF(KeyDataStr), O_RDONLY);
	ENCRYPT_STR(KeyDataStr);
	if(fd < 0) {
		DECRYPT_STR(NoKeyDataStr);
		puts(ENCRYPT_STR_REF(NoKeyDataStr));
		return 0;
	}

	readlen = read(fd, KeyData, AES_BLOCKLEN);
	close(fd);
	if(readlen != AES_BLOCKLEN) {
		DECRYPT_STR(BadKeyDataStr);
		puts(ENCRYPT_STR_REF(BadKeyDataStr));
		return 0;
	}

	return 1;
}

ENCRYPT(WelcomeMsg)
{
	DECRYPT_STR(WelcomeMsgStr);
	puts(ENCRYPT_STR_REF(WelcomeMsgStr));
	ENCRYPT_STR(WelcomeMsgStr);
	return 0;
}

ENCRYPT(FirstDecode)
{
	int64_t Ret;

	CallFunc(ENC_GenerateCRCTable,0,0,0,0);

	if(!CallFunc(ENC_ReadKeyData,0,0,0,0))
		return -1;

	CallFunc(ENC_WelcomeMsg,0,0,0,0);

	while(1) {
		Ret = CallFunc(ENC_GetIdentity,0,0,0,0);
		if(Ret == -1)
			break;

		if(Ret && (CallFunc(ENC_CheckVerificationCode, 0, 0, 0, 0) == 1)) {
			CallFunc(ENC_Congrats, 0, 0, 0, 0);
			break;
		}
	}
	return 0;
}

ENCRYPT(ConvertHash)
{
	uint64_t ReadLen = (uint64_t)Param2;
	uint8_t *Data = (uint8_t *)Param1;
	uint8_t *WriteData = Data;
	uint8_t NewChar1, NewChar2;

	//convert a hash to something more useful

	//if not a multiple of 2 then fail
	if(ReadLen & 1)
		return 0;

	//all good on length, convert
	while(ReadLen) {
		NewChar1 = Data[0] - 0x30;
		NewChar2 = Data[1] - 0x30;

		//if not a number then try A then a
		if(NewChar1 > 9)   NewChar1 -= 7;
		if(NewChar1 > 0xf) NewChar1 -= 0x20;
		if(NewChar1 > 0xf) return 0;
	
		if(NewChar2 > 9)   NewChar2 -= 7;
		if(NewChar2 > 0xf) NewChar2 -= 0x20;
		if(NewChar2 > 0xf) return 0;

		//write the char out
		*WriteData = (NewChar1 << 4) | NewChar2;

		//advance
		ReadLen -= 2;
		Data += 2;
		WriteData++;
	}

	//return how many bytes we decoded
	return (uint64_t)Param2 >> 1;
}

ENCRYPT(GetData)
{
	int ReadLen;
	int DataLen;
	EncryptStrStruct *Str = (EncryptStrStruct *)Param1;
	uint8_t *Buffer = (uint8_t *)Param2;

	//ask for the identity
	DECRYPT_STR(*Str);
	puts(ENCRYPT_STR_REF(*Str));
	ENCRYPT_STR(*Str);
	Buffer[0] = 0;

	for(DataLen = 0; DataLen < (uint64_t)Param3 - 1; DataLen++) {
		ReadLen = read(0, &Buffer[DataLen], 1);
		if(ReadLen <= 0) {
			return 0;
		}

		if(Buffer[DataLen] == '\n') {
			Buffer[DataLen] = 0;
			break;
		}
	}

	Buffer[DataLen] = 0;
	return DataLen;
}

ENCRYPT(Memcpy)
{
	uint8_t *Out = (uint8_t *)Param1;
	uint8_t *In = (uint8_t *)Param2;
	uint64_t Len = (uint64_t)Param3;

	while(Len) {
		*Out = *In;
		Out++;
		In++;
		Len--;
	}

	return 0;
}

ENCRYPT(Memset)
{
	uint8_t *Out = (uint8_t *)Param1;
	uint8_t In = (uint8_t)(uint64_t)Param2;
	uint64_t Len = (uint64_t)Param3;

	while(Len) {
		*Out = In;
		Out++;
		Len--;
	}

	return 0;
}

int main(int argc, char **argv)
{
	return CallFunc(ENC_FirstDecode,0,0,0,0);
}