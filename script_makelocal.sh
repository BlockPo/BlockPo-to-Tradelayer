#!/bin/bash

echo "Doing Make Locally"
# cd depends
# make HOST=i686-w64-mingw32
# cd ..
./autogen.sh
./configure CFLAGS=-fPIC CXXFLAGS=-fPIC
make --jobs=24
make install DESTDIR=/mnt/d/Datadir/litecoin
echo "Done"
