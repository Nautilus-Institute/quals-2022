#ifndef _BIGINT_H
#define _BIGINT_H

#include <climits>
#include "util.h"
#include <sstream> 

// Based on:
// http://homepage.cs.uiowa.edu/~sriram/30/fall03/code/bigint.h
// http://homepage.cs.uiowa.edu/~sriram/30/fall03/code/bigint.cxx

static const string lookup[256] = {"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0a", "0b", "0c", "0d", "0e", "0f", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "1a", "1b", "1c", "1d", "1e", "1f", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "2a", "2b", "2c", "2d", "2e", "2f", "30", "31", "32", "33", "34", "35", "36", "37", "38", "39", "3a", "3b", "3c", "3d", "3e", "3f", "40", "41", "42", "43", "44", "45", "46", "47", "48", "49", "4a", "4b", "4c", "4d", "4e", "4f", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "5a", "5b", "5c", "5d", "5e", "5f", "60", "61", "62", "63", "64", "65", "66", "67", "68", "69", "6a", "6b", "6c", "6d", "6e", "6f", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79", "7a", "7b", "7c", "7d", "7e", "7f", "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "8a", "8b", "8c", "8d", "8e", "8f", "90", "91", "92", "93", "94", "95", "96", "97", "98", "99", "9a", "9b", "9c", "9d", "9e", "9f", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "aa", "ab", "ac", "ad", "ae", "af", "b0", "b1", "b2", "b3", "b4", "b5", "b6", "b7", "b8", "b9", "ba", "bb", "bc", "bd", "be", "bf", "c0", "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "ca", "cb", "cc", "cd", "ce", "cf", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "da", "db", "dc", "dd", "de", "df", "e0", "e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8", "e9", "ea", "eb", "ec", "ed", "ee", "ef", "f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "f9", "fa", "fb", "fc", "fd", "fe", "ff"};
static const int BASE = 256;


using uint8 = uint8_t;

static const int pos = 1;
static const int neg = -1;
static const unsigned int MAXLEN = 65;
static const unsigned int MAXBUFF = 75;

class BigInt {
 public:
  BigInt() : length_(1), sign_(pos) {}
  BigInt(int);
  BigInt(const BigInt& rhs)
      : sign_(rhs.sign()) {copyDigits(rhs);};
  BigInt(string s, bool hexadecimal=true);
  const BigInt& operator=(const BigInt& rhs);
  ~BigInt(){};

  void copyDigits(const BigInt& rhs);
  uint8 getDigit(unsigned int k) const;
  void pushDigit(uint8 value);
  void setDigit(unsigned int k, uint8 value);
  void setDigit(unsigned int k, const BigInt& value);

  string toString() const;  // convert to string
  string toHexString() const; 

  bool negative() const { return sign_ < 0 ; };
  bool positive() const { return sign_ > 0; };
  bool odd() const { return (getDigit(0) % 2); };
  unsigned int length() const { return length_; };
  int sign() const { return sign_; };

  BigInt& operator+=(const BigInt&);
  BigInt& operator-=(const BigInt&);
  BigInt& operator*=(const BigInt&);
  BigInt& operator*=(int num);
  BigInt& operator/=(const BigInt&);
  BigInt& operator/=(int num);
  BigInt& operator%=(const BigInt&);
  BigInt& operator>>=(int i);
  BigInt& operator++();

  bool isCoPrime(const BigInt& rhs) const;
  bool isPrime(int k) const;

  
 private:
  uint8 digits_[MAXBUFF] = {0};
  unsigned int length_;
  int sign_;

  void normalize();
  void divide(const BigInt& lhs, const BigInt& rhs, BigInt& quotient,
              BigInt& remainder);
};
ostream& operator<<(ostream&, const BigInt&);
istream& operator>>(istream&, BigInt&);

bool operator==(const BigInt& lhs, const BigInt& rhs);
bool operator<(const BigInt& lhs, const BigInt& rhs);
bool operator!=(const BigInt& lhs, const BigInt& rhs);
bool operator>(const BigInt& lhs, const BigInt& rhs);
bool operator>=(const BigInt& lhs, const BigInt& rhs);
bool operator<=(const BigInt& lhs, const BigInt& rhs);

BigInt operator+(const BigInt& lhs, const BigInt& rhs);
BigInt operator-(const BigInt& lhs, const BigInt& rhs);
BigInt operator*(const BigInt& lhs, const BigInt& rhs);
BigInt operator*(const BigInt& lhs, int num);
BigInt operator*(int num, const BigInt& rhs);
BigInt operator/(const BigInt& lhs, const BigInt& rhs);
BigInt operator/(const BigInt& lhs, int num);
BigInt operator%(const BigInt& lhs, const BigInt& rhs);

BigInt gcdExtended(BigInt a, const BigInt& b, BigInt* x, BigInt* y);
BigInt inv(const BigInt& base, const BigInt& n);
BigInt pow(const BigInt& base, const BigInt& exp, const BigInt& mod);
bool millerTest(BigInt d, const BigInt& n);

const static BigInt zero = BigInt(0);
/////////////////////// CONSTRUCTORS ///////////////////////
BigInt::BigInt(int num) : length_(0), sign_(pos) {
  unsigned int tmp;
  if (num < 0) {
    sign_ = neg;
    if (num == INT_MIN)
      tmp = unsigned(INT_MAX) + 1;
    else
      tmp = -1 * num;
  } else
    tmp = num;

  pushDigit(tmp % BASE);
  tmp = tmp >> 8;
  while (tmp != 0) {
    pushDigit(tmp % BASE);
    tmp = tmp >> 8;
  }
}

BigInt::BigInt(string s, bool hexadecimal) : length_(0), sign_(pos) {

  if (s.length() == 0) {
    pushDigit(0);
    return;
  }

  if (hexadecimal) {
    if (s.length() > 2 && s[0] == '0' && s[1] == 'x') {
      int n;
      string hexstr = s.substr(2);

      if(hexstr.length()/2 >= MAXLEN){
        hexstr = hexstr.substr(0,MAXLEN*2);
      } 

      for (int  i = hexstr.length() - 2; i >= 0; i -= 2) {
        string byte = hexstr.substr(i, 2);
        if (!isHex(byte)) {goto not_hex;}
        istringstream(byte) >> hex >> n;
        pushDigit(n);
      }

      if (hexstr.length() % 2) {
        istringstream(hexstr.substr(0, 1)) >> hex >> n;
        pushDigit(n);
      }

      normalize();
      return;
    }
  }

not_hex:
  if(s.length() >= MAXLEN){
    s = s.substr(0, MAXLEN);
  }
  for (int  i = s.length() - 1; i >=0; i--){
    pushDigit((uint8)s[i]);
  }
  normalize();
}

const BigInt& BigInt::operator=(const BigInt& rhs) {
  
  sign_ = rhs.sign();
  copyDigits(rhs);
  return *this;
}

 void BigInt::copyDigits(const BigInt& rhs) {
      memset(digits_, 0, MAXBUFF);
      length_ = (unsigned int) rhs.length(); 
      if ((unsigned int) rhs.length() > MAXLEN || rhs.length() == 0) length_ = MAXLEN;
      for(unsigned int i = 0; i < length_; i++){
        digits_[i] = rhs.digits_[i];
      }
      if (length_ == MAXLEN) {
        normalize();
      }
  };

///////////////////// GETTERS & SETTERS /////////////////////
uint8 BigInt::getDigit(unsigned int k) const {
  if (0 <= k && k < length_ && k < MAXLEN) return digits_[k];
  return 0;
}

void BigInt::setDigit(unsigned int k, uint8 value) {

  if (0 <= k && k < length_ && k < MAXLEN) digits_[k] = (uint8)value;
}

void BigInt::setDigit(unsigned int k, const BigInt& value) {
  if (value.length() == 1 &&
    0 <= k && k < length_ && k < MAXLEN) digits_[k] = (uint8)value.getDigit(0);
}

void BigInt::pushDigit(uint8 value) {

  if (length_ >= MAXLEN) normalize();
  if (length_ < MAXLEN){
    digits_[length_] = (uint8)value;
    length_++;
  }
}


void BigInt::normalize() {
  int k;
  if (length_ == 0 || length_ > MAXLEN) {
    length_ = MAXLEN;
  }
  unsigned int  len = length_;
  for (k = len - 1; k >= 0; k--) {
    if (getDigit(k) != 0) break;
    length_--;
  }
  if (k < 0)  // all zeros
  {
    length_ = 1;
    sign_ = pos;
  }
}

/////////////////////// PPRINTER ///////////////////////
string BigInt::toHexString() const {
  string s = "";
  unsigned int len = length_;
  if (length_ >= MAXLEN) len = MAXLEN;

  for (int i = len - 1; i >= 0; i--)
    s += lookup[getDigit(i)];

  return s;
}

string BigInt::toString() const {
  string s = "";
  unsigned int len = length_;
  if (length_ >= MAXLEN) len = MAXLEN;
  for (int i = len - 1; i >= 0; i--)
    s += getDigit(i);
  return s;
}

ostream& operator<<(ostream& out, const BigInt& big) {
  out << "0x" << big.toHexString();
  return out;
}

istream& operator>>(istream& in, BigInt& big) {
  string s;
  in >> s;
  big = BigInt(s);
  return in;
}

/////////////////////// COMPARISONS ///////////////////////
bool operator==(const BigInt& lhs, const BigInt& rhs) {
  if (lhs.length() != rhs.length() || lhs.positive() != rhs.positive()) return false;
  for (unsigned int  i = 0; i < lhs.length(); i++) {
    if (lhs.getDigit(i) != rhs.getDigit(i)) return false;
  }
  return true;
}

bool operator!=(const BigInt& lhs, const BigInt& rhs) { return !(lhs == rhs); }

bool operator<(const BigInt& lhs, const BigInt& rhs) {
  if (lhs.positive() != rhs.positive()) return lhs.negative();
  if (lhs.length() != rhs.length()) {
    return (lhs.length() < rhs.length() && lhs.positive()) ||
           (lhs.length() > rhs.length() && lhs.negative());
  }
  for (int i = lhs.length() - 1; i >= 0; i--) {
    if (lhs.getDigit(i) < rhs.getDigit(i)) return lhs.positive();
    if (lhs.getDigit(i) > rhs.getDigit(i)) return lhs.negative();
  }

  return false;
}

bool operator>(const BigInt& lhs, const BigInt& rhs) { return rhs < lhs; }

bool operator<=(const BigInt& lhs, const BigInt& rhs) {
  return lhs == rhs || lhs < rhs;
}

bool operator>=(const BigInt& lhs, const BigInt& rhs) {
  return lhs == rhs || lhs > rhs;
}

//////////////////////// PRIMALITY ////////////////////////

bool BigInt::isCoPrime(const BigInt& rhs) const { 
  BigInt x, y;
  BigInt gcd = gcdExtended(*this, rhs, &x, &y);
  return gcd == 1;
}

// From: https://gist.github.com/djinn/d707f72a87cde59d782395fcc0294091
bool BigInt::isPrime(int k) const {
  if (*this <= 1 || *this == 4) return false;
  if (*this <= 3) return true;

  BigInt d = *this - 1;
  while (d % 2 == 0) d /= 2;
  for (int i = 0; i < k; i++) {
    if (!millerTest(d, *this)) return false;
  }
  return true;
}

// From: https://gist.github.com/djinn/d707f72a87cde59d782395fcc0294091
bool millerTest(BigInt d, const BigInt& n) {
  BigInt a = 2 + rand() % (n - 4);

  BigInt x = pow(a, d, n);

  if (x == 1 || x == n - 1) return true;

  while (d != n - 1) {
    x = (x * x) % n;
    d *= 2;
    if (x == 1) {
      return false;}
    if (x == n - 1) {
      return true;}
  }
  return false;
}

//////////////////////// ADDITION ////////////////////////
BigInt& BigInt::operator+=(const BigInt& rhs) {
  // Identity Element
  if (rhs == 0) return *this;
  // Same
  if (this == &rhs) {
    *this *= 2;
    return *this;
  }

  if (positive() != rhs.positive()) {
    *this -= (-1 * rhs);
    return *this;
  }

  unsigned int  len = length_;
  if (len < rhs.length()) len = rhs.length();

  int carry = 0;
  for (unsigned int i = 0; i < len && i < MAXLEN; i++) {
    int sum = getDigit(i) + rhs.getDigit(i) + carry;
    carry = sum / BASE;
    sum = sum % BASE;
    if (i < length_)
      setDigit(i, sum);
    else
      pushDigit(sum);
  }
  if (carry != 0) pushDigit(carry);
  return *this;
}

BigInt operator+(const BigInt& lhs, const BigInt& rhs) {
  BigInt result(lhs);
  result += rhs;
  return result;
}

BigInt& BigInt::operator++() { return *this += 1; }

/////////////////////// SUBTRACTION ///////////////////////
BigInt& BigInt::operator-=(const BigInt& rhs) {
  if (rhs == 0) return *this;

  if (this == &rhs) {
    *this = 0;
    return *this;
  }

  if (positive() != rhs.positive()) {
    *this += (-1 * rhs);
    return *this;
  }

  if ((positive() && (*this) < rhs) || (negative() && (*this) > rhs)) {
    *this = rhs - *this;
    if (positive())
      sign_ = neg;
    else
      sign_ = pos;
    return *this;
  }

  int borrow = 0;
  for (unsigned int i = 0; i < length_ && i < MAXLEN; i++) {
    int diff = getDigit(i) - rhs.getDigit(i) - borrow;
    borrow = 0;
    if (diff < 0) {
      diff += BASE;
      borrow = 1;
    }
    setDigit(i, diff);
  }
  normalize();
  return *this;
}

BigInt operator-(const BigInt& lhs, const BigInt& rhs) {
  BigInt result(lhs);
  result -= rhs;
  return result;
}

///////////////////// MULTIPLICATION /////////////////////
BigInt& BigInt::operator*=(int num) {
  if (1 == num) return *this;

  if (0 == num) {
    *this = 0;
    return *this;
  }

  if (BASE < num || num < 0) {
    *this *= BigInt(num);
    return *this;
  }

  int carry = 0;
  for (unsigned int i = 0; i < length_; i++) {
    int product = num * getDigit(i) + carry;
    carry = product / BASE;
    setDigit(i, product % BASE);
  }

  while (carry != 0) {
    pushDigit(carry % BASE);
    carry /= BASE;
  }
  return *this;
}

BigInt& BigInt::operator*=(const BigInt& rhs) {
  if (rhs == BigInt(0)) {
    *this = 0;
    return *this;
  }

  if (rhs == BigInt(1)) return *this;

  if (negative() != rhs.negative())
    sign_ = neg;
  else
    sign_ = pos;

  BigInt self(*this);
  BigInt sum(0);
  for (unsigned int i = 0; i < rhs.length(); i++) {
    sum += self * rhs.getDigit(i);
    self *= BASE;
  }
  *this = sum;
  return *this;
}

BigInt operator*(const BigInt& a, int num) {
  BigInt result(a);
  result *= num;
  return result;
}

BigInt operator*(int num, const BigInt& a) {
  BigInt result(a);
  result *= num;
  return result;
}

BigInt operator*(const BigInt& lhs, const BigInt& rhs) {
  BigInt result(lhs);
  result *= rhs;
  return result;
}

/////////////////////// DIVISION ///////////////////////

BigInt& BigInt::operator/=(const BigInt& rhs) {
  BigInt quotient, remainder;
  bool resultNegative = (negative() != rhs.negative());
  sign_ = pos;

  if (rhs.negative())
    divide(*this, -1 * rhs, quotient, remainder);

  else
    divide(*this, rhs, quotient, remainder);

  *this = quotient;
  sign_ = resultNegative ? neg : pos;
  normalize();
  return *this;
}

BigInt& BigInt::operator/=(int num) {
  if (num > BASE) {
    return operator/=(BigInt(num));
  }

  if (0 == num) {
    abort();
  }

  int carry = 0;
  for (int i = length_ - 1; i >= 0; i--) {
    int quotient = (BASE * carry + getDigit(i));
    carry = quotient % num;
    setDigit(i, quotient / num);
  }
  normalize();
  return *this;
}

BigInt operator/(const BigInt& lhs, const BigInt& rhs) {
  BigInt result(lhs);
  result /= rhs;
  return result;
}

BigInt operator/(const BigInt& a, int num) {
  BigInt result(a);
  result /= num;
  return result;
}

void BigInt::divide(const BigInt& lhs, const BigInt& rhs, BigInt& quotient,
                    BigInt& remainder) {
  if (lhs < rhs) {
    quotient = 0;
    remainder = lhs;
    return;

  } else if (lhs == rhs) {
    quotient = 1;
    remainder = 0;
    return;
  }

  quotient = remainder = 0;
  BigInt dividend(lhs);
  BigInt divisor(rhs);

  int zeroCount = 0;
  while (divisor.length() < dividend.length()) {
    divisor *= BASE;
    zeroCount++;
  }
  if (divisor > dividend) {
    divisor /= BASE;
    zeroCount--;
  }

  int trial;
  int divisorSig = divisor.getDigit(divisor.length() - 1);

  BigInt tmp;
  for (int i = 0; i <= zeroCount; i++) {
    int dividendSig = dividend.getDigit(dividend.length() - 1);
    trial = (dividendSig * BASE + dividend.getDigit(dividend.length() - 2)) /
            divisorSig;

    if (BASE <= trial) {
      trial = BASE - 1;
    }
    while ((tmp = divisor * trial) > dividend) {
      trial--;
    }

    quotient *= BASE;
    quotient += trial;
    dividend -= tmp;
    divisor /= BASE;
    divisorSig = divisor.getDigit(divisor.length() - 1);
  }
  remainder = dividend;
}

///////////////////// MOD /////////////////////

BigInt operator%(const BigInt& lhs, const BigInt& rhs) {
  BigInt result(lhs);
  result %= rhs;
  return result;
}

BigInt& BigInt::operator%=(const BigInt& rhs) {
  BigInt quotient, remainder;
  bool resultNegative = negative();
  sign_ = pos;
  if (rhs.negative())
    divide(*this, -1 * rhs, quotient, remainder);

  else
    divide(*this, rhs, quotient, remainder);

  *this = remainder;
  sign_ = resultNegative ? neg : pos;

  return *this;
}

///////////////////// MODULAR ARITHMETIC /////////////////////

BigInt pow(const BigInt& base, const BigInt& exp, const BigInt& mod) {
  BigInt x = 1, y, z;
  y = base;
  z = exp;

  while (z != zero) {
    if (z.odd()) x = (x * y) % mod;
    z /= 2;
    y = (y * y) % mod;
  }
  return x;
}

// From: https://www.geeksforgeeks.org/euclidean-algorithms-basic-and-extended/
BigInt gcdExtended(BigInt a, const BigInt& b, BigInt* x, BigInt* y) {
  if (a == 0) {
    *x = 0, *y = 1;
    return b;
  }

  BigInt x1, y1;
  BigInt gcd = gcdExtended(b % a, a, &x1, &y1);

  *x = y1 - (b / a) * x1;
  *y = x1;
  return gcd;
}

BigInt inv(const BigInt& a, const BigInt& n) {
  BigInt x, y;
  BigInt gcd = gcdExtended(a, n, &x, &y);
  if (gcd != 1)
    return 0;
  else {
    BigInt inv = (x % n + n) % n;
    return inv;
  }
}


#endif
