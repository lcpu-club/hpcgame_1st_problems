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
	N            int
}

func Point(
	n int, in, ans, timeStr string, maxScoreTime float64, minScore, maxScore float64,
) *PointConfigure {
	t, _ := ParseTime(timeStr)
	return &PointConfigure{
		N:            n,
		In:           in,
		Ans:          ans,
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
	Point(2048, "256.dat", "256.dat.out", "00:00:03", 0.66, 1, 10),
	Point(1024, "512.dat", "512.dat.out", "00:00:09", 1.15, 1, 20),
	Point(1024, "1024.dat", "1024.dat.out", "00:01:00", 6.70, 1, 70),
}

const base = "/lustre/shared_data/conway/455f8753bd36923c7333f391b8c78076/"

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	var duration time.Duration
	inRaw := base + pnt.In
	in := pnt.In
	_, _, _, err := RunCommand("cp", inRaw, in)
	if err != nil {
		return nil, fmt.Errorf("copy input file failed: %w", err)
	}
	defer os.Remove(in)
	ans := base + pnt.Ans
	job, err := SlurmAlloc("-p", "GPU40G", "--gres=gpu:1", "")
	if err != nil {
		return nil, fmt.Errorf("alloc job failed: %w", err)
	}
	defer job.Cancel()
	out := "out.data"
	defer os.Remove(out)
	cmd := job.Command(nil, "time", "-f", "%E %M", "-o", "time.out", "./answer", in, out, strconv.Itoa(pnt.N))
	defer os.Remove("time.out")
	// cmd.Env = append(os.Environ(), "OMP_NUM_THREADS=8")
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
			DetailedMessage: strings.ReplaceAll(fmt.Sprintf("%v%v", string(stdout), string(stderr)), base, "<hidden>"),
		}, nil
	}
	timeOut, err := os.ReadFile("time.out")
	if err != nil {
		return nil, err
	}
	fmt.Println("time.out:", string(timeOut))
	timeOutS, _, _ := strings.Cut(TrimBlank(string(timeOut)), "\n")
	wallStr, memStr, ok := strings.Cut(TrimBlank(timeOutS), " ")
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
	err = cmp.CompareFile(out, ans)
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
	compareJob, err := SlurmAlloc("-p", "GPU", "--gres=gpu:1")
	if err != nil {
		return err
	}
	defer compareJob.Cancel()
	compileCmd := compareJob.Command(nil, "/usr/local/cuda/bin/nvcc", "-O3", "-o", "answer", "answer.cu")
	stdout, stderr, stat, err := WaitCommandAndGetOutput(compileCmd)
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
