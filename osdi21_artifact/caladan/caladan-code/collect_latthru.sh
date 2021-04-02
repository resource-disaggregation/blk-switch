#!/bin/bash

config=$1

(echo "#size iodepth iops mibs latavg lattail"; for sz in 4 16 32 64 128; do for qd in 1 2 4 8 16 32 64 128; do paste <(echo $sz) <(echo $qd) <(cat ~/shenango-eval/$config-sz$sz-qd$qd.txt | grep "iops" | awk '{print $2, 0, $8, $12}') ;done; done) > ~/shenango-eval/$config-latthru.dat