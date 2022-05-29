import string
import binascii
import itertools
import sys


CHARSET = (string.ascii_letters + string.digits).encode("ascii")
EXPECTED = binascii.crc32(b"the")


def main():
    if len(sys.argv) == 2:
        prefix = sys.argv[1].encode("ascii")
    else:
        prefix = b""
    for length in range(3, 20):
        for x in itertools.permutations(CHARSET, length):
            if binascii.crc32(prefix + bytes(x)) == EXPECTED:
                print(prefix + bytes(x))


if __name__ == "__main__":
    main()

