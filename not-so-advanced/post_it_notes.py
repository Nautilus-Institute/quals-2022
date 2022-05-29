#!/usr/bin/env python3
"""
Very simple HTTP server in python for logging requests
Usage::
    ./server.py [<port>]
"""
from http.server import BaseHTTPRequestHandler, HTTPServer
import urllib
import logging

def adler32x( buf ):
	if type(buf) == type(b''):
		buf = buf.decode('utf-8')

	s1 = 1
	s2 = 0

	for i in buf:
		s1 = (ord(i) + s1) % 65521
		s2 = (s2 + s1) % 65521

	return s1 ^ s2

class S(BaseHTTPRequestHandler):
    def send_succ(self):
        self.send_response(200)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write("POST request for {}".format(self.path).encode('utf-8'))

    def send_fail(self):
        self.send_response(666)
        self.send_header('Content-type', 'text/html')
        self.end_headers()
        self.wfile.write("POST request for {}".format(self.path).encode('utf-8'))

    def do_POST(self):
        content_length = int(self.headers['Content-Length']) # <--- Gets the size of data
        post_data = urllib.parse.parse_qs(self.rfile.read(content_length).decode('utf-8'))
        logging.info("POST request,\nPath: %s\nHeaders:\n%s\n\nBody:\n%s\n",
                str(self.path), str(self.headers), post_data)

        if 'answer' in post_data:
            if adler32x(post_data['answer'][0]) == 0x12e1:
                self.send_succ()
            else:
                self.send_fail()
        else:
            self.send_fail()


def run(server_class=HTTPServer, handler_class=S, port=8080):
    logging.basicConfig(level=logging.INFO)
    server_address = ('', port)
    httpd = server_class(server_address, handler_class)
    logging.info('Starting httpd...\n')
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()
    logging.info('Stopping httpd...\n')

if __name__ == '__main__':
    from sys import argv

    if len(argv) == 2:
        run(port=int(argv[1]))
    else:
        run()
