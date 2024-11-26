#!/bin/bash

make clean
make augrey_test CFLAGS=-DN=240
echo "N = 240"
for pts_per_line in 1 2 4 7 8;
do
    sudo wrmsr 0x48 0x401
    ./augrey_test $pts_per_line 0
    sudo wrmsr 0x48 0x505
    ./augrey_test $pts_per_line 1
done
