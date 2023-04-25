#!/bin/bash -x

PORT=8090
start_time=$(date +%s)
PROBLEM_response="INITIAL VALUE"
while (( $(date +%s) - start_time < 90 )); do
  PROBLEM_response=$(curl -s --data '{"jsonrpc": "2.0","method": "database_api.get_dynamic_global_properties","id": 1}' localhost:$PORT | grep -o '"head_block_number":1000000' | grep -o -E "[0-9]+")
  if [[ $PROBLEM_response = "1000000" ]]; then
    docker stop "$HIVED_IMAGE_NAME"
    echo "Success! Replayed: $PROBLEM_response blocks."
    break
  else
    echo "Waiting for replay. Retrying in 2 seconds..."
    sleep 2
  fi
done