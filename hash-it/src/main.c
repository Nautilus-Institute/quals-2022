#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <openssl/evp.h>



#define ALARM_SECONDS 10

void be_a_ctf_challenge() {
    alarm(ALARM_SECONDS);
}


typedef const EVP_MD * (* hash_algo_t)(void) ;


hash_algo_t HASH_ALGOS[] = {
    EVP_md5,
    EVP_sha1,
    EVP_sha256,
    EVP_sha512
};


int hash_byte(
    uint8_t input_byte_0,
    uint8_t input_byte_1,
    uint8_t * output_byte,
    const EVP_MD * (* evp_md)(void)
) {
    EVP_MD_CTX * mdctx;

    uint8_t input[2];
    input[0] = input_byte_0;
    input[1] = input_byte_1;

    if ((mdctx = EVP_MD_CTX_new()) == NULL) {
        return -1;
    }

    if (1 != EVP_DigestInit_ex(mdctx, evp_md(), NULL)) {
        return -1;
    }

    if (1 != EVP_DigestUpdate(mdctx, input, 2)) {
        return -1;
    }

    uint8_t * digest = malloc(EVP_MD_size(evp_md()));

    if (digest == NULL) {
        return -1;
    }

    unsigned int digest_len = 0;
    if (1 != EVP_DigestFinal_ex(mdctx, digest, &digest_len)) {
        return -1;
    }

    EVP_MD_CTX_free(mdctx);

    *output_byte = digest[0];

    free(digest);

    return 0;
}


int read_all(FILE * fh, void * buf, size_t len) {
    uint8_t * b8 = (uint8_t *) buf;
    size_t bytes_read = 0;
    while (bytes_read < len) {
        int r = fread(&b8[bytes_read], 1, len - bytes_read, fh);
        if (r <= 0) {
            return -1;
        }
        bytes_read += r;
    }
    return 0;
}


int main(int argc, char * argv) {
    be_a_ctf_challenge();

    uint32_t shellcode_len = 0;
    if (read_all(stdin, &shellcode_len, sizeof(uint32_t))) {
        return -1;
    }

    shellcode_len = ntohl(shellcode_len);

    uint8_t * shellcode_mem = malloc(shellcode_len);
    if (shellcode_mem == NULL) {
        return -1;
    }

    if (read_all(stdin, shellcode_mem, shellcode_len)) {
        return -1;
    }

    unsigned int i;
    for (i = 0; i < shellcode_len; i += 2) {
        uint8_t new_byte;
        if (hash_byte(shellcode_mem[i],
                      shellcode_mem[i + 1],
                      &new_byte,
                      HASH_ALGOS[(i >> 1) % 4])) {
            return -1;
        }
        shellcode_mem[i / 2] = new_byte;
    }


    /* If they can't figure out shellcode_len needs to be page-aligned that's
       their problem. */
    void * mem = mmap(0,
                      shellcode_len / 2,
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,
                      0);

    memcpy(mem, shellcode_mem, shellcode_len / 2);

    ((void (*)()) mem)();

    return 0;
}
