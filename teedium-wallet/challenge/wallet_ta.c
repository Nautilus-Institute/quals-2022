/*
 * Copyright (c) 2016, Linaro Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

#include <string.h>
#include <wallet_ta.h>
#include <wallet_internal.h>

// TODO: this will probably need to be restored somehow?
uint32_t g_next_wallet_id = 0;
wallet_t *g_wallets = NULL;

/* Wrapper for mbedtls_ecp_gen_keypair's f_rng argument */
int Wallet_TA_Random(void *p, unsigned char *out, size_t len)
{
  (void)p;
  TEE_GenerateRandom(out, len);
  return 0;
}

uint16_t ntoh16(uint16_t in)
{
  uint8_t data[2] = {0};

  TEE_MemMove(&data, &in, sizeof(data));

  return (uint16_t)data[0] << 8 | (uint16_t)data[1];
}

uint32_t ntoh32(uint32_t in)
{
  uint8_t data[4] = {0};

  TEE_MemMove(&data, &in, sizeof(data));

  return (uint32_t)data[0] << 24 \
       | (uint32_t)data[1] << 16 \
       | (uint32_t)data[2] << 8  \
       | (uint32_t)data[3];
}

uint64_t ntoh64(uint64_t in)
{
  uint8_t data[8] = {0};

  TEE_MemMove(&data, &in, sizeof(data));

  return (uint64_t)data[0] << 56 \
       | (uint64_t)data[1] << 48 \
       | (uint64_t)data[2] << 40 \
       | (uint64_t)data[3] << 32 \
       | (uint64_t)data[4] << 24 \
       | (uint64_t)data[5] << 16 \
       | (uint64_t)data[6] << 8 \
       | (uint64_t)data[7];
}


/* --- LINKED LIST OPERATIONS --- */
static void link_add(ll_t **head, ll_t *elem)
{
  if (*head == NULL)
  {
    *head = elem;
  }
  else
  {
    ll_t *cur = NULL;
    for(cur = *head; cur->next; cur = cur->next);
    cur->next = elem;
  }
}

/* --- WALLET OPERATIONS --- */

TEE_Result allocate_wallet(wallet_t **wallet)
{
  wallet_t *new_wallet;
  
  new_wallet = TEE_Malloc(sizeof(wallet_t), 0);
  if (new_wallet == NULL)
  {
    return TEE_ERROR_OUT_OF_MEMORY;
  }

  mbedtls_mpi_init(&new_wallet->d);
  mbedtls_ecp_point_init(&new_wallet->Q);

  *wallet = new_wallet;
  
  return TEE_SUCCESS;
}

TEE_Result save_wallet(wallet_t *wallet)
{
  TEE_Result res = TEE_SUCCESS;
  serialized_wallet_t serialized;

  serialized.id = wallet->id;

  if (mbedtls_mpi_write_binary(&wallet->d, serialized.d, sizeof(serialized.d)))
  {
    return TEE_ERROR_GENERIC;
  }

  if (mbedtls_mpi_write_binary(&wallet->Q.X, serialized.q_X, sizeof(serialized.q_X)))
  {
    return TEE_ERROR_GENERIC;
  }

  if (mbedtls_mpi_write_binary(&wallet->Q.Y, serialized.q_Y, sizeof(serialized.q_Y)))
  {
    return TEE_ERROR_GENERIC;
  }

  TEE_MemMove(serialized.ripemd160_cxsum, &wallet->ripemd160_cxsum, sizeof(serialized.ripemd160_cxsum));

  TEE_ObjectHandle object;

  res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE,
                                   &wallet->id, sizeof(wallet->id),
                                   TEE_DATA_FLAG_ACCESS_WRITE|TEE_DATA_FLAG_OVERWRITE,
                                   TEE_HANDLE_NULL,
                                   NULL, 0,
                                   &object);

  if (res != TEE_SUCCESS)
  {
    return res;
  }

  res = TEE_WriteObjectData(object, (void *)&serialized, sizeof(serialized));
  if (res != TEE_SUCCESS)
  {
    TEE_CloseAndDeletePersistentObject1(object);
  }
  else
  {
    TEE_CloseObject(object);
  }

  return res;
}

TEE_Result load_wallet(serialized_wallet_t *serialized)
{
  TEE_Result res = TEE_SUCCESS;
  wallet_t *wallet;

  wallet = TEE_Malloc(sizeof(wallet_t), 0);
  if (wallet == NULL)
  {
    return TEE_ERROR_OUT_OF_MEMORY;
  }

  wallet->id = serialized->id;

  if (mbedtls_mpi_read_binary(&wallet->d, serialized->d, sizeof(serialized->d)))
  {
    TEE_Free(wallet);
    return TEE_ERROR_GENERIC;
  }

  if (mbedtls_mpi_read_binary(&wallet->Q.X, serialized->q_X, sizeof(serialized->q_X)))
  {
    TEE_Free(wallet);
    return TEE_ERROR_GENERIC;
  }

  if (mbedtls_mpi_read_binary(&wallet->Q.Y, serialized->q_Y, sizeof(serialized->q_Y)))
  {
    TEE_Free(wallet);
    return TEE_ERROR_GENERIC;
  }

  TEE_MemMove(wallet->ripemd160_cxsum, serialized->ripemd160_cxsum, sizeof(wallet->ripemd160_cxsum));

  char *wallet_address = b58_encode_check(0, (unsigned char *)&wallet->ripemd160_cxsum, 20);
  if (wallet_address == NULL)
  {
    TEE_Free(wallet);
    return TEE_ERROR_GENERIC;
  }

  wallet->address = wallet_address;

  if (wallet->id > g_next_wallet_id)
  {
    g_next_wallet_id = wallet->id + 1;
  }

  link_add((ll_t **)&g_wallets, (ll_t *)wallet);

  return res;
}

TEE_Result wallet_pubkey(wallet_t *wallet, uint8_t *pubkey)
{
  // allocate space for the binary data
  unsigned char q_X[33];
  unsigned char q_Y[32];
  if (mbedtls_mpi_write_binary(&wallet->Q.Y, q_Y, 32))
  {
    return TEE_ERROR_GENERIC;
  }

  if (mbedtls_mpi_write_binary(&wallet->Q.X, &q_X[1], 32))
  {
    return TEE_ERROR_GENERIC;
  }

  // q_X now is our compressed public key
  q_X[0] = q_Y[31] % 2 == 0 ? 0x02 : 0x03;

  TEE_MemMove(pubkey, q_X, 33);
  return TEE_SUCCESS;
}

TEE_Result set_wallet_address(wallet_t *wallet)
{
  TEE_Result res = TEE_SUCCESS;
  unsigned char pubkey[33];
  if (wallet_pubkey(wallet, pubkey) != TEE_SUCCESS)
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  // turn this into an address - an address is ripe160(sha256(pubkey))

  mbedtls_sha256_context sha256_ctx;
  mbedtls_ripemd160_context ripemd160_ctx;
  unsigned char sha256_cxsum[32];

  mbedtls_sha256_init(&sha256_ctx);

  if (mbedtls_sha256_starts_ret(&sha256_ctx, 0))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  if (mbedtls_sha256_update_ret(&sha256_ctx, pubkey, 33))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  mbedtls_sha256_finish_ret(&sha256_ctx, sha256_cxsum);

  mbedtls_ripemd160_init(&ripemd160_ctx);

  if (mbedtls_ripemd160_starts_ret(&ripemd160_ctx))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  if (mbedtls_ripemd160_update_ret(&ripemd160_ctx, sha256_cxsum, sizeof(sha256_cxsum)))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  if (mbedtls_ripemd160_finish_ret(&ripemd160_ctx, (unsigned char *)&wallet->ripemd160_cxsum))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  char *wallet_address = b58_encode_check(0, (unsigned char *)&wallet->ripemd160_cxsum, 20);
  if (wallet_address == NULL)
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  wallet->address = wallet_address;

out:
  return res;
}


char *get_mpi_string(mbedtls_mpi *m)
{
  size_t required_len;
  mbedtls_mpi_write_string(m, 16, NULL, 0, &required_len);

  char *string = TEE_Malloc(required_len, 0);
  if (string == NULL) {
    return NULL;
  }

  mbedtls_mpi_write_string(m, 16, string, required_len, &required_len);

  return string;
}

/* Ugly debug print wallet, not meant for public consumption */
/* void print_wallet(wallet_t *wallet) */
/* { */
/*   char *X_string = get_mpi_string(&wallet->Q.X); */
/*   char *Y_string = get_mpi_string(&wallet->Q.Y); */
/*   char *d_string = get_mpi_string(&wallet->d); */

/*   IMSG("d: %s", d_string); */
/*   IMSG("X: %s", X_string); */
/*   IMSG("Y: %s", Y_string); */

/*   TEE_Free(X_string); */
/*   TEE_Free(Y_string); */
/*   TEE_Free(d_string); */
/* } */

/* --- SERIALIZATION ROUTINES --- */
static int deserialize_int(uint32_t *out, uint8_t *s, uint32_t *pos, size_t *n)
{
  if (*n >= sizeof(uint32_t))
  {
    TEE_MemMove(out, &s[*pos], sizeof(uint32_t));
    *n -= sizeof(uint32_t);
    *pos += sizeof(uint32_t);
    return 0;
  }
  return -1;
}

static int deserialize_int64(uint64_t *out, uint8_t *s, uint32_t *pos, size_t *n)
{
  if (*n >= sizeof(uint64_t))
  {
    TEE_MemMove(out, &s[*pos], sizeof(uint64_t));
    *n -= sizeof(uint64_t);
    *pos += sizeof(uint64_t);
    return 0;
  }
  return -1;
}

static int deserialize_var_int(uint64_t *out, uint8_t *s, uint32_t *pos, size_t *n)
{
  size_t enc = 0;
  
  if (*n >= sizeof(uint8_t))
  {
    enc = s[*pos];
    if (enc < 253)
    {
      *out = enc;
      *pos += 1;
      *n -= 1;
      return 0;
    }

    *n -= 1;
    *pos += 1;

    switch(enc)
    {
    case 0xFD:
      if (*n >= sizeof(uint16_t))
      {
        //*out =  ntoh16(*((uint16_t *)&s[*pos]));
        uint16_t _t = ntoh16(*((uint16_t *)&s[*pos]));
        TEE_MemMove(out, &_t, sizeof(uint16_t));
        *pos += sizeof(uint16_t);
        *n -= sizeof(uint16_t);
        return 0;
      }
      break;
    case 0xFE:
      if (*n >= sizeof(uint32_t))
      {
        *out = ntoh32(*((uint32_t *)&s[*pos]));
        *pos += sizeof(uint32_t);
        *n -= sizeof(uint32_t);
        return 0;
      }
      break;
    case 0xFF:
      if (*n >= sizeof(uint64_t))
      {
        *out = ntoh64(*((uint64_t *)&s[*pos]));
        *pos += sizeof(uint64_t);
        *n -= sizeof(uint64_t);
        return 0;
      }
      break;
    default:
      break;
    }
  }

  return -1;
}

static int deserialize_bytes(uint8_t *out, size_t sz_bytes, uint8_t *s, uint32_t *pos, size_t *n)
{
  if (*n >= sz_bytes)
  {
    TEE_MemMove(out, &s[*pos], sz_bytes);
    *n -= sz_bytes;
    *pos += sz_bytes;
    return 0;
  }

  return -1;
}

static int deserialize_var_string(uint8_t **out, size_t *out_s_sz, uint8_t *s, uint32_t *pos, size_t *n)
{
  uint64_t s_sz = 0;
  if (deserialize_var_int(&s_sz, s, pos, n))
  {
    return -1;
  }

  if (s_sz > 0)
  {
    uint8_t *out_p = NULL;
    out_p = TEE_Malloc(s_sz, 0);
    if (out_p == NULL)
    {
      return -1;
    }

    if (deserialize_bytes(out_p, s_sz, s, pos, n))
    {
      TEE_Free(out_p);
      return -1;
    }
    *out = out_p;
  }

  *out_s_sz = s_sz;
  return 0;
}

static input_t *destroy_input(input_t *input)
{
  input_t *next = input->ll.next;
  TEE_Free(input->scriptSig);
  TEE_Free(input);
  return next;
}

static output_t *destroy_output(output_t *output)
{
  output_t *next = output->ll.next;
  TEE_Free(output->lockScript);
  TEE_Free(output);
  return next;
}

TEE_Result deserialize_tx(uint8_t *tx_bytes, size_t tx_bytes_len, tx_t **out_tx)
{
  tx_t *tx = NULL;
  size_t pos = 0;
  TEE_Result res = TEE_SUCCESS;
  size_t tx_bytes_left = tx_bytes_len;

  tx = TEE_Malloc(sizeof(tx_t), 0);
  if (tx == NULL)
  {
    return TEE_ERROR_OUT_OF_MEMORY;
  }

  if (deserialize_int(&tx->version, tx_bytes, &pos, &tx_bytes_left))
  {
    res = TEE_ERROR_BAD_PARAMETERS;
    goto err;
  }

  uint64_t n_input_entries = 0;
  if (deserialize_var_int(&n_input_entries, tx_bytes, &pos, &tx_bytes_left))
  {
    res = TEE_ERROR_BAD_PARAMETERS;
    goto err;
  }

  uint64_t i = 0;
  input_t *cur_input = NULL;
  for (i = 0; i < n_input_entries; i++)
  {
    cur_input = TEE_Malloc(sizeof(input_t), 0);
    if (cur_input == NULL)
    {
      res = TEE_ERROR_OUT_OF_MEMORY;
      goto err;
    }


    if (deserialize_bytes(cur_input->tx_hash,
                          sizeof(cur_input->tx_hash),
                          tx_bytes,
                          &pos,
                          &tx_bytes_left))
    {
      destroy_input(cur_input);
      res = TEE_ERROR_BAD_PARAMETERS;
      goto err;
    }

    int start = 0;
    int end = 31;
    while (start < end)
    {
      cur_input->tx_hash[start] ^= cur_input->tx_hash[end];
      cur_input->tx_hash[end] ^= cur_input->tx_hash[start];
      cur_input->tx_hash[start] ^= cur_input->tx_hash[end];
      start++;
      end--;
    }

    if (deserialize_int(&cur_input->index, tx_bytes, &pos, &tx_bytes_left))
    {
      destroy_input(cur_input);
      res = TEE_ERROR_BAD_PARAMETERS;
      goto err;
    }

    if (deserialize_var_string(&cur_input->scriptSig,
                               &cur_input->scriptSig_len,
                               tx_bytes,
                               &pos,
                               &tx_bytes_left))
    {
      destroy_input(cur_input);
      res = TEE_ERROR_BAD_PARAMETERS;
      goto err;
    }

    if (deserialize_int(&cur_input->sequence, tx_bytes, &pos, &tx_bytes_left))
    {
      destroy_input(cur_input);
      res = TEE_ERROR_BAD_PARAMETERS;
      goto err;
    }

    // link into tx inputs
    link_add((ll_t **)&tx->inputs, (ll_t *)cur_input);
  }

  uint64_t n_output_entries = 0;
  if (deserialize_var_int(&n_output_entries, tx_bytes, &pos, &tx_bytes_left))
  {
    res = TEE_ERROR_BAD_PARAMETERS;
    goto err;
  }

  output_t *cur_output = NULL;
  for (i = 0; i < n_output_entries; i++)
  {
    cur_output = TEE_Malloc(sizeof(output_t), 0);
    if (cur_output == NULL)
    {
      res = TEE_ERROR_OUT_OF_MEMORY;
      goto err;
    }

    if (deserialize_int64(&cur_output->value, tx_bytes, &pos, &tx_bytes_left))
    {
      destroy_output(cur_output);
      res = TEE_ERROR_BAD_PARAMETERS;
      goto err;
    }

    if (deserialize_var_string(&cur_output->lockScript,
                               &cur_output->lockScript_len,
                               tx_bytes,
                               &pos,
                               &tx_bytes_left))
    {
      destroy_output(cur_output);
      res = TEE_ERROR_BAD_PARAMETERS;
      goto err;
    }

    link_add((ll_t **)&tx->outputs, (ll_t *)cur_output);
  }

  if (deserialize_int(&tx->locktime, tx_bytes, &pos, &tx_bytes_left))
  {
    res = TEE_ERROR_BAD_PARAMETERS;
    goto err;
  }

  *out_tx = tx;
  return TEE_SUCCESS;

 err:
  cur_input = tx->inputs;
  while (cur_input) { cur_input = destroy_input(cur_input); }

  cur_output = tx->outputs;
  while (cur_output) { cur_output = destroy_output(cur_output); }

  if (tx)
  {
    TEE_Free(tx);
  }

  return res;
}

static uint8_t *serialize_ensure_size(uint8_t **s, size_t n, uint32_t *pos, size_t *cap)
{
  while (*pos + n > *cap)
  {
    *cap += SERIALIZE_INC_AMOUNT;
    *s = TEE_Realloc(*s, *cap);
  }
  return *s;
}

static int serialize_int(uint32_t in, uint8_t **s, uint32_t *pos, size_t *cap)
{
  *s = serialize_ensure_size(s, sizeof(uint32_t), pos, cap);
  if (*s == NULL)
  {
    return -1;
  }

  TEE_MemMove(*s + *pos, &in, sizeof(uint32_t));
  *pos += sizeof(uint32_t);

  return 0;
}

static int serialize_int64(uint64_t in, uint8_t **s, uint32_t *pos, size_t *cap)
{
  *s = serialize_ensure_size(s, sizeof(uint64_t), pos, cap);
  if (*s == NULL)
  {
    return -1;
  }

  TEE_MemMove(*s + *pos, &in, sizeof(uint64_t));
  *pos += sizeof(uint64_t);

  return 0;
}

static int serialize_var_int(uint64_t in, uint8_t **s, uint32_t *pos, size_t *cap)
{
  size_t len = 8;
  if (in <= 0xFFFFFFFF) len = 4;
  if (in <= 0xFFFF) len = 2;
  if (in <= 0xFC) len = 1;

  *s = serialize_ensure_size(s, len, pos, cap);
  if (*s == NULL)
  {
    return -1;
  }

  // TODO fix serialize here, needs to include tag, 0xFE, 0xFD, etc
  
  TEE_MemMove(*s + *pos, &in, len);
  *pos += len;

  return 0;
}

static int serialize_bytes(uint8_t *in, size_t sz_bytes, uint8_t **s, uint32_t *pos, size_t *cap)
{
  *s = serialize_ensure_size(s, sz_bytes, pos, cap);
  if (*s == NULL)
  {
    return -1;
  }

  TEE_MemMove(*s + *pos, in, sz_bytes);
  *pos += sz_bytes;

  return 0;
}

static int serialize_var_string(uint8_t *in, size_t sz_bytes, uint8_t **s, uint32_t *pos, size_t *cap)
{
  if (serialize_var_int(sz_bytes, s, pos, cap))
  {
    return -1;
  }

  if (serialize_bytes(in, sz_bytes, s, pos, cap))
  {
    return -1;
  }

  return 0;
}

static int serialize_sighash(uint8_t **s, uint32_t *pos, size_t *cap)
{
  uint32_t sighash = 1;

  TEE_MemMove(*s + *pos, &sighash, sizeof(uint32_t));
  *pos += sizeof(uint32_t);

  return 0;
}

TEE_Result serialize_tx(tx_t *tx, uint8_t **out, size_t *out_sz)
{
  TEE_Result res = TEE_SUCCESS;
  size_t pos = 0;
  size_t capacity = SERIALIZE_INC_AMOUNT;

  uint8_t *tx_bytes = TEE_Malloc(capacity, 0);
  if (tx_bytes == NULL)
  {
    return TEE_ERROR_OUT_OF_MEMORY;
  }

  if (serialize_int(TX_VERSION, &tx_bytes, &pos, &capacity))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  input_t *cur_input = tx->inputs;
  uint64_t n_input_entries = 0;
  for (; cur_input; cur_input = cur_input->ll.next, n_input_entries++);

  if (serialize_var_int(n_input_entries, &tx_bytes, &pos, &capacity))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  cur_input = tx->inputs;
  for(; cur_input; cur_input = cur_input->ll.next)
  {
    uint8_t tx_hash[32];
    TEE_MemMove(tx_hash, cur_input->tx_hash, sizeof(tx_hash));

    int start = 0;
    int end = 31;
    while (start < end)
    {
      tx_hash[start] ^= tx_hash[end];
      tx_hash[end] ^= tx_hash[start];
      tx_hash[start] ^= tx_hash[end];
      start++;
      end--;
    }

    if (serialize_bytes(tx_hash,
                        sizeof(tx_hash),
                        &tx_bytes,
                        &pos,
                        &capacity))
    {
      res = TEE_ERROR_GENERIC;
      goto out;
    }

    if (serialize_int(cur_input->index, &tx_bytes, &pos, &capacity))
    {
      res = TEE_ERROR_GENERIC;
      goto out;
    }

    if (serialize_var_string(cur_input->scriptSig,
                             cur_input->scriptSig_len,
                             &tx_bytes,
                             &pos,
                             &capacity))
    {
      res = TEE_ERROR_GENERIC;
      goto out;
    }

    if (serialize_int(cur_input->sequence, &tx_bytes, &pos, &capacity))
    {
      res = TEE_ERROR_GENERIC;
      goto out;
    }
  }

  output_t *cur_output = tx->outputs;
  uint64_t n_output_entries = 0;
  for (; cur_output; cur_output = cur_output->ll.next, n_output_entries++);

  if (serialize_var_int(n_output_entries, &tx_bytes, &pos, &capacity))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  cur_output = tx->outputs;
  for(; cur_output; cur_output = cur_output->ll.next)
  {
    if (serialize_int64(cur_output->value, &tx_bytes, &pos, &capacity))
    {
      res = TEE_ERROR_GENERIC;
      goto out;
    }

    if (serialize_var_string(cur_output->lockScript,
                             cur_output->lockScript_len,
                             &tx_bytes,
                             &pos,
                             &capacity))
    {
      res = TEE_ERROR_GENERIC;
      goto out;
    }
  }

  if (serialize_int(tx->locktime, &tx_bytes, &pos, &capacity))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  if (serialize_sighash(&tx_bytes, &pos, &capacity))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }
  
  *out = tx_bytes;
  *out_sz = pos;

  return res;

 out:
  TEE_Free(tx_bytes);
  return res;
}

TEE_Result serialize_signature(mbedtls_mpi *r, mbedtls_mpi *s, uint8_t **out_sig, size_t *out_sig_len)
{
  uint8_t _r[33];
  uint8_t _s[33];
  uint8_t *r_p = &_r[1];
  uint8_t *s_p = &_s[1];

  if (mbedtls_mpi_write_binary(r, r_p, 32))
  {
    return TEE_ERROR_GENERIC;
  }

  if (mbedtls_mpi_write_binary(s, s_p, 32))
  {
    return TEE_ERROR_GENERIC;
  }

  if (r_p[0] >= 0x80)
  {
    _r[0] = 0x00;
    r_p = &_r[0];
  }

  if (s_p[0] >= 0x80)
  {
    _s[0] = 0x00;
    s_p = &_s[0];
  }
  size_t r_len = r_p == &_r[0] ? 33: 32;
  size_t s_len = s_p == &_s[0] ? 33: 32;

  size_t sig_len = 6 +\
    r_len +\
    s_len +\
    1 +\
    1; // one for SIGHASH, one for the VarInt length byte

  uint8_t *sig = TEE_Malloc(sig_len, 0);
  if (sig == NULL)
  {
    return TEE_ERROR_OUT_OF_MEMORY;
  }
  // push forward
  sig += 1;

  // DER magic byte
  sig[0] = 0x30;
  // DER total length byte
  sig[1] = r_len + s_len + 4;
  // DER integer follows byte
  sig[2] = 0x02;
  sig[3] = r_len;

  TEE_MemMove(sig + 4, r_p, r_len);

  // DER integer follows byte
  sig[4 + r_len] = 0x02;
  sig[4 + r_len + 1] = s_len;
  TEE_MemMove(sig + 4 + r_len + 2, s_p, s_len);

  // SIGHASH_ALL
  sig[4 + r_len + 2 + s_len] = 0x01;

  sig -= 1;
  sig[0] = sig_len - 1;

  *out_sig = sig;
  *out_sig_len = sig_len;

  return TEE_SUCCESS;
}

/* --- BASE58 OPERATIONS --- */
char *b58_encode(unsigned char *b58_input, size_t b58_input_len)
{
  unsigned int zero_counter = 0;
  unsigned long long carry = 0;
  char alphabet[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

  size_t size = b58_input_len;

  for (;zero_counter < size && b58_input[zero_counter] == 0; zero_counter++);
  
  size = (zero_counter +                        \
          (size - zero_counter) * 555 / 406 +   \
          1);

  char *b58_out = TEE_Malloc(size, 0);
  if (b58_out == NULL)
  {
    return NULL;
  }

  int i = 0;
  int high = 0;
  
  high = size - 1;

  uint32_t x = 0;
  for (x = 0; x < b58_input_len; x++)
  {
    i = size - 1;
    unsigned char b = b58_input[x];
    for (carry = b; i > high || carry != 0; i--)
    {
      carry = (carry + 256 * (b58_out[i])) & 0xffffffff;
      b58_out[i] = carry % 58;
      carry = carry / 58;
    }
    high = i;
  }

  size_t j = 0;
  for (j = 0; j < size; j++) {
    b58_out[j] = alphabet[(uint8_t)b58_out[j]];
  }
  b58_out[j] = '\0';

  return b58_out;
}

char *b58_encode_check(unsigned char version, unsigned char *input, size_t input_len)
{
  char *ret = NULL;
  unsigned char *out = NULL;
  size_t out_len = 0;
  mbedtls_sha256_context sha256_ctx1;
  mbedtls_sha256_context sha256_ctx2;
  unsigned char sha256_cxsum1[32];
  unsigned char sha256_cxsum2[32];

  out_len = input_len + 5;
  out = TEE_Malloc(out_len, 0);
  if (out == NULL)
  {
    return NULL;
  }

  // explicitly set the version byte for public address
  out[0] = version;

  TEE_MemMove(out + 1, input, input_len);
  
  mbedtls_sha256_init(&sha256_ctx1);

  if (mbedtls_sha256_starts_ret(&sha256_ctx1, 0))
  {
    goto out;
  }

  if (mbedtls_sha256_update_ret(&sha256_ctx1, out, input_len + 1))
  {
    goto out;
  }

  mbedtls_sha256_finish_ret(&sha256_ctx1, sha256_cxsum1);

  mbedtls_sha256_init(&sha256_ctx2);

  if (mbedtls_sha256_starts_ret(&sha256_ctx2, 0))
  {
    goto out;
  }

  if (mbedtls_sha256_update_ret(&sha256_ctx2, sha256_cxsum1, sizeof(sha256_cxsum1)))
  {
    goto out;
  }

  mbedtls_sha256_finish_ret(&sha256_ctx2, sha256_cxsum2);

  TEE_MemMove(out + 1 + input_len, sha256_cxsum2, 4);

  ret = b58_encode(out, out_len);

out:
  TEE_Free(out);
  return ret;
}

/*
 * Called when the instance of the TA is created. This is the first call in
 * the TA.
 */
TEE_Result TA_CreateEntryPoint(void)
{
  return TEE_SUCCESS;
}

/*
 * Called when the instance of the TA is destroyed if the TA has not
 * crashed or panicked. This is the last call in the TA.
 */
void TA_DestroyEntryPoint(void)
{
}

/*
 * Called when a new session is opened to the TA. *sess_ctx can be updated
 * with a value to be able to identify this session in subsequent calls to the
 * TA. In this function you will normally do the global initialization for the
 * TA.
 */
void _utee_flag(char *, size_t);
TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
		TEE_Param __maybe_unused params[4],
		void __maybe_unused **sess_ctx)
{
  uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
                                             TEE_PARAM_TYPE_NONE,
                                             TEE_PARAM_TYPE_NONE,
                                             TEE_PARAM_TYPE_NONE);

  TEE_Result res = TEE_SUCCESS;
  char flag_buf[32] = {0};
  void *obj_id;
  size_t obj_id_len;
  size_t read_bytes = 0;
  TEE_ObjectHandle object;
  TEE_ObjectInfo obj_info;
  TEE_ObjectEnumHandle enumerator;
  serialized_wallet_t serialized_wallet;

  TEE_Result search_result = TEE_SUCCESS;

  if (param_types != exp_param_types)
    return TEE_ERROR_BAD_PARAMETERS;

  /* Unused parameters */
  (void)&params;
  (void)&sess_ctx;

  if (params[0].value.a == 0x1337)
    {
      EMSG("Showing you how to read the flag (it's a flag syscall!)...");
      _utee_flag(flag_buf, sizeof(flag_buf));
      memset(flag_buf, 0, sizeof(flag_buf));
    }

  // Create enumerator and load all wallets from persistent storage
  res = TEE_AllocatePersistentObjectEnumerator(&enumerator);
  if (res != TEE_SUCCESS)
    return res;


  res = TEE_StartPersistentObjectEnumerator(enumerator, TEE_STORAGE_PRIVATE);
  if (res != TEE_SUCCESS && res != TEE_ERROR_ITEM_NOT_FOUND)
    {
      goto out;
    }

  if (res != TEE_ERROR_ITEM_NOT_FOUND)
    {
      do
        {
          search_result = TEE_GetNextPersistentObject(enumerator,
                                                      &obj_info,
                                                      &obj_id,
                                                      &obj_id_len);

          // TODO: how does this work? why is obj_id the valut itself?
          if (search_result == TEE_SUCCESS)
            {
              res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
                                             &obj_id, obj_id_len,
                                             TEE_DATA_FLAG_ACCESS_READ,
                                             &object);
              if (res != TEE_SUCCESS)
                {
                  goto out;
                }

              res = TEE_ReadObjectData(object,
                                       &serialized_wallet,
                                       sizeof(serialized_wallet),
                                       &read_bytes);

              if (res != TEE_SUCCESS)
                {
                  TEE_CloseObject(object);
                  goto out;
                }

              if (read_bytes != sizeof(serialized_wallet))
                {
                  TEE_CloseObject(object);
                  goto out;
                }

              res = load_wallet(&serialized_wallet);
              if (res != TEE_SUCCESS)
                {
                  TEE_CloseObject(object);
                  res = TEE_SUCCESS;
                  goto out;
                }

              TEE_CloseObject(object);
            }
        } while (search_result == TEE_SUCCESS);
    }
  else
    {
      res = TEE_SUCCESS;
    }

 out:
  TEE_FreePersistentObjectEnumerator(enumerator);

  /* If return value != TEE_SUCCESS the session will not be created. */
  return res;
}

/*
 * Called when a session is closed, sess_ctx hold the value that was
 * assigned by TA_OpenSessionEntryPoint().
 */
void TA_CloseSessionEntryPoint(void __maybe_unused *sess_ctx)
{
  (void)&sess_ctx; /* Unused parameter */
}

static TEE_Result create_wallet(uint32_t param_types,
	TEE_Param params[4])
{
  uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
                                             TEE_PARAM_TYPE_NONE,
                                             TEE_PARAM_TYPE_NONE,
                                             TEE_PARAM_TYPE_NONE);
  wallet_t *wallet;
  char *wallet_address;
  mbedtls_ecp_group grp;
  unsigned char pubkey[33];
  unsigned char sha256_cxsum[32];
  mbedtls_sha256_context sha256_ctx;
  mbedtls_ripemd160_context ripemd160_ctx;
  TEE_Result result = TEE_SUCCESS;

  wallet = NULL;

  mbedtls_ecp_group_init( &grp );

  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) != 0)
    {
      // not sure how this could ever happen
      result = TEE_ERROR_GENERIC;
      goto err;
    }
        
  if (param_types != exp_param_types)
    return TEE_ERROR_BAD_PARAMETERS;

  result = allocate_wallet(&wallet);
  if (result != TEE_SUCCESS) goto err;

  if (mbedtls_ecp_gen_keypair(&grp,
                              &wallet->d,
                              &wallet->Q,
                              &Wallet_TA_Random,
                              NULL) != 0)
    {
      result = TEE_ERROR_GENERIC;
      goto err;
    }

  result = set_wallet_address(wallet);
  if (result != TEE_SUCCESS)
  {
    goto err;
  }
  
  wallet->id = g_next_wallet_id++;

  link_add((ll_t **)&g_wallets, (ll_t *)wallet);

  params[0].value.a = wallet->id;

  // add the completed wallet to the persistent storage
  if (save_wallet(wallet) != TEE_SUCCESS)
    {
      ;
    }
  else
    {
      ;
    }

  return TEE_SUCCESS;
 err:
  // delete wallet
  TEE_Free(wallet);
  return result;
}

static TEE_Result get_address_for_wallet(uint32_t param_types,
                                         TEE_Param params[4])
{
  uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT, // wallet id, compression
                                             TEE_PARAM_TYPE_MEMREF_OUTPUT, // wallet address
                                             TEE_PARAM_TYPE_NONE,
                                             TEE_PARAM_TYPE_NONE);

  uint32_t wallet_id = 0;
  uint32_t compression_type;
  wallet_t *wallet = NULL;
  wallet_t *cur_wallet = g_wallets;
  size_t copy_len = 0;
  size_t outbuf_len = 0;
  size_t address_len = 0;
  unsigned char *outbuf = NULL;

  if (param_types != exp_param_types)
    return TEE_ERROR_BAD_PARAMETERS;

  wallet_id = params[0].value.a;
  compression_type = params[1].value.b;

  wallet = NULL;
  cur_wallet = g_wallets;
  for (;cur_wallet; cur_wallet = cur_wallet->ll.next)
  {
    if (cur_wallet->id == wallet_id)
    {
      wallet = cur_wallet;
      break;
    }
  }

  if (wallet == NULL)
  {
    return TEE_ERROR_ITEM_NOT_FOUND;
  }

  outbuf = params[1].memref.buffer;
  outbuf_len = params[1].memref.size;

  address_len = strlen(wallet->address);
  copy_len = outbuf_len > address_len ? address_len : outbuf_len;
  TEE_MemMove(outbuf, wallet->address, copy_len);
  params[1].memref.size = copy_len;

  return TEE_SUCCESS;;
}

static TEE_Result import_key(uint32_t param_types,
	TEE_Param params[4])
{
  uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
                                             TEE_PARAM_TYPE_VALUE_OUTPUT,
                                             TEE_PARAM_TYPE_NONE,
                                             TEE_PARAM_TYPE_NONE);

  TEE_Result res = TEE_SUCCESS;
  wallet_t *wallet;
  mbedtls_ecp_group grp;
  unsigned char privkey[32];
  uint32_t inbuf_len;
  const unsigned char *inbuf;

  if (param_types != exp_param_types)
    return TEE_ERROR_BAD_PARAMETERS;

  inbuf = params[0].memref.buffer;
  inbuf_len = params[0].memref.size;

  // we expect private key size
  if (inbuf_len != 32)
  {
    return TEE_ERROR_BAD_PARAMETERS;
  }

  TEE_MemMove(privkey, inbuf, sizeof(privkey));

  res = allocate_wallet(&wallet);
  if (res != TEE_SUCCESS)
  {
    return res;
  }

  if (mbedtls_mpi_read_binary(&wallet->d, privkey, sizeof(privkey)))
  {
    res = TEE_ERROR_GENERIC;
    goto err;
  }

  mbedtls_ecp_group_init(&grp);

  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) != 0)
  {
    res = TEE_ERROR_GENERIC;
    goto err;
  }

  if (mbedtls_ecp_mul(&grp, &wallet->Q, &wallet->d, &grp.G, Wallet_TA_Random, NULL) != 0)
  {
    res = TEE_ERROR_GENERIC;
    goto err;
  }

  res = set_wallet_address(wallet);
  if (res != TEE_SUCCESS)
  {
    goto err;
  }

  wallet->id = g_next_wallet_id++;

  link_add((ll_t **)&g_wallets, (ll_t *)wallet);

  params[1].value.a = wallet->id;

  // add the completed wallet to the persistent storage
  if (save_wallet(wallet) != TEE_SUCCESS)
    {
      ;
    }
  else
    {
      ;
    }


  return res;

  // only link the wallet in when we're successful
 err:
  TEE_Free(wallet);
  return res;
}

static TEE_Result sign_transaction(uint32_t param_types,
        TEE_Param params[4])
{
  uint32_t exp_param_types = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INPUT,    // wallet id
                                             TEE_PARAM_TYPE_VALUE_INPUT,    // index of input
                                             TEE_PARAM_TYPE_MEMREF_INPUT,   // transaction message
                                             TEE_PARAM_TYPE_MEMREF_OUTPUT); // signed transaction


  uint32_t i = 0;
  tx_t *tx = NULL;
  uint8_t *p2pkh = NULL;
  input_t *s_input = NULL;
  input_t *cur_input = NULL;
  uint32_t input_idx = 0;
  uint32_t wallet_id = 0;
  wallet_t *wallet = NULL;
  uint32_t inbuf_len = 0;
  mbedtls_mpi r;
  mbedtls_mpi s;
  mbedtls_ecp_group grp;
  wallet_t *cur_wallet = NULL;
  output_t *cur_output = NULL;
  uint32_t outbuf_len = 0;
  unsigned char *outbuf = NULL;
  TEE_Result res = TEE_SUCCESS;
  uint8_t *signed_tx = NULL;
  size_t signed_tx_len = 0;
  size_t solve_script_len = 0;
  uint8_t *signature_form = NULL;
  size_t signature_form_len = 0;
  uint8_t *signature = NULL;
  size_t signature_len = 0;
  uint32_t copy_len = 0;
  unsigned char *copy_inbuf = NULL;
  const unsigned char *inbuf = NULL;
  unsigned char sha256_cxsum1[32];
  unsigned char sha256_cxsum2[32];
  mbedtls_sha256_context sha256_ctx1;
  mbedtls_sha256_context sha256_ctx2;

  if (param_types != exp_param_types)
    return TEE_ERROR_BAD_PARAMETERS;

  wallet_id = params[0].value.a;

  // lookup wallet
  wallet = NULL;
  cur_wallet = g_wallets;
  for (; cur_wallet; cur_wallet = cur_wallet->ll.next)
  {
    if (cur_wallet->id == wallet_id)
    {
      wallet = cur_wallet;
      break;
    }
  }

  if (wallet == NULL)
  {
    res = TEE_ERROR_ITEM_NOT_FOUND;
    goto out;
  }

  // now get index of the input to sign
  input_idx = params[1].value.a;
  
  // we assume this transaction is coming from a pybitcointools 'mktx'
  // it's a serialized transaction with none of the scriptSig fields populated
  inbuf = params[2].memref.buffer;
  inbuf_len = params[2].memref.size;

  copy_inbuf = TEE_Malloc(inbuf_len, 0);
  if (copy_inbuf == NULL)
  {
    res = TEE_ERROR_OUT_OF_MEMORY;
    goto out;
  }
  TEE_MemMove(copy_inbuf, inbuf, inbuf_len);

  // deserialize inbuf
  res = deserialize_tx(copy_inbuf, inbuf_len, &tx);
  if (res != TEE_SUCCESS)
  {
    goto out;
  }

  // NOTE: should be okay to just overwrite the new tx?
  // sanitize the scriptSigs and also grab the idx of the input to solve for
  s_input = NULL;
  cur_input = tx->inputs;
  for (i = 0; cur_input; cur_input = cur_input->ll.next, i++)
  {
    if (i == input_idx)
    {
      s_input = cur_input;
    }

    TEE_Free(cur_input->scriptSig);
    cur_input->scriptSig = NULL;
    cur_input->scriptSig_len = 0;
  }

  if (s_input == NULL)
  {
    res = TEE_ERROR_BAD_PARAMETERS;
    goto out;
  }

  // serialize the message to prepare for signing
  // first thing here is to replace the input's scriptSig field
  // with a P2PKH (Pay-2-Public-Key-Hash) of our address
  p2pkh = TEE_Malloc(P2PKH_LENGTH, 0);
  if (p2pkh == NULL)
  {
    res = TEE_ERROR_OUT_OF_MEMORY;
    goto out;
  }

  p2pkh[0] = 0x76;
  p2pkh[1] = 0xa9;
  p2pkh[2] = 0x14;

  TEE_MemMove(p2pkh + 3, &wallet->ripemd160_cxsum, 20);

  p2pkh[23] = 0x88;
  p2pkh[24] = 0xac;

  s_input->scriptSig = p2pkh;
  s_input->scriptSig_len = P2PKH_LENGTH;

  // now the internal tx is ready to be signed, serialize it back out to
  // give ourselves something to sign
  res = serialize_tx(tx, &signature_form, &signature_form_len);
  if (res != TEE_SUCCESS)
  {
    goto out;
  }

  // we have to add the sighash code now
  // CTF - bug idea? append the null sighash here
  /* size_t final_size = signature_form_len + 4; */
  /* signature_form = TEE_Realloc(signature_form, final_size); */
  /* if (signature_form == NULL) */
  /* { */
  /*   res = TEE_ERROR_OUT_OF_MEMORY; */
  /*   goto out; */
  /* } */
  /* // bug idea end, to add bug, remove the realloc call */
  /* uint32_t sighash_hashcode = 1; */
  /* TEE_MemMove(signature_form + signature_form_len, */
  /*             &sighash_hashcode, */
  /*             sizeof(sighash_hashcode)); */

  // finally, hash the message, this is what is ECDSA signed

  mbedtls_sha256_init(&sha256_ctx1);

  if (mbedtls_sha256_starts_ret(&sha256_ctx1, 0))
  {
    goto out;
  }

  if (mbedtls_sha256_update_ret(&sha256_ctx1, signature_form, signature_form_len))
  {
    goto out;
  }

  mbedtls_sha256_finish_ret(&sha256_ctx1, sha256_cxsum1);

  mbedtls_sha256_init(&sha256_ctx2);

  if (mbedtls_sha256_starts_ret(&sha256_ctx2, 0))
  {
    goto out;
  }

  if (mbedtls_sha256_update_ret(&sha256_ctx2, sha256_cxsum1, sizeof(sha256_cxsum1)))
  {
    goto out;
  }

  mbedtls_sha256_finish_ret(&sha256_ctx2, sha256_cxsum2);

  // now sign that hash
  mbedtls_ecp_group_init( &grp );

  if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256K1) != 0)
  {
      // not sure how this could ever happen
      res = TEE_ERROR_GENERIC;
      goto out;
  }

  mbedtls_mpi_init(&r);
  mbedtls_mpi_init(&s);

  if (mbedtls_ecdsa_sign(&grp,
                         &r,
                         &s,
                         &wallet->d,
                         sha256_cxsum2,
                         sizeof(sha256_cxsum2),
                         &Wallet_TA_Random,
                         NULL))
  {
    res = TEE_ERROR_GENERIC;
    goto out;
  }

  // signing is done, now we need encode and emplace the signature
  res = serialize_signature(&r, &s, &signature, &signature_len);
  if (res != TEE_SUCCESS)
  {
    goto out;
  }

  // one more task, append the pubkey and adjust sizes
  // total size is signature length, pubkey size, and one byte for pubkey size encoding
  solve_script_len = signature_len + 33 + 1;
  signature = TEE_Realloc(signature, solve_script_len);
  if (signature == NULL)
  {
    res = TEE_ERROR_OUT_OF_MEMORY;
    goto out;
  }
  signature[signature_len] = 33;
  wallet_pubkey(wallet, &signature[signature_len + 1]);

  // emplace the encoded signature
  TEE_Free(s_input->scriptSig);

  s_input->scriptSig = signature;
  s_input->scriptSig_len = solve_script_len;

  // return signed transaction
  res = serialize_tx(tx, &signed_tx, &signed_tx_len);
  if (res != TEE_SUCCESS)
  {
    goto out;
  }

  // subtract sighash appendage
  signed_tx_len -= 4;

  // if we got here, we're all done
  outbuf = params[3].memref.buffer;
  outbuf_len = params[3].memref.size;

  copy_len = signed_tx_len > outbuf_len ? \
    outbuf_len : signed_tx_len;

  TEE_MemMove(outbuf, signed_tx, copy_len);
  params[3].memref.size = copy_len;

 out:
  if (copy_inbuf)
  {
    TEE_Free(copy_inbuf);
  }

  if (signature_form)
  {
    TEE_Free(signature_form);
  }

  // clean up everything
  if (tx)
  {
    cur_input = tx->inputs;
    while (cur_input) { cur_input = destroy_input(cur_input); }

    cur_output = tx->outputs;
    while (cur_output) { cur_output = destroy_output(cur_output); }

    TEE_Free(tx);
  }

  return res;
}

/*
 * Called when a TA is invoked. sess_ctx hold that value that was
 * assigned by TA_OpenSessionEntryPoint(). The rest of the paramters
 * comes from normal world.
 */
TEE_Result TA_InvokeCommandEntryPoint(void __maybe_unused *sess_ctx,
			uint32_t cmd_id,
			uint32_t param_types, TEE_Param params[4])
{
	(void)&sess_ctx; /* Unused parameter */

	switch (cmd_id) {
	case TA_WALLET_CMD_CREATE_WALLET:
          return create_wallet(param_types, params);
        case TA_WALLET_CMD_GET_ADDRESS_FOR_WALLET:
          return get_address_for_wallet(param_types, params);
	case TA_WALLET_CMD_IMPORT_KEY:
          return import_key(param_types, params);
        case TA_WALLET_CMD_SIGN_TRANSACTION:
          return sign_transaction(param_types, params);
	default:
          return TEE_ERROR_BAD_PARAMETERS;
	}
}
