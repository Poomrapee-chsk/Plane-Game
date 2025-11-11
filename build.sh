#!/bin/bash
mkdir -p build
cd build
cmake ..
make

# Run the game
./Simple3DGame
