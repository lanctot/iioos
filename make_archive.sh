#!/bin/sh

dir=/tmp/Lanctot

mkdir -p $dir
cp * $dir
cp -r figs plots $dir

cd $dir
cd ..
zip -r Lanctot-AAAI14-W4.zip Lanctot


