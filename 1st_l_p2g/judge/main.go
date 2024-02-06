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
	In           string
	Ans          string
	Time         time.Duration
	TimeStr      string
	MaxScoreTime time.Duration
	MinScore     float64
}

func Point(
	in, ans, timeStr string, t, maxScoreTime float64, minScore, maxScore float64,
) *PointConfigure {
	return &PointConfigure{
		In:           in,
		Ans:          ans,
		Time:         time.Duration(t * float64(time.Second)),
		TimeStr:      timeStr,
		MaxScoreTime: time.Duration(maxScoreTime * float64(time.Second)),
		MinScore:     minScore,
		PointConfigureBase: PointConfigureBase{
			MaxScore: maxScore,
		},
	}
}

var points = []PointConfigureInterface{
	// Point("inputs/512-8.dat", "outputs/512-8.dat", "00:00:15", 0.15, 0.06, 1, 2),
	// Point("inputs/512-32.dat", "outputs/512-32.dat", "00:00:15", 0.20, 0.14, 1, 3),
	// Point("inputs/1024-8.dat", "outputs/1024-8.dat", "00:00:15", 0.25, 0.16, 1, 5),
	// Point("inputs/1024-32.dat", "outputs/1024-32.dat", "00:00:20", 0.60, 0.60, 1, 10),
	Point("inputs/2048-8.dat", "outputs/2048-8.dat", "00:00:20", 0.75, 0.75, 1, 20),
	Point("inputs/2048-32.dat", "outputs/2048-32.dat", "00:00:20", 2.00, 2.00, 1, 40),
	Point("inputs/4096-8.dat", "outputs/4096-8.dat", "00:00:20", 2.40, 2.40, 1, 40),
}

const base = "/lustre/shared_data/p2g/34bd9abe21570f808843512bdb325e6a/"

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	var duration time.Duration
	in := base + pnt.In
	ans := base + pnt.Ans
	job, err := SlurmAlloc("-p", "C064M0256G", "-N1", "-n64")
	if err != nil {
		return nil, fmt.Errorf("slurm alloc failed: %w", err)
	}
	defer job.Cancel()
	err = os.Symlink(in, "input.dat")
	if err != nil {
		return nil, err
	}
	defer os.Remove("input.dat")
	out := "output.dat"
	defer os.Remove(out)
	cmd := job.Command(nil, "time", "-f", "%E %M", "-o", "time.out", "./answer", "input.dat")
	defer os.Remove("time.out")
	cmd.Env = append(os.Environ(), "OMP_NUM_THREADS=64")
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
	stdout, _, _, err = WaitCommandAndGetOutput(job.Command(nil, "./judge-cpp", out, ans))
	if err != nil {
		err = fmt.Errorf("%v; %v", string(stdout), err)
	}
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
		DetailedMessage: message + "\n" + string(stdout),
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
	stdout, stderr, stat, err := RunCommand("g++", "-O3", "-fopenmp", "--std=c++17", "-o", "answer", "answer.cpp")
	if stat != 0 || err != nil {
		defer j.Halt()
		ReportMessage(ResultToMap(&Result{
			Score:           0,
			Message:         "Compile Error",
			DetailedMessage: string(stderr) + string(stdout) + err.Error(),
		}, ResultFinal))
		return ErrCompileFailed
	}
	_, _, _, err = RunCommand("g++", "-O3", "-fopenmp", "--std=c++17", "-o", "judge-cpp", GetProblemPath("judge.cpp"))
	if err != nil {
		j.Halt()
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
