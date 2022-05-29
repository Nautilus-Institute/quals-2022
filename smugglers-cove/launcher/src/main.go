package main

import (
	"flag"
	"github.com/valyala/gorpc"
	"time"
	"log"
	"net/url"
	"os"
	"github.com/docker/docker/api/types"
	"github.com/docker/go-units"
	"github.com/docker/docker/api/types/container"
	dockerapi "github.com/docker/docker/client"
	"io/ioutil"
	"context"
	"crypto/rand"
	"encoding/hex"
	"path/filepath"
	"sync"
)


var (
	docker    *dockerapi.Client
)

var rpcsock = flag.String("sock", "/launchersock/launchersock", "Unix socket for launcher")
var code_dir = flag.String("code", "/code", "Local directory to save code")
var code_volume = flag.String("volume", "/code", "Directory to attach as code volume (must be absolute)")

func randomHex(n int) (string, error) {
  bytes := make([]byte, n)
  if _, err := rand.Read(bytes); err != nil {
    log.Printf("UNABLE TO READ RANDOM BYTES %v", err)
    return "", err
  }
  return hex.EncodeToString(bytes), nil
}


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
	codeChan chan string
}

var results = make(map[string]string)
var results_mutex sync.RWMutex

func set_result(uid, val string) {
	results_mutex.Lock()
	defer results_mutex.Unlock()
	results[uid] = val
}
func pop_result(uid string) interface{} {
	results_mutex.Lock()
	defer results_mutex.Unlock()
	v, ok := results[uid]
	log.Printf("Getting res for %s -> %s %v", uid, v, ok)
	if !ok {
		return false
	}
	delete(results, uid)
	return v
}

func startContainer(uid, code, ticket string) error {
	ctx := context.Background()

	if (len(code) > 433) {
		log.Printf("Code too long")
		set_result(uid, "Code too long")
		return nil
	}

	rb, err := randomHex(16)
	if err != nil {
		return err
	}
	rb2, err := randomHex(10)
	if err != nil {
		return err
	}
	rb3, err := randomHex(10)
	if err != nil {
		return err
	}

	code_local,err := filepath.Abs(*code_dir)
	if err != nil {
		log.Printf("Cannot write code %v",err)
		return err
	}

	fname := rb+".lua"

	container_dir := "/"+rb2
	container_code_file := container_dir + "/"+fname
	container_ticket_file := "/.ticket"
	log.Printf("Mounting as %s",container_code_file)

	volume_dir := *code_volume + container_dir
	local_dir := code_local + container_dir
	local_code_file := local_dir + "/"+fname
	local_ticket_file := local_dir + "/.ticket"
	volume_ticket_file := volume_dir + "/.ticket"
	os.MkdirAll(local_dir, os.ModePerm)
	//os.Chmod(local_dir, 0777)

	container_dir_out := "/"+rb3
	container_file_out := container_dir_out + "/"+fname
	volume_dir_out := *code_volume + container_dir_out

	local_dir_out := code_local + container_dir_out
	local_file_out := local_dir_out + "/"+fname
	os.MkdirAll(local_dir_out, os.ModePerm)
	os.Chmod(local_dir_out, 0777)

	log.Printf("Writing ticket to %s", local_ticket_file)
	f, err := os.Create(local_ticket_file)
	if err != nil {
		log.Printf("Cannot write ticket %v",err)
		return err
	}
	n, err := f.WriteString("ticket="+url.QueryEscape(ticket))
	if err != nil {
		log.Printf("Cannot write ticket %v",err)
		return err
	}
	log.Printf("Wrote %u bytes", n)
	f.Sync()


	log.Printf("Writing code to %s", local_code_file)
	f, err = os.Create(local_code_file)
	if err != nil {
		log.Printf("Cannot write code %v",err)
		return err
	}
	n, err = f.WriteString(code)
	if err != nil {
		log.Printf("Cannot write code %v",err)
		return err
	}
	log.Printf("Wrote %u bytes", n)
	f.Sync()
	f.Close()

	resp, err := docker.ContainerCreate(ctx, &container.Config{
		Image: "smugglers-cove-challenge:latest",
		Cmd:   []string{"/bin/bash","-c","timeout 1500 /challenge/cove "+container_code_file+" &> "+container_file_out },
		AttachStdout: true,
		AttachStderr: true,
		Tty: true,
	}, &container.HostConfig{
		Binds: []string{
			volume_dir + ":" + container_dir,
			volume_dir_out + ":" + container_dir_out,
			volume_ticket_file + ":" + container_ticket_file,
		},
		Resources: container.Resources{
			Ulimits: []*units.Ulimit{
				&units.Ulimit{
					Name: "fsize",
					Soft: 0x2000,
					Hard: 0x2000,
				},
			},
		},
	}, nil, nil, "")
	if err != nil {
		log.Printf("Failed to create container: %v", err)
		return err
	}
	if err := docker.ContainerStart(ctx, resp.ID, types.ContainerStartOptions{}); err != nil {
		log.Printf("Failed to start container %s: %v", resp.ID, err)
		return err
	}

	waitCtx, cancel := context.WithTimeout(ctx, 1500*time.Second)

	go (func(){
		defer cancel()
		defer (func(){
			os.RemoveAll(local_dir)
			os.RemoveAll(local_dir_out)
		})()

		statusCh, errCh := docker.ContainerWait(waitCtx, resp.ID, container.WaitConditionNotRunning)
		select {
		case err := <-errCh:
			log.Printf("Container %s timed out: %v", resp.ID, err)
			if err := docker.ContainerKill(ctx, resp.ID, "SIGKILL"); err != nil {
				log.Printf("Failed to kill container %s: %v", resp.ID, err)
			}
		case <-statusCh:
		}
		log.Printf("Container finished")
		time.Sleep(1 * time.Second)

		content, err := ioutil.ReadFile(local_file_out)
		if err != nil {
			log.Printf("Failed to get logs %v", err)
			set_result(uid, "Error reading output")
		} else {
			if len(content) > 0x1000 {
				content = content[:0x1000]
			}
			log.Printf("Final Output %s", content)
			set_result(uid, string(content[:]))
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
		if startContainer(v.Uid, v.Code, v.Ticket) != nil{
			return false
		}
		return true
	}
	if v, ok := request.(GetResultMessage); ok {
		return pop_result(v.Uid)
	}

	log.Printf("Not valid message...")
	return nil
}

func main() {
	flag.Parse()

	gorpc.RegisterType(LaunchMessage{})
	gorpc.RegisterType(GetResultMessage{})

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
