import sys
import string
import binascii

from flask import Flask, request


CHARSET = string.ascii_letters + string.digits
EXPECTED = binascii.crc32(b"the")


def verify(s: str, team_punyname: str="") -> bool:
    if team_punyname:
        if not s.startswith(team_punyname):
            return False
        remaining = s[len(team_punyname):]
    else:
        remaining = s

    # if not all(ch in CHARSET for ch in remaining):
    #     return False
    if remaining == "the":
        return False

    print(binascii.crc32(s.encode("ascii")))
    return binascii.crc32(s.encode("ascii")) == EXPECTED


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "test":
        print(f"Expected result: {EXPECTED}")
        while True:
            s = input("String: ")
            print(verify(s))


app = Flask(__name__)

@app.route("/", methods=['POST'])
def validate_from_post():
    team_punyname = request.form.get('team_name_punycode', None)
    if not team_punyname:
        return 'wrong', 406
    candidate_answer = request.form.get('answer', None)
    if not candidate_answer:
        return 'wrong', 406
    app.logger.info('%s guessin %s', team_punyname, candidate_answer)
    if verify(candidate_answer, team_punyname=team_punyname):
        return 'OK', 200
    return 'wrong', 466


if __name__ == "__main__":
    main()

