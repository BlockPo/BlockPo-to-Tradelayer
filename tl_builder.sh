#!/bin/bash

# clearing folder
make clean
make distclean
echo "building linux binary..."
./autogen.sh
cd depends
make HOST=x86_64-linux-gnu
cd ..
./configure --prefix=`pwd`/depends/x86_64-linux-gnu --without-gui
make -j 3
cd ..
echo "creating binary zip..."
mkdir binary_linux
cp BlockPo-to-Tradelayer/src/litecoind binary_linux
cp BlockPo-to-Tradelayer/src/litecoin-cli binary_linux
zip -o binary_linux.zip -r binary_linux

echo "building windows 64 binary..."
cd BlockPo-to-Tradelayer
make clean
make distclean
cd depends
make HOST=x86_64-w64-mingw32
cd ..
./autogen.sh
CONFIG_SITE=$PWD/depends/x86_64-w64-mingw32/share/config.site ./configure --prefix=/ --without-gui
make -j 3
cd ..
echo "creating binary zip..."
mkdir binary_win64
cp BlockPo-to-Tradelayer/src/litecoind.exe binary_win64
cp BlockPo-to-Tradelayer/src/litecoin-cli.exe binary_win64
zip -o binary_win64.zip -r binary_win64

# clearing folder
cd BlockPo-to-Tradelayer
make clean
make distclean

echo "building windows 32 binary..."
cd depends
make HOST=i686-w64-mingw32
cd ..
./autogen.sh
CONFIG_SITE=$PWD/depends/i686-w64-mingw32/share/config.site ./configure --prefix=/ --without-gui
make -j 3
cd ..
echo "creating binary zip..."
mkdir binary_win32
cp BlockPo-to-Tradelayer/src/litecoind.exe binary_win32
cp BlockPo-to-Tradelayer/src/litecoin-cli.exe binary_win32
zip -o binary_win32.zip -r binary_win32

echo "building macos binary..."
cd BlockPo-to-Tradelayer
make clean
make distclean
DIR="depends/SDKs"
if ! [ -d "$DIR" ];
then
mkdir $DIR
curl https://bitcoincore.org/depends-sources/sdks/MacOSX10.11.sdk.tar.gz -o depends/SDKs/MacOSX10.11.sdk.tar.gz
tar -C depends/SDKs -xf depends/SDKs/MacOSX10.11.sdk.tar.gz
fi

cd depends
make HOST=x86_64-apple-darwin11 -j3
cd ..
./autogen.sh
./configure --prefix=$PWD/depends/x86_64-apple-darwin11 --without-gui --disable-tests --disable-bench --disable-gui-tests
make -j 3
cd ..
echo "creating binary zip..."
mkdir binary_macos
cp BlockPo-to-Tradelayer/src/litecoind  binary_macos
cp BlockPo-to-Tradelayer/src/litecoin-cli binary_macos
zip -o binary_macos.zip -r binary_macos

# clearing folder
cd BlockPo-to-Tradelayer
make clean
make distclean
