#!/bin/bash

echo "Doing Make Locally"
./autogen.sh
./configure --without-gui
make --jobs=6
echo "Done"