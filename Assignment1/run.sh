#!/bin/bash

# Run generate.sh
bash generate.sh

# Run make
make

# Run the final command
./a.out $1 $2 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
