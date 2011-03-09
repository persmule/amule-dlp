#!/bin/sh
echo 'This script is deperated. Please use "make all" instead.'
CXXFLAGS=`wx-config --cppflags`
gcc $CXXFLAGS *.cpp -fPIC -c -DBUILD_ANTILEECH
gcc -shared *.o -o antiLeech.so
