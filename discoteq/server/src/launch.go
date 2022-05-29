package main

import (
	"github.com/valyala/gorpc"
	"log"
)

type LaunchMessage struct {
	Token string
}

type Launcher struct {
	conn *gorpc.Client
	tokenChan chan string
}

func (r *Launcher) do_launch(token string) {
	log.Printf("Launching %s", token);

	r.conn.Call(&LaunchMessage{
		Token: token,
	})
}

func (r *Launcher) run() {
	gorpc.RegisterType(LaunchMessage{})

	conn := gorpc.NewUnixClient(*rpcsock)
	conn.Start()
	defer conn.Stop()
	r.conn = conn
	for {
		select {
		case token := <-r.tokenChan:
			log.Printf("Got token %v", token)
			err := TryFunction(func() {
				r.do_launch(token)
			})
			if err != nil {
				log.Printf("Encountered panic %v", err)
			}
			break
		}
	}
}

func (r *Launcher) launch(token string) {
	log.Printf("Waiting on chan")
	r.tokenChan <- token
	log.Printf("Wrote to chan %s",token)
}



func newLauncher() * Launcher{
	return &Launcher{
		tokenChan: make(chan string),
	}
}
