ENCRYPT(FirstDecode)
ENCRYPT(crc32)
ENCRYPT(GenerateCRCTable)

#define ENCRYPTION_START_CRC_CHECK 3

ENCRYPT(ConvertHash)
ENCRYPT(DecryptStr)
ENCRYPT(Rol)
ENCRYPT(ReadKeyData)
ENCRYPT(EncryptStr)
ENCRYPT(CheckVerificationCode)
ENCRYPT(GetIdentity)
ENCRYPT(RunStateMachine)
ENCRYPT(Add)
ENCRYPT(GetBits)
ENCRYPT(XorData)
ENCRYPT(GetData)
ENCRYPT(WelcomeMsg)
ENCRYPT(IncPointer)
ENCRYPT(And)
ENCRYPT(Memset)
ENCRYPT(BitFlip)
ENCRYPT(InitStateMachine)
ENCRYPT(Memcpy)
ENCRYPT(DisplayError)
ENCRYPT(Sub)

//the IV of this last function is the target of an overwrite for control
ENCRYPT(Congrats)