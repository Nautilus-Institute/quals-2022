#include "mes.h"
#include "rot13.h"
#include "rsa.h"
#include "cipher.h"
#include "util.h"


int main() {
  setvbuf(stdout, NULL, _IONBF, 0);

  int ck = 0;
  vector<shared_ptr<Cipher>> ciphers = {};
  vector<string> cipheroptions = {};
  string prompt = "Welcome to the Nautilus Encryption Service, where we offer a range of options to secure the messages you share with friends.\n";
  cout << prompt; 

  while(1){

    vector<string> opts = {"Create Key", "Encrypt Message", "Decrypt Message"};
    prompt = "What can we assist you with?";
    unsigned int menu_option = menuprompt(prompt, opts);
    
    if (menu_option == 0){
      if (ciphers.size() > 16) {
        cout << "We're sorry to inform you that you have reached your maximum cipher alotment";
        continue;
      }
      
      shared_ptr<Cipher> newCipher;
      if (!ck) {
          prompt = "We have a hand-curated selection of encryption algorithms just for you. Which would you prefer?";
          ck ++;
      }

      else prompt = "Please select a new encryption algorithm to add to your collection.";
      vector<string> cipheropts = {"Missouri Encryption Standard (MES)", "Algorithm 13", "Algorithm MCCCXXXVII", "Algorithm X (Pronounced Algorithm 10)"};
      unsigned int createKey_option = menuprompt(prompt, cipheropts);
      string name; 
        
      unsigned int success;
      if (createKey_option == 0) {
        newCipher = make_shared<MES>();
        success = 1;
      }

      else if (createKey_option == 1) {
        newCipher = make_shared<ROT13>();
        success = 1;
      }

      else if (createKey_option == 2) {
          auto key = make_shared<BigInt>(inputHex("Please enter a key.", 1), true);
          newCipher = make_shared<Caesar>();
          success = newCipher->createKey(key);
          cout << endl;
      }
    
      else if (createKey_option == 3) {
          newCipher = make_shared<RSA>();
          shared_ptr<BigInt> e = make_shared<BigInt>(inputHex("\nPlease enter a key.", MAXKEYLEN), true);
          shared_ptr<BigInt> pd = make_shared<BigInt>(inputHex("\nPlease enter another key.", MAXKEYLEN), true);
          shared_ptr<BigInt> qn = make_shared<BigInt>(inputHex("\nPlease enter another key.", MAXKEYLEN), true);
          success = newCipher->createKey(e, pd, qn);
          cout << endl;
      }

      else {
        success = 0;
      }

      if (!success) {
        cout << "Our apologies, we're not sure what to do with this." << endl;
        exit(0);
      }
      
      ciphers.push_back(newCipher);
      cipheroptions.push_back(cipheropts.at(createKey_option)); 
    }

    else if (menu_option == 1) {

      if (ciphers.size() == 0) {
        cout << "Unfortunately, you have no keys at this time." << endl;
        continue;
      }

      unsigned int encrypt_option = menuprompt("Please select a key.", cipheroptions);
      if (encrypt_option >= ciphers.size()) {
          cout << "Our most sincere regrets, but we are unable to fulfill this request. Goodbye." << endl;
          exit(0);
      }

      string plain = input("Please input a message to encrypt.");
      Message message(plain);

      if (ciphers.at(encrypt_option)->encrypt(message)) {
        cout << "\nYour encrypted message is:\n\n" << message.getCipherText() << endl << endl;
      }
      else {
          cout << "We encountered an unexpected error. Please try again later" << endl << endl;
          continue;
      }
    }

    else if (menu_option == 2) {

      if (ciphers.size() == 0) {
        cout << "Unfortunately, you have no keys at this time." << endl;
        continue;
      }

      unsigned int decrypt_option = menuprompt("Please select a key.", cipheroptions);
      if (decrypt_option >= ciphers.size()) {
          cout << "Our most sincere regrets, but we are unable to fulfill this request. Goodbye." << endl;
          exit(0);
      }

      unsigned int n = inputN("How many lines do you have to decrypt?");
      if (n > 8){
          cout << "Where did you get these ciphertexts? Surely not from us!" << endl;
          exit(0);
      }

      Message message("");
      string s = inputHex("Please input the message to decrypt.", MAXCIPHERLEN) + "\n";
      for (unsigned int i = 1; i < n; i++){
          s += inputHex("", MAXCIPHERLEN) + "\n";
      }
      message.setCipherText(s);
      
      if (ciphers.at(decrypt_option)->decrypt(message)) {
        cout << "\nYour decrypted message is:\n\n" << message.getPlainText() << endl << endl;
      }

      else {
        cout << "We encountered an unexpected error. Please try again later" << endl << endl;
        continue;
      }
    }
    
    else {
        cout << "Our most sincere regrets, but we are unable to fulfill this request. Goodbye." << endl;
        exit(0);
    }
  }
  exit(0);
}