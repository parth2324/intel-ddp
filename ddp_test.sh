#!/bin/bash

make clean
make augrey_test CFLAGS=-DN=240
echo "N = 240"
for pts_per_line in 1 2 3 6 7 8;
do
    ./augrey_test $pts_per_line 0
    ./augrey_test $pts_per_line 1
done
