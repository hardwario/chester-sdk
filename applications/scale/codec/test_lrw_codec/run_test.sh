#!/usr/bin/bash

# clean the build
rm build/ -fr

# build once
west build -b native_posix

mkdir -p data

./build/zephyr/zephyr.elf && west chester-lrw-decode --folder data --compare y

# clean up
rm data/*.b64
rm data/*.json
