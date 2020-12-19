#!/bin/bash

makeAll() {
  if [ ! -d "build" ]; then 
    mkdir build
  fi
  cd build
  mkdir logs
  mkdir weights
  cmake ..
  make
  cd ..
  cp config/* build/
  cp -r srcs/others/mnist_layers build
  cp -r srcs/others/cifar_layers build
}

if [ $# == 0 ]; then
  makeAll
fi

if [[ $# == 1 && $1 = "clean" ]]; then
  rm -r build
fi

if [[ $# == 1 && $1 = "all" ]]; then  
  rm -r build
  makeAll
fi
