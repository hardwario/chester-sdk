#/bin/bash

# Run with
# API_KEY="xxx" DEVICE_ID="xxx" ./codec/config.sh

set -x

curl -X 'POST' \
  'https://api.prod.hardwario.cloud/v2/messages' \
  -H 'accept: application/json' \
  -H "Authorization: ApiKey $API_KEY" \
  -H 'Content-Type: application/json' \
  -d '{
"body": [
    "app config analog-interval-sample 61",
    "app config analog-interval-aggreg 301",
],
  "device_id": "'"$DEVICE_ID"'",
  "type": "config"
}'
