#!/bin/sh
CXXFLAGS=`wx-config --cppflags`
gcc $CXXFLAGS *.cpp -fPIC -c -DBUILD_ANTILEECH
gcc -shared *.o -o antiLeech.so
