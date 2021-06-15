#!/bin/sh
# Using linux cross compilation

echo "Installing general dependencies ..."
sudo apt install build-essential libtool autotools-dev automake pkg-config bsdmainutils curl git

#The first step is to install the mingw-w64 cross-compilation tool chain.
ubuntuBionic='~18.04'
os=$(uname -v)

echo "Installing mingw-w64 ..."
if[[ "$os" == *"$ubuntuBionic"* ]]; then
  echo "for Ubuntu Bionic..."
  sudo update-alternatives --config i686-w64-mingw32-g++
else
  sudo apt install g++-mingw-w64-x86-64
fi

echo "Building Berkeley DB 4.8 ..."
./contrib/install_db4.sh `pwd`

echo "Building depencies ..."
./autogen.sh
cd depends
make HOST=x86_64-linux-gnu
cd ..
./configure --prefix=`pwd`/depends/x86_64-linux-gnu --without-gui
echo "Building tradelayer core ..."
make -j 3

echo "Creating ./litecoin folder ..."
mkdir $HOME/.litecoin
cd $HOME/.litecoin

echo "Creating litecoin.conf file ..."
touch litecoin.conf
echo "# litecoin.conf configuration file. Lines beginning with # are comments." > litecoin.conf
echo "# Run on the test network instead of the real litecoin network." >> litecoin.conf
echo "testnet=1" >> litecoin.conf
echo "# server=1 tells litecoin-QT to accept JSON-RPC commands" >> litecoin.conf
echo "server=1" >> litecoin.conf
echo "txindex=1" >> litecoin.conf
echo "# You must set rpcuser and rpcpassword to secure the JSON-RPC api" >> litecoin.conf
echo "rpcuser=user" >> litecoin.conf
echo "rpcpassword=whyDidTheBulletProofElephantCrossTheRoad" >> litecoin.conf
echo "# Listen for RPC connections on this TCP port:" >> litecoin.conf
echo "rpcport=9432" >> litecoin.conf

echo "Starting tradelayer server, this can take some time (synch) ..."
cd -; cd src ; ./litecoind -daemon
