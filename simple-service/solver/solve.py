#!/usr/bin/env python3

import os
import re
import socket
import sys


if __name__ == "__main__":
    # get host from environment
    host = os.getenv("HOST")
    if not host:
        print("No HOST supplied from environment")
        sys.exit(-1)

    # get port from environment
    port = int(os.getenv("PORT", "0"))
    if port <= 0 or port > 65535:
        print("Missing or invalid PORT supplied from environment")
        sys.exit(-1)

    ticket = os.getenv("TICKET", 'ticket{ShipWaterline3603n22:AH9-vzMSvCbPofN-SGHodCXkPX3McbRBo30Q4yv77wzWG4iG}')

    # connect to service
    print("HOST={}".format(host))
    print("PORT={}".format(port))
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((host, port))

    # receive "Ticket please:"
    res = s.recv(128)
    print(res)

    # send ticket
    s.send((ticket + '\n').encode('utf-8'))

    # receive math challenge
    challenge = s.recv(128)
    m = re.match(r"(\d+) \+ (\d+)", challenge.decode("utf-8"))
    x = int(m.group(1))
    y = int(m.group(2))

    # send math answer
    print("%d + %d = %d" % (x, y, x + y))
    s.send(("%d\n" % (x + y)).encode("utf-8"))

    # get flag
    result = s.recv(256)

    # output flag
    print(result.decode("utf-8"))
