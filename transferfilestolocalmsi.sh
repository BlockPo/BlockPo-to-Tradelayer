#!/bin/bash

echo "Copying .cpp files"

find . -name '*.cpp' | cpio -pdm /media/sf_Ubuntu/TradeLayer/C++Codes/dev-lihki-domake
hecho "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -pdm /media/sf_Ubuntu/TradeLayer/C++Codes/dev-lihki-domake
echo "Done"

echo "Copying .sh files"
find . -name '*.sh' | cpio -pdm /media/sf_Ubuntu/TradeLayer/C++Codes/dev-lihki-domake
echo "Done"

echo "Copying .am files"
find . -name '*.am' | cpio -pdm /media/sf_Ubuntu/TradeLayer/C++Codes/dev-lihki-domake
echo "Done"

echo "Copying .include files"
find . -name '*.include' | cpio -pdm /media/sf_Ubuntu/TradeLayer/C++Codes/dev-lihki-domake
echo "Done"

echo "Copying .ac files"
find . -name '*.ac' | cpio -pdm /media/sf_Ubuntu/TradeLayer/C++Codes/dev-lihki-domake

echo "Done"
