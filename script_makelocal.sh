#!/bin/bash

echo "Doing Make Locally"
./autogen.sh
./configure
make --jobs=12
echo "Done"
