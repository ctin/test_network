#!/bin/sh

PORT=${1:-8000}
./build.sh
parentdir=$(basename $(cd `dirname $0` && pwd))
./build/${parentdir}_linux $PORT
