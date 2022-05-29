
package main

import (
	"github.com/valyala/gorpc"
	"log"
)

type LaunchMessage struct {
	Uid string
	Code string
	Ticket string
}
type GetResultMessage struct {
	Uid string
}

type Launcher struct {
	conn *gorpc.Client
	closeChan chan bool
}

func (r *Launcher) launch(uid, code, ticket string) (bool,error) {
	log.Printf("Launching %s", code);

	resp, err := r.conn.Call(&LaunchMessage{
		Uid: uid,
		Code: code,
		Ticket: ticket,
	})
	log.Printf("Got %v %v", resp, err)
	if err != nil {
		return false, err
	}
	if r, ok := resp.(bool); ok {
		return r, nil
	}
	return false, nil
}

func (r *Launcher) get_output(uid string) (bool, string, error) {
	log.Printf("getting output %s", uid);

	resp, err := r.conn.Call(&GetResultMessage{
		Uid: uid,
	})
	log.Printf("Got %v %v", resp, err)
	if err != nil {
		return false, "", err
	}
	if r, ok := resp.(bool); ok {
		return r, "", nil
	}
	if r, ok := resp.(string); ok {
		return true, r, nil
	}
	return false, "", nil
}

func (r *Launcher) run() {
	gorpc.RegisterType(LaunchMessage{})
	gorpc.RegisterType(GetResultMessage{})

	conn := gorpc.NewUnixClient(*rpcsock)
	conn.Start()
	defer conn.Stop()
	r.conn = conn
	for {
		select {
		case <-r.closeChan:
			break
		}
	}
}

func newLauncher() * Launcher{
	return &Launcher{
		closeChan: make(chan bool,1),
	}
}
