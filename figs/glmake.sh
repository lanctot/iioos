#!/bin/sh
gnuplot < $1.gp && latex $1.tex && dvips -o $1.ps $1.dvi && epstopdf $1.ps
