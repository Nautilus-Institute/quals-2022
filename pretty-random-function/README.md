# Pretty Random Function
This is an ssl server with a few mean things.  The first is the fact that it requires a proof of work before connecting, the reason for this proof of work is mostly to avoid DDOS concerns.  However, it does require more than one core, which is just annoying.

Next, it generats a PRF via an HMAC key, this key is used based on their user id.  This makes it slightly more annoying to work with, where it's going to require them to understand how openssl works, and what pre shared key does inside of openssl.  Next, there isn't a python library to do Preshared tls1.3 I believe, this means that the exploit will need to be written in C.

There are several red herring bugs, a stack overflow, a horrible heap corruption bug.  The bug I'm planning on exploiting is an alloca to go on the next stack of a pthread.  

The binary is entirely statically compiled, this gives them a lot of rop gadgets and makes it so there is no required pointer leak.  Secondly they can leak a large amount of heap data, that data is pretty much useless.


## What this looks like

The goal of this challenge is look like a proprietary protocol on an internet of shit device.  These devices are common with one big static binary doing something that doesnt make much sene.  Half the challenge is just trying to figure out how to work with the challenge.

The hardest part of the challenge is being able to talk to it.  Once a client is written it's pretty easy to exploit.  The threading throws in some issues, but once you can reliably talk to it, you can thrash around until you land.


## Client
ctf_client.c is an example exploit that lands and reads out the ./flag directory.

