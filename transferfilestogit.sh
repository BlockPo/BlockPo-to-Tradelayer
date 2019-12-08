#!/bin/bash

echo "Copying .cpp files"

find . -name '*.cpp' | cpio -pdm /mnt/hgfs/SharedUbuntu/TradeLayer/dev-lihki-github
echo "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -pdm /mnt/hgfs/SharedUbuntu/TradeLayer/dev-lihki-github
echo "Done"

echo "Copying .sh files"
find . -name '*.sh' | cpio -pdm /mnt/hgfs/SharedUbuntu/TradeLayer/dev-lihki-github
echo "Done"

echo "Copying .am files"
find . -name '*.am' | cpio -pdm /mnt/hgfs/SharedUbuntu/TradeLayer/dev-lihki-github
echo "Done"

echo "Copying .include files"
find . -name '*.include' | cpio -pdm /mnt/hgfs/SharedUbuntu/TradeLayer/dev-lihki-github
echo "Done"

echo "Copying .ac files"
find . -name '*.ac' | cpio -pdm /mnt/hgfs/SharedUbuntu/TradeLayer/dev-lihki-github

echo "Done"
