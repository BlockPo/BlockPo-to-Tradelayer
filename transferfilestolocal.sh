#!/bin/bash

echo "Copying .cpp files"

find . -name '*.cpp' | cpio -pdm /media/sf_VirtualBox/tradelayer/dev-lihki-local
hecho "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -pdm /media/sf_VirtualBox/tradelayer/dev-lihki-local
echo "Done"

echo "Copying .sh files"
find . -name '*.sh' | cpio -pdm /media/sf_VirtualBox/tradelayer/dev-lihki-local
echo "Done"

echo "Copying .am files"
find . -name '*.am' | cpio -pdm /media/sf_VirtualBox/tradelayer/dev-lihki-local
echo "Done"

echo "Copying .include files"
find . -name '*.include' | cpio -pdm /media/sf_VirtualBox/tradelayer/dev-lihki-local
echo "Done"

echo "Copying .ac files"
find . -name '*.ac' | cpio -pdm /media/sf_VirtualBox/tradelayer/dev-lihki-local

echo "Done"
