#!/bin/sh

opener="evince"
system=`uname`
if [ "$system" = "Darwin" ]
then
  opener="open"
fi

gnuplot < $1.gp && latex $1.tex && dvips -o $1.ps $1.dvi && epstopdf $1.ps && $opener $1.pdf
