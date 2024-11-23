#!/bin/bash

for train_size in 1 2 4 8 16 32 64 128 206 224 240 248 252 254;
do
    make clean
    make augrey_test CFLAGS=-DN=$train_size
    echo "N = $train_size"
    for pts_per_line in 1 2 3 6 7 8;
    do
        ./augrey_test $pts_per_line
    done
done
