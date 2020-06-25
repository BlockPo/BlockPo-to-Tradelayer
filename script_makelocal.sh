#!/bin/bash

echo "Doing Make Locally"
./autogen.sh
./configure --without-gui --with-incompatible-bdb
make --jobs=4
echo "Done"