# Crypto Chall #

**Category:** Pwnable/Crypto

**Author:** @[perribus](perrib.us)

Feel free to compile with -O3, it doesn't make your life easier. 

## Solution ##

**Given**: x86-64 stripped (and fno-rtti) C++ binary. Compiled without optimizations. 

**High Level Summary**: The bug is a type confusion: the C++ smart pointer, `shared_ptr`, which does reference counting, doesn't increment the reference count when it's passed by reference to a C++ lambda. If that closure persists after the last reference to the `shared_ptr` is decremented, its memory will be freed which, if abused correctly, can result in a type confusion. But you're very constrained in how you can read from and write to that freed memory. You have to reverse engineer RSA operations to leak an VTable address and then find a valid public exponent for an RSA key that will calculate a modular inverse equal to the address of a different VTable, which contains a function that will give you the flag. 

Program is an encryption/decryption service. There are four encryption algorithms you can use.

You can: 

1. Create a Key -> Creates a `Cipher` object based on the encryption algorithm you choose.
2. Encrypt a Message -> Encrypts a message you input with the `Cipher` object of your choosing.
3. Decrypt a Message -> Decrypts a message you input with the `Cipher` object of your choosing.

Cipher is a pure abstract base class. There are four (technically five) derived classes. Players had to reverse engineer the encryption algorithms from the stripped binary, algorithms were given psuedonyms:

1. Missouri Encryption Standard (MES): b64 encode/decode
2. Algorithm MCCCXXXVII: Caesar's Cipher
3. Algorithm 13: ROT14 (derived from Algorithm MCCCXXXVII)
4. Algorithm X: RSA
5. Flag One Time Pad - a hidden class that was never invoked but contains encrypt function that reads in the flag and XORs it with your message. 

All of the challenges rely on a custom `BigInt` class. 

In the RSA, `e`, `p`, and `q` are passed to the `createKey()` method as a `shared_ptr<BigInt>` each
    
The RSA class contains a function pointer member variable, `totient_`. 
    
Within createKey(), even though Euler's totient `Φ` is used to compute `d`, `p` and `q` are passed to a lambda 
which creates a closure that takes in `e` and recomputes `d` with Carmichael's totient `λ`. The resulting `d` 
is stored in `*p` and returned. This lamda is assigned to `totient_`. 
    
### Bug ###     
`p` and `q` are passed by reference to the lambda and their ref pointers are not incremented. 
When `p` and `q` go out of scope in main, their heap pointers are freed even though references to the `p` and `q`
shared_ptr stack objects remains. As long as those don't get clobbered, the heap pointers will remain
and the chunks pointed to will be popped off the tcache freed list when new chunks are allocated. `totient_` 
is called if you use the same key to decrypt 10 times. 

This is what the source code looked like:

```cpp
unsigned int RSA::createKey(shared_ptr<BigInt> e, shared_ptr<BigInt> p, shared_ptr<BigInt> q){
    ...
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
    ...
 }
```
    
### Exploit ###
1. Allocate RSA obj
2. Allocate MES obj -> `p`
    This is the same size as a BigInt obj, has VTable pointer at top, followed by a b64 symbol lookup `char[]`
    and followed by a very helpful value of 0x16 which wil be interpreted by BigInt as the number of digits
3. Allocate a ROT14 obj -> `BigInt(14)` -> `q`
    ROT14 chunk is smaller than `BigInt` but contains a `BigInt` object of value 14, which goes in `q`. 
4. Decrypt 10 times, which allows you to call totient_ function:
```
    p = 0x48474645444342410000<address of vtable> 
    q = 0xd
    d = e ^ (-1) mod λ(p - 1, q - 1)
    d is returned
```
5. Find discrete log of ciphertext mod `n`, recovering `d` which allows you to recover `λ` with known upper bits, and some arithmetic gives you the address in `p`. See more details in exploit below. 
6. Use MES vtable address to find address of FlagOTP VTable and find a new `e` such that `e` is the modular inverse of the latter address mod lambda, then find a new `p` and `q` that work with new `e` to get past RSA checks
7. Re-trigger the bug, writing the new address into `p`
8. Call encrypt on MES object with the overwritten VTable, calling the functiont that reads from flag file and XORs with your message

`solver/x.py` has more details about the challenge solution explained in the comments. 

Idea for the bug came from this blog post: https://alexgaynor.net/2019/apr/21/modern-c++-wont-save-us/

  
## This Repo ##

You can build the challenge with the `challenge/Makefile` or you can build a Docker image with `challenge/Dockerfile`. If you want to throw against it, change `challenge/Dockerfile`: 

1. Add the line ```RUN apt-get update && apt-get -y install socat```  
2. Change the `CMD` at the bottom to ```CMD ["socat", "TCP-LISTEN:31337,fork,reuseaddr", "EXEC:/perribus/challenge/cryptochall"]``` 

You can run the exploit against it with `solver/Dockerfile` or by installing Python3, Pwntools and SageMath. 
