#!/bin/bash

echo "Doing Make"
./autogen.sh
./configure
make --jobs=20
echo "Done"