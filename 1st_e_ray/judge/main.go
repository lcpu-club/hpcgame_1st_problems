package main

import (
	"errors"
	"fmt"
	"log"
	"math"
	"math/rand"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/google/uuid"
)

type PointConfigure struct {
	PointConfigureBase
	Time         time.Duration
	TimeStr      string
	MaxScoreTime time.Duration
	MinScore     float64
}

func Point(
	timeStr string, maxScoreTime float64, minScore, maxScore float64,
) *PointConfigure {
	t, _ := ParseTime(timeStr)
	return &PointConfigure{
		PointConfigureBase: PointConfigureBase{
			MaxScore: maxScore,
		},
		Time:         t,
		TimeStr:      timeStr,
		MaxScoreTime: time.Duration(maxScoreTime * float64(time.Second)),
		MinScore:     minScore,
	}
}

var points = []PointConfigureInterface{
	Point("00:07:00", 240, 1, 100),
}

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	var duration time.Duration
	job, err := SlurmAlloc("-p", "JUDGE", "-N4", "--ntasks-per-node=64", "-n256")
	if err != nil {
		return nil, err
	}
	defer job.Cancel()
	// **** Fetch hostname ****
	hostnameStr, _, _, err := WaitCommandAndGetOutput(job.Command(nil, "hostname"))
	if err != nil {
		log.Println("hostname error")
		return nil, err
	}
	hostnameArr := strings.Split(TrimBlank(string(hostnameStr)), "\n")
	if len(hostnameArr) != 4 {
		return nil, errors.New("hostname error")
	}
	masterHost := hostnameArr[0]
	slaveHosts := hostnameArr[1:]
	// ---- End ----
	// **** Init Ray Cluster ****
	ipAddrRaw, _, _, err := WaitCommandAndGetOutput(job.ShellCommand([]string{"-w", masterHost}, "hostname -I | awk '{ print $2 }'"))
	if err != nil {
		return nil, errors.New("get ip error")
	}

	rand.Seed(time.Now().UnixNano())
	portStart := rand.Intn(3500) + 36500
	portNum := strconv.Itoa(portStart)
	rangePortStart := strconv.Itoa(portStart + 1)
	rangePortEnd := strconv.Itoa(portStart + 1000)
	masterIPAddr := TrimBlank(string(ipAddrRaw))
	ReportMessage(ResultToMap(
		&Result{
			Score: 0,
			// Message:         "Running",
			Message: fmt.Sprintf("master: %v:%v", masterIPAddr, portNum),
		}, ResultPartial,
	))
	rayTmp := filepath.Join("/tmp/h_rayT_" + portNum + "_" + uuid.New().String()[:6])
	// wd, _ := os.Getwd()
	// rayTmp := filepath.Join(wd, "ray_tmp")
	masterCmd := job.Command([]string{"-w", masterHost, "-n4", "--exact"}, "bash", "-c", "if [ $SLURM_PROCID -eq 0 ]; then ray start --node-ip-address "+masterIPAddr+" --head --port "+portNum+" --num-cpus 4 --include-dashboard=false --temp-dir "+rayTmp+" --block --min-worker-port "+rangePortStart+" --max-worker-port "+rangePortEnd+"; fi")
	masterCmd.Stdout = os.Stdout
	masterCmd.Stderr = os.Stderr
	err = masterCmd.Start()
	if err != nil {
		ReportMessage(ResultToMap(
			&Result{
				Score: 0,
				// Message:         "Running",
				Message: fmt.Sprintf("master ray error: %v", err.Error()),
			}, ResultPartial,
		))
		return nil, err
	}
	ReportMessage(ResultToMap(
		&Result{
			Score: 0,
			// Message:         "Running",
			Message: fmt.Sprintf("master ray started: 256.256.256.256:65536%v", ""),
		}, ResultPartial,
	))
	time.Sleep(5 * time.Second)
	slaveCmd := job.Command([]string{"-w", strings.Join(slaveHosts, ","), "--cores-per-socket=4", "--sockets-per-node=1"}, "ray", "start", fmt.Sprintf("--address=%v:%v", masterIPAddr, portNum), "--num-cpus", "4", "--block")
	slaveCmd.Stderr = os.Stderr
	slaveCmd.Stdout = os.Stdout
	err = slaveCmd.Start()
	if err != nil {
		ReportMessage(ResultToMap(
			&Result{
				Score: 0,
				// Message:         "Running",
				Message: fmt.Sprintf("slave ray error: %v", err.Error()),
			}, ResultPartial,
		))
		return nil, err
	}
	time.Sleep(5 * time.Second)
	ReportMessage(ResultToMap(
		&Result{
			Score: 0,
			// Message:         "Running",
			Message: fmt.Sprintf("slave ray started: %v:%v", masterIPAddr, portNum),
		}, ResultPartial,
	))
	defer masterCmd.Process.Kill()
	defer slaveCmd.Process.Kill()

	err = os.Symlink("/lustre/shared_data/ray/820ede54516fbcb6319ca8dc048d1cfe/inputs", "./inputs")
	if err != nil {
		return nil, err
	}
	defer os.Remove("./inputs")
	err = os.Symlink("/lustre/shared_data/ray/820ede54516fbcb6319ca8dc048d1cfe/weights", "./weights")
	if err != nil {
		return nil, err
	}
	defer os.Remove("./weights")
	ReportMessage(ResultToMap(
		&Result{
			Score: 0,
			// Message:         "Running",
			Message: "problem environment prepared",
		}, ResultPartial,
	))
	// ---- End ----
	cmd := job.Command([]string{"-w", masterHost, "-n1", "--cores-per-socket=4", "--sockets-per-node=1", "--exact"}, "time", "-f", "%E %M", "-o", "time.out", "python3", "main.py")
	defer os.Remove("time.out")
	cmd.Env = append(os.Environ(), "RAY_CLUSTER_ADDR="+masterIPAddr+":"+portNum)
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
	ReportMessage(ResultToMap(
		&Result{
			Score: 0,
			// Message:         "Running",
			Message: "comparing result",
		}, ResultPartial,
	))
	cmpJob, err := SlurmAlloc("-N1", "-n1")
	if err != nil {
		return nil, err
	}
	defer cmpJob.Cancel()
	cmpCmd := cmpJob.Command(nil, "python3", GetProblemPath("compare.py"))
	cmpOut, _, _, err := WaitCommandAndGetOutput(cmpCmd)
	if TrimBlank(string(cmpOut)) != "all equal" {
		if err == nil {
			err = errors.New(TrimBlank(string(cmpOut)))
		} else {
			err = fmt.Errorf("%v\n%v", err, TrimBlank(string(cmpOut)))
		}
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
		DetailedMessage: message,
		Custom:          custom,
	}, nil
}

var ErrCompileFailed = errors.New("compile error")

func before(j *StandardJudger) error {
	err := UnarchiveSolution(".")
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
