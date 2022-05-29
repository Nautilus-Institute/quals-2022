#include "secrecy.h"

#undef ENCRYPT
#define ENCRYPT(func) {func, {0, 0}, {0, 0}},
EncryptFuncDataStruct EncryptedFuncs[] = {
	#include "encrypted_funcs.h"
	{0, {0, 0}, {0, 0}}
};