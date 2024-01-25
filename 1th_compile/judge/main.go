package main

import (
	"errors"
	"fmt"
	"time"
)

type PointConfigure struct {
	PointConfigureBase
	Time    time.Duration
	TimeStr string
	File    string
}

func Point(
	file, timeStr string, maxScore float64,
) *PointConfigure {
	t, _ := ParseTime(timeStr)
	return &PointConfigure{
		File:    file,
		Time:    t,
		TimeStr: timeStr,
		PointConfigureBase: PointConfigureBase{
			MaxScore: maxScore,
		},
	}
}

var points = []PointConfigureInterface{
	Point("Makefile", "00:01:00", 40),
	Point("CMakeLists.txt", "00:01:00", 60),
}

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	job, err := SlurmAlloc("-p", "GPU40G", "--gres=gpu:1", "-t", pnt.TimeStr)
	if err != nil {
		return nil, err
	}
	defer job.Cancel()
	defer BashRun("rm hello_cuda hello_mpi hello_omp -f")
	switch pnt.File {
	case "Makefile":
		{
			cmd := job.Command(nil, "make")
			stdout, stderr, statusCode, err := WaitCommandAndGetOutput(cmd)
			if err != nil || statusCode != 0 {
				return &Result{
					Score:           0,
					Message:         "Compile Error",
					DetailedMessage: fmt.Sprintf("exit code %v\r\n%s%s\n%v", statusCode, stdout, stderr, err),
				}, nil
			}
		}
	case "CMakeLists.txt":
		{
			fmt.Println("Compiling CMake")
			cmd := job.ShellCommand(nil, "mkdir -p build; cd build; cmake ..; make; cp hello_* ..")
			stdout, stderr, statusCode, err := WaitCommandAndGetOutput(cmd)
			fmt.Println("Compiled CMake")
			if err != nil || statusCode != 0 {
				return &Result{
					Score:           0,
					Message:         "Compile Error",
					DetailedMessage: fmt.Sprintf("exit code %v\r\n%s%s\n%v", statusCode, stdout, stderr, err),
				}, nil
			}
		}
	}
	cmd := job.Command(nil, "bash", GetProblemPath("do_test.sh"))
	stdout, stderr, statusCode, err := WaitCommandAndGetOutput(cmd)
	fmt.Printf("stdout: %s\nstderr: %s\n", stdout, stderr)
	if statusCode == 1 {
		return &Result{
			Score:           0,
			Message:         "Compile Error",
			DetailedMessage: "compiled executable not found",
		}, nil
	}
	if statusCode == 2 {
		return &Result{
			Score:           0,
			Message:         "Runtime Error",
			DetailedMessage: "compiled executable runtime error",
		}, nil
	}
	if statusCode == 3 {
		return &Result{
			Score:           0,
			Message:         "Wrong Answer",
			DetailedMessage: "compiled executable output not match",
		}, nil
	}
	if statusCode != 0 || err != nil {
		return &Result{
			Score:           0,
			Message:         "Runtime Error",
			DetailedMessage: fmt.Sprintf("test script exit code %v\n%v", statusCode, err),
		}, nil
	}
	return &Result{
		Score:           pnt.MaxScore,
		Message:         "Accepted",
		DetailedMessage: "Accepted",
	}, nil
}

var ErrCompileFailed = errors.New("compile error")

func before(j *StandardJudger) error {
	err := UnarchiveSolution(".")
	if err != nil {
		return err
	}
	_, _, _, err = BashRun(fmt.Sprintf("cp -r %s/hello_* .", GetProblemPath()))
	if err != nil {
		return err
	}
	return nil
}

func main() {
	sj := NewStandardJudger(points, judge)
	sj.Before(before)
	err := sj.Judge()
	if err != nil && err != ErrCompileFailed {
		sj.ReportError(err)
	}
}
