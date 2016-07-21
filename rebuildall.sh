#!/bin/bash

mkdir -p bin

echo
echo "Build native"
echo

make -f Makefile.linux clean
make -f Makefile.linux
cp icetunnel ./bin/
make -f Makefile.linux clean

echo
echo "Build Win32 (mingw)"
echo

make -f Makefile.mingw32 clean
make -f Makefile.mingw32
cp icetunnel32.exe ./bin/
make -f Makefile.mingw32 clean

echo
echo "Build Win64 (mingw)"
echo

make -f Makefile.mingw64 clean
make -f Makefile.mingw64
cp icetunnel64.exe ./bin/
make -f Makefile.mingw64 clean

echo
echo "Done"
echo
