#ifndef _MES_H
#define _MES_H

#include <memory>

#include "biginteger.h"
#include "cipher.h"

class MES : public Cipher {

  uint8 b64[65] = {0};
  uint rofl = 16;

 public:
  MES();

  unsigned int createKey(shared_ptr<BigInt> x = nullptr, shared_ptr<BigInt> y = nullptr,
                shared_ptr<BigInt> z = nullptr);
  unsigned int encrypt(Message& message);
  unsigned int decrypt(Message& message);
};

MES::MES() {
  for (uint8 i = 0; i < 26; i++) b64[i] = 65 + i;
  for (uint8 i = 26; i < 52; i++) b64[i] = 97 + i - 26;
  for (uint8 i = 52; i < 62; i++) b64[i] = 48 + i - 52;
  b64[62] = '+';
  b64[63] = '/';
  b64[64] = 0;
}

unsigned int MES::createKey(shared_ptr<BigInt> x, shared_ptr<BigInt> y,
                shared_ptr<BigInt> z) {
    return 1;
}

unsigned int MES::encrypt(Message& message) {
  string mess = message.getPlainText();
  for (long unsigned int i = 0; i < mess.length(); i +=24) {
    string s = mess.substr(i, 24);
    BigInt plain = BigInt(s, false);
    BigInt cipher(0), base(1);
    unsigned int len = plain.length();
    if (len % 3 != 0){
      plain *= 4;
      cipher += (uint8)'=';
      base *= BASE;
    }
    if (len % 3 == 1){
      plain *= 4;
      cipher += (uint8)'=' * base;
      base *= BASE;
    }
    while (plain > zero) {
      cipher += (uint8)b64[plain.getDigit(0) % 64] * base;
      plain /= 64;
      base *= BASE;
    }
    message.addCipherText(cipher.toHexString());
  }
  return 1;
}

unsigned int MES::decrypt(Message& message) {
  string s = message.getCipherText();
  istringstream iss(s);
  string token = "";
  string plaintext = "";
  while (getline(iss, token)) {
    if (token.length() > 66) return 0;
    BigInt cipher = BigInt(token, true);
    BigInt plain(0), base(1);
    while (cipher > zero) {
      uint ch = cipher.getDigit(0) % BASE;
      cipher /= BASE;
      if (ch == '=') {
        base *= 64;
        continue;
      }
      uint i = 0;
      for (; i < 64; i++) {
        if (ch == (uint8)b64[i]) break;
      }
      if (i == 64) return 0;
      plain += (i)*base;
      base *= 64;
    }
    plaintext += plain.toString();
  }
  message.setPlainText(plaintext);
  return 1;
}

#endif