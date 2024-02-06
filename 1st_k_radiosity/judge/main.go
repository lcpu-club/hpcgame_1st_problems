package main

import (
	"errors"
	"fmt"
	"math"
	"os"
	"strconv"
	"strings"
	"time"
)

type PointConfigure struct {
	PointConfigureBase
	Conf         string
	Ref          []string
	Time         time.Duration
	TimeStr      string
	MaxScoreTime time.Duration
	MinScore     float64
}

func Point(
	conf string, ref []string, timeStr string, maxScoreTime float64, minScore, maxScore float64,
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
	Point("conf_1.data", []string{"cornellbox0_ref.bmp", "cornellbox1_ref.bmp", "cornellbox2_ref.bmp", "cornellbox3_ref.bmp", "cornellbox4_ref.bmp", "cornellbox5_ref.bmp"}, "00:02:00", 3.50, 5, 100),
}

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	var duration time.Duration
	cnf := GetProblemPath(pnt.Conf)
	// ref := GetProblemPath(pnt.Ref)
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
	out := []string{"cornellbox0.bmp", "cornellbox1.bmp", "cornellbox2.bmp", "cornellbox3.bmp", "cornellbox4.bmp", "cornellbox5.bmp"}
	// defer os.Remove(out)
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
	cmp := NewBmpComparer(2e5)
	for i := 0; i < 6; i++ {
		err = cmp.CompareFile(out[i], GetProblemPath(pnt.Ref[i]))
		if err != nil {
			return &Result{
				Score:           0,
				Message:         "Wrong Answer",
				DetailedMessage: err.Error(),
				Custom:          custom,
			}, nil
		}
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
		defer j.Halt()
		return err
	}
	stdout, stderr, stat, err := RunCommand("g++", "-O3", "-fopenmp", "-march=native", "-ffast-math", "-std=c++20", "-o", "answer", "answer.cpp")
	if stat != 0 || err != nil {
		defer j.Halt()
		ReportMessage(ResultToMap(&Result{
			Score:           0,
			Message:         "Compile Error",
			DetailedMessage: string(stderr) + string(stdout) + err.Error(),
		}, ResultFinal))
		return ErrCompileFailed
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
