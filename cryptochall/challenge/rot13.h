#ifndef _ROT13_H
#define _ROT13_H

#include "caesar.h"
#include "util.h"

class ROT13 : public Caesar {

  public:
    ROT13() {
      key1_ = make_shared<BigInt>(14);
      key2_ = make_shared<BigInt>(12);
    }
    unsigned int createKey(shared_ptr<BigInt> x=nullptr, shared_ptr<BigInt> y= nullptr, shared_ptr<BigInt> z= nullptr);

};

unsigned int ROT13::createKey(shared_ptr<BigInt> x, shared_ptr<BigInt> y,
                shared_ptr<BigInt> z) {
  return 1;
}
  
#endif

