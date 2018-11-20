#!/bin/bash

echo "Copying .cpp files"
find . -name '*.cpp' | cpio -pdm /mnt/d/omnicore-litecoin-0.16.3-local
echo "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -pdm /mnt/d/omnicore-litecoin-0.16.3-local
echo "Done"

echo "Copying .sh files"
find . -name '*.sh' | cpio -pdm /mnt/d/omnicore-litecoin-0.16.3-local
echo "Done"

echo "Copying .am files"
find . -name '*.am' | cpio -pdm /mnt/d/omnicore-litecoin-0.16.3-local
echo "Done"

echo "Copying .include files"
find . -name '*.include' | cpio -pdm /mnt/d/omnicore-litecoin-0.16.3-local
echo "Done"

echo "Copying .ac files"
find . -name '*.ac' | cpio -pdm /mnt/d/omnicore-litecoin-0.16.3-local
echo "Done"
