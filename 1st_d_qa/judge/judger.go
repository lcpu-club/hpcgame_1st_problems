package main

import "fmt"

type StandardJudger struct {
	pointConfigures []PointConfigureInterface
	judger          func(j *StandardJudger, point int, conf interface{}) (*Result, error)
	beforeFunc      func(j *StandardJudger) error
	afterFunc       func(j *StandardJudger) error
	Score           float64
	Message         string
	DetailedMessage string
	Subtasks        []interface{}
	Custom          map[string]interface{}
	halted          bool
}

const MsgAccepted = "Accepted"

type PointConfigureInterface interface {
	GetMaxScore() float64
}

type PointConfigureBase struct {
	MaxScore float64
}

func (p *PointConfigureBase) GetMaxScore() float64 {
	return p.MaxScore
}

func NewStandardJudger(
	pointConfigures []PointConfigureInterface, judger func(j *StandardJudger, point int, conf interface{}) (*Result, error),
) *StandardJudger {
	return &StandardJudger{
		pointConfigures: pointConfigures,
		judger:          judger,
		Score:           0,
		Message:         MsgAccepted,
		DetailedMessage: "",
		Subtasks:        []interface{}{},
		Custom:          make(map[string]interface{}),
		beforeFunc:      nil,
		afterFunc:       nil,
		halted:          false,
	}
}

func (j *StandardJudger) Before(fn func(j *StandardJudger) error) {
	j.beforeFunc = fn
}

func (j *StandardJudger) After(fn func(j *StandardJudger) error) {
	j.afterFunc = fn
}

func (j *StandardJudger) ReportError(err error) {
	ReportMessage(ResultToMap(&Result{
		Score:           0,
		Message:         "Internal Error",
		DetailedMessage: err.Error(),
	}, ResultFinal))
}

func (j *StandardJudger) ErrorToResult(err error, msg string) *Result {
	return &Result{
		Score:           0,
		Message:         msg,
		DetailedMessage: err.Error(),
	}
}

func (j *StandardJudger) Judge() error {
	if j.beforeFunc != nil {
		err := j.beforeFunc(j)
		if err != nil {
			return err
		}
	}
	for k, i := range j.pointConfigures {
		r, err := j.judger(j, k, i)
		if err != nil {
			return err
		}
		if r.MaxScore == 0 {
			r.MaxScore = i.GetMaxScore()
		}
		j.Score += r.Score
		if r.Message != MsgAccepted {
			j.Message = r.Message
		}
		j.DetailedMessage += fmt.Sprintf("Point %v:\n  msg: %v\n", k, r.DetailedMessage)
		j.Subtasks = append(j.Subtasks, ResultToMap(r, ResultSubtask))
		ReportMessage(ResultToMap(&Result{
			Score:   j.Score,
			Message: fmt.Sprintf("Finished judge point %v: %v", k, r.Message),
		}, ResultPartial))
		if j.halted {
			break
		}
	}
	if j.afterFunc != nil {
		err := j.beforeFunc(j)
		if err != nil {
			return err
		}
	}
	ReportMessage(ResultToMap(&Result{
		Score:           j.Score,
		Message:         j.Message,
		DetailedMessage: j.DetailedMessage,
		Subtasks:        j.Subtasks,
		Custom:          j.Custom,
	}, ResultFinal))
	return nil
}

func (j *StandardJudger) Halt() {
	j.halted = true
}
