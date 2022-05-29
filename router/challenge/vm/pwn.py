import sys

from ctypes import CDLL


def main():
    seed = int(sys.argv[1])
    libc = CDLL("libc.so.6")

    LOAD_REG = 1
    CLEAR_REG = 2
    MOVE_REG = 3
    MOVE_MEM = 4
    SYSCALL_FOPEN = 30
    SYSCALL_FREAD = 31
    SYSCALL_FCLOSE = 32
    SYSCALL_PRINT = 33
    SYSCALL_MAKEGOD = 34
    ADD = 100
    SUB = 101
    A, B, C = 0, 1, 2

    ins = [
        (LOAD_REG, B, 2),
        (LOAD_REG, A, ord('f') + 2),
        (SUB, A, B),
        (MOVE_MEM, 0x13, 0x37, A),
        (CLEAR_REG, A),

        (CLEAR_REG, B),
        (LOAD_REG, B, 1),
        (LOAD_REG, A, ord('l') + 1),
        (SUB, A, B),
        (MOVE_MEM, 0x13, 0x38, A),
        (CLEAR_REG, A),

        (LOAD_REG, A, ord('a') + 1),
        (SUB, A, B),
        (MOVE_MEM, 0x13, 0x39, A),
        (CLEAR_REG, A),

        (LOAD_REG, A, ord('g') + 1),
        (SUB, A, B),
        (MOVE_MEM, 0x13, 0x3a, A),
        (CLEAR_REG, A),

        (LOAD_REG, A, ord('3') + 1),
        (SUB, A, B),
        (MOVE_MEM, 0x13, 0x3b, A),
        (CLEAR_REG, A),

        (MOVE_MEM, 0x13, 0x3c, A),

        # Make god
        (LOAD_REG, A, ord('g') + 1),
        (SUB, A, B),
        (LOAD_REG, A, ord('o') + 1),
        (SUB, A, B),
        (LOAD_REG, A, ord('d') + 1),
        (SUB, A, B),
        (LOAD_REG, C, 1),
        (LOAD_REG, A, 2),
        (MOVE_REG, B, A),
        (SUB, B, C),
        (SUB, A, C),
        (SUB, A, C),
        # (MOVE_REG, C, A),
        # (ADD, C, 2),
        (SYSCALL_MAKEGOD, ),

        # fopen
        (CLEAR_REG, A),
        (LOAD_REG, A, 0x13),
        (LOAD_REG, A, 0x37),
        (SYSCALL_FOPEN, ),

        # fread
        (CLEAR_REG, B),
        (LOAD_REG, B, 0xff),
        (CLEAR_REG, C),
        (LOAD_REG, C, 0x20),
        (LOAD_REG, C, 0x0),
        (SYSCALL_FREAD, ),

        # print
        (MOVE_REG, A, C),
        (CLEAR_REG, B),
        (LOAD_REG, B, 0xff),
        (SYSCALL_PRINT, )
    ]

    CODE_SIZE = 4096
    code = [None] * (CODE_SIZE + 3)
    pc = 0
    instr_id = 0
    offset_to_instrid = {}

    # first: calculate conflicts
    libc.srand(seed)
    nop_instrs = set()
    while instr_id < len(ins):
        instr = ins[instr_id]
        instr_bytes = bytes(instr)
        if b'g' in instr_bytes or b'd' in instr_bytes:
            import ipdb; ipdb.set_trace()
        has_conflict = False
        for i, b in enumerate(instr_bytes):
            if code[pc + i] is not None:
                print(f"Potential conflict at {pc + i}; Instr ID {instr_id} -> {offset_to_instrid[pc + i]}")
                nop_instrs.add(offset_to_instrid[pc + i])
        if not has_conflict:
            for i, b in enumerate(instr_bytes):
                code[pc + i] = b
                offset_to_instrid[pc + i] = instr_id
            instr_id += 1
        else:
            for i, b in enumerate(instr_bytes):
                if code[pc + i] is None:
                    code[pc + i] = 0xce

        pc = libc.rand() % CODE_SIZE

    # adjust ins
    new_ins = [ ]
    for i in range(len(ins)):
        if i in nop_instrs:
            new_ins.append((0xcd, 0xcd, 0xcd))
        new_ins.append(ins[i])

    # actually do the write
    code = [None] * CODE_SIZE
    pc = 0
    instr_id = 0
    offset_to_instrid = {}

    libc.srand(seed)
    ins = new_ins
    while instr_id < len(ins):
        instr = ins[instr_id]
        instr_bytes = bytes(instr)
        has_conflict = False
        for i, b in enumerate(instr_bytes):
            if code[pc + i] is not None:
                print(f"Potential conflict at {pc + i}; Instr ID {instr_id} -> {offset_to_instrid[pc + i]}")
                has_conflict = True
        if not has_conflict:
            for i, b in enumerate(instr_bytes):
                code[pc + i] = b
                offset_to_instrid[pc + i] = instr_id
            instr_id += 1
        else:
            for i, b in enumerate(instr_bytes):
                if code[pc + i] is None:
                    code[pc + i] = 0xce

        pc = libc.rand() % CODE_SIZE

    freebies = 0
    for i in range(len(code)):
        if code[i] is None:
            code[i] = 0xcc
            freebies += 1
    print(f"Got {freebies} bytes unused.")
    with open("input.bin", "wb") as f:
        f.write(bytes(code))


if __name__ =="__main__":
    main()


