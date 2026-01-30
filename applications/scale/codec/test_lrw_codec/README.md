# Run script

`./run_test.sh`


# How LRW encode/decode tester works

All starts in `encode.c` where each ZTEST creates a `app_data` structure from the scale application's `src/app_data.h` file, encodes it to base64 file `data/*.b64` and at the same time it creates a JSON file `data/*.json` that is later used as a comparison for NodeJs decoder.

cJSON encodes `app_data` structures using `ctr_test_lrw.h` macros and custom `add_scale_json()` function for scale-specific data.

Then in the script the `node decode.js` is run. This script calls `require()` and includes the JavaScript decoder.
Script loads `*.b64`, decodes it and compare to the `*.json` file. Comparison is in JSON, not string comparison.


# Manual run

Decode all files in a folder and compare with expected JSON
`node decode.js --folder data --print y --compare y`

Decode a single base64 string
`node decode.js --b64str <base64_string> --print y`

Decode a single base64 file and compare with expected JSON file
`node decode.js --b64 data/out.b64 --json data/out.json --print y --compare y`

# cJSON and floats

cJSON encodes Numbers using `%g` literal in formatting function, so it can be ugly number like `3.49999999...`
To overcome this, you have to use "nice" decimal values that plays fine with float. So that is why I used
`0.25` or `0.125`, these values are nicely represented as a float.

# Scale-specific data

The scale application encodes:
- Battery data (voltage_rest, voltage_load, current_load)
- Accelerometer orientation
- Thermometer temperature
- Scale channel mask (bits 0-3 for A1, A2, B1, B2 active)
- Scale raw values (a1_raw, a2_raw, b1_raw, b2_raw as int32)

Invalid values are encoded as:
- INT32_MAX (0x7FFFFFFF) for raw scale values -> decoded as null
- 0xFFFF for voltage values -> decoded as null
- 0xFF for current_load and orientation -> decoded as null
- 0x7FFF for temperature -> decoded as null
