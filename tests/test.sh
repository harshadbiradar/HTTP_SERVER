#!/bin/bash

echo "Starting Tests..."

echo "Test 1: Single server instance with single Queue"

./test1.sh >>stress_test1

echo "Test 2: Single server instance with multiple Queues"

./test2.sh >>stress_test2