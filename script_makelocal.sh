#!/bin/bash

echo "Doing Make"
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure --prefix=/
make --jobs=24
make install DESTDIR=/mnt/d/Datadir/litecoin
echo "Done"
