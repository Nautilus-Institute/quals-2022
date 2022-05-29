#ifndef _MESSAGE_H
#define _MESSAGE_H

#include <string>

using namespace std;

class Message {
 private:
  string plain_ = "";
  string cipher_ = "";

 public:
  Message();
  Message(string pt, string ct = "") : plain_(pt), cipher_(ct) {}

  string getCipherText() const { return cipher_; }
  void setPlainText(const string& pt) { plain_ = pt; }
  string getPlainText() const { return plain_; };
  void setCipherText(const string& ct) { cipher_ = ct; }
  void addCipherText(const string& ct) { 
    if (cipher_.length() != 0) cipher_ += "\n";
    cipher_ += "0x" + ct; 
  }
};




#endif