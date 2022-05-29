/*
 * Copyright (c) 2016-2017, Linaro Limited
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
#ifndef TA_WALLET_H
#define TA_WALLET_H

/*
 * This UUID is generated with uuidgen
 * the ITU-T UUID generator at http://www.itu.int/ITU-T/asn1/uuid.html
 */

// 7dc089d2-883b-4f7b-8154-ea1db9f1e7c3
#define TA_WALLET_UUID \
  { 0x7dc089d2, 0x883b, 0x4f7b, \
  { 0x81, 0x54, 0xea, 0x1d, 0xb9, 0xf1, 0xe7, 0xc3} }

/* The function IDs implemented in this TA */

#define TA_WALLET_CMD_CREATE_WALLET          0
#define TA_WALLET_CMD_GET_ADDRESS_FOR_WALLET 1
#define TA_WALLET_CMD_IMPORT_KEY             2
#define TA_WALLET_CMD_SIGN_TRANSACTION       3
#define TA_WALLET_CMD_ASSIGN_WALLET_PASSWORD 4

// 3 prefix bytes + 20 byte ripemd of pubkey + 2 suffix bytes
#define P2PKH_LENGTH 25

#endif /*TA_WALLET_H*/
