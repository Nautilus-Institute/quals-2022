import random


def main():
    flag = "FLAG{helping_adam_customize_cpython_3.12_is_fun_702e0da000}"

    code = [ ]
    for i, ch in enumerate(flag):
        ch = ord(ch)
        # due to my changes in _PyBytes_FromList, we need to mangle the character here
        ch ^= 0xc8
        ch += i * 2
        ch &= 0xff
        for bit in range(8):
            mask = 1 << bit
            expected_bit = ch & mask
            line = f"    r &= (flag_bytes[{i}] & {mask}) == {expected_bit}"
            code.append(line)

    random.shuffle(code)
    code.insert(0, "    r = True")
    code.insert(1, f"    if len(flag_bytes) != {len(flag)}: return False")
    code.append("    return r")
    print("\n".join(code))


if __name__ == "__main__":
    main()

