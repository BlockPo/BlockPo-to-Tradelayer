#!/bin/bash

echo "Doing Make"
./autogen.sh
./configure
make 
echo "Done"
