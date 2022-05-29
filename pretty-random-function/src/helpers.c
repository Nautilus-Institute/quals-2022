#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include "openssl/rand.h"
#include "openssl/crypto.h"
#include "openssl/ssl.h"
#include "challenge_consts.h"
#include "ssl_helper.h"

int socketFD = -1;
uint32_t dataSent = 0;
uint32_t dataRecieved = 0;

int do_tcp_listen(const char *server_ip, uint16_t port)
{
    struct sockaddr_in addr;
    int optval = 1;
    int lfd;
    int ret;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        PRINT("Socket creation failed\n");
        return -1;
    }

    addr.sin_family = AF_INET;
    if (inet_aton(server_ip, &addr.sin_addr) == 0) {
        PRINT("inet_aton failed\n");
        goto err_handler;
    }
    addr.sin_port = htons(port);

    if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))) {
        PRINT("set sock reuseaddr failed\n");
    }
    ret = bind(lfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret) {
        PRINT("bind failed %s:%d\n", server_ip, port);
        goto err_handler;
    }

    PRINT("TCP listening on %s:%d...\n", server_ip, port);
    ret = listen(lfd, TCP_MAX_LISTEN_COUNT);
    if (ret) {
        PRINT("listen failed\n");
        goto err_handler;
    }
    PRINT("TCP listen fd=%d\n", lfd);
    return lfd;
err_handler:
    close(lfd);
    return -1;
}

int do_tcp_accept(int lfd)
{
    struct sockaddr_in peeraddr;
    socklen_t peerlen = sizeof(peeraddr);
    int cfd;

    PRINT("\n\n###Waiting for TCP connection from client...\n");
    cfd = accept(lfd, (struct sockaddr *)&peeraddr, &peerlen);
    if (cfd < 0) {
        PRINT("accept failed, errno=%d\n", errno);
        return -1;
    }

    PRINT("TCP connection accepted fd=%d\n", cfd);
    return cfd;
}

void check_and_close(int *fd)
{
    if (*fd < 0) {
        return;
    }
    if (*fd == 0 || *fd == 1 || *fd == 2) {
        PRINT("Trying to close fd=%d, skipping it !!!\n", *fd);
    }
    PRINT("Closing fd=%d\n", *fd);
    close(*fd);
    *fd = -1;
}



int do_tcp_connection(const char *server_ip, uint16_t port)
{
    struct sockaddr_in serv_addr;
    int count = 0;
    int fd;
    int ret;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        PRINT("Socket creation failed\n");
        return -1;
    }
    PRINT("Client fd=%d created\n", fd);

    serv_addr.sin_family = AF_INET;
    if (inet_aton(server_ip, &serv_addr.sin_addr) == 0) {
        PRINT("inet_aton failed\n");
        goto err_handler;
    }
    serv_addr.sin_port = htons(port);

    PRINT("Connecting to %s:%d...\n", server_ip, port);
    do {
        ret = connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
        if (ret) {
            PRINT("Connect failed, errno=%d\n", errno);
            goto err_handler;
        } else {
            break;
        }
        count++;
        usleep(TCP_CON_RETRY_WAIT_TIME_MS);
    } while (count < TCP_CON_RETRY_COUNT);

    PRINT("TLS connection succeeded, fd=%d\n", fd);
    return fd;
err_handler:
    close(fd);
    return -1;
}



unsigned int get_psk_key(SSL *ssl, const char *id,
                                            unsigned char *psk,
                                            unsigned int max_psk_len)
{

    unsigned int result_len;
    unsigned char result[EVP_MAX_MD_SIZE];

    PRINT("Generating HK: for %s size %lu\n", id, strlen(id));
    

    HMAC(EVP_sha256(),
         HMAC_KEY, sizeof(HMAC_KEY),
         (const uint8_t *) id, strlen(id),
         result, &result_len);

    if(strlen(id) < ID_LENGTH)
    {
        ERR("ID Too short\n");
        goto err;
    }
    if(result_len == 0)
    {
        ERR("HMAC Failed\n");
        goto err;
    }

    if(result_len > max_psk_len)
    {
        result_len = max_psk_len - 1;
    }

    memcpy(psk, result, result_len);
    dumpBuf("SPSK", psk, result_len);

    return result_len;
err:
    return 0;
}

void sleepThread() // this gives a function without a stack canary to ovewrite from the other thread, so you don't need a full pivot
{
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0x300;
    select(0,NULL,NULL,NULL,&timeout);
}

int doPOWChallenge()
{
    PRINT("Starting POW Challenge with client\n");
    uint8_t challenge[POW_CHAL_SIZE]; 
    uint8_t resp[POW_CHAL_RESP_SIZE]; 
    SHA512_CTX ctx;
    unsigned char digest[SHA512_DIGEST_LENGTH];
    uint32_t ret;

    RAND_bytes(challenge,POW_CHAL_SIZE);

    dumpBuf("CHA1", challenge, POW_CHAL_SIZE);
    write(socketFD, challenge, POW_CHAL_SIZE); //well below mtu
    read(socketFD, resp, POW_CHAL_RESP_SIZE); //well below mtu should be safe
    dumpBuf("RESP", resp, POW_CHAL_RESP_SIZE);
    dumpBuf("CHA2", challenge, POW_CHAL_SIZE);

    if(memcmp(resp, challenge, POW_CHAL_SIZE) != 0)
    {
        return -1;
    }
    PRINT("Client responded with chal\n");

    SHA384_Init(&ctx);
    SHA384_Update(&ctx, resp, POW_CHAL_RESP_SIZE);
    SHA384_Final(digest, &ctx);
    dumpBuf("DIG", digest, 4);

    ret = digest[0] | (digest[1]<<8) | (digest[2] << 16) | (digest[3] << 24);
    ret = ret & POW_MASK;
    
    PRINT("Challenge result = %d\n", ret==0);
    return (ret == 0);
}
