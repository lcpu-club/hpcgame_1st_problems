package main

import (
	"errors"
	"fmt"
	"math"
	"os"
	"os/exec"
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
	in, ans, timeStr string, maxScoreTime float64, minScore, maxScore float64,
) *PointConfigure {
	t, _ := ParseTime(timeStr)
	return &PointConfigure{
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
	Point("1G.bin", "1G.out", "00:00:03", 1.40, 1, 15),
	Point("4G.bin", "4G.out", "00:00:08", 2.75, 1, 35),
	Point("16G.bin", "16G.out", "00:00:20", 6.30, 1, 50),
}

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	var duration time.Duration
	inRaw := "/lustre/shared_data/overlap/81bc78e5750d774ca78bff560e799ad/" + pnt.In
	in := pnt.In
	_, _, _, err := RunCommand("cp", inRaw, in)
	if err != nil {
		return nil, err
	}
	defer os.Remove(in)
	ans := GetProblemPath(pnt.Ans)
	err = MaskRead(ans)
	if err != nil {
		return nil, err
	}
	job, err := SlurmAlloc("-p", "C064M0256G", "-N2", "--ntasks-per-node=4")
	if err != nil {
		return nil, err
	}
	defer job.Cancel()
	out := "out.data"
	defer os.Remove(out)
	hostnameStr, _, _, err := WaitCommandAndGetOutput(job.Command(nil, "hostname"))
	if err != nil {
		return nil, err
	}
	hostnameArr := strings.Split(TrimBlank(string(hostnameStr)), "\n")
	if len(hostnameArr) != 2 {
		return nil, errors.New("hostname error")
	}
	// prepareCmd := job.Command([]string{"-N2", "-n2", "-w", hostnameArr[0]}, "bash", GetProblemPath("prepare_memdisk.sh"), pnt.In)
	// cleanCmd := job.Command([]string{"-N2", "-n2", "-w", hostnameArr[0]}, "bash", GetProblemPath("clean_memdisk.sh"))
	// _, _, _, err = WaitCommandAndGetOutput(prepareCmd)
	// if err != nil {
	// 	return nil, err
	// }
	// defer WaitCommandAndGetOutput(cleanCmd)
	cmd := exec.Command("mpirun", "-np", "8", "time", "-f", "%E %M", "-o", "time.out", "./answer", in, out)
	cmd.Env = append(
		os.Environ(),
		fmt.Sprintf("SLURM_JOBID=%v", job.GetJobID()),
		fmt.Sprintf("SLURM_JOB_ID=%v", job.GetJobID()),
		fmt.Sprintf("SLURM_NODELIST=%v", strings.Join(hostnameArr, ",")),
		fmt.Sprintf("SLURM_TASKS_PER_NODE=%v", "4,4"),
	)
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
			DetailedMessage: strings.ReplaceAll(fmt.Sprintf("%v%v", string(stdout), string(stderr)), "81bc78e5750d774ca78bff560e799ad", "<hidden>"),
		}, nil
	}
	err = Unmask(ans)
	if err != nil {
		return nil, err
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
	err = func() error {
		ansF, err := os.ReadFile(ans)
		if err != nil {
			return err
		}
		outF, err := os.ReadFile(out)
		if err != nil {
			return err
		}
		if TrimBlank(string(ansF)) != TrimBlank(string(outF)) {
			return fmt.Errorf("wrong answer, expected %v..., got %v", TrimBlank(string(ansF))[:5], TrimBlank(string(outF)))
		}
		return nil
	}()
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
	stdout, stderr, stat, err := RunCommand("mpicxx", "-I/usr/include/openssl3", "-L/usr/lib64/openssl3", "-lcrypto", "-O3", "--std=c++20", "-o", "answer", "answer.cpp")
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
