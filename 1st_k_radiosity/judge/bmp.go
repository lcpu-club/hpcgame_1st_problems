package main

import (
	"fmt"
	"io"
	"math"
	"os"

	"golang.org/x/image/bmp"
)

type BmpComparer struct {
	threshold float64
}

func NewBmpComparer(threshold float64) *BmpComparer {
	return &BmpComparer{
		threshold: threshold,
	}
}

// Compare returns nil when same
func (c *BmpComparer) Compare(ans io.Reader, out io.Reader) error {
	ansImg, err := bmp.Decode(ans)
	if err != nil {
		return err
	}
	outImg, err := bmp.Decode(out)
	if err != nil {
		return err
	}
	ansBounds := ansImg.Bounds()
	outBounds := outImg.Bounds()
	if ansBounds != outBounds {
		return fmt.Errorf("bounds not equal: (%v, %v) != (%v, %v)", ansBounds.Min, ansBounds.Max, outBounds.Min, outBounds.Max)
	}
	var sumOfDeltaPower float64 = 0.0
	for i := ansBounds.Min.X; i < ansBounds.Max.X; i++ {
		for j := ansBounds.Min.Y; j < ansBounds.Max.Y; j++ {
			ansR, ansG, ansB, _ := ansImg.At(i, j).RGBA()
			r1, g1, b1 := float64(ansR>>8), float64(ansG>>8), float64(ansB>>8)
			outR, outG, outB, _ := outImg.At(i, j).RGBA()
			r2, g2, b2 := float64(outR>>8), float64(outG>>8), float64(outB>>8)
			deltaR := math.Abs(r1 - r2)
			deltaG := math.Abs(g1 - g2)
			deltaB := math.Abs(b1 - b2)
			sumOfDeltaPower += deltaR + deltaG + deltaB
		}
	}
	if sumOfDeltaPower >= c.threshold {
		return fmt.Errorf("delta too large: %v", sumOfDeltaPower)
	}
	return nil
}

func (c *BmpComparer) CompareFile(ans string, out string) error {
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
