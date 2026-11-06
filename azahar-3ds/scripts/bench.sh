#!/bin/sh
# bench.sh NAME [FRAMES]: Zelda title screen for FRAMES frames with rgds-play profiling; EXTRA_OPTS / env pass through.
ROM="/data/roms/3ds/Legend of Zelda, The - Ocarina of Time 3D (USA) (En,Fr,Es).3ds"
start=$(date +%s)
RGDS_PLAY_PROFILE=1 /data/rgds/azahar/run3ds.sh "$ROM" ${2:-1500} > /data/rgds/azahar/bench-$1.log 2>&1
end=$(date +%s)
echo "== $1: $((end-start)) s wall for ${2:-1500} frames"
grep "play: profile" /data/rgds/azahar/bench-$1.log | tail -3
