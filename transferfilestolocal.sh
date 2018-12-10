#!/bin/bash

echo "Copying .cpp files"
find . -name '*.cpp' | cpio -pdm /home/lihkir/Documents/omnicore-litecoin-demo-local
echo "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -pdm /home/lihkir/Documents/omnicore-litecoin-demo-local
echo "Done"

echo "Copying .sh files"
find . -name '*.sh' | cpio -pdm /home/lihkir/Documents/omnicore-litecoin-demo-local
echo "Done"

echo "Copying .am files"
find . -name '*.am' | cpio -pdm /home/lihkir/Documents/omnicore-litecoin-demo-local
echo "Done"

echo "Copying .include files"
find . -name '*.include' | cpio -pdm /home/lihkir/Documents/omnicore-litecoin-demo-local
echo "Done"

echo "Copying .ac files"
find . -name '*.ac' | cpio -pdm /home/lihkir/Documents/omnicore-litecoin-demo-local
echo "Done"
