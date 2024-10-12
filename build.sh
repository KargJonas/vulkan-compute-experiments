#/bin/bash

cd build
cmake ..
make

cp ../src/*.spv .
