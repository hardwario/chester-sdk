#!/usr/bin/bash

# build once
west build -b native_posix

mkdir -p data

./build/zephyr/zephyr.elf && node decode.js --folder data --compare y

# clean up
rm data/*.b64
rm data/*.json
