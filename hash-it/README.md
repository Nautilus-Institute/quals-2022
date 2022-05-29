# hash-it-0

## Problem Description

This is a simple/baby's first challenge. It's a simple, "Can you do basic
reversing and write some basic shellcode."

The challenge is, roughly speaking:

```
HASH_ALGOS = md5, sha1, sha256, sha512

for (i = 0; i < input.len, i += 2)
    shellcode += HASH_ALGOS[(i/2) % 4](input[0] + input[1])[0]

execute shellcode
```

