package main

import (
	"fmt"
	"os/exec"
	"regexp"
	"strconv"
	"strings"
	"time"
)

type SlurmJob struct {
	jobID    string
	jobIDInt int
}

var regSlurmAllocJob = regexp.MustCompile("Granted job allocation ([0-9]+)")

func SlurmAlloc(options ...string) (*SlurmJob, error) {
	stdout, stderr, statusCode, err := RunCommand("salloc", append([]string{"--no-shell"}, options...)...)
	if statusCode != 0 {
		return nil, fmt.Errorf("salloc returned %v, saying %v %v", statusCode, string(stderr), string(stdout))
	}
	if err != nil {
		return nil, err
	}
	// Sample stderr:
	// salloc: Granted job allocation 1078
	matches := regSlurmAllocJob.FindSubmatch(stderr)
	if len(matches) != 2 {
		return nil, fmt.Errorf("salloc output unexpected: %v %v", string(stderr), string(stdout))
	}
	jobID := string(matches[1])
	jobIDInt, err := strconv.Atoi(jobID)
	if err != nil {
		return nil, err
	}
	return &SlurmJob{
		jobID:    jobID,
		jobIDInt: jobIDInt,
	}, nil
}

func NewSlurmJob(jobID int) *SlurmJob {
	return &SlurmJob{
		jobID:    strconv.Itoa(jobID),
		jobIDInt: jobID,
	}
}

func (sj *SlurmJob) Command(opts []string, cmd string, args ...string) *exec.Cmd {
	if opts == nil {
		opts = []string{}
	}
	return exec.Command(
		"srun", append(
			append([]string{"--jobid", sj.jobID}, opts...), append([]string{cmd}, args...)...,
		)...,
	)
}

func (sj *SlurmJob) ShellCommand(opts []string, cmdline string) *exec.Cmd {
	return sj.Command(opts, "bash", "-c", cmdline)
}

func (sj *SlurmJob) Cancel() error {
	stdout, stderr, statusCode, err := RunCommand("scancel", sj.jobID)
	if statusCode != 0 {
		return fmt.Errorf("scancel returned %v, saying %s %s", statusCode, stderr, stdout)
	}
	if err != nil {
		return err
	}
	return nil
}

func (sj *SlurmJob) GetJobID() int {
	return sj.jobIDInt
}

func ParseTime(c string) (time.Duration, error) {
	parts := strings.Split(c, ":")
	mul := 1.0
	total := 0.0
	for k := range parts {
		part := parts[len(parts)-k-1]
		f, err := strconv.ParseFloat(part, 64)
		if err != nil {
			return 0, err
		}
		total += mul * f
		mul *= 60.0
	}
	return time.Duration(total * float64(time.Second)), nil
}

func ParseSize(ss string) (int64, error) {
	mp := map[string]int64{
		"b":  1,
		"k":  1024,
		"kb": 1024,
		"m":  1024 * 1024,
		"mb": 1024 * 1024,
		"g":  1024 * 1024 * 1024,
		"gb": 1024 * 1024 * 1024,
		"t":  1024 * 1024 * 1024 * 1024,
		"tb": 1024 * 1024 * 1024 * 1024,
	}
	s := TrimBlank(strings.ToLower(ss))
	if s == "" {
		return 0, nil
	}
	for k, v := range mp {
		if strings.HasSuffix(s, k) {
			i, err := strconv.ParseInt(TrimBlank(strings.TrimSuffix(s, k)), 10, 64)
			if err != nil {
				return 0, err
			}
			return i * v, err
		}
	}
	if val, err := strconv.ParseInt(s, 10, 64); err == nil {
		return val, nil
	}
	return 0, fmt.Errorf("parse-size: %v is not a valid size", ss)
}

func (sj *SlurmJob) GetUsage() (*SlurmUsage, error) {
	stdout, stderr, statusCode, err := RunCommand(
		"sacct", "-j", sj.jobID, "--format",
		"JobID,TotalCPU,UserCPU,SystemCPU,MaxRSS,ElapsedRaw", "-P", "-n",
	)
	if statusCode != 0 {
		return nil, fmt.Errorf("sacct returns %v, saying %s %s", statusCode, stderr, stdout)
	}
	if err != nil {
		return nil, err
	}
	lines := strings.Split(TrimBlank(string(stdout)), "\n")
	su := &SlurmUsage{
		Elapsed:     0,
		MaxRSSBytes: 0,
	}
	for k, line := range lines {
		l := strings.Split(TrimBlank(line), "|")
		if len(l) != 6 {
			fmt.Println("sacct output may be invalid:", string(stdout), string(stderr))
			continue
			// return nil, fmt.Errorf("sacct output invalid: %s %s", stdout, stderr)
		}
		if k == 0 {
			var err error
			su.TotalCPU, err = ParseTime(l[1])
			if err != nil {
				return nil, err
			}
			su.UserCPU, err = ParseTime(l[2])
			if err != nil {
				return nil, err
			}
			su.SystemCPU, err = ParseTime(l[3])
			if err != nil {
				return nil, err
			}
		} else {
			mb, err := ParseSize(l[4])
			if err != nil {
				return nil, err
			}
			su.MaxRSSBytes += mb
			el, err := strconv.Atoi(l[5])
			if err != nil {
				return nil, err
			}
			su.Elapsed += time.Duration(el) * time.Second
		}
	}
	return su, nil
}

type SlurmUsage struct {
	TotalCPU    time.Duration
	UserCPU     time.Duration
	SystemCPU   time.Duration
	MaxRSSBytes int64
	Elapsed     time.Duration
}
