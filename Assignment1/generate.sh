#!/bin/bash

for i in {1..6}
do
    python3 generator.py -f test${i}.txt -c 10000 -m 100000
done
