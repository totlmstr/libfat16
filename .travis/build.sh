#!/usr/bin/env bash

mkdir build
cd build/
${CMAKE} -DCMAKE_BUILD_TYPE=Release ..
make -j4
