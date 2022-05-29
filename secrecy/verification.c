#include <stdio.h>
#include "secrecy.h"

uint8_t VerificationData[AES_BLOCKLEN - 1];
uint8_t Identity[100];
uint64_t IdentityLen;

extern EncryptStrStruct CongratsStr;
extern EncryptStrStruct GetIdentityStr;
extern EncryptStrStruct GetVerificationStr;
extern EncryptStrStruct VerificationSucceededStr;

ENCRYPT(GetIdentity)
{
	uint8_t AlteredIdentity[AES_BLOCKLEN];
	uint8_t InputIV[(AES_BLOCKLEN * 2) + 2];
	int CurPos;
	int ReadLen;
	uint8_t SpecialChar;

	//ask for the identity
	CallFunc(ENC_Memset, Identity, 0, (void *)sizeof(Identity), 0);
	IdentityLen = CallFunc(ENC_GetData, (void *)&GetIdentityStr, Identity, (void *)sizeof(Identity), 0);
	if(!IdentityLen) {
		return -1;
	}

	//ask for the verification string
	ReadLen = CallFunc(ENC_GetData, (void *)&GetVerificationStr, InputIV, (void *)sizeof(InputIV), 0);
	if(!ReadLen) {
		return -1;
	}

	//convert the hash
	if(!CallFunc(ENC_ConvertHash, InputIV, (void *)(uint64_t)ReadLen, 0, 0)) {
		CallFunc(ENC_DisplayError, (void *)INVALID_HASH, 0, 0, 0);
		return 0;
	}

	//make sure the rest of the data is null bytes
	ReadLen >>= 1;
	CallFunc(ENC_Memset, &InputIV[ReadLen], 0, (void *)(sizeof(InputIV) - ReadLen), 0);

	if(ReadLen != AES_BLOCKLEN) {
		CallFunc(ENC_DisplayError, (void *)INVALID_HASH, 0, 0, 0);
		return 0;
	}

	//generate a block size value from the input identity
	CallFunc(ENC_Memcpy, AlteredIdentity, Identity, (void *)sizeof(AlteredIdentity), 0);
	for(ReadLen = sizeof(AlteredIdentity), CurPos = 0; ReadLen < IdentityLen; ReadLen++, CurPos = (CurPos + 1) % sizeof(AlteredIdentity)) {
		AlteredIdentity[CurPos] = (AlteredIdentity[CurPos] ^ Identity[ReadLen]) + ReadLen;
	}

	//BUG: oops, we allowing a padding attack resulting in formation leakage
	ReadLen = DecryptBlock(KeyData, AlteredIdentity, sizeof(AlteredIdentity), InputIV);
	if(ReadLen < 0) {
		CallFunc(ENC_DisplayError, (void *)(uint64_t)ReadLen, 0, 0, 0);
		return 0;
	}
	else if(ReadLen != sizeof(VerificationData)) {
		CallFunc(ENC_DisplayError, (void *)INVALID_VERIFY_LEN, 0, 0, 0);
		return 0;
	}

	//copy the result into memory
	CallFunc(ENC_Memcpy, VerificationData, AlteredIdentity, (void *)sizeof(VerificationData), 0);
	return 1;
}

ENCRYPT(CheckVerificationCode)
{
	uint8_t Val = 0;

	//see if the state machine is acceptable, all bytes added should be 0 for the 15 bytes
	for(int i = 0; i < sizeof(VerificationData); i++) {
		Val += VerificationData[i];
	}

	//if we have a value then the state machine didn't validate
	if(Val) {
		CallFunc(ENC_DisplayError, (void *)INVALID_STATE, 0, 0, 0);
		return 0;
	}

	//run the state machine code
	if(CallFunc(ENC_RunStateMachine, VerificationData, Identity, (void *)IdentityLen, 0) != 1) {
		CallFunc(ENC_DisplayError, (void *)INVALID_STATE, 0, 0, 0);
		return 0;
	}

	return 1;
}

ENCRYPT(Congrats)
{
	DECRYPT_STR(VerificationSucceededStr);
	puts(ENCRYPT_STR_REF(VerificationSucceededStr));
	ENCRYPT_STR(VerificationSucceededStr);

	DECRYPT_STR(CongratsStr);
	puts(ENCRYPT_STR_REF(CongratsStr));
	ENCRYPT_STR(CongratsStr);
}