package main

import (
	"bytes"
	"errors"
	"encoding/json"
	"os/exec"
)

type JSON map[string]interface{}

func jsonDecode(data []byte) (JSON,error) {
	out := make(map[string]interface{})
	err := json.Unmarshal(data, &out);
	return out, err
}

func jsonEncode(data JSON) ([]byte) {
	s,_ := json.Marshal(data)
	return s
}

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


