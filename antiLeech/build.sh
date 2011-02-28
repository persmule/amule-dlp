#!/bin/sh
CXXFLAGS=`wx-config --cppflags`
gcc $CXXFLAGS *.cpp -fPIC -c
gcc -shared *.o -o antiLeech.so
