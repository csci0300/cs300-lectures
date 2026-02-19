#!/bin/bash
#perf stat -e LLC-misses,LLC-loads $@
perf stat -e cache-references,cache-misses $@
