#include <stdio.h>
#include "secrecy.h"

DEFINE_ENCRYPT_STR(InvalidHashStr, "Invalid hash")
DEFINE_ENCRYPT_STR(InvalidDecryptBlockStr, "Invalid decrypt block")
DEFINE_ENCRYPT_STR(InvalidLengthStr, "Invalid block length")
DEFINE_ENCRYPT_STR(InvalidPaddingStr, "Invalid padding")
DEFINE_ENCRYPT_STR(InvalidVerifyStr, "Invalid verification length")
DEFINE_ENCRYPT_STR(InvalidStateStr, "Invalid verification state")
const void *ErrorMsgs[] = {&InvalidPaddingStr, &InvalidDecryptBlockStr, &InvalidLengthStr, &InvalidHashStr, &InvalidVerifyStr, &InvalidStateStr};

ENCRYPT(DisplayError)
{
	char *Str;
	uint64_t ErrorID = (uint64_t)Param1;
	EncryptStrStruct *ErrorMsg;

	ErrorID = (ErrorID * -1) - 1;
	if(ErrorID >= (SECRECY_ERROR_COUNT * -1))
		return 0;

	ErrorMsg = (EncryptStrStruct *)ErrorMsgs[ErrorID];
	DECRYPT_STR(*ErrorMsg);
	puts(ENCRYPT_STR_REF(*ErrorMsg));
	ENCRYPT_STR(*ErrorMsg);
	return 0;
}
