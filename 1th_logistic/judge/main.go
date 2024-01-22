package main

import (
	"errors"
	"fmt"
	"log"
	"math"
	"os"
	"strconv"
	"strings"
	"time"
)

type PointConfigure struct {
	PointConfigureBase
	Conf         string
	Ref          string
	Time         time.Duration
	TimeStr      string
	MaxScoreTime time.Duration
	MinScore     float64
}

func Point(
	conf, ref, timeStr string, maxScoreTime float64, minScore, maxScore float64,
) *PointConfigure {
	t, _ := ParseTime(timeStr)
	return &PointConfigure{
		Conf:         conf,
		Ref:          ref,
		Time:         t,
		TimeStr:      timeStr,
		MaxScoreTime: time.Duration(maxScoreTime * float64(time.Second)),
		MinScore:     minScore,
		PointConfigureBase: PointConfigureBase{
			MaxScore: maxScore,
		},
	}
}

var points = []PointConfigureInterface{
	Point("conf_1.data", "ref_1.data", "00:01:00", 1.5, 10, 50),
	Point("conf_2.data", "ref_2.data", "00:02:00", 3.0, 10, 50),
}

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	var duration time.Duration
	cnf := GetProblemPath(pnt.Conf)
	ref := GetProblemPath(pnt.Ref)
	err := MaskRead(ref)
	if err != nil {
		return nil, err
	}
	job, err := SlurmAlloc("-p", "C064M0256G", "-N1", "-n8")
	if err != nil {
		return nil, err
	}
	defer job.Cancel()
	err = os.Symlink(cnf, "conf.data")
	if err != nil {
		return nil, err
	}
	defer os.Remove("conf.data")
	out := "out.data"
	defer os.Remove(out)
	cmd := job.Command(nil, "time", "-f", "%E %M", "-o", "time.out", "./answer")
	defer os.Remove("time.out")
	cmd.Env = append(os.Environ(), "OMP_NUM_THREADS=8")
	finCh := make(chan bool)
	tCh := CommandTimeout(cmd, pnt.Time, finCh)
	var stdout, stderr []byte
	var stat int
	go func() {
		stdout, stderr, stat, err = WaitCommandAndGetOutput(cmd, &duration)
		finCh <- true
	}()
	tErr := <-tCh
	if tErr != nil {
		_, _, _, err := WaitCommandAndGetOutput(job.ShellCommand(nil, "pkill -u $(whoami)"))
		if err != nil {
			log.Println(err)
		}
		return &Result{
			Score:           0,
			Message:         "Time Limit Exceeded",
			DetailedMessage: tErr.Error(),
		}, nil
	}
	if stat != 0 || err != nil {
		return &Result{
			Score:           0,
			Message:         "Runtime Error",
			DetailedMessage: fmt.Sprintf("%v%v", string(stdout), string(stderr)),
		}, nil
	}
	err = Unmask(ref)
	if err != nil {
		return nil, err
	}
	timeOut, err := os.ReadFile("time.out")
	if err != nil {
		return nil, err
	}
	wallStr, memStr, ok := strings.Cut(TrimBlank(string(timeOut)), " ")
	if !ok {
		return nil, errors.New("time.out format error")
	}
	wall, err := ParseTime(wallStr)
	if err != nil {
		return nil, err
	}
	mem, err := strconv.ParseInt(memStr, 10, 64)
	if err != nil {
		return nil, err
	}
	custom := map[string]interface{}{
		"time": wall.String(),
		"mem":  MemKBToString(mem),
	}
	fmt.Println("starting to compare")
	cmp := NewBinaryComparer()
	err = cmp.CompareFile(ref, out)
	if err != nil {
		return &Result{
			Score:           0,
			Message:         "Wrong Answer",
			DetailedMessage: err.Error(),
			Custom:          custom,
		}, nil
	}
	if wall > pnt.Time {
		return &Result{
			Score:           0,
			Message:         "Time Limit Exceeded",
			DetailedMessage: tErr.Error(),
			Custom:          custom,
		}, nil
	}
	score := pnt.MaxScore + (pnt.MinScore-pnt.MaxScore)*math.Log(float64(wall)/float64(pnt.MaxScoreTime))/math.Log(float64(pnt.Time)/float64(pnt.MaxScoreTime))
	message := "Success"
	if score >= pnt.MaxScore {
		score = pnt.MaxScore
		message = "Accepted"
	}
	return &Result{
		Score:           score,
		Message:         message,
		DetailedMessage: message,
		Custom:          custom,
	}, nil
}

var ErrCompileFailed = errors.New("compile error")

func before(j *StandardJudger) error {
	err := UnarchiveSolution(".")
	if err != nil {
		return err
	}
	stdout, stderr, stat, err := RunCommand("g++", "-O3", "-fopenmp", "-mavx512f", "-o", "answer", "answer.cpp")
	if stat != 0 || err != nil {
		defer j.Halt()
		ReportMessage(ResultToMap(&Result{
			Score:           0,
			Message:         "Compile Error",
			DetailedMessage: string(stderr) + string(stdout) + err.Error(),
		}, ResultFinal))
		return ErrCompileFailed
	}
	defer RunCommand("make", "clean")
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
