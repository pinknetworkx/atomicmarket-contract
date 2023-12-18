#!/bin/sh

# Compiles with a modern CDT and CMake. At the moment, doe not use this as we're not sure the build
# works

echo Build disabled
exit -1

# get the location of this script file, relative to the current shell working directory
BASEDIR=$(dirname "$0")
# get the absolute, path, docker does not allow mounting volumes by a relative path
hostpath="$( cd "$BASEDIR" && pwd )"
rm -rf $BASEDIR/build_new
mkdir -p $BASEDIR/build_new
docker run --rm --name cdt --volume $hostpath:/project \
  -w /project ghcr.io/wombat-tech/antelope.cdt:v4.0.0 /bin/bash -c \
  "mkdir -p build_new && cd build_new && cmake -DCMAKE_TOOLCHAIN_FILE=/usr/opt/cdt/4.0.0/lib/cmake/cdt/CDTWasmToolchain.cmake .. && make"
  # Need to set the toolchain here for cross compilation
