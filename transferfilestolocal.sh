#!/bin/bash

echo "Copying .cpp files"

find . -name '*.cpp' | cpio -pdm /root/dev-margin-domake
hecho "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -pdm /root/dev-margin-domake
echo "Done"

echo "Copying .sh files"
find . -name '*.sh' | cpio -pdm /root/dev-margin-domake
echo "Done"

echo "Copying .am files"
find . -name '*.am' | cpio -pdm /root/dev-margin-domake
echo "Done"

echo "Copying .include files"
find . -name '*.include' | cpio -pdm /root/dev-margin-domake
echo "Done"

echo "Copying .ac files"
find . -name '*.ac' | cpio -pdm /root/dev-margin-domake

echo "Done"
