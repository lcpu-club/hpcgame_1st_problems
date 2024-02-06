package main

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"os/exec"
	"path/filepath"

	"github.com/mholt/archiver/v3"
)

var problemPath = ""

func GetProblemPath(subpath ...string) string {
	if problemPath == "" {
		stdout, stderr, statusCode, err := RunCommand("hpcgame", "problem-path")
		if statusCode != 0 {
			fmt.Println("ERROR:", stderr, stdout)
			os.Exit(statusCode)
		}
		if err != nil {
			fmt.Println("ERROR:", err)
			os.Exit(-1)
		}
		problemPath = TrimBlank(string(stdout))
	}
	return filepath.Join(append([]string{problemPath}, subpath...)...)
}

var solutionPath = ""

func GetSolutionPath() string {
	if solutionPath == "" {
		stdout, stderr, statusCode, err := RunCommand("hpcgame", "solution-path")
		if statusCode != 0 {
			fmt.Println("ERROR:", stderr, stdout)
			os.Exit(statusCode)
		}
		if err != nil {
			fmt.Println("ERROR:", err)
			os.Exit(-1)
		}
		solutionPath = TrimBlank(string(stdout))
	}
	return solutionPath
}

func FetchSolution(target string) error {
	s, err := os.Open(GetSolutionPath())
	if err != nil {
		return err
	}
	defer s.Close()
	t, err := os.Create(target)
	if err != nil {
		return err
	}
	defer t.Close()
	_, err = io.Copy(t, s)
	return err
}

// UnarchiveFile supports tar.gz, tar.bz2, tar.xz, tar, zip and rar
func UnarchiveFile(archivePath string, destPath string) error {
	f, err := os.Open(archivePath)
	if err != nil {
		return err
	}
	var u archiver.Unarchiver
	if b, err := archiver.DefaultTarGz.Match(f); b && (err != nil) {
		u = archiver.NewTarGz()
	} else if b, err := archiver.DefaultTarBz2.Match(f); b && (err != nil) {
		u = archiver.NewTarBz2()
	} else if b, err := archiver.DefaultTarXz.Match(f); b && (err != nil) {
		u = archiver.NewTarXz()
	} else {
		u, err = archiver.ByHeader(f)
		if err != nil {
			return err
		}
	}
	return u.Unarchive(archivePath, destPath)
}

func UnarchiveSolution(destPath string) error {
	return UnarchiveFile(GetSolutionPath(), destPath)
}

type Result struct {
	Score           float64
	MaxScore        float64
	Message         string
	DetailedMessage string
	Subtasks        []interface{}
	Custom          map[string]interface{}
}

type ResultType int

const (
	ResultPartial ResultType = 1
	ResultFinal   ResultType = 2
	ResultSubtask ResultType = 3
)

func ResultToMap(rslt *Result, typ ResultType) map[string]interface{} {
	r := make(map[string]interface{})
	if typ == ResultPartial {
		r["done"] = false
		r["score"] = rslt.Score
		r["message"] = rslt.Message
		return r
	}
	if typ == ResultFinal {
		r["done"] = true
	}
	r["score"] = rslt.Score
	r["max-score"] = rslt.MaxScore
	r["message"] = rslt.Message
	r["detailed-message"] = rslt.DetailedMessage
	if rslt.Subtasks != nil {
		r["subtasks"] = rslt.Subtasks
	}
	r["custom"] = rslt.Custom
	return r
}

func ReportMessage(msg map[string]interface{}) {
	js, err := json.Marshal(msg)
	if err != nil {
		fmt.Println("ERROR:", err)
		os.Exit(-1)
	}
	cnt := 0
	for {
		cnt++
		if cnt > 2 {
			fmt.Println("ERROR:", err)
			os.Exit(-1)
		}
		err = reportMessageInternal(js)
		if err == nil {
			return
		}
	}
}

func reportMessageInternal(js []byte) error {
	fmt.Println(string(js))
	err := os.WriteFile("hpcgame.judge.result.json", js, os.FileMode(0600))
	if err != nil {
		return err
	}
	defer os.Remove("hpcgame.judge.result.json")
	cmd := exec.Command("hpcgame", "report", "hpcgame.judge.result.json")
	out, er, st, err := WaitCommandAndGetOutput(cmd)
	if st != 0 {
		return fmt.Errorf("report returns %v with %s %s", st, out, er)
	}
	if err != nil {
		return err
	}
	return nil
}

func MaskRead(f ...string) error {
	stdout, stderr, stat, err := RunCommand("hpcgame", append([]string{"mask-read"}, f...)...)
	if stat != 0 {
		return fmt.Errorf("%s %s", stdout, stderr)
	}
	if err != nil {
		return err
	}
	return nil
}

func MaskWrite(f ...string) error {
	stdout, stderr, stat, err := RunCommand("hpcgame", append([]string{"mask-write"}, f...)...)
	if stat != 0 {
		return fmt.Errorf("%s %s", stdout, stderr)
	}
	if err != nil {
		return err
	}
	return nil
}

func Unmask(f ...string) error {
	stdout, stderr, stat, err := RunCommand("hpcgame", append([]string{"unmask"}, f...)...)
	if stat != 0 {
		return fmt.Errorf("%s %s", stdout, stderr)
	}
	if err != nil {
		return err
	}
	return nil
}
