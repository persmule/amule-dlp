#!/bin/sh
echo 'This script is deperated. Please use "make all" instead.'
CXXFLAGS=`wx-config --cppflags`
gcc $CXXFLAGS *.cpp -fPIC -c
gcc -shared *.o -o antiLeech.so
