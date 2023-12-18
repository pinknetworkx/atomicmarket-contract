#!/bin/bash

# Compiles with eosio CDT 1.7 which reproduces the current production build

BASEDIR=$(dirname "$0") # get the location of this script file, relative to the current shell working directory

hostpath="$( cd "$BASEDIR" && pwd )" #get the absolute, path, docker does not allow mounting volumes by a relative path
rm -rf $BASEDIR/build
mkdir -p $BASEDIR/build
docker run --rm --name eosio.cdt_v1.7.0 --volume $hostpath:/project -w /project eostudio/eosio.cdt:v1.7.0 /bin/bash -c \
"eosio-cpp -abigen -I include -R resource -contract atomicmarket -o build/atomicmarket.wasm src/atomicmarket.cpp"
