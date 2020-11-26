#!/bin/bash

makeAll() {
  if [ ! -d "build" ]; then 
    mkdir build
  fi
  cd build
  mkdir logs
  cmake ..
  make
  cd ..
  cp config/* build/
}

if [ $# == 0 ]; then
  makeAll
fi

if [[ $# == 1 && $1 = "clean" ]]; then
  rm -r build
fi
