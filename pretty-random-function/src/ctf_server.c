/*
 * The goal of this challenge is look like a proprietary protocol on an internet of shit device
 * This challenge uses SSL with a preshared key based on your identity
 * It also writes 4 random bytes after each time you do anything so you need to modify most ssl libraries in order to work
 * There is an unexploitable stack overflow memcpy
 * The pointer leak is due to when reading in the setenv command it won't null terminate this means on get env you can read out the item again
 * The whole thing is one big static binary
 * I'm trying to inline as much as possible because i hate inlining
 * Due to it being an SSL challenge I also require a 30 byte proof of work, I debated making this an HMAC but i figured that was too mean, where this POW has solvers online from previous challenges
 * I don't know what the exploitable bug is going to be yet, i really just want to make it a dumb command injection at the end but i don't know how to do that yet
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <byteswap.h>
#include <sys/types.h>
#include <sys/wait.h>


#include "immintrin.h"
#include "smmintrin.h"
#include "wmmintrin.h"
#include "openssl/rand.h"
#include "openssl/crypto.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "ssl_helper.h"
#include "challenge_consts.h"



static int numObjects = 0;
static workObj headPointer __attribute__ ((aligned (32)));
static __m128i aes_ks[NUM_SUBKEYS];



static int doStackOverflow(SSL *ssl, uint8_t * buffer, size_t bufLen);
static inline SSL_CTX *create_context();
void getEnvFunc(workObj * base);
void setEnvFunc(workObj * base);
void printfn(workObj * obj, workObj * cThread);
void lockThread(workObj * obj);
void unlockThread(workObj * obj);
void writeResp(SSL * ssl, uint64_t num, uint64_t num2);
void getData(workObj * obj, workObj * cThread);
static void aes128_load_key(uint8_t *enc_key);
static __m128i generateCookie(workObj * wo);
void getEnvObjFunc(workObj * base);
void setEnvObjFunc(workObj * base);
static void fixLast(void);
static workObj * doGetCommand(int objNum, uint32_t targetObj,  SSL * ssl);
void hexEncodeFunc(workObj * wo, workObj * cThread);


static inline __attribute__((always_inline))  SSL_CTX *create_context()
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        PRINT("SSL ctx new failed\n");
        return NULL;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);

    return ctx;
}

void printObj(workObj * wo)
{
#ifdef DEBUG
    PRINT("CallFunc = %p\n", (void*)wo->callFunc);
    PRINT("Next     = %p\n", (void*)wo->next);
    PRINT("Prev     = %p\n", (void*)wo->prev);
    PRINT("last     = %p\n", (void*)wo->lastProcessed);
    PRINT("Args     = %p\n", (void*)wo->args);
    PRINT("type     = %016lx\n", wo->objType);
    PRINT("num      = %08x\n", wo->objNum);
#endif
}


void doThread(workObj * cThread)
{
    while(1)
    {
        workObj * toProcess;

        if(cThread->lastProcessed->next == NULL)
        {
            sleepThread();
            continue;
        }

        toProcess = cThread->lastProcessed->next;
        pthread_mutex_lock(&cThread->pts.waitingMutex);// there's a deadlock that can happen if you get the lock via a different thread

        while(cThread->lastProcessed->next != NULL){
           
            __m128i cookieValue = generateCookie(toProcess);
            __m128i eqValue = _mm_xor_si128(cookieValue, toProcess->cookie);
            int decryptedProperly = _mm_test_all_zeros(eqValue, eqValue);
            if(decryptedProperly != 1)
            {
                dumpBuf("EQ   ",(void*) &eqValue, 16);
                dumpBuf("COOK ",(void*) &cookieValue, 16);
                dumpBuf("ECOOK",(void*) &toProcess->cookie, 16);
                exit(0);
            }

            if(toProcess->callFunc == NULL)
            {
                ;
            }
            else if(((toProcess->objNum ^ cThread->objNum) & THREAD_MASK) == 0 )
            {
                pthread_mutex_lock(&toProcess->pts.waitingMutex);
                PRINT("Processing %p on %d\n", toProcess,  cThread->objNum);
                toProcess->callFunc(toProcess,cThread);
                pthread_mutex_unlock(&toProcess->pts.waitingMutex);
            }
            else if((toProcess->objNum & THREAD_MASK) == THREAD_MASK)
            {
                pthread_mutex_lock(&toProcess->pts.waitingMutex);
                PRINT("Processing ALL %p on %d\n", toProcess,  cThread->objNum);
                toProcess->callFunc(cThread, toProcess);
                pthread_mutex_unlock(&toProcess->pts.waitingMutex);
            }

            cThread->lastProcessed = toProcess;
            toProcess = toProcess->next;
        }
        pthread_mutex_unlock(&cThread->pts.waitingMutex);
    }
}


static inline __attribute__((always_inline))
 SSL *create_ssl_object(SSL_CTX *ctx, int lfd)
{
    SSL *ssl;
    int fd;
    pid_t child;
    int wstatus;
    while(1)
    {
        fd = do_tcp_accept(lfd);
        child = fork();
        if(child == 0)
        {
            child = fork();
            if(child == 0)
            {
                child = fork();
                if(child == 0)
                {
                    break;
                }
                exit(0);
            }
            exit(0);
        }
        waitpid(child, &wstatus, WUNTRACED);
        if(child < 0)
        {
            exit(-1);
        }
        close(fd);
        continue;
    }


    alarm(60);  // This actually is needed for the POW challenge, may need to be two minutes
    if (fd < 0) {
        PRINT("TCP connection establishment failed\n");
        return NULL;
    }

    socketFD = fd;

    if(doPOWChallenge() != 1)
    {
        exit(0);
    }

    alarm(60); // Give them a minute to do the challenge, try to avoid deadlock

    ssl = SSL_new(ctx);
    if (!ssl) {
        PRINT("SSL object creation failed\n");
        return NULL;
    }

    SSL_set_fd(ssl, fd);
    SSL_set_psk_server_callback(ssl, get_psk_key);
    PRINT("SSL object creation finished\n");
    return ssl;
}

static inline __attribute__((always_inline))
void createThread(SSL * ssl)
{
    int threadRet;
    pthread_attr_t attr;
    uintptr_t mod16Ptr = (uintptr_t) malloc(sizeof(workObj)*WORKER_THREADS + 16) ;
    workObj * thread_contexts = (workObj *)(mod16Ptr + (16 - (mod16Ptr % 16)));
    
    headPointer.next = &thread_contexts[0];

    thread_contexts[0].prev = &headPointer;
    thread_contexts[WORKER_THREADS-1].next = NULL;

    threadRet = pthread_attr_init(&attr);
    if (threadRet != 0)
    {
        exit(0);
    }

    threadRet = pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    if (threadRet != 0)
    {
        exit(0);
    }

    for(int iter =0; iter < WORKER_THREADS; iter++){

        initializeWorkObj(&thread_contexts[iter], ssl, NULL,  (1 << iter), OBJ_TYPE_THREAD);
        thread_contexts[iter].lastProcessed = &thread_contexts[WORKER_THREADS-1];
        if(iter > 0)
        {
            thread_contexts[iter].prev = &thread_contexts[iter-1];
            thread_contexts[iter-1].next = &thread_contexts[iter];
        }
        numObjects++;

        pthread_create(&thread_contexts[iter].pts.threadId, &attr,
            (void *)&doThread, &thread_contexts[iter]);
    }

    pthread_attr_destroy(&attr);

}

static inline __attribute__((always_inline))
workObj * doPrintfn(int objNum, uint64_t arg, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    if(wo == NULL)
    {
        exit(0);
    }

    initializeWorkObj(wo, ssl, printfn, objNum, OBJ_PRINT);
    wo->args = (void *) arg;
    return wo;
}


static inline __attribute__((always_inline))
workObj * doHexEncode(int objNum, uint64_t arg, uint32_t outputLength, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    if(wo == NULL)
    {
        exit(0);
    }

    initializeWorkObj(wo, ssl, hexEncodeFunc, objNum, OBJ_HEXENC);
    wo->args = (void *) arg;
    wo->length = outputLength;
    return wo;
}


static inline __attribute__((always_inline))
workObj * doGetEnvObj(int objNum, struct envObjStruct * ev, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));

    if(wo == NULL)
    {
        exit(0);
    }

    initializeWorkObj(wo, ssl, getEnvObjFunc, objNum, OBJ_GET_ENV_OBJ);
    wo->args = ev;
    wo->length = sizeof(struct envObjStruct);

    return wo;
}

static inline __attribute__((always_inline))
workObj * doSetEnvObj(int objNum, struct envObjStruct * ev, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));

    if(wo == NULL)
    {
        exit(0);
    }

    initializeWorkObj(wo, ssl, setEnvObjFunc, objNum, OBJ_SET_ENV_OBJ);
    wo->args = ev;
    wo->length = sizeof(struct envObjStruct);

    return wo;
}


static inline __attribute__((always_inline))
workObj * doGetEnv(int objNum, struct envStruct * ev, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    
    if(wo == NULL)
    {
        exit(0);
    }

    initializeWorkObj(wo, ssl, getEnvFunc, objNum, OBJ_GET_ENV);

    wo->args = ev;
    wo->length = sizeof(struct envStruct);

    return wo;
}


static inline __attribute__((always_inline))
workObj * doSetEnv(int objNum, struct envStruct * ev, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    if(wo == NULL)
    {
        exit(0);
    }
    initializeWorkObj(wo, ssl, setEnvFunc, objNum, OBJ_SET_ENV);

    wo->args = ev;
    wo->length = sizeof(struct envStruct);

    return wo;
}

//Stack cookies are on so this is just a red herring
static __attribute__((noinline)) int doStackOverflow(SSL *ssl,
    uint8_t * buffer, size_t bufLen)
{
    uint8_t * stackBuf[STACK_BUF_SIZE];
    int bufLength = readInt(buffer);
    memcpy(stackBuf, &buffer[4], bufLength);
    writeResp(ssl, STACK_OVERFLOW_CMD, bufLength);
    return bufLength;
}

static inline __attribute__((always_inline))
workObj * doStoreCommand(int objNum, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    initializeWorkObj(wo, ssl, NULL, objNum, OBJ_DATA);
    return wo;
}

static inline __attribute__((always_inline))
workObj * doGetCommand(int objNum, uint32_t targetObj,  SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    uint64_t targ = targetObj; // This is to promote target object up so that this way gcc doesn't mind the void * cast
    initializeWorkObj(wo, ssl, getData, objNum, OBJ_DATA);
    wo->args = (void *) targ;
    wo->length = sizeof(targetObj);
    return wo;
}


static inline __attribute__((always_inline))
workObj * doLockThread(int objNum, uintptr_t targ, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    initializeWorkObj(wo, ssl, lockThread, objNum, OBJ_LOCK_THREAD);
    wo->args = (void*) targ;
    wo->length = sizeof(targ);
    return wo;
}

static inline __attribute__((always_inline))
workObj * doUnlockThread(int objNum, uintptr_t targ, SSL * ssl)
{
    workObj * wo = malloc(sizeof(workObj));
    initializeWorkObj(wo, ssl, unlockThread, objNum, OBJ_UNLOCK_THREAD);
    wo->args = (void*) targ;
    wo->length = sizeof(targ);
    return wo;
}

static inline __attribute__((always_inline))
void generatePassword(void)
{
    uint8_t basePassword[16];
    RAND_bytes(basePassword, 16);
    __builtin_ia32_rdrand64_step((void *)basePassword);
    aes128_load_key(basePassword);
}

static inline __attribute__((always_inline))
void lockAllThreads(void)
{
    PRINT("Locking All threads\n");
    workObj * processing = &headPointer;
    while(processing != NULL)
    {
        if(processing->objType == OBJ_TYPE_THREAD)
        {
            pthread_mutex_lock(&processing->pts.waitingMutex);
        }
        processing = processing->next;
    }
}

static inline __attribute__((always_inline))
void unlockAllThreads(void)
{
    PRINT("Unlocking All threads\n");
    workObj * processing = &headPointer;
    while(processing != NULL)
    {
        if(processing->objType == OBJ_TYPE_THREAD)
        {
            pthread_mutex_unlock(&processing->pts.waitingMutex);
        }
        processing = processing->next;
    }
}

static inline __attribute__((always_inline))
int testCookie(workObj * wo)
{
    __m128i cookieValue;
    __m128i eqValue;
    cookieValue = generateCookie(wo);
    eqValue = _mm_xor_si128(cookieValue, wo->cookie);
    return _mm_test_all_zeros(eqValue, eqValue);
}



static inline __attribute__((always_inline))
int freeObj(uint32_t whichObj, workObj ** lastObj)
{
    int iter = 0;
    workObj * objToRemove = headPointer.next;

    PRINT("Freeing object %d of %d\n", whichObj, numObjects);

    if(whichObj > (numObjects-2))
    {
        return -EINVAL;
    }

    while(iter < whichObj)// this leads to an off by on null deref
    {
        objToRemove = objToRemove->next;
        iter++;
    }


    if(testCookie(objToRemove) != 1)
    {
        return -EKEYREJECTED;
    }

    if(objToRemove->objType == OBJ_TYPE_THREAD)
    {
        return -EINVAL;
    }

    if(objToRemove->next != NULL)
    {
        objToRemove->prev->next = objToRemove->next;
        objToRemove->next->prev = objToRemove->prev;
    }
    else
    {
        lockAllThreads();
        objToRemove->prev->next = NULL;
        *lastObj = (*lastObj)->prev;
        fixLast();
        unlockAllThreads();
    }

    if(objToRemove->args != NULL)
    {
        if(objToRemove->objType == OBJ_DATA)
        {
            free(objToRemove->args);
        }
        else if(objToRemove->objType == OBJ_GET_ENV ||
            objToRemove->objType == OBJ_SET_ENV)
        {
            struct envStruct * ev= objToRemove->args;
            if(ev->name != NULL)
            {
                free(ev->name);
            }
            if(ev->value != NULL)
            {
                free(ev->value);
            }
            free(ev);
        }
        else if(objToRemove->objType == OBJ_GET_ENV_OBJ ||
            objToRemove->objType == OBJ_SET_ENV_OBJ)
        {
            free(objToRemove->args);
        }
    }

    memset(objToRemove, 0, sizeof(workObj));
    free(objToRemove);
    atomic_store(&numObjects, numObjects-1);
    return 1;
}

static inline __attribute__((always_inline))
void fixLast(void)
{
    workObj * processing = &headPointer;
    while(processing != NULL)
    {
        if(processing->objType == OBJ_TYPE_THREAD)
        {
            if(processing->lastProcessed->next == NULL)
            {
                processing->lastProcessed = processing->lastProcessed->prev;
            }
        }
        processing = processing->next;
    }
}


// This is going to be where we start doing things
int doChallenge(SSL *ssl)
{
    int cObjects = 1;
    workObj * lastObj = &headPointer;
    uint8_t buffer[WORKING_BUF_LEN];
    memset(buffer,0, WORKING_BUF_LEN);

    generatePassword();

    initializeWorkObj(&headPointer, ssl, NULL, 0, OBJ_HEAD);

    atomic_store(&numObjects, cObjects);

    createThread(ssl);

    while(lastObj->next != NULL)
    {
        lastObj = lastObj->next;
    }

    while(1)
    {
        int amountRead = sslReadData(ssl, buffer, WORKING_BUF_LEN);
        int processedIdx = 0;
        workObj * toDo = NULL;
        PRINT("Read %d bytes\n", amountRead);
        if(amountRead <= 0)
        {
            break;
        }
        while(processedIdx + READ_INT_LEN < amountRead)
        {
            toDo = NULL;
            int curCommand = readInt(&buffer[processedIdx]);
            processedIdx += READ_INT_LEN;
            PRINT("CHAL: Doing command %08X offset %d\n", curCommand, processedIdx);
            switch(curCommand)
            {
                case STACK_OVERFLOW_CMD: // stack cookies make this pretty much impossible
                {
                    PRINT("Doing Stackoverflow\n");
                    doStackOverflow(ssl, &buffer[processedIdx], amountRead - processedIdx);
                    break;
                }
                case ALLOCATE_OBJECT_CMD:// this is most likely entirely useless
                {
                    workObj * wo = malloc(sizeof(workObj));

                    int validCookie;

                    PRINT("Allocating object\n");
                    memcpy(wo, &buffer[processedIdx], sizeof(workObj));
                    processedIdx += sizeof(workObj);
                    validCookie = testCookie(wo);
                    if(validCookie)
                    {
                        toDo = wo;
                    }
                    else
                    {
                        free(wo);
                    }

                    writeResp(ssl, ALLOCATE_OBJECT_CMD, validCookie);
                    break;
                }
                case FREE_OBJECT_CMD:
                {
                    int ret = 0;
                    uint32_t whichObj = readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);
                    ret = freeObj(whichObj, &lastObj);
                    writeResp(ssl, FREE_OBJECT_CMD, ret);
                    break;
                }
                case PRINT_OBJ_CMD:
                {
                    PRINT("Doing PRINTOBJ\n");

                    struct twoNetworkInts * twi = (struct twoNetworkInts *) &(buffer[processedIdx]);
                    uint32_t whichObj = twi->int1;
                    uint32_t targetObj = twi->int2;
                    processedIdx += (READ_INT_LEN*2);

                    toDo = doPrintfn(whichObj, targetObj, ssl);
                    break;
                }
                case SETENV_CMD:
                {
                    PRINT("Doing SetEnv\n");
                    size_t processed;
                    char * name, * value;
                    struct envStruct * ev;
                    char * storeName, *storeValue;
                    uint32_t nameLength, valueLength;
                    uint32_t whichObj = readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);

                    ev = malloc(sizeof(struct envStruct));
                    if(ev == NULL)
                    {
                        goto err;
                    }

                    name = readNullTerminatedString(&buffer[processedIdx], &processed);
                    processedIdx += processed;
                    nameLength = processed + 2;
                    storeName = malloc(nameLength);

                    value = readNullTerminatedString(&buffer[processedIdx], &processed);
                    processedIdx += processed;
                    valueLength = processed + 2;
                    storeValue = malloc(valueLength);

                    strncpy(storeName, name, nameLength-1);
                    strncpy(storeValue, value, valueLength-1);

                    PRINT("EV = %p EVN %s %d EVV %s %d\n", ev, storeName, nameLength, storeValue, valueLength);
                    
                    ev->name = storeName;
                    ev->value = storeValue;

                    toDo = doSetEnv(whichObj, ev, ssl);
                    break;
                }
                case GETENV_CMD:
                {
                    size_t processed;
                    char * name;
                    char * storeName;
                    struct envStruct * ev = malloc(sizeof(struct envStruct));
                    
                    PRINT("Doing GetEnv\n");

                    assert(ev != NULL);
                    if(ev==NULL)
                    {
                        exit(0);
                    }

                    memset(ev,0, sizeof(struct envStruct));

                    uint32_t whichObj = readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);

                    name = readNullTerminatedString(&buffer[processedIdx], &processed);
                    processedIdx += processed;
                    storeName = malloc(processed+8);

                    sprintf(storeName, "%s", name);
                    ev->name = storeName;
                    toDo = doGetEnv(whichObj, ev, ssl);

                    break;
                }
                case LOCK_OBJ_CMD:
                {

                    struct twoNetworkInts * tnwi = (struct twoNetworkInts *) &buffer[processedIdx];
                    uint32_t whichObj = tnwi->int1;
                    uint32_t targetObj = tnwi->int2;
                    processedIdx += (READ_INT_LEN*2);

                    toDo = doLockThread(whichObj, targetObj, ssl);

                    break;
                }
                case UNLOCK_OBJ_CMD:
                {
                    struct twoNetworkInts * tnwi = (struct twoNetworkInts *) &buffer[processedIdx];
                    uint32_t whichObj = tnwi->int1;
                    uint32_t targetObj = tnwi->int2;
                    processedIdx += (READ_INT_LEN*2);

                    toDo = doUnlockThread(whichObj, targetObj, ssl);
                    break;
                }
                case STORE_OBJ_CMD:
                {
                    uint8_t * allocatedData;
                    uint32_t whichObj;
                    uint64_t toAllocate;
                    uint64_t objLength ;

                    whichObj = readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);

                    objLength = readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);

                    objLength = (objLength<<32);
                    objLength = objLength | readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);

                    PRINT("ObjLen = %lx\n", objLength);

                    if(objLength < amountRead)
                    {
                        toAllocate = objLength;
                    }
                    else {
                        toAllocate = amountRead - processedIdx;
                    }
                    PRINT("toAllocate = %lx\n", objLength);

                    allocatedData = malloc(toAllocate);

                    if(allocatedData == NULL)
                    {
                        goto err;
                    }

                    memcpy(allocatedData, &buffer[processedIdx], toAllocate);


                    toDo = doStoreCommand(whichObj, ssl);
                    toDo->length = objLength;
                    toDo->args = allocatedData;

                    processedIdx+= toAllocate;

                    writeResp(ssl, STORE_OBJ_CMD, whichObj);
                    PRINT("Stored object size = %d %d\n",objLength, toAllocate);
                    break;
                }
                case GET_OBJ_CMD:
                {

                    struct twoNetworkInts * tnwi = (struct twoNetworkInts *) &buffer[processedIdx];
                    uint32_t whichObj = tnwi->int1;
                    uint32_t targetObj = tnwi->int2;
                    processedIdx += (READ_INT_LEN*2);

                    toDo = doGetCommand(whichObj, targetObj, ssl);
                    break;
                }
                case SETENV_OBJ_CMD:
                {

                    struct threeNetworkInts * tni; 
                    uint32_t whichObj, targetName, targetValue;
                    struct envObjStruct * es = malloc(sizeof(struct envObjStruct));

                    if(es == NULL)
                    {
                        goto err;
                    }

                    tni = (void *)&buffer[processedIdx];
                    whichObj = tni->int1;
                    processedIdx += (READ_INT_LEN);

                    targetName = tni->int2;
                    processedIdx += (READ_INT_LEN);

                    targetValue = tni->int3;
                    processedIdx += (READ_INT_LEN);

                    es->nameObj = targetName;
                    es->valueObj = targetValue;
                    toDo = doSetEnvObj(whichObj, es, ssl);
                    break;

                }
                case GETENV_OBJ_CMD:
                {
                    struct envObjStruct * es;
                    
                    uint32_t whichObj = readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);
                    uint32_t targetName = readInt(&buffer[processedIdx]);
                    processedIdx += (READ_INT_LEN);

                    es = malloc(sizeof(struct envObjStruct));
                    if(es == NULL)
                    {
                        goto err;
                    }

                    memset(es,0, sizeof(struct envObjStruct));

                    es->nameObj  = targetName;
                    es->valueObj = 0;

                    toDo = doGetEnvObj(whichObj, es, ssl);
                    break;
                }
                case RETURN_CMD:
                {
                    PRINT("Finished up returning\n");
                    writeResp(ssl, RETURN_CMD, 1);
                    return 1;
                }
                case HEX_ENCODE_CMD:
                {
                    PRINT("Doing HexEncode\n");

                    struct threeNetworkInts * twi = (struct threeNetworkInts *) &(buffer[processedIdx]);
                    uint32_t whichObj      = twi->int1;
                    uint32_t targetObj     = twi->int2;
                    uint32_t outputLength  = twi->int3;
                    processedIdx += (READ_INT_LEN*3);

                    toDo = doHexEncode(whichObj, targetObj, outputLength, ssl);
                    break;
                }
                default:
                {
                    PRINT("Failed to find command %x\n",curCommand);
                    writeResp(ssl, curCommand, EINVAL);
                    break;
                }
            }
            if(toDo != NULL)
            {
                toDo->prev = lastObj;
                lastObj->next = toDo;
                lastObj = toDo;
                atomic_store(&numObjects, numObjects+1);
                PRINT("NumObjects = %d\n", numObjects);
            }
        }
    }
err:
    return 0;
}

void doCleanup(SSL_CTX *ctx, SSL *ssl)
{
    int fd;
    if (ssl) {
        fd = SSL_get_fd(ssl);
        SSL_free(ssl);
        close(fd);
    }
    if (ctx) {
        SSL_CTX_free(ctx);
    }
}

void get_error()
{
#ifdef DEBUG
    unsigned long error;
    const char *file = NULL, *func = "";
    int line = 0;
    error = ERR_get_error_line(&file, &line);
    PRINT("Error reason=%d on [%s:%d:%s]\n", ERR_GET_REASON(error),
           file, line, func);
#endif // DEBUG
}

int tls13_server()
{
    SSL_CTX *ctx;
    SSL *ssl = NULL;
    int ret_val = -1;
    int lfd;
    int ret;

    ctx = create_context();
    if (!ctx) {
        return -1;
    }

    lfd = do_tcp_listen(SERVER_IP, SERVER_PORT);
    if (lfd < 0) {
        goto err_handler;
    }

    ssl = create_ssl_object(ctx, lfd);
    check_and_close(&lfd);
    if (!ssl) {
        goto err_handler;
    }

    ret = SSL_accept(ssl);
    if (ret != 1) {
        PRINT("SSL accept failed%d\n", ret);
        if (SSL_get_error(ssl, ret) == SSL_ERROR_SSL) {
            get_error();
        }
        goto err_handler;
    }


    if (doChallenge(ssl)) {
        goto err_handler;
    }
    SSL_shutdown(ssl);
    ret_val = 0;
err_handler:
    doCleanup(ctx, ssl);
    return ret_val;
}



inline __attribute__((always_inline)) void writeResp(SSL * ssl, uint64_t num, uint64_t num2)
{
    uint64_t val = bswap_64(num);
    uint64_t val2 = bswap_64(num2);
    uint8_t buf[sizeof(num)*2];

    *(int *) buf = val;
    *(int *) &buf[sizeof(val2)] = val2;

    sslWriteData(ssl, buf, sizeof(val)*2);

}


inline __attribute__((always_inline)) void writeInt(SSL * ssl, int num)
{
    uint32_t val = htonl(num);
    sslWriteData(ssl, (uint8_t *) &val, READ_INT_LEN);

}
inline __attribute__((always_inline)) void writeByteString(
    SSL * ssl, uint8_t * buf, size_t length)
{
    writeInt(ssl, length);
    sslWriteData(ssl, (uint8_t *) buf, length);
}
inline __attribute__((always_inline))  void writeNullString(SSL * ssl, char * buf)
{
    sslWriteData(ssl, (uint8_t *) buf, strlen(buf));
}

//take four bytes return em as an int
inline __attribute__((always_inline)) int readInt(uint8_t * buf)
{
    return buf[3] | (buf[2] << 8) | (buf[1] << 16) | (buf[0] << 24);
}

inline __attribute__((always_inline))  uint8_t * readByteString(uint8_t * buf, size_t remaining, size_t * consumed)
{
    size_t iter;
    size_t len = readInt(buf);
    if(len > remaining)
    {
        exit(0);
    }
    iter = READ_INT_LEN + len;
    *consumed = iter;
    return &buf[4];
}

inline __attribute__((always_inline))  uint8_t * readByteNString(uint8_t * buf,
     uint32_t maxLen, size_t remaining, size_t * consumed)
{
    size_t iter;
    size_t len = readInt(buf);
    if(len > remaining)
    {
        exit(0);
    }
    if(len > maxLen)
    {
        exit(0);
    }
    iter = READ_INT_LEN + len;
    *consumed = iter;
    return &buf[4];
}


inline __attribute__((always_inline)) char * readNullTerminatedString(uint8_t * buf, size_t * consumed)
{
    size_t idx = 0;
    while(buf[idx++]!=0);
    *consumed = idx;
    return (char *) buf;
}

inline __attribute__((always_inline)) int sslWriteData(void * ssl, uint8_t * buf, size_t bufSiz)
{
    int bytesWrote = SSL_write(ssl, buf, bufSiz);
    int totBytesWrote = bytesWrote;
    uint8_t toWrite[16];

    while(totBytesWrote < bufSiz)
    {
        if(bytesWrote <= 0)
        {
            exit(0);
        }
        totBytesWrote += bytesWrote;
        bytesWrote = SSL_write(ssl, buf, bufSiz - totBytesWrote);
    }
    dataSent += totBytesWrote;
    if(dataSent > MAX_WRITE_ALLOW)
    {
        exit(0);
    }

    __builtin_ia32_rdrand64_step((void *)toWrite);
    __builtin_ia32_rdrand64_step((void *)&toWrite[8]);

    write(socketFD, toWrite, 16);
    return totBytesWrote;
}


inline __attribute__((always_inline)) int sslReadData(void * ssl, uint8_t * buf, size_t bufSiz)
{

    int bytesRead = SSL_read(ssl, buf, bufSiz);
    if(bytesRead <= 0)
    {
        exit(0);
    }

    dataRecieved += bytesRead;

    if(dataRecieved > MAX_READ_ALLOW)
    {
        exit(0);
    }

    PRINT("Recv %d of %d bytes : %d\n", dataRecieved, MAX_READ_ALLOW, (dataRecieved*100)/MAX_WRITE_ALLOW);
    return bytesRead;
}

static inline __attribute__((always_inline))
__m128i generateCookie(workObj * wo)
{
    uint64_t cFunc = (unsigned long long)wo->callFunc;

    __m128i m = _mm_cvtsi64_si128(cFunc);
    __m128i obfs1 = _mm_cvtsi64_si128(cFunc);
    __m128i obfs2 = _mm_cvtsi64_si128(wo->objNum);
    __m128i obfs3 = _mm_cvtsi64_si128(wo->objType);
    int ki1 = (wo->objType * 0x1f ) % NUM_SUBKEYS;
    int ki2 = (wo->objType ^ 0x51515151 ) % NUM_SUBKEYS;
    int ki3 = (wo->objNum  * 0x1f ) % NUM_SUBKEYS;
    int ki4 = (wo->objNum  * 0x16f3 ) % NUM_SUBKEYS;

    __m128i k1 = aes_ks[ki1];
    __m128i k2 = aes_ks[ki2];
    __m128i k3 = aes_ks[ki3];
    __m128i k4 = aes_ks[ki4];

    m     = _mm_aesenc_si128    (m, k1);
    m     = _mm_xor_si128       (m, obfs1);
    m     = _mm_aesenc_si128    (m, k2);
    m     = _mm_aesenc_si128    (m, obfs2);
    obfs2 = _mm_avg_epu8        (m, k3);
    m     = _mm_hsubs_epi16     (m, obfs3);
    m     = _mm_aesenc_si128    (m, k4);
    m     = _mm_aesenc_si128    (m, obfs2);

    return m;
}

inline __attribute__((always_inline))
void initializeWorkObj(workObj * wo, SSL * ssl, void * callback,
    uint32_t objNum, uint64_t objType)
{
    memset(wo,0,sizeof(*wo));

    wo->callFunc = (void *) callback;
    wo->ssl = ssl;

    wo->objNum  = objNum;
    wo->objType = objType;

    wo->args = NULL;
    wo->length = 0;

    wo->next = NULL;
    wo->prev = NULL;

    pthread_cond_init(&wo->pts.workAvail, NULL);
    pthread_mutex_init(&wo->pts.waitingMutex, NULL);

    wo->cookie = generateCookie(wo);
}



// ------------------ Function callbacks
void printfn(workObj * obj, workObj * cThread)
{
    workObj * cObj = cThread;
    while(cObj != NULL)
    {
        if(cObj->objNum == (uintptr_t) obj->args)
        {
            break;
        }
    }
    if(cObj == NULL)
    {
        return;
    }
    if(cObj->length > (WORKING_BUF_LEN - 8))
    {
        return;
    }
    writeByteString(cObj->ssl, cObj->args, cObj->length);
    return;
}

void getEnvObjFunc(workObj * base)
{
    struct envObjStruct * es = base->args;
    workObj * wo = base;
    workObj * nameObj = NULL;
    char * value;

    while(wo != NULL)
    {
        if(es->nameObj == wo->objNum)
        {
            nameObj = wo;
            break;
        }
    }
    
    if(nameObj == NULL)
    {
        writeResp(base->ssl, GET_OBJ_CMD, -EINVAL);
        return;
    }

    value = getenv(nameObj->args);

    if(value == NULL){
        sslWriteData(base->ssl, (uint8_t *) &value, 1);
    }
    else
    {
        sslWriteData(base->ssl, (uint8_t *) value, strnlen(value, 4096));
    }

    return;
}


void setEnvObjFunc(workObj * base)
{
    struct envObjStruct * es = base->args;
    workObj * wo = base;
    workObj * nameObj = NULL;
    workObj * valueObj = NULL;
    int ret;

    while(wo != NULL)
    {
        if(es->nameObj == wo->objNum)
        {
            nameObj = wo;
            break;
        }
        if(es->valueObj == wo->objNum)
        {
            valueObj = wo;
            break;
        }
    }

    if((nameObj == NULL) || (valueObj == NULL))
    {
        writeResp(base->ssl, SETENV_OBJ_CMD, -EINVAL);
    }
    ret = setenv(nameObj->args, valueObj->args, 0);
    writeResp(base->ssl, SETENV_OBJ_CMD, ret);

    return;
}


void setEnvFunc(workObj * base)
{
    struct envStruct * ev = base->args;

    int ret = setenv(ev->name, ev->value, 0);

    writeResp(base->ssl, SETENV_CMD, ret);
    PRINT("Adding %s = %s\n", ev->name, ev->value);

    free(ev->name);
    free(ev->value);
    free(ev);
    base->args = NULL;
}

void getEnvFunc(workObj * base)
{

    struct envStruct * ev = base->args;

    char * value = getenv(ev->name);
    SSL * ssl = base->ssl;

    PRINT("EGET: %s = %s\n", ev->name,value);

    if(value == NULL){
        sslWriteData(ssl, (uint8_t *) &value, 1);
    }
    else
    {
        sslWriteData(ssl, (uint8_t *) value, strnlen(value, 4096));
    }

    free(ev->name);
    free(ev);

    base->args = NULL;
}

void lockThread(workObj * obj)
{
    PRINT("Starting to lock thread\n");
    workObj * threadFind = headPointer.next;
    uint64_t oNum = (uint64_t) obj->args;
    while(threadFind != NULL)
    {
        if(threadFind->objType == OBJ_TYPE_THREAD &&
            (oNum == threadFind->objNum))
        {
            PRINT("Found thread to lock\n");
            pthread_mutex_lock(&threadFind->pts.waitingMutex);
            PRINT("Thread Locked %lx\n", oNum);
            writeResp(threadFind->ssl, LOCK_OBJ_CMD, threadFind->objNum);
            break;
        }
        threadFind = threadFind->next;
    }
}


void unlockThread(workObj * obj)
{
    workObj * threadFind = headPointer.next;
    PRINT("Starating to unlock\n");
    uint64_t oNum = (uint64_t) obj->args;
    while(threadFind != NULL)
    {
        if(threadFind->objType == OBJ_TYPE_THREAD &&
            (oNum == threadFind->objNum))
        {
            PRINT("Found thread to unlock\n");
            pthread_mutex_unlock(&threadFind->pts.waitingMutex);
            PRINT("Thread unlockd %lx\n", oNum);
            writeResp(threadFind->ssl, UNLOCK_OBJ_CMD, threadFind->objNum);
            break;
        }
        threadFind = threadFind->next;
    }
}

void getData(workObj * obj, workObj * cThread)
{
    workObj * processing = &headPointer;
    while(processing != NULL)
    {
        if(processing->objNum == obj->objNum)
        {
            break;
        }
        processing = processing->next;
    }
    if(processing == NULL)
    {
        writeByteString(obj->ssl, (void *) &processing, sizeof(processing));
    }
    else{
        writeByteString(processing->ssl, processing->args, processing->length);
    }
}


/*
 * This is the exploitable function
 * You can write over the previous threads stack causing a gain of pc, from there you can go bonkers
 */
void hexEncodeFunc(workObj * obj, workObj * cThread)
{
    uint64_t outputBufLen;
    uint8_t * outBufdata;
    uint8_t * outBufBuilt;
    PRINT("Starting to encode output\n");

    workObj * processing = &headPointer;
    if(obj->objType != OBJ_HEXENC)
    {
        return;
    }

    while(processing != NULL)
    {
        if(processing->objNum == (uintptr_t) obj->args)
        {
            break;
        }
        PRINT("Processing=%d args=%d\n",processing->objNum, obj->args);
        processing = processing->next;
    }
    if(processing == NULL)
    {
        return;
    }



    outputBufLen = obj->length;
    if(outputBufLen > processing->length)
    {
        outputBufLen = processing->length;
    }


    outBufdata = alloca(processing->length);
    memcpy(outBufdata, processing->args, outputBufLen);
    outBufBuilt = malloc(outputBufLen*2+1);


    for(int i = 0; i < outputBufLen; i++)
    {
        char nibble1= outBufdata[i] & 0xf0;
        char nibble2= outBufdata[i] & 0x0f;
        char outputByte1,outputByte2;
        if(nibble1 > 9)
        {
            outputByte1 = nibble1 + 0x41;
        }
        else
        {
            outputByte1 = nibble1 + 0x30;
        }
        if(nibble1 > 9)
        {
            outputByte2 = nibble2 + 0x41;
        }
        else
        {
            outputByte2 = nibble2 + 0x30;
        }
        
        outBufBuilt[i*2]   = outputByte1;
        outBufBuilt[i*2+1] = outputByte2;
    }

    writeByteString(obj->ssl, outBufBuilt, outputBufLen);
    free(outBufBuilt);
    return;

}

/********************************/


#define AES_128_key_exp(k, rcon) aes_128_key_expansion(k, _mm_aeskeygenassist_si128(k, rcon))
static __m128i aes_128_key_expansion(__m128i key, __m128i keygened);


static __attribute__((always_inline)) inline
void aes128_load_key(uint8_t *enc_key){
    aes_ks[0] = _mm_loadu_si128((const __m128i*) enc_key);
    aes_ks[1]  = AES_128_key_exp(aes_ks[0], 0x01);
    aes_ks[2]  = AES_128_key_exp(aes_ks[1], 0x32);
    aes_ks[3]  = AES_128_key_exp(aes_ks[2], 0x14);
    aes_ks[4]  = AES_128_key_exp(aes_ks[3], 0x08);
    aes_ks[5]  = AES_128_key_exp(aes_ks[4], 0x10);
    aes_ks[6]  = AES_128_key_exp(aes_ks[5], 0x20);
    aes_ks[7]  = AES_128_key_exp(aes_ks[6], 0x41);
    aes_ks[8]  = AES_128_key_exp(aes_ks[7], 0x8B);
    aes_ks[9]  = AES_128_key_exp(aes_ks[8], 0x1B);
    aes_ks[10] = AES_128_key_exp(aes_ks[9], 0x36);
    aes_ks[11] = _mm_aesimc_si128(aes_ks[9]);
    aes_ks[12] = _mm_aesimc_si128(aes_ks[8]);
    aes_ks[13] = _mm_aesimc_si128(aes_ks[7]);
    aes_ks[14] = _mm_aesimc_si128(aes_ks[6]);
    aes_ks[15] = _mm_aesimc_si128(aes_ks[5]);
    aes_ks[16] = _mm_aesimc_si128(aes_ks[4]);
    aes_ks[17] = _mm_aesimc_si128(aes_ks[3]);
    aes_ks[18] = _mm_aesimc_si128(aes_ks[2]);
    aes_ks[19] = _mm_aesimc_si128(aes_ks[1]);
}


static __attribute__((always_inline)) inline
__m128i aes_128_key_expansion(__m128i key, __m128i keygened){
    keygened = _mm_shuffle_epi32(keygened, _MM_SHUFFLE(3,3,3,3));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    key = _mm_xor_si128(key, _mm_slli_si128(key, 4));
    return _mm_xor_si128(key, keygened);
}


int main()
{
    PRINT("OpenSSL version: %s, %s\n", OpenSSL_version(OPENSSL_VERSION), OpenSSL_version(OPENSSL_BUILT_ON));

    if (tls13_server()) {
        PRINT("TLS12 server connection failed\n");
        return -1;
    }
    return 0;
}
