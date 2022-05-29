#ifndef _RSA_H
#define _RSA_H

#include <random>
#include <functional>

#include "cipher.h"

static const int MAXN = 32;
static const int MINN = 24;
class RSA : public Cipher {
  int mlen_;
  int usage_ = 0;
  std::function<BigInt(shared_ptr<BigInt>)> totient_ = 0;
  shared_ptr<BigInt> e_;
  shared_ptr<BigInt> d_;
  shared_ptr<BigInt> n_;

 public:
  shared_ptr<BigInt>& getE() { return e_; };
  shared_ptr<BigInt>& getD() { return d_; };
  shared_ptr<BigInt>& getN() { return n_; };
  BigInt encryptBlock(BigInt b);
  BigInt decryptBlock(BigInt b);
  unsigned int encrypt(Message& m);
  unsigned int decrypt(Message& m);
  unsigned int createKey(shared_ptr<BigInt> x, shared_ptr<BigInt> y, shared_ptr<BigInt> z);
  unsigned int messageLength(const BigInt& n) {return (n.length()/2) - 2;}
  unsigned int checkKey(const shared_ptr<BigInt>& e, const shared_ptr<BigInt>& d, const shared_ptr<BigInt>& n);
};

unsigned int RSA::createKey(shared_ptr<BigInt> e, shared_ptr<BigInt> p, shared_ptr<BigInt> q){
  if (checkKey(e, p, q)) {
    getE() = e;
    getD() = p; //check if p is actually an d
    getN() = q; //check if q is actually an n
    mlen_ = messageLength(*p);
    return true;
  }
  else {
    if (p->length() != q->length()) return false;
    if ((p->length() < MINN/2) || (p->length() > MAXN/2)) return false;
    if ((q->length() < MINN/2) || (q->length() > MAXN/2)) return false;
    
    if (p->negative() || q->negative()) return false;
    if (!p->isPrime(MAXN) && !q->isPrime(MAXN)) return false;

    shared_ptr<BigInt> n = make_shared<BigInt>((*p) * (*q));

    shared_ptr<BigInt> phi = make_shared<BigInt>((*p-1) * (*q-1));
    if (!e->isCoPrime(*phi)) return false;

    shared_ptr<BigInt> d = make_shared<BigInt>(inv(*e, *phi));
    if ((*e * *d) % *phi != 1) return false;
    if (!checkKey(e, d, n)) return false;

    totient_ =  [&](shared_ptr<BigInt> ee) {
          *p -= 1;
          *q -= 1;
          BigInt euler = (*p)*(*q);
          BigInt tmp;
          while (*q != 0) {
            tmp = *q;
            *q = *p % *q;
            *p = tmp;
          }
          *p = euler / *p;
          *p = inv(*ee, *p);
          return *p;
    };
    getE() = e;
    getD() = d;
    getN() = n;
    mlen_ = messageLength(*n);
    return 1;
  }
}

unsigned int RSA::encrypt(Message& message) {
  if (e_ == nullptr || n_ == nullptr || d_ == nullptr) return false;
  string pt = message.getPlainText();
  string ct = "";
  for (unsigned int i = 0; i < pt.length(); i += mlen_) {
    message.addCipherText(encryptBlock(BigInt(pt.substr(i, mlen_), false)).toHexString());  
  }
  return 1;
}

unsigned int RSA::decrypt(Message& message) {
  if (e_ == nullptr || n_ == nullptr || d_ == nullptr) return false;
  
  usage_++;
  if (usage_ == 10 && totient_ ) {
    if (inputYN(string("You seem to be using this key a lot. Would you like to speed things up?\n"))){
      auto newD = make_shared<BigInt>(totient_(e_));
      getD() = newD;
    }
  }

  string ct = message.getCipherText();
  std::istringstream iss(ct);
  string token = "";
  string pt = "";
  unsigned int i = 0;
  while (getline(iss, token)) {
    if (token.length() > ((MAXN * 2) + 2)) return 0; 
    BigInt b(token, true);
    if (b >= *n_) return 0;
    pt += decryptBlock(b).toString();
    i++;
  }
  message.setPlainText(pt);
  return 1;
}


BigInt RSA::encryptBlock(BigInt b) { return pow(b, *e_, *n_);}

BigInt RSA::decryptBlock(BigInt b) { return pow(b, *d_, *n_);};

unsigned int RSA::checkKey(const shared_ptr<BigInt>& e, const shared_ptr<BigInt>& d, const shared_ptr<BigInt>& n){
  
  if (n->negative() || d->negative() || e->negative()) return 0;
  if ((e->length() < 3) || (e->length() > MAXN)) return 0;
  if ((d->length() < MINN/2) || (d->length() > MAXN)) return 0;
  if ((n->length() < MINN) || (n->length() > MAXN)) return 0;
  if (*d > *n) return 0;

  std::random_device rd;
  BigInt m_tmp, m2_tmp, c_tmp;
  for (unsigned int k = 0; k < 5; k++) {
    stringstream sstream;
    for (unsigned int i = 0; i < messageLength(*n)/2; i++) {
      sstream << hex << rd();
    }
    m_tmp = BigInt(sstream.str().substr(0, messageLength(*n)), false);
    c_tmp = pow(m_tmp, *e, *n);
    m2_tmp = pow(c_tmp, *d, *n);
    if (m2_tmp != m_tmp) return 0;
    
  }
  return 1;
}


#endif