#ifndef TA_WALLET_INTERNAL_H
#define TA_WALLET_INTERNAL_H

#include <mbedtls/ecdsa.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ripemd160.h>

#define TX_VERSION 0x1

#define SERIALIZE_INC_AMOUNT 0x100

/* structures, types */
typedef struct {
  void *next;
  void *prev;
} ll_t;

typedef struct {
  ll_t     ll;
  uint8_t  tx_hash[32];
  uint32_t index;
  size_t   scriptSig_len;
  uint8_t *scriptSig;
  uint32_t sequence;
} input_t;

typedef struct {
  ll_t      ll;
  uint64_t  value;
  size_t    lockScript_len;
  uint8_t  *lockScript;
} output_t;

typedef struct {
  uint32_t  version;
  input_t  *inputs;
  output_t *outputs;
  uint32_t  locktime;
} tx_t;

typedef struct {
  uint32_t id;
  unsigned char d[32];
  unsigned char q_X[32];
  unsigned char q_Y[32];
  unsigned char ripemd160_cxsum[20];
} serialized_wallet_t;

typedef struct {
  ll_t ll;
  uint32_t id;
  mbedtls_mpi d; // secret part
  mbedtls_ecp_point Q; // public part
  char *address;
  unsigned char ripemd160_cxsum[20]; // handy address retrieval
} wallet_t;

/* function protos */
int Wallet_TA_Random(void *, unsigned char *, size_t);

uint16_t ntoh16(uint16_t);
uint32_t ntoh32(uint32_t);
uint64_t ntoh64(uint64_t);

TEE_Result allocate_wallet(wallet_t **);
TEE_Result save_wallet(wallet_t *);
TEE_Result load_wallet(serialized_wallet_t *);
TEE_Result wallet_pubkey(wallet_t *, uint8_t *);
void print_wallet(wallet_t *);
TEE_Result set_wallet_address(wallet_t *);

char *get_mpi_string(mbedtls_mpi *);

char *b58_encode_check(unsigned char version, unsigned char *input, size_t input_len);
char *b58_encode(unsigned char *b58_input, size_t b58_input_len);

TEE_Result deserialize_tx(uint8_t *tx_bytes, size_t tx_bytes_len, tx_t **out_tx);
TEE_Result serialize_tx(tx_t *tx, uint8_t **out, size_t *out_sz);
TEE_Result serialize_signature(mbedtls_mpi *r, mbedtls_mpi *s, uint8_t **out_sig, size_t *out_sig_len);

#endif /*TA_WALLET_INTERNAL_H*/
