#!/bin/sh

PORT=${1:-8000}
./build.sh
./build/test_linux $PORT
