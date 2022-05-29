from typing import List
import itertools
import re
import random


def get_funcs(asm: List[str]) -> List[List[str]]:
    funcs = [ ]
    func = [ ]
    for line in asm:
        if "@function" in line:
            if func:
                funcs.append(func)
                func = [ ]
        func.append(line)

    if func:
        funcs.append(func)
    return funcs


label_ctr = itertools.count(1)

def mangle_call(func: List[str]) -> None:
    for idx, line in enumerate(list(func)):
        m_call = re.search(r"call\s+(\S+)", line)
        if m_call is not None:
            call_func = m_call.group(1)
            lbl_ctr = next(label_ctr)
            random_offset_0 = random.randint(0x300000, 0x400000)
            random_offset_1 = random.randint(0x200000, 0x400000)
            xmm = random.randint(0, 7)
            func[idx] = f"""
  lea rax, .lbl_{lbl_ctr}-{random_offset_0}
  push rax
  add dword ptr [rsp], {random_offset_0}
  lea rax, {call_func}-{random_offset_1}
  vmovq xmm{xmm}, rax
  movq rax, xmm{xmm}
  add rax, {random_offset_1}
  jmp rax
.lbl_{(lbl_ctr)}:
"""


def main():
    with open("vm.s", "r") as f:
        asm = f.read().split("\n")

    funcs = get_funcs(asm)
    for func in funcs:
        mangle_call(func)

    random.seed(31337)

    # reconstruct
    new_asm = [ ]
    for func in funcs:
        new_asm += func

    with open("vm.s", "w") as f:
        f.write("\n".join(new_asm))

if __name__ == "__main__":
    main()

