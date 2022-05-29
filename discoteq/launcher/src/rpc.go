package main

import (
	"github.com/valyala/gorpc"
	"net"
	"log"
	"io"
	"os"
)

type netListener struct {
	F func(addr string) (net.Listener, error)
	L net.Listener
}

func (ln *netListener) Init(addr string) (err error) {
	ln.L, err = ln.F(addr)
	return
}

func (ln *netListener) ListenAddr() net.Addr {
	if ln.L != nil {
		return ln.L.Addr()
	}
	return nil
}

func (ln *netListener) Accept() (conn io.ReadWriteCloser, clientAddr string, err error) {
	c, err := ln.L.Accept()
	if err != nil {
		return nil, "", err
	}
	return c, c.RemoteAddr().String(), nil
}

func (ln *netListener) Close() error {
	return ln.L.Close()
}

// NewUnixServer creates a server listening for unix connections
// on the given addr and processing incoming requests
// with the given HandlerFunc.
//
// The returned server must be started after optional settings' adjustment.
//
// The corresponding client must be created with NewUnixClient().
func NewUnixServer(addr string, handler gorpc.HandlerFunc) *gorpc.Server {
	return &gorpc.Server{
		Addr:    addr,
		Handler: handler,
		Listener: &netListener{
			F: func(addr string) (net.Listener, error) {
				os.Remove(addr)
				res, err := net.Listen("unix", addr)
				os.Chmod(addr, 0777)
				log.Printf("after creating socket %s", addr)
				return res, err
			},
		},

		// Sacrifice the number of Write() calls to the smallest
		// possible latency, since it has higher priority in local IPC.
		FlushDelay: -1,
	}
}
