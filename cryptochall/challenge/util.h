#ifndef _UTIL_H
#define _UTIL_H

#include <cstring>
#include <iostream>
#include <vector>
#include "message.h"

typedef uint8_t uint8;

#define MAXPLAINLEN 128
#define MAXCIPHERLEN 64
#define MAXKEYLEN MAXN

bool isHex(const string& s) {
  for (long unsigned int i = 0; i < s.length(); i++) {
    if ((s.at(i) >= 48 && s.at(i) <= 57) ||
        (s.at(i) >= 65 && s.at(i) <= 70) || (s.at(i) >= 97 && s.at(i) <= 102)) {
      continue;
    } 
    else
      return false;
  }
  return true;
}

// Borrowed from: https://github.com/o-o-overflow/dc2019f-aoool-public/blob/master/service/src/utils.cpp
string read_line() {
  char buffer[MAXPLAINLEN + 16];
  memset(buffer, 0, MAXPLAINLEN + 16);

  size_t i = 0;
  int n;
  while (i < MAXPLAINLEN) {
    n = fread(&buffer[i], 1, 1, stdin);
    if (n != 1) {
      exit(0);
    }
    i++;
    if (buffer[i - 1] == '\n') {
      break;
    }
  }

  buffer[i-1] = '\0';
  string ret(buffer);
  return ret;
}

string read_n() {
  string s = read_line();
  cout << endl;

  if(s.length() > 1) return "9";
  s = s.substr(0, 1);
  return s;
}

unsigned int menuprompt(const string& prompt, const vector<string>& options) {
  cout << prompt << endl << endl;
  for (long unsigned int i = 0; i < options.size(); i++) {
    cout << i << ". " << options.at(i) << endl;
  }
  cout << endl << "> ";
  string s = read_n();
  if (!isdigit(s[0])) return 32;
  unsigned int n = stoi(s);
  if (n >= options.size()) {
    cout << "We regret to inform you that this option is not currently "
            "available.\n";
    exit(0);
  }
  return n;
}

string inputHex(const string& prompt, unsigned int max){
  if (prompt != "") {
    cout << prompt << endl << endl;
  }
  cout << "> ";
  string s = read_line();
  if (s.substr(0,2) != "0x" || !isHex(s.substr(2)) || s.length() > (max*2)+2) {
    cout << "We're sorry to inform you that this input is invalid. Goodbye." << endl;
    exit(0);
  }
  return s;
}


string input(const string& prompt) {
  cout << prompt << endl << endl;
  cout << "> ";
  string s = read_line();
  return s;
}

unsigned int inputN(const string& prompt) {
  cout <<  prompt << endl << endl;
  cout << "> ";
  string s = read_n();
  if (!isdigit(s[0])) return 32;
  int n = stoi(s); 
  return n;
}

bool inputYN(const string& prompt) {
  cout << endl << prompt << endl << endl;
  cout << "y/N > ";
  char n = read_n()[0];
  if (n == 'y' || n == 'Y') {
    return true;
  }
  if (n == 'n' || n == 'N') {
    return false;
  }
  cout << "This was a yes or no question! Goodbye." << endl;
  exit(0);
}



#endif
