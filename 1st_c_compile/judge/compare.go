package main

import (
	"bufio"
	"bytes"
	"fmt"
	"io"
	"os"
	"strings"
)

type Comparer interface {
	Compare(ans io.Reader, out io.Reader) error
	CompareFile(ans string, out string) error
}

type LineComparer struct {
	comp func(la string, lb string) error
}

func NewLineComparer(comp func(la string, lb string) error) *LineComparer {
	return &LineComparer{
		comp: comp,
	}
}

// Compare returns nil when same
func (c *LineComparer) Compare(ans io.Reader, out io.Reader) error {
	ansBuf, outBuf := bufio.NewReader(ans), bufio.NewReader(out)
	line := 0
	for {
		line++
		ansLine, err := ansBuf.ReadString('\n')
		if err == io.EOF {
			for {
				s, err := outBuf.ReadString('\n')
				if err == io.EOF {
					break
				}
				if TrimBlank(s) != "" {
					return fmt.Errorf("output has more lines than answer")
				}
			}
			break
		}
		if err != nil {
			return err
		}
		ansLine = TrimBlank(ansLine)
		if ansLine == "" {
			continue
		}
		outLine, err := outBuf.ReadString('\n')
		if err == io.EOF {
			return fmt.Errorf("output has fewer lines than answer")
		}
		if err != nil {
			return err
		}
		outLine = TrimBlank(outLine)
		err = c.comp(ansLine, outLine)
		if err != nil {
			return fmt.Errorf("difference on line %v: %v", line, outLine)
		}
	}
	return nil
}

func (c *LineComparer) CompareFile(ans string, out string) error {
	ansF, err := os.Open(ans)
	if err != nil {
		return err
	}
	defer ansF.Close()
	outF, err := os.Open(out)
	if err != nil {
		return err
	}
	defer outF.Close()
	return c.Compare(ansF, outF)
}

type BinaryComparer struct {
}

func NewBinaryComparer() *BinaryComparer {
	return &BinaryComparer{}
}

const chunkSize = 16

func (c *BinaryComparer) Compare(ans io.Reader, out io.Reader) error {
	ansB, outB := make([]byte, chunkSize), make([]byte, chunkSize)
	flag := false
	cnt := 0
	for {
		cnt++
		_, err := io.ReadFull(ans, ansB)
		if err != nil {
			if err == io.EOF || err == io.ErrUnexpectedEOF {
				flag = true
			} else {
				return err
			}
		}
		_, err = io.ReadFull(out, outB)
		if err != nil {
			if err == io.EOF || err == io.ErrUnexpectedEOF {
				flag = true
			} else {
				return err
			}
		}
		if !bytes.Equal(ansB, outB) {
			return fmt.Errorf("binary data not equal on byte %v to %v", cnt*chunkSize, cnt*chunkSize+chunkSize)
		}
		if flag {
			break
		}
	}
	return nil
}

func (c *BinaryComparer) CompareFile(ans string, out string) error {
	ansF, err := os.Open(ans)
	if err != nil {
		return err
	}
	defer ansF.Close()
	outF, err := os.Open(out)
	if err != nil {
		return err
	}
	defer outF.Close()
	return c.Compare(ansF, outF)
}

type TokenComparer struct {
	comp func(la string, lb string) error
	lc   *LineComparer
}

func NewTokenComparer(comp func(la string, lb string) error) *TokenComparer {
	c := &TokenComparer{
		comp: comp,
	}
	c.lc = NewLineComparer(c.compInternal)
	return c
}

func SplitWithoutEmpty(str string, cut string) []string {
	sp := strings.Split(str, cut)
	rslt := []string{}
	for _, s := range sp {
		if s != "" {
			rslt = append(rslt, s)
		}
	}
	return rslt
}

func (c *TokenComparer) compInternal(la string, lb string) error {
	a := SplitWithoutEmpty(strings.ReplaceAll(la, "\t", " "), " ")
	b := SplitWithoutEmpty(strings.ReplaceAll(lb, "\t", " "), " ")
	if len(a) != len(b) {
		return fmt.Errorf("\"%v\" and \"%v\" has different number of tokens", a, b)
	}
	for i := range a {
		err := c.comp(a[i], b[i])
		if err != nil {
			return err
		}
	}
	return nil
}

func (c *TokenComparer) Compare(ans io.Reader, out io.Reader) error {
	return c.lc.Compare(ans, out)
}

func (c *TokenComparer) CompareFile(ans string, out string) error {
	return c.lc.CompareFile(ans, out)
}
