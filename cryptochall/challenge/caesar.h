#ifndef _CAESAR_H
#define _CAESAR_H

#include "cipher.h"
#include "util.h"

const static BigInt lower = BigInt(65);
const static BigInt upper = BigInt(97);
const static BigInt alpha = BigInt(26);

class Caesar : public Cipher {
 protected:
  shared_ptr<BigInt> key1_;
  shared_ptr<BigInt> key2_;

 public:

  virtual unsigned int encrypt(Message& message);
  virtual unsigned int decrypt(Message& message);
  virtual unsigned int createKey(shared_ptr<BigInt> x, shared_ptr<BigInt> y=nullptr, shared_ptr<BigInt> z=nullptr);

  BigInt applyKey(const BigInt& b, const BigInt& key) const;
};

unsigned int Caesar::createKey(shared_ptr<BigInt> x, shared_ptr<BigInt> y/* =nullptr */, shared_ptr<BigInt> z/* =nullptr */) {
  if (x->length() < 1 || x->length() > 1 || *x < zero || *x > alpha) return 0;
  key1_ = x;
  key2_ = make_shared<BigInt>(alpha - *x);
  return 1;
}

unsigned int Caesar::encrypt(Message& message) {
  if (key1_ == nullptr) return 0;
  string mess = message.getPlainText();
  for (long unsigned int i = 0; i < mess.length(); i+=32){
    string s = mess.substr(i, 32);
    BigInt plain = BigInt(s, false);
    BigInt cipher = applyKey(plain, *key1_);
    message.addCipherText(cipher.toHexString());
  }
  return 1;
}

unsigned int Caesar::decrypt(Message& message) {
  if (key2_ == nullptr) return 0;
  string s = message.getCipherText();
  istringstream iss(s);
  string token = "";
  string plaintext = "";
  while (getline(iss, token)) {
    if (token.length() > 66) return 0;
    BigInt cipher = BigInt(token, true);
    BigInt plain = applyKey(cipher, *key2_);
    plaintext += plain.toString();
  }
  message.setPlainText(plaintext);
  return 1;
}

BigInt Caesar::applyKey (const BigInt& b, const BigInt& key) const {
  BigInt result(0);
  BigInt base(1);
  for (unsigned int i = 0; i < b.length(); i++) {
      BigInt c = (b/base) % BASE;
      BigInt tmp = c;
      BigInt l = c < upper ? lower : upper;
      c -= l;
      if (zero <= c && c < alpha) {
        BigInt x = c + key;
        BigInt y = x % alpha;
        BigInt z = l + y;
        result += z * base;
      }
      else {result += tmp * base;}
      base *= BASE;
    }
  return result;
}

#endif
