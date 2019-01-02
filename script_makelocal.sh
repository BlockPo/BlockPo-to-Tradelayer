#!/bin/bash

echo "Doing Make Locally"
./autogen.sh
./configure
make --jobs=24
echo "Done"
