from typing import List, Callable, Tuple, Dict
import random
import struct
import tempfile
import subprocess
import os
import shutil
import functools
import hashlib

import progressbar


DEBUG = False

_PROGRESS_WIDGETS = [progressbar.Percentage(), ' ', progressbar.Bar(), ' ', progressbar.Timer(), ' ', progressbar.ETA()]


def chop_data(data: bytes) -> List[int]:
    lst = [ ]
    for i in range(0, len(data), 8):
        chunk = data[i : i + 8]
        if len(chunk) < 8:
            chunk = chunk + b"\x00" * (8 - len(chunk))
        lst.append(struct.unpack("<Q", chunk)[0])
    return lst


def binary_type(binary_path: str) -> str:
    proc = subprocess.Popen(["file", binary_path],
                            stdout=subprocess.PIPE,
                            )
    stdout, _ = proc.communicate()
    if b"MS Windows" in stdout:
        return "win64"

    with open(binary_path, "rb") as f:
        data = f.read()
        if b"Crystal" in data:
            return "crystal"

    if b"LSB" in stdout:
        return "elf-le"
    return "elf-be"


def main():
    img_path = "flag.jpg"
    output_dir = "output"

    with open(img_path, "rb") as f:
        data = f.read()

    chopped_ints = chop_data(data)
    pbar = progressbar.ProgressBar(widgets=_PROGRESS_WIDGETS, max_value=len(chopped_ints)).start()
    successes, failures, ignored = 0, 0, 0

    for idx, int_ in enumerate(chopped_ints):
        nbytes = struct.pack("<Q", int_)

        binary_path = f"{output_dir}/{idx}"
        # determine the type of the binary
        btype = binary_type(binary_path)

        if btype == "win64":
            cmd = ["wine64", binary_path]
        else:
            cmd = [binary_path]

        proc = subprocess.Popen(cmd,
                                stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE)

        if btype == "crystal":
            # feed the number
            stdout, stderr = proc.communicate(str(int_).encode("ascii"))
        else:
            if btype == "elf-be":
                # swap endianness
                int_ = struct.unpack(">Q", struct.pack("<Q", int_))[0]
            # feed the number
            stdout, stderr = proc.communicate(str(int_).encode("ascii"))

        if b"Congrats" in stdout:
            successes += 1
        elif b":(" in stdout:
            print(f"Failure with binary {binary_path}. Intended input: {int_}")
            failures += 1
            print(successes, failures, ignored)
        else:
            ignored += 1
            print(successes, failures, ignored)

        pbar.update(idx)
    pbar.finish()
    print(successes, failures, ignored)

if __name__ == "__main__":
   main()

