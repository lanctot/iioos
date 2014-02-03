#!/bin/sh

files=`ls --color=never *.gp`

for file in $files
do
  f=`echo $file | awk -F'.' '{print $1}'`
  echo "building $f"
  #gnuplot < $f.gp
  #gnuplot < $f.gp && latex $f.tex && dvips -o $f.ps $f.dvi && epstopdf $f.ps >/dev/null 
  gnuplot < $f.gp && epstopdf $f.eps 
done



