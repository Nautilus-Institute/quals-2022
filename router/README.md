# router

Challenge type:

- Web/Reversing/Baby (flag 1)
- Web/Reversing (flag 2)
- Web/Pwnable/Shellcoding (flag 3)

## Description on the scoreboard

- Flag 1


```
The flag is in the RAM. You are lucky that a router does not have that much RAM!

Leak it.

Updating the router's firmware will possibly brick the router and will *not* get you this flag.
Don't do it.
```

- Flag 2


```
The flag is still in the RAM.

Reverse some bytes and get the flag.

Again, updating the router's firmware will possibly brick the router and will *not* get you this flag.
Don't ruin the game for other players!
```

- Flag 3

```
Update the router. As a reward, you are now allowed to read `/flag3`.
```

## Vulnerabilities

- Dumb default credentials.

- Authentication can be bypassed by setting "username" in cookies.

- `/ping` does not properly check the boundary of `offset`.
The main executable can be leaked using this vulnerability.
The random seed can also be leaked this way.


## Flags

There are three flags, and all of them require leaking the binary first.
The first flag is trivial: 
Simply access the proper URL and then decrypt the flag in the header (or just read it from the disassembly).
The second flag is a bit difficult.
Players must understand the logic and then reimplement the `get_byte()` function using a faster algorithm.
The third flag is a shellcoding challenge.
Players must first leak the random seed (stored right after the global ping-result buffer), and then write VM-shellcode to read the content in `flag3`.

## Acknowledgement

This challenge used code from the following sources:

- pifs: https://github.com/philipl/pifs/blob/master/src/piqpr8.c (while pifs is licensed under GPL3, the code from piqpr8.c does not seem to be under the same license)
- ttmath: https://www.ttmath.org
- Simple-Web-Server: https://gitlab.com/eidheim/Simple-Web-Server (MIT license)
- base64.h: https://gist.github.com/tomykaira/f0fd86b6c73063283afe550bc5d77594 (license unknown)

Note that this challenge repo does not re-license any source code or projects that it uses.
All my additions are licensed under GPL3.
A license file will be added before the formal public release.
