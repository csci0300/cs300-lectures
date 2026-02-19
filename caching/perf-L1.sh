#!/bin/bash
perf stat -e L1-dcache-loads,L1-dcache-misses $@
