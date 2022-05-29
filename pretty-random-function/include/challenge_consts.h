#ifndef _CHAL_CONSTS_H__
#define _CHAL_CONSTS_H__

#include "openssl/ssl.h"
#include "wmmintrin.h"

#define NUM_SUBKEYS (20)


#define CMD_OFFSET              (6)
#define WORKER_THREADS          (5)
#define OBJ_OFFSET              (WORKER_THREADS+1)
#define THREAD_MASK             ((1<<(WORKER_THREADS+1))-1)
#ifndef DEBUG
#define POW_MASK                (0xfffffff)
#else
#define POW_MASK                (0xffff)
#endif

#define STACK_OVERFLOW_CMD      (1  << CMD_OFFSET)
#define CAT_OBJECT_CMD          (2  << CMD_OFFSET)
#define ALLOCATE_OBJECT_CMD     (3  << CMD_OFFSET)
#define FREE_OBJECT_CMD         (4  << CMD_OFFSET)
#define OTHER_CMD               (5  << CMD_OFFSET)
#define SETENV_CMD              (6  << CMD_OFFSET)
#define GETENV_CMD              (7  << CMD_OFFSET)
#define PRINT_OBJ_CMD           (8  << CMD_OFFSET)
#define LOCK_OBJ_CMD            (9  << CMD_OFFSET)
#define UNLOCK_OBJ_CMD          (10 << CMD_OFFSET)
#define GETENV_OBJ_CMD          (11 << CMD_OFFSET)
#define SETENV_OBJ_CMD          (12 << CMD_OFFSET)
#define HEX_ENCODE_CMD          (13 << CMD_OFFSET)
#define STORE_OBJ_CMD           (11)
#define GET_OBJ_CMD             (12)
#define RETURN_CMD              (9)

#define OBJ_TYPE_THREAD         (1 << OBJ_OFFSET)
#define OBJ_GET_ENV             (2 << OBJ_OFFSET)
#define OBJ_SET_ENV             (3 << OBJ_OFFSET)
#define OBJ_PRINT               (5 << OBJ_OFFSET)
#define OBJ_GET_ENV_OBJ         (6 << OBJ_OFFSET)
#define OBJ_SET_ENV_OBJ         (7 << OBJ_OFFSET)
#define OBJ_HEXENC              (8 << OBJ_OFFSET)
#define OBJ_LOCK_THREAD         (2)
#define OBJ_UNLOCK_THREAD       (3)
#define OBJ_DATA                (9)
#define OBJ_HEAD                (1)

#define WORKING_BUF_LEN (8192*2) // This is ssl_max_frag size
#define STACK_BUF_SIZE (WORKING_BUF_LEN + 0x100)

#define MAX_READ_ALLOW (1024*256)
#define MAX_WRITE_ALLOW (1024*256)
#define READ_INT_LEN (4)

#define END_VALUE "\x00\n\x98\xff\x20\t\xf1\xd0"
#define HMAC_KEY "\x01\x0a\x12\xf9~?\x8c^\x84|\xc2\x9d\xa8\x9a_\xc3\xe3\x1c\x01\x81\x15\x05:wq\xe1\x80\xcd\x8b{\xaf"
#define ID_LENGTH (32)
#define POW_CHAL_SIZE (32)
#define POW_CHAL_RESP_SIZE (POW_CHAL_SIZE + 16)
#define POW_CHAL_HARD (30)
#define NUM_THREADS (7)
#define THREAD_STACK_SIZE (8192*4)
#define BASE_OBJ_NUM (0x1a2b)


struct pthreadState {
    pthread_cond_t  workAvail;
    pthread_mutex_t waitingMutex;
	pthread_t threadId;
} ;

struct __attribute__((scalar_storage_order("big-endian"))) envStruct {
	char * name;
	char * value;
} ;

struct __attribute__((scalar_storage_order("big-endian"))) envObjStruct { // this keeps native order so talking to it is going to require not htonl 
    uint32_t nameObj;
    uint32_t valueObj;
} ;

 
struct __attribute__((scalar_storage_order("big-endian"))) twoNetworkInts
{
    uint32_t int1;
    uint32_t int2;
} __attribute__((__packed__)) ;

struct __attribute__((scalar_storage_order("big-endian"))) threeNetworkInts
{
    uint32_t int1;
    uint32_t int2;
    uint32_t int3;
} __attribute__((__packed__)) ;

struct __attribute__((scalar_storage_order("big-endian"))) storedObj 
{
    uint32_t length;
    uint32_t inUse;
    uint8_t  data[0];
} __attribute__((__packed__));

struct __attribute__((scalar_storage_order("big-endian"))) workObjStruct {
    void (*callFunc)(void *, void *);
    struct workObjStruct * next;
    struct workObjStruct * prev;
    struct workObjStruct * lastProcessed;
    __m128i cookie;
    void * args;
    SSL * ssl;
    uint64_t objType;
	uint64_t length;
    uint32_t objNum;
    struct pthreadState pts __attribute__((aligned(16)));
} __attribute__((__packed__));

typedef struct workObjStruct workObj;

#define SIL static inline __attribute__((always_inline))

#endif // _CHAL_CONSTS_H__