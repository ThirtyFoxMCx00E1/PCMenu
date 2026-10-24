#!/bin/sh
# Host-side simulation of the real main.cpp against a fake game (64-bit layout).  Usage: PCM_DIR=<.../files/PCMenu> ./run.sh
cd "$(dirname "$0")"
g++ -std=c++17 -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wno-unused-function -Wno-unused-variable -I stub -I ../jni test.cpp -o /tmp/pcm_test || exit 1
/tmp/pcm_test
