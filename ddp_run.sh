#!/bin/bash

for train_size in 2 8 16 32 128 224 240 248 254;
do
    make clean
    make augrey_test CFLAGS=-DN=$train_size
    echo "N = $train_size"
    for pts_per_line in 1 2 7 8;
    do
        ./augrey_test $pts_per_line 0
        ./augrey_test $pts_per_line 1
    done
done
