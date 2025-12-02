# Run script

`./run_test.sh`


# How LRW encode/decode tester works

All starts in `encode.c` where each ZTEST creates a `app_data` structure from the catalog application's `src/app_data.h` file, encodes it to base64 file `data/*.b64` and at the same time it creates a JSON file `data/*.json` that is later used as a comparison for NodeJs decoder.

cJSON encodes `app_data` structures using `ctr_test_lrw.h` macros.

Then in the script the `node decode.js` is run. This script calls `require()` and includes the JavaScript decoder.
Script loads `*.b64`, decodes it and compare to the `*.json` file. Comparison is in JSON, not string comparison.


# Manual run

Decode all files in a folder and compare with expected JSON
`node decode.js --folder data --print y --compare y`

Decode a single base64 string
`node decode.js --b64str d///////ApgJAAAAAAEAAAAAAAAAAAAAAgAAAwAA~ --print y`

Decode a single base64 file and compare with expected JSON file
`node decode.js --b64 data/out.b64 --json data/out.json --print y --compare y`

# cJSON and floats

cJSON encodes Numbers using `%g` literal in formatting function, so it can be ugly number like `3.49999999...`
To overcome this, you have to use "nice" decimal values that plays fine with float. So that is why I used
`0.25` or `0.125`, these values are nicely represented as a float.
