package main

import (
	"fmt"
	"io"
	"os/exec"
	"strings"
	"time"
)

func WaitCommandAndGetOutput(c *exec.Cmd, elapsed ...*time.Duration) (
	stdout []byte, stderr []byte, statusCode int, err error,
) {
	var outPipe, errPipe io.ReadCloser
	outPipe, err = c.StdoutPipe()
	if err != nil {
		return
	}
	defer outPipe.Close()
	errPipe, err = c.StderrPipe()
	if err != nil {
		return
	}
	defer errPipe.Close()
	st := time.Now()
	err = c.Start()
	if err != nil {
		return
	}
	stdout, err = io.ReadAll(outPipe)
	if err != nil {
		return
	}
	stderr, err = io.ReadAll(errPipe)
	if err != nil {
		return
	}
	err = c.Wait()
	elapsedReal := time.Since(st)
	for _, elapsedItem := range elapsed {
		*elapsedItem = elapsedReal
	}
	statusCode = c.ProcessState.ExitCode()
	return
}

func RunCommand(
	cmd string, args ...string,
) (
	stdout []byte, stderr []byte, statusCode int, err error,
) {
	return WaitCommandAndGetOutput(exec.Command(cmd, args...))
}

func BashRun(cmd string) (stdout []byte, stderr []byte, statusCode int, err error) {
	return RunCommand("bash", "-c", cmd)
}

func CommandTimeout(cmd *exec.Cmd, timeout time.Duration, finish chan bool) chan error {
	ch := make(chan error)
	go func() {
		select {
		case <-time.NewTimer(timeout).C:
			if cmd.Process == nil {
				ch <- fmt.Errorf("process not started")
				return
			}
			if cmd.ProcessState == nil {
				err := cmd.Process.Kill()
				if err != nil {
					ch <- err
				} else {
					ch <- fmt.Errorf("time limit of %v exceeded", timeout)
				}
				return
			}
			ch <- nil
			return
		case <-finish:
			ch <- nil
			return
		}
	}()
	return ch
}

const blank = "\r\n\t "

func TrimBlank(str string) string {
	return strings.Trim(str, blank)
}

func MemKBToString(mem int64) string {
	if mem < 1024 {
		return fmt.Sprintf("%dK", mem)
	}
	if mem < 1024*1024 {
		return fmt.Sprintf("%.2fM", float64(mem)/1024)
	}
	return fmt.Sprintf("%.2fG", float64(mem)/(1024*1024))
}
