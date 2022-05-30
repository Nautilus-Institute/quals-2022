package main

import (
	"flag"
	"github.com/valyala/gorpc"
	"time"
	"log"
	"github.com/docker/docker/api/types"
	"github.com/docker/docker/api/types/container"
	dockerapi "github.com/docker/docker/client"
	"context"
)


var (
	docker    *dockerapi.Client
)

var rpcsock = flag.String("sock", "/launchersock/launchersock", "Unix socket for launcher")

type LaunchMessage struct {
	Token string
}

type Launcher struct {
	conn *gorpc.Client
	tokenChan chan string
}

func startContainer(token string) error {
	ctx := context.Background()

	// TODO: Change your hostname here
	url := "http://127.0.0.1:8080"

	resp, err := docker.ContainerCreate(ctx, &container.Config{
		Image: "discoteq-challenge:latest",
		Cmd:   []string{"timeout", "120", "/run.sh", url, token, "auto", "/responses.txt"},
	},nil, nil, nil, "")
	if err != nil {
		log.Printf("Failed to create container: %v", err)
		return err
	}
	if err := docker.ContainerStart(ctx, resp.ID, types.ContainerStartOptions{}); err != nil {
		log.Printf("Failed to start container %s: %v", resp.ID, err)
		return err
	}

	waitCtx, cancel := context.WithTimeout(ctx, 120*time.Second)

	go (func() {
		defer cancel()

		statusCh, errCh := docker.ContainerWait(waitCtx, resp.ID, container.WaitConditionNotRunning)
		select {
		case err := <-errCh:
			log.Printf("Container %s timed out: %v", resp.ID, err)
			if err := docker.ContainerKill(ctx, resp.ID, "SIGKILL"); err != nil {
				log.Printf("Failed to kill container %s: %v", resp.ID, err)
			}
		case <-statusCh:
		}

		if err := docker.ContainerRemove(ctx, resp.ID, types.ContainerRemoveOptions{}); err != nil {
			log.Printf("Failed to remove container %s: %v", resp.ID, err)
		}
	})()
	return nil
}

func handler(clientAddr string, request interface{}) interface{} {
	log.Printf("Obtained request %+v from the client %s\n", request, clientAddr)
	if v, ok := request.(LaunchMessage); ok {
		log.Printf("Starting client");
		if startContainer(v.Token) != nil {
			return false
		}
		return true
	}
	log.Printf("Not valid message")
	return nil
}

func main() {
	gorpc.RegisterType(LaunchMessage{})

	s := NewUnixServer(*rpcsock, handler)

	var err error
	docker, err = dockerapi.NewEnvClient()
	if err != nil {
		log.Fatal("Failed to create docker environment: %v", err)
	}

	if err := s.Serve(); err != nil {
		log.Fatalf("Cannot start rpc server: %s", err)
	}
}
