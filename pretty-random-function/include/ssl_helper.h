
#ifndef _SSL_HELPER_H__
#define _SSL_HELPER_H__


#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "openssl/ssl.h"
#include "challenge_consts.h"


#define TWT_SUCCESS     0
#define TWT_FAILURE    -1

#define RED_COLOUR    "\x1B[31m"
#define RESET_COLOUR  "\x1B[0m"

#define TCP_MAX_LISTEN_COUNT 10


#ifndef DEBUG
#define LOG(...)
#define DBG(...)
#define ERR(...)
#define PRINT(...)
#define dumpBuf(...)

#else

#define LOG(colour_start, colour_end, fmt, loglevel, ...) \
    do { \
        char *token, *filename = __FILE__; \
        char *rest_orig, *rest; \
        rest_orig = rest = strdup(filename); \
        if (rest != NULL) { \
            while ((token = strtok_r(rest, "/", &rest)) != NULL) { \
                filename = token; \
            } \
        } \
        printf("%s[%s][%s:%d]"fmt"%s", colour_start, loglevel, filename, \
                __LINE__, ##__VA_ARGS__, colour_end); \
        fflush(stdout); \
        if (rest_orig != NULL) free(rest_orig); \
    } while (0)

#define DBG(fmt, ...) \
    LOG("", "", fmt, "DBG", ##__VA_ARGS__)

#define ERR(fmt, ...) \
    LOG(RED_COLOUR, RESET_COLOUR, fmt, "ERR", ##__VA_ARGS__)

#define PRINT(fmt, ...) \
    do { \
        char baseFmt[1572]; \
        snprintf(baseFmt, sizeof(baseFmt), "%-20s - %d : %s",__FUNCTION__, __LINE__, fmt); \
        printf(baseFmt, ##__VA_ARGS__); \
        fflush(stdout); \
    } while (0)

static inline void dumpBuf(char * prefix, uint8_t * buf, unsigned int len)
{
    int offset = 0;
    char printBuf[1024];
    printBuf[0] = 0;
    char tmpBuf[32];
    while(len > 0)
    {
        strcat(printBuf,prefix);
        strcat(printBuf, " : ");
        for(int i =0; (i < 32) && (len > 0); i ++)
        {
            sprintf(tmpBuf, "%02X ", buf[i + offset]);
            strcat(printBuf, tmpBuf);
            len--;
        }
        strcat(printBuf, "\n");
        offset += 32;
        if((offset % 768 )== 0)
        {
            PRINT(printBuf);
            printBuf[0] = 0;
        }
    }
    PRINT(printBuf);
}

#endif

#define SERVER_IP "0.0.0.0"
#define SERVER_PORT 4433

#define TCP_CON_RETRY_COUNT 20
#define TCP_CON_RETRY_WAIT_TIME_MS 2

#define TLS_SOCK_TIMEOUT_MS 8000

int create_udp_sock();

int create_udp_serv_sock(const char *server_ip, uint16_t port);

int do_tcp_connection(const char *server_ip, uint16_t port);

int do_tcp_listen(const char *server_ip, uint16_t port);

int do_tcp_accept(int lfd);

int set_receive_to(int fd, int secs);

void check_and_close(int *fd);

void writeNullString(SSL * ssl, char * buf);
void writeByteString(SSL * ssl, uint8_t * buf, size_t length);
void writeInt(SSL * ssl, int num);
char * readNullTerminatedString(uint8_t * buf, size_t * consumed);
uint8_t * readByteString(uint8_t * buf, size_t remaining, size_t * consumed);
int sslReadData(void * ssl, uint8_t * buf, size_t bufSiz);
int sslWriteData(void * ssl, uint8_t * buf, size_t bufSiz);
void initializeWorkObj(workObj * wo, SSL * ssl, void * callback,
    uint32_t objNum, uint64_t objType);
void printfn(workObj * obj, workObj * cThread);
int readInt(uint8_t * buf);
unsigned int get_psk_key(SSL *ssl, const char *id,
                                            unsigned char *psk,
                                            unsigned int max_psk_len);
void sleepThread(void); 
int doPOWChallenge(void);



int ssl_custom_add(SSL *s, unsigned int ext_type,
                                      unsigned int context,
                                      const unsigned char **out,
                                      size_t *outlen, X509 *x,
                                      size_t chainidx, int *al,
                                      void *add_arg);

void ssl_custom_free(SSL *s, unsigned int ext_type,
                                        unsigned int context,
                                        const unsigned char *out,
                                        void *add_arg);

int ssl_custom_parse(SSL *s, unsigned int ext_type,
                                        unsigned int context,
                                        const unsigned char *in,
                                        size_t inlen, X509 *x,
                                        size_t chainidx, int *al,
                                        void *parse_arg);

extern int socketFD;
extern uint32_t dataSent;
extern uint32_t dataRecieved;

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

#endif //_SSL_HELPER_H__