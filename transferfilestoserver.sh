#!/bin/bash

echo "Copying .cpp files"
find . -name '*.cpp' | cpio -o -Bav -H crc | sshpass -p 'lihki' ssh lihki@200.86.176.38 'cd /home/lihki/omnicore-litecoin0.16.3; cpio -i -vumd'
echo "Done"

echo "Copying .h files"
find . -name '*.h' | cpio -o -Bav -H crc | sshpass -p 'lihki' ssh lihki@200.86.176.38 'cd /home/lihki/omnicore-litecoin0.16.3; cpio -i -vumd'
echo "Done"

echo "Copying .sh files"
find . -name '*.sh' | cpio -o -Bav -H crc | sshpass -p 'lihki' ssh lihki@200.86.176.38 'cd /home/lihki/omnicore-litecoin0.16.3; cpio -i -vumd'
echo "Done"

echo "Copying .am files"
find . -name '*.am' | cpio -o -Bav -H crc | sshpass -p 'lihki' ssh lihki@200.86.176.38 'cd /home/lihki/omnicore-litecoin0.16.3; cpio -i -vumd'
echo "Done"

echo "Copying .include files"
find . -name '*.include' | cpio -o -Bav -H crc | sshpass -p 'lihki' ssh lihki@200.86.176.38 'cd /home/lihki/omnicore-litecoin0.16.3; cpio -i -vumd'
echo "Done"

# echo "Copying .ac files"
# find . -name '*.ac' | cpio -o -Bav -H crc | sshpass -p 'lihki' ssh lihki@200.86.176.38 'cd /home/lihki/omnicore-litecoin0.16.3; cpio -i -vumd'
# echo "Done"


