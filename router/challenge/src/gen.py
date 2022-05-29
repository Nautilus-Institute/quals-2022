#!/usr/bin/env python
# -*- coding: utf-8 -*-

def main():
    # generate flag_0.inc
    flag_0 = "FLAG{r0uter_p0rtals_are_ultimately_impenetrable_because_they_are_real_web_apps}"
    print(f"Flag 0 length: {len(flag_0)}")

    lines = [ ]
    for i, ch in enumerate(flag_0):
        lines.append(f"flag_0[{i}] = '{ch}';")
    with open("flag_0.inc", "w") as f:
        f.write("\n".join(lines))

    # generate flag_byte_offsets
    with open("pis.txt", "r") as f:
        lines = f.read().split("\n")
    flag_byte_offsets = {}
    for line in lines:
        if not line.strip(" "):
            continue
        offset, n = line.split("-")
        offset = offset.strip(" ")
        n = n.strip(" ")
        offset = int(offset)
        n = int(n)
        flag_byte_offsets[n] = offset
    s = ""
    for i in b"FLAG{great_j0b_g0ttfried_leibniz_david_bailey_peter_b0rwein_and_sim0n_pl0uffe}":
        s += f"{flag_byte_offsets[i] if i in flag_byte_offsets else -1}, "
    print(s)


if __name__ == "__main__":
    main()

