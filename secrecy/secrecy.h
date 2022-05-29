#ifndef SECRECY_H
#define SECRECY_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "aes.h"

typedef enum SecrecyErrorEnum {
	SECRECY_ERROR_COUNT = -7,
	INVALID_STATE = -6,
	INVALID_VERIFY_LEN = -5,
	INVALID_HASH = -4,
	INVALID_LENGTH = 3,
	INVALID_DECRYPT_BLOCK = -2,
	INVALID_PADDING = -1
} SecrecyErrorEnum;

typedef uint64_t (*EncryptedFunc)(void *Param1, void *Param2, void *Param3, void *Param4);

typedef struct EncryptFuncDataStruct {
	EncryptedFunc Func;
	uint32_t FuncLen[2];
	uint64_t IV[2];
} EncryptFuncDataStruct;

#define ENCRYPT(func) ENC_ ## func,
typedef enum EncryptedFuncEnum {
	#include "encrypted_funcs.h"
	ENCRYPTED_FUNC_COUNT
} EncryptedFuncEnum;
#undef ENCRYPT

uint32_t EncryptBlock(const uint8_t *Key, uint8_t *buffer, uint32_t buflen, uint8_t *IV);
uint32_t DecryptBlock(const uint8_t *Key, uint8_t *buffer, uint32_t buflen, uint8_t *IV);
int DecryptFunc(EncryptedFuncEnum Func);
void EncryptFunc(EncryptedFuncEnum Func);

uint64_t CallFunc(EncryptedFuncEnum Func, void *Param1, void *Param2, void *Param3, void *Param4);

extern EncryptFuncDataStruct EncryptedFuncs[];
extern uint8_t KeyData[];

#define ENCRYPT(func) uint64_t func(void *Param1, void *Param2, void *Param3, void *Param4);
#include "encrypted_funcs.h"
#undef ENCRYPT

#define ENCRYPT(func) __attribute__((noinline, section(".text.enc." #func))) uint64_t func(void *Param1, void *Param2, void *Param3, void *Param4)

typedef struct EncryptStrStruct
{
	uint8_t Size;
	char Str[];
} EncryptStrStruct;

//define a struct per string allowing us to dictate the size of the data and pad out for encryption without wasting space
#define DEFINE_ENCRYPT_STR(name, str) \
	typedef struct EncryptStrStruct_ ## name \
	{ \
		uint8_t Size; \
		char Str[sizeof(str)]; \
		uint8_t padding[AES_BLOCKLEN - (sizeof(str) & 0xf)]; \
	} EncryptStrStruct_ ## name; \
	__attribute__((section(".data.enc"), aligned(1))) const EncryptStrStruct_ ## name name = {sizeof(str), str};

	//__attribute__((section(".data.enc"))) EncryptStrStruct name = {sizeof(str), str};

#define DECRYPT_STR(name) CallFunc(ENC_DecryptStr, (void *)&name, 0, 0, 0)
#define ENCRYPT_STR(name) CallFunc(ENC_EncryptStr, (void *)&name, 0, 0, 0)
#define ENCRYPT_STR_REF(name) (*(EncryptStrStruct *)&name).Str

//we use puts everywhere, simple cheat to make sure data is sent
#define puts(x) {puts(x); fflush(stdout);}

#endif