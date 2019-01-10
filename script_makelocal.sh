#!/bin/bash

echo "Doing Make Locally"
./autogen.sh
./configure
make --jobs=18
echo "Done"
