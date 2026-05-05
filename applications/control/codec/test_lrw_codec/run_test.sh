#!/usr/bin/bash

# build once
west build -b native_sim

mkdir -p data

./build/zephyr/zephyr.elf && west chester-lrw-decode --folder data --compare y

# clean up
rm data/*.b64
rm data/*.json
