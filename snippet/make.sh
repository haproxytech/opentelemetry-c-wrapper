#!/bin/sh -ux
#
mkdir -p build && cd build
cmake ..
make
