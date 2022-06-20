#!/bin/sh

gcc template.c
python3 translater.py $1 output.txt
./a.out output.txt > result.txt
python3 display.py result.txt

rm output.txt
rm result.txt
rm a.out