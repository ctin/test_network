#!/bin/sh
git submodule update --init --recursive
cmake -Bbuild .
make -C build
