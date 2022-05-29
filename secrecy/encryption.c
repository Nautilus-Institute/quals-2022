#include <stdio.h>
#include <string.h>
#include "secrecy.h"
#include "aes.h"
#include "crc.h"
#include "firstdecodeoffset.h"

//note, these two keys are altered in 2 places during init
uint8_t FuncKeyData[AES_BLOCKLEN] = {0xe1, 0x8c, 0x9b, 0x4d, 0x60, 0xb2, 0xaf, 0x4a, 0x4a, 0x1f, 0xf7, 0x3f, 0xc5, 0x0d, 0x62, 0x5b};
uint8_t StrKeyData[AES_BLOCKLEN] = {0xcb, 0x52, 0xda, 0x85, 0xc5, 0x79, 0xd4, 0x35, 0x95, 0xd5, 0x03, 0x60, 0x75, 0x3e, 0x99, 0xb2};

//do this to make it less obvious that the value is fixed
uint32_t EncFuncTableCRC = 0x11223344;

__attribute__((constructor)) static void AlterKeyData()
{
	FuncKeyData[5] = 0xd2;
	StrKeyData[8] = 0x85;
}

uint64_t ConfirmFuncTable(uint64_t Func)
{
	int32_t gen_crc;
	uint64_t RIP;
	uint64_t TempAddr;

	//calculate our RIP
	RIP = (uint64_t)EncryptedFuncs[0].Func - FIRST_DECODE_OFFSET;

	DecryptFunc(ENC_crc32);

	//generate a crc of the EncryptedFuncs table
	gen_crc = 0;
	for(int i = 0; i < ENCRYPTED_FUNC_COUNT; i++) {
		//hash the base address
		TempAddr = (uint64_t)EncryptedFuncs[i].Func - RIP;
		gen_crc = crc32((void *)(uint64_t)gen_crc, (uint8_t *)&TempAddr, (void *)(sizeof(TempAddr)), 0);
		gen_crc = crc32((void *)(uint64_t)gen_crc, (uint8_t *)&EncryptedFuncs[i].FuncLen, (void *)(sizeof(EncryptFuncDataStruct) - sizeof(TempAddr)), 0);
	}

	EncryptFunc(ENC_crc32);

	return (gen_crc == EncFuncTableCRC);
}

uint64_t CallFunc(EncryptedFuncEnum Func, void *Param1, void *Param2, void *Param3, void *Param4)
{
	uint64_t Ret;

	if(Func > ENCRYPTED_FUNC_COUNT)
		return 0;

	if((Func > ENCRYPTION_START_CRC_CHECK) && !ConfirmFuncTable(Func))
		exit(0);

	//decrypt the function
	if(!DecryptFunc(Func)) {
		exit(0);
	}

	//call the function
	Ret = EncryptedFuncs[Func].Func(Param1, Param2, Param3, Param4);

	//encrypt the function
	EncryptFunc(Func);

	return Ret;
}

int DecryptFunc(EncryptedFuncEnum Func)
{
	int64_t enc_funclen, dec_funclen, orig_funclen;
	int32_t gen_crc;

	//decrypt an encrypted function

	//get the original function length by looking up the CRCs
	if(Func >= ENCRYPTION_START_CRC_CHECK) {
		for(enc_funclen = 0, dec_funclen = 0x100; enc_funclen < 0x100; enc_funclen++) {
			//if we see the second byte then set it
			if(EncryptedFuncs[Func].FuncLen[1] == crc_table[enc_funclen])
				dec_funclen = enc_funclen;

			//if we found the lower then stop looking
			if(EncryptedFuncs[Func].FuncLen[0] == crc_table[enc_funclen])
				break;

		}

		//got low byte, get second byte if it wasn't found
		if(dec_funclen == 0x100) {
			for(dec_funclen = enc_funclen + 1; dec_funclen < 0x100; dec_funclen++) {
				if(EncryptedFuncs[Func].FuncLen[1] == crc_table[dec_funclen])
					break;
			}
		}

	}
	else {
		enc_funclen = EncryptedFuncs[Func].FuncLen[0] & 0xff;
		dec_funclen = EncryptedFuncs[Func].FuncLen[1] & 0xff;
	}

	//now combine
	orig_funclen = (dec_funclen << 8) | enc_funclen;

	//calculate the encrypted size
	enc_funclen = orig_funclen + (AES_BLOCKLEN - (orig_funclen % AES_BLOCKLEN));

	//decrypt
	dec_funclen = DecryptBlock(FuncKeyData, (uint8_t *)EncryptedFuncs[Func].Func, enc_funclen, (uint8_t *)EncryptedFuncs[Func].IV);
	if(dec_funclen != orig_funclen) {
		return 0;
	}

	//if this is the very first couple of functions in the list then it is the first decode function, don't crc check as the crc code isn't generated yet
	if(Func < ENCRYPTION_START_CRC_CHECK)
		return 1;

	//last 4 bytes are CRC data, confirm CRC
	gen_crc = CallFunc(ENC_crc32, 0, (uint8_t *)EncryptedFuncs[Func].Func, (void *)(dec_funclen - 4), 0);
	if(gen_crc == *(uint32_t *)&((uint8_t *)EncryptedFuncs[Func].Func)[dec_funclen-4])
		return 1;

	return 0;
}

void EncryptFunc(EncryptedFuncEnum Func)
{
	int64_t funclen1, funclen2, orig_funclen;

	//encrypt an decrypted function

	if(Func >= ENCRYPTION_START_CRC_CHECK) {
		//get the original function length by looking up the CRCs
		for(funclen1 = 0, funclen2 = 0x100; funclen1 < 0x100; funclen1++) {
			//if we see the second byte then set it
			if(EncryptedFuncs[Func].FuncLen[1] == crc_table[funclen1])
				funclen2 = funclen1;

			//if we found the lower then stop looking
			if(EncryptedFuncs[Func].FuncLen[0] == crc_table[funclen1])
				break;

		}

		//got low byte, get second byte if it wasn't found
		if(funclen2 == 0x100) {
			for(funclen2 = funclen1 + 1; funclen2 < 0x100; funclen2++) {
				if(EncryptedFuncs[Func].FuncLen[1] == crc_table[funclen2])
					break;
			}
		}

	}
	else {
		funclen1 = EncryptedFuncs[Func].FuncLen[0] & 0xff;
		funclen2 = EncryptedFuncs[Func].FuncLen[1] & 0xff;
	}

	//now combine
	orig_funclen = (funclen2 << 8) | funclen1;

	//encrypt data
	EncryptBlock(FuncKeyData, (uint8_t *)EncryptedFuncs[Func].Func, orig_funclen, (uint8_t *)EncryptedFuncs[Func].IV);
}

uint32_t EncryptBlock(const uint8_t *Key, uint8_t *buffer, uint32_t buflen, uint8_t *IV) {
	struct AES_ctx ctx;
	uint8_t PaddingLen;

	//add padding to the end of the buffer
	PaddingLen = AES_BLOCKLEN - (buflen % AES_BLOCKLEN);
	for(uint32_t i = PaddingLen; i > 0; i--)
		buffer[buflen+i-1] = PaddingLen;

	buflen += PaddingLen;

	//go through and decrypt a block
	AES_init_ctx_iv(&ctx, Key, IV);
	AES_CBC_encrypt_buffer(&ctx, buffer, buflen);

	return buflen;
}

uint32_t DecryptBlock(const uint8_t *Key, uint8_t *buffer, uint32_t buflen, uint8_t *IV) {
	struct AES_ctx ctx;
	uint8_t PaddingLen;
	uint8_t TempBuffer[32];

	if(buflen == 32) {
		memcpy(TempBuffer, buffer, sizeof(TempBuffer));
	}

	//go through and decrypt a block
	AES_init_ctx_iv(&ctx, Key, IV);
	AES_CBC_decrypt_buffer(&ctx, buffer, buflen);

	//now handle padding bytes
	PaddingLen = buffer[buflen-1];
	if(((uint8_t)(PaddingLen-1) >= AES_BLOCKLEN)) {
		return INVALID_DECRYPT_BLOCK;
	}
	else {
		//make sure all bytes match for padding allowing a padding oracle attack
		for(uint32_t i = PaddingLen; i; i--) {
			if(buffer[buflen-i] != PaddingLen)
				return INVALID_PADDING;
		}
	}

	return buflen - PaddingLen;
}

ENCRYPT(DecryptStr)
{
	EncryptStrStruct *StrData = (EncryptStrStruct *)Param1;
	int64_t dec_len;
	uint8_t StrIV[16];

	//decrypt a string
	dec_len = StrData->Size + (AES_BLOCKLEN - (StrData->Size % AES_BLOCKLEN));

	//copy the low byte of StrSize into the rest of StrIV
	memset(StrIV, StrData->Size & 0xff, sizeof(StrIV));

	//now decrypt the string
	dec_len = DecryptBlock(StrKeyData, StrData->Str, dec_len, StrIV);
	if(dec_len != StrData->Size) {
		return 0;
	}

	return 1;
}

ENCRYPT(EncryptStr)
{
	EncryptStrStruct *StrData = (EncryptStrStruct *)Param1;
	int64_t dec_len;
	uint8_t StrIV[16];

	//encrypt a string

	//copy the low byte of StrSize into the rest of StrIV
	memset(StrIV, StrData->Size & 0xff, sizeof(StrIV));

	//now encrypt the string
	EncryptBlock(StrKeyData, StrData->Str, StrData->Size, StrIV);

	return 1;
}

//create a .tm_clone_table area, we embed this into the .data area via a custom linker script
//so it doesn't show up as a separate section but in order to make use of this during load we need this section name
static uint64_t __TMC_END__[] __attribute__((used, section(".tm_clone_table"), aligned(sizeof(void*)))) = { };
static uint64_t __TMC_LIST__[] __attribute__((used, section(".tm_clone_table"), aligned(sizeof(void*)))) = { 0 };

void _ITM_registerTMCloneTable(void *ptr, void *size)
{
	FuncKeyData[1] = 0x6c;
	StrKeyData[3] = 0x05;
}