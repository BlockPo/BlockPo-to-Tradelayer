#!/bin/bash

echo "Doing Make Locally"
./autogen.sh
./configure --without-gui --disable-tests
make --jobs=12
echo "Done"
