package main

import (
	"errors"
	"bytes"
	"os/exec"
)

func CatchPanic(r interface{}) error {
	var err error
	if r != nil {
		switch t := r.(type) {
		case string:
			err = errors.New(t)
		case error:
			err = t
		default:
			err = errors.New("Unknown error")
		}
	}
	return err
}

func TryFunction(f func()) (err error) {
	defer func() {
		err = CatchPanic(recover())
	}()
	f()
	return nil
}

func Communicate(cmd *exec.Cmd, data string) (string,error) {
	wr, err := cmd.StdinPipe()
	if err != nil {
		return "", err
	}
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	if err = cmd.Start(); err != nil {
		return "", err
	}
	wr.Write([]byte(data))
	if err = cmd.Wait(); err != nil {
		return "", err
	}
	return string(stdout.Bytes()), nil
}
