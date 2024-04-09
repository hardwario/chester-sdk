#/bin/bash

# Run with
# API_KEY="xxx" DEVICE_ID="xxx" ./codec/control.sh

set -x

curl -X 'POST' \
  'https://api.prod.hardwario.cloud/v2/messages' \
  -H 'accept: application/json' \
  -H "Authorization: ApiKey $API_KEY" \
  -H 'Content-Type: application/json' \
  -d '{
"body": {
  "output_1_state": 0,
  "output_2_state": 0,
  "output_3_state": 0,
  "output_4_state": 0
},
  "device_id": "'"$DEVICE_ID"'",
  "type": "data"
}'
