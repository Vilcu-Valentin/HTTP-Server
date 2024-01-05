#!/bin/bash

# Number of concurrent requests
REQUEST_COUNT=10

# URL to request
URL="http://localhost:8080/"

# Data to be sent in POST request
POST_DATA="data=example"

for i in $(seq 1 $REQUEST_COUNT)
do
    curl -X POST -d "$POST_DATA" $URL &  # Sending POST request in background
done

wait  # Wait for all background processes to finish
echo "All requests sent."

