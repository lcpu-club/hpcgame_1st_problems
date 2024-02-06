package main

import (
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
)

type PointConfigure struct {
	PointConfigureBase
	Key    string
	Answer string
}

var solution map[string]string = make(map[string]string)
var username = ""

func Point(
	maxScore float64, key string, answer string,
) *PointConfigure {
	return &PointConfigure{
		PointConfigureBase: PointConfigureBase{
			MaxScore: maxScore,
		},
		Key:    key,
		Answer: answer,
	}
}

var points = []PointConfigureInterface{
	Point(40, "job_id", ""),
	Point(60, "password", "83637877eac153e236112f98bebae50b905c62685c519e5b2cb57ab8bfda31cb"),
}

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	userAns, ok := solution[pnt.Key]
	if !ok {
		userAns = ""
	}
	switch pnt.Key {
	case "job_id":
		{
			job_id, err := strconv.Atoi(userAns)
			if err != nil {
				return &Result{
					Score:           0,
					Message:         "Wrong Answer",
					DetailedMessage: "job_id is not a number",
				}, nil
			}
			out, _, status, _ := RunCommand("bash", GetProblemPath("check.sh"), strconv.Itoa(job_id))
			username = TrimBlank(string(out))
			if status != 0 || username == "" {
				return &Result{
					Score:           0,
					Message:         "Wrong Answer",
					DetailedMessage: "job was not run with 4 nodes, 4 tasks per node",
				}, nil
			}
			return &Result{
				Score:           40,
				Message:         "Accepted",
				DetailedMessage: "job was run with 4 nodes, 4 tasks per node by user `" + username + "`",
			}, nil
		}
	case "password":
		{
			h := sha256.New()
			h.Write([]byte(fmt.Sprintf("%s %s\n", pnt.Answer, username)))
			ans := hex.EncodeToString(h.Sum(nil))
			if TrimBlank(ans) != TrimBlank(userAns) {
				return &Result{
					Score:           0,
					Message:         "Wrong Answer",
					DetailedMessage: "password is not correct",
				}, nil
			}
			return &Result{
				Score:           60,
				Message:         "Accepted",
				DetailedMessage: "password is correct",
			}, nil
		}
	}
	return nil, nil
}

func before(j *StandardJudger) error {
	err := UnarchiveSolution(".")
	if err != nil {
		return err
	}
	f, err := os.ReadFile("answer.json")
	if err != nil {
		return err
	}
	err = json.Unmarshal(f, &solution)
	if err != nil {
		return err
	}
	return nil
}

func main() {
	sj := NewStandardJudger(points, judge)
	sj.Before(before)
	err := sj.Judge()
	if err != nil {
		sj.ReportError(err)
	}
}
