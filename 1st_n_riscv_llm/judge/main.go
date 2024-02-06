package main

func main() {
	ReportMessage(ResultToMap(&Result{
		Score:           0,
		Message:         "Manual Judgement",
		DetailedMessage: "This problem requires manual judgement. Please wait for the judge to check your submission.",
	}, ResultFinal))
}
