from typing import List, Callable, Tuple, Dict
import random
import struct
import tempfile
import subprocess
import os
import shutil
import functools
import hashlib

import numpy
import progressbar


DEBUG = False
STATIC = True

_PROGRESS_WIDGETS = [progressbar.Percentage(), ' ', progressbar.Bar(), ' ', progressbar.Timer(), ' ', progressbar.ETA()]


def t0_args(n: int, endness="le"):
    if endness == "be":
        raw_bytes = struct.pack("<Q", n)
        n0, n1 = struct.unpack(">I", raw_bytes[0:4])[0], struct.unpack(">I", raw_bytes[4:8])[0]
        n = (n1 << 32) | n0

    # a * n0 + b * n1 = n2
    # a * n3 - b * n4 = n5
    # a ^ b - 2 = n6
    a = n & 0xffff_ffff
    b = (n >> 32) & 0xffff_ffff
    n0 = random.randint(0, 99999)
    n1 = random.randint(0, 99999)
    n2 = (a * n0 + b * n1) & 0xffff_ffff

    n3 = random.randint(0, 99999)
    n4 = random.randint(0, 99999)
    n5 = (a * n3 - b * n4) & 0xffff_ffff

    n6 = ((a ^ b) - 2) & 0xffff_ffff
    n7 = a;
    return {"n0": n0, "n1": n1, "n2": n2, "n3": n3, "n4": n4, "n5": n5, "n6": n6, "n7": n7}


tmpl_0 = """
#include <stdio.h>

int main()
{{
    unsigned int user_input[2] = {{0}};
    if (scanf("%llu", (unsigned long long*)&user_input) != 1) {{
        puts(":(");
        return -1;
    }}
    if (0xffffffff & (user_input[0] * {n0} + user_input[1] * {n1}) == {n2}) {{
        if (0xffffffff & (user_input[0] * {n3} - user_input[1] * {n4}) == {n5}) {{
            if ((user_input[0] ^ user_input[1]) - 2 == {n6}) {{
                if (user_input[0] == {n7}) {{
                    puts("Congrats!");
                    return 0;
                }}
            }}
        }}
    }}
    puts(":(");
    return -1;
}}
"""


def t1_verify(n: int, var_name: str, bit: int, ch: str) -> str:
    mask = 1 << bit
    the_bit = ord(ch) & mask
    rnd = random.randint(1, 45)

    s = f"""if (({var_name}[{n}] & {mask}) >> {bit} != {the_bit >> bit})
{{
    result |= {rnd};
}}
"""
    return s 


def t1_args(n: int, endness="le") -> Dict:
    target = struct.pack("<Q", n)

    lines = [ ]
    for i, ch in enumerate(target):
        for bit in range(8):
            s = t1_verify(i, "ptr", bit, bytes([ch]))
            lines.append(s)
    random.shuffle(lines)
    return {"verify_code": "\n".join(lines)}


tmpl_1 = """
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int verify(char* ptr)
{{
    int result = 0;
    {verify_code}
    return result == 0;
}}

int main()
{{
    unsigned char user_input[8] = {{0}};
    if(scanf("%llu", (unsigned long long*)&user_input) != 1) {{
        puts(":(");
        return 1;
    }}
    if (verify(user_input)) {{
        puts("Congrats!");
        return 0;
    }}
    puts(":(");
    return 1;
}}
"""


def t2_args(n: int, endness="le") -> Dict:
    bs = struct.pack("<Q", n)
    d = { }
    for idx, b in enumerate(bs):
        hash_ = hashlib.md5(bytes([b])).digest()
        # convert the output to "\x??"
        hash_str = ""
        for n in hash_:
            hash_str += f"\\x{n:02x}"
        d[f"md5_{idx}"] = hash_str
    return d


tmpl_2 = """
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void * md5_buffer (const char* buffer, unsigned int len, void* resblock);

int main()
{{
    unsigned char user_input[8] = {{0}};
    if (scanf("%llu", (unsigned long long*)&user_input) != 1) {{
        puts(":(");
        return 1;
    }}
    // MD5 each byte
    unsigned char res[16];
    md5_buffer(user_input + 0, 1, res);
    if (memcmp(res, "{md5_0}", 16)) {{
        puts(":(");
        return 1;
    }}
    md5_buffer(user_input + 1, 1, res);
    if (memcmp(res, "{md5_1}", 16)) {{
        puts(":(");
        return 1;
    }}
    md5_buffer(user_input + 2, 1, res);
    if (memcmp(res, "{md5_2}", 16)) {{
        puts(":(");
        return 1;
    }}
    md5_buffer(user_input + 3, 1, res);
    if (memcmp(res, "{md5_3}", 16)) {{
        puts(":(");
        return 1;
    }}
    md5_buffer(user_input + 4, 1, res);
    if (memcmp(res, "{md5_4}", 16)) {{
        puts(":(");
        return 1;
    }}
    md5_buffer(user_input + 5, 1, res);
    if (memcmp(res, "{md5_5}", 16)) {{
        puts(":(");
        return 1;
    }}
    md5_buffer(user_input + 6, 1, res);
    if (memcmp(res, "{md5_6}", 16)) {{
        puts(":(");
        return 1;
    }}
    md5_buffer(user_input + 7, 1, res);
    if (memcmp(res, "{md5_7}", 16)) {{
        puts(":(");
        return 1;
    }}
    puts("Congrats!");
    return 0;
}}
"""


# tmpl_3: matrix multiplication

def t3_args(n: int, endness="le") -> Dict:
    # break n into 8 numbers
    matrix_1 = [ ]
    for i in range(8):
        matrix_1.append(n & 0xff)
        n >>= 8
    assert n == 0
    matrix_1.append(0)

    matrix_0 = [ ]
    for i in range(9):
        matrix_0.append(random.randint(0, 99999));
    rank = numpy.linalg.matrix_rank((matrix_0[0:3],
                                     matrix_0[3:6],
                                     matrix_0[6:9]))
    if rank != 3:
        import ipdb; ipdb.set_trace()

    matrix_r = [ ]
    for i in range(9):
        x = i % 3
        y = i // 3
        r = 0
        for j in range(3):
            r += matrix_0[y * 3 + j] * matrix_1[x + j * 3]
        matrix_r.append(r)

    matrix_0_str = ", ".join(map(str, matrix_0))
    matrix_r_str = ", ".join(map(str, matrix_r))
    return {"matrix_0": matrix_0_str, "matrix_r": matrix_r_str}


tmpl_3 = """
#include <stdio.h>

int main()
{{
    unsigned char user_input[8] = {{0}};
    if (scanf("%llu", (unsigned long long*)&user_input) != 1) {{
        puts(":(");
        return -1;
    }}
    unsigned int matrix_0[9] = {{ {matrix_0} }};
    unsigned int matrix_1[9] = {{0}};
    unsigned int matrix_2[9];
    unsigned int matrix_r[9] = {{ {matrix_r} }};
    for (int i = 0; i < 8; ++i) {{
        matrix_1[i] = user_input[i];
    }}

    // matrix multiplication
    for (int i = 0; i < 9; ++i) {{
        int x = i % 3, y = i / 3;
        int r = 0;
        for (int j = 0; j < 3; ++j) {{
            r += matrix_0[y * 3 + j] * matrix_1[x + j * 3];
        }}
        matrix_2[i] = r;
    }}

    // check
    for (int i = 0; i < 9; ++i) {{
        if (matrix_2[i] != matrix_r[i]) {{
            puts(":(");
            return -1;
        }}
    }}
    puts("Congrats!");
    return 0;
}}
"""

# tmpl_4: matrix addition


# tmpl_5: Rust simple comparison


# tmpl_6: Rust matrix multiplication


# tmpl_7: Go simple comparison


# tmpl_8: C MD5 with modified IV


# tmpl_9: C SHA256 with modified IV


# tmpl_10: Crystal simple comparison

def t10_args(n: int, endness="le") -> Dict:
    all_bytes = struct.pack("<Q", n)
    d = {}
    for i in range(8):
        d[f"b{i}"] = all_bytes[i]
    return d

tmpl_10 = """
user_input = read_line.chomp.to_u64
exit if user_input.nil?
u0 = (user_input >> 0) & 0xff
u1 = (user_input >> 8) & 0xff
u2 = (user_input >> 16) & 0xff
u3 = (user_input >> 24) & 0xff
u4 = (user_input >> 32) & 0xff
u5 = (user_input >> 40) & 0xff
u6 = (user_input >> 48) & 0xff
u7 = (user_input >> 56) & 0xff
if u0 == {b0}
    if u1 == {b1}
        if u2 == {b2}
            if u6 == {b6}
                if u4 == {b4}
                    if u5 == {b5}
                        if u3 == {b3}
                            if u7 == {b7}
                                puts "Congrats!"
                                exit
                            end
                        end
                    end
                end
            end
        end
    end
end
puts ":("
"""


def compile_c(arch: str, source: str, dst_path: str) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        src_path = os.path.join(tmpdir, "src.c")
        tmp_dst_path = os.path.join(tmpdir, "dst")
        with open(src_path, "w") as f:
            f.write(source)
        has_md5 = "md5_buffer" in source

        if has_md5:
            # copy necessary files
            md5_dst_path = os.path.join(tmpdir, "md5.c")
            shutil.copy("md5.inc", md5_dst_path)

        if arch == "win64":
            gcc = "x86_64-w64-mingw32-gcc"
            strip = "x86_64-w64-mingw32-strip"
            tmp_dst_path += ".exe"
        elif arch == "x86_64":
            gcc = "gcc"
            strip = "strip"
        elif arch == "arm":
            gcc = "arm-linux-gnueabi-gcc"
            strip = "arm-linux-gnueabi-strip"
        elif arch == "aarch64":
            gcc = "aarch64-linux-gnu-gcc"
            strip = "aarch64-linux-gnu-strip"
        elif arch == "mips":
            gcc = "mips-linux-gnu-gcc"
            strip = "mips-linux-gnu-strip"
        elif arch == "mipsel":
            gcc = "mipsel-linux-gnu-gcc"
            strip = "mipsel-linux-gnu-strip"
        elif arch == "mips64":
            gcc = "mips64-linux-gnuabi64-gcc"
            strip = "mips64-linux-gnuabi64-strip"
        elif arch == "powerpc":
            gcc = "powerpc-linux-gnu-gcc"
            strip = "powerpc-linux-gnu-strip"
        elif arch == "powerpc64":
            gcc = "powerpc64-linux-gnu-gcc"
            strip = "powerpc64-linux-gnu-strip"
        elif arch == "riscv64":
            gcc = "riscv64-linux-gnu-gcc"
            strip = "riscv64-linux-gnu-strip"
        elif arch == "s390x":
            gcc = "s390x-linux-gnu-gcc"
            strip = "s390x-linux-gnu-strip"
        elif arch == "sparc64":
            gcc = "sparc64-linux-gnu-gcc"
            strip = "sparc64-linux-gnu-strip"
        elif arch == "sh4":
            gcc = "sh4-linux-gnu-gcc"
            strip = "sh4-linux-gnu-strip"
        elif arch == "alpha":
            gcc = "alpha-linux-gnu-gcc"
            strip = "alpha-linux-gnu-strip"
        elif arch == "m68k":
            gcc = "m68k-linux-gnu-gcc"
            strip = "m68k-linux-gnu-strip"
        elif arch == "hppa":
            gcc = "hppa-linux-gnu-gcc"
            strip = "hppa-linux-gnu-strip"
        else:
            raise NotImplementedError(f"Unknown arch {arch}")

        if has_md5:
            src = [src_path, md5_dst_path]
        else:
            src = [src_path]
        if STATIC:
            static_ = ["-static"]
        else:
            static_ = [ ]
        if DEBUG:
            subprocess.check_call([gcc, "-g"] + static_ + src + ["-o", tmp_dst_path], stdin=subprocess.DEVNULL, shell=False)
        else:
            subprocess.check_call([gcc, "-O2"] + static_ + src + ["-o", tmp_dst_path], stdin=subprocess.DEVNULL, shell=False)
            subprocess.check_call([strip, "--strip-all", tmp_dst_path], stdin=subprocess.DEVNULL, shell=False)
        shutil.move(tmp_dst_path, dst_path)


def compile_crystal(arch: str, source: str, dst_path: str) -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        src_path = os.path.join(tmpdir, "src.c")
        tmp_dst_path = os.path.join(tmpdir, "dst")
        with open(src_path, "w") as f:
            f.write(source)

        if arch == "x86_64":
            cc = ["crystal", "build", "--release"]
            strip = "strip"
        elif arch == "x86_64_dbg":
            cc = ["crystal", "build"]
            strip = "strip"
        else:
            raise NotImplementedError(f"Unknown arch {arch}")

        if DEBUG:
            subprocess.check_call(cc + [src_path, "-o", tmp_dst_path], stdin=subprocess.DEVNULL, shell=False)
        else:
            subprocess.check_call(cc + [src_path, "-o", tmp_dst_path], stdin=subprocess.DEVNULL, shell=False)
            subprocess.check_call([strip, "--strip-all", tmp_dst_path], stdin=subprocess.DEVNULL, shell=False)
        shutil.move(tmp_dst_path, dst_path)

ENDNESS = {
    'x86_64': 'le',
    'win64': 'le',
    'arm': 'le',
    'aarch64': 'le',
    'mips': 'be',
    'mipsel': 'le',
    'mips64': 'be',
    'powerpc': 'be',
    'powerpc64': 'be',
    'riscv64': 'le',
    's390x': 'be',
    'sparc64': 'be',
    'sh4': 'le',
    'alpha': 'le',
    'm68k': 'be',
    'hppa': 'be',
}

TEMPLATES: List[Tuple[str,Callable]] = [
    (tmpl_0,
     t0_args,
     [functools.partial(compile_c, "x86_64"),
      functools.partial(compile_c, "win64"),
      functools.partial(compile_c, "arm"),
      functools.partial(compile_c, "aarch64"),
      functools.partial(compile_c, "mips"),
      functools.partial(compile_c, "mipsel"),
      functools.partial(compile_c, "mips64"),
      functools.partial(compile_c, "powerpc"),
      functools.partial(compile_c, "powerpc64"),
      functools.partial(compile_c, "riscv64"),
      functools.partial(compile_c, "s390x"),
      functools.partial(compile_c, "sparc64"),
      functools.partial(compile_c, "sh4"),
      functools.partial(compile_c, "alpha"),
      functools.partial(compile_c, "m68k"),
      functools.partial(compile_c, "hppa"),
      ]
     ),
    (tmpl_1,
     t1_args,
     [functools.partial(compile_c, "x86_64"),
      functools.partial(compile_c, "win64"),
      functools.partial(compile_c, "arm"),
      functools.partial(compile_c, "aarch64"),
      functools.partial(compile_c, "mips"),
      functools.partial(compile_c, "mipsel"),
      functools.partial(compile_c, "mips64"),
      functools.partial(compile_c, "powerpc"),
      functools.partial(compile_c, "powerpc64"),
      functools.partial(compile_c, "riscv64"),
      functools.partial(compile_c, "s390x"),
      functools.partial(compile_c, "sparc64"),
      functools.partial(compile_c, "sh4"),
      functools.partial(compile_c, "alpha"),
      functools.partial(compile_c, "m68k"),
      functools.partial(compile_c, "hppa"),
      ]
     ),
    (tmpl_2,
     t2_args,
     [functools.partial(compile_c, "x86_64"),
      functools.partial(compile_c, "win64"),
      functools.partial(compile_c, "arm"),
      functools.partial(compile_c, "aarch64"),
      functools.partial(compile_c, "mips"),
      functools.partial(compile_c, "mipsel"),
      functools.partial(compile_c, "mips64"),
      functools.partial(compile_c, "powerpc"),
      functools.partial(compile_c, "powerpc64"),
      functools.partial(compile_c, "riscv64"),
      functools.partial(compile_c, "s390x"),
      functools.partial(compile_c, "sparc64"),
      functools.partial(compile_c, "sh4"),
      functools.partial(compile_c, "alpha"),
      functools.partial(compile_c, "m68k"),
      functools.partial(compile_c, "hppa"),
      ]
     ),
    (tmpl_3,
     t3_args,
     [functools.partial(compile_c, "x86_64"),
      functools.partial(compile_c, "win64"),
      functools.partial(compile_c, "arm"),
      functools.partial(compile_c, "aarch64"),
      functools.partial(compile_c, "mips"),
      functools.partial(compile_c, "mipsel"),
      functools.partial(compile_c, "mips64"),
      functools.partial(compile_c, "powerpc"),
      functools.partial(compile_c, "powerpc64"),
      functools.partial(compile_c, "riscv64"),
      functools.partial(compile_c, "s390x"),
      functools.partial(compile_c, "sparc64"),
      functools.partial(compile_c, "sh4"),
      functools.partial(compile_c, "alpha"),
      functools.partial(compile_c, "m68k"),
      functools.partial(compile_c, "hppa"),
      ]
     ),
    (tmpl_10,
     t10_args,
     [functools.partial(compile_crystal, "x86_64"),
      functools.partial(compile_crystal, "x86_64_dbg"),
      ]
     ),
]


def chop_data(data: bytes) -> List[int]:
    lst = [ ]
    for i in range(0, len(data), 8):
        chunk = data[i : i + 8]
        if len(chunk) < 8:
            chunk = chunk + b"\x00" * (8 - len(chunk))
        lst.append(struct.unpack("<Q", chunk)[0])
    return lst


def gen_source_file(template: str, **kwargs) -> str:
    return template.format(**kwargs)


def main():
    # TODO: Generate different jpeg files for each team
    img_path = "flag.jpg"
    output_dir = "output"

    with open(img_path, "rb") as f:
        data = f.read()

    chopped_ints = chop_data(data)
    pbar = progressbar.ProgressBar(widgets=_PROGRESS_WIDGETS, max_value=len(chopped_ints)).start()

    # generate C files
    random.seed(0x1337)

    # exppand TEMPLATES
    templates_expanded = [ ]
    for tmpl, get_args, compile_choices in TEMPLATES:
        for compile_choice in compile_choices:
            templates_expanded.append((tmpl, get_args, compile_choice))

    for idx, int_ in enumerate(chopped_ints):
        tmpl, get_args, compile_ = random.choice(templates_expanded)
        # tmpl, get_args, compile_ = templates_expanded[idx]
        arch = compile_.args[0]

        endness = ENDNESS.get(arch, "le")

        kwargs = get_args(int_, endness)
        src = gen_source_file(tmpl, **kwargs)

        try:
            os.mkdir(output_dir)
        except FileExistsError:
            pass
        dst = os.path.join(output_dir, str(idx))
        compile_(src, dst)
        pbar.update(idx)
    pbar.finish()

if __name__ == "__main__":
   main()

