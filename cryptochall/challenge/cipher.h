#ifndef _CIPHER_H
#define _CIPHER_H

#include <memory>
#include <fstream>
#include <sstream> 

#include "biginteger.h"

class Cipher {
    
    public: 
        virtual unsigned int createKey(shared_ptr<BigInt> x=nullptr, shared_ptr<BigInt> y=nullptr, shared_ptr<BigInt> z=nullptr) = 0;
        virtual unsigned int encrypt(Message& message) = 0;
        virtual unsigned int decrypt(Message& message) = 0;

}; 

class FlagOTP : public Cipher {
    
    public: 
        unsigned int createKey(shared_ptr<BigInt> x=nullptr, shared_ptr<BigInt> y=nullptr, shared_ptr<BigInt> z=nullptr) { return 1;};
        unsigned int encrypt(Message& message);
        unsigned int decrypt(Message& message) { return 1;};
        
};

unsigned int FlagOTP::encrypt(Message& message) {
    string plain = message.getPlainText();
    ifstream flagstream("/perribus/challenge/flag.txt");
    stringstream flagbuff;
    flagbuff << flagstream.rdbuf();
    flagstream.close();
    string flag = flagbuff.str();
    for (long unsigned int i = 0; i < flag.length() && i < plain.length(); i++){
        plain[i]^=flag[i];
    }
    message.addCipherText(BigInt(plain, false).toHexString());
    return 1;
}
#endif
