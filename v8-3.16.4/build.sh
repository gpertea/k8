#!/bin/sh
export CXXFLAGS='-msse2 -fno-delete-null-pointer-checks'
make -j4 x64.release
