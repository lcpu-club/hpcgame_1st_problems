package main

import (
	"encoding/json"
	"os"
	"strings"
)

type PointConfigure struct {
	PointConfigureBase
	Key    string
	Answer string
}

var solution map[string]string = make(map[string]string)

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
	Point(10, "t1", "65.396"),
	Point(5, "t2.1", "105.26"),
	Point(5, "t2.2", "107.14"),
	Point(10, "t3", "iv. GCC"),
	Point(3, "t4.1", "ii. 进程"),
	Point(3, "t4.2", "i. 线程"),
	Point(4, "t4.3", "i. 线程"),
	Point(4, "t5.1", "8"),
	Point(3, "t5.2", "i. 在 CPU 中设计独立的 AVX512 运算单元，但有可能导致调用 AVX512 指令集时因功耗过大而降频"),
	Point(3, "t5.3", "ii. 复用两个 AVX2 运算单元执行 AVX512 运算"),
	Point(10, "t6", "iv. OpenGL"),
	Point(10, "t7", "iii,iv"),
	Point(10, "t8", "iv. HBM"),
	Point(10, "t9", "ii. Slurm"),
	Point(10, "t10", "i. 缓存未命中"),
}

func judge(j *StandardJudger, point int, conf interface{}) (*Result, error) {
	pnt := conf.(*PointConfigure)
	userAns, ok := solution[pnt.Key]
	if !ok {
		userAns = ""
	}
	if TrimBlank(userAns) == pnt.Answer {
		return &Result{
			Score:           pnt.MaxScore,
			Message:         "Accepted",
			DetailedMessage: "answer is correct",
		}, nil
	}
	return &Result{
		Score:           0,
		Message:         "Wrong Answer",
		DetailedMessage: "answer of " + strings.TrimPrefix(pnt.Key, "t") + " is not correct",
	}, nil
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
