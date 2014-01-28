#!/bin/sh

matches="30 50 100 200 300 400 500 750 1000"
f1="aggfile.mcts.txt"
f2="aggfile.oos.txt"

echo "" > $f1
echo "" > $f2

for m in $matches 
do
  ./sim scratch/iss.initial.dat scratch/iss.initial.dat 1 1 agg $m > agg-ismcts.${m}.txt
  ./sim scratch/iss.initial.dat scratch/iss.initial.dat 2 2 agg $m > agg-oos.${m}.txt

  t1=`cat agg-ismcts.${m}.txt | grep "Exploitabilities"`
  t2=`cat agg-oos.${m}.txt | grep "Exploitabilities"`

  echo "ismcts $m $t1" >> $f1
  echo "oos $m $t2" >> $f2
done

