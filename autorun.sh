#!/usr/bin/env bash
# If flag true, it will use linux cross compilation

echo "Installing general dependencies ..."
sudo apt install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev
sudo apt-get install libboost-all-dev bsdmainutils curl git python3

echo "Installing mingw-w64 ..."
if [[ $1 == "true" ]]
  #Installing the mingw-w64 cross-compilation tool chain.
  ubuntuBionic='~18.04'
  os=$(uname -v)
  then
    if [[ $os == *"$ubuntuBionic"* ]]
      then
        echo "for Ubuntu Bionic..."
        sudo update-alternatives --config i686-w64-mingw32-g++
      else
        echo "other distros..."
        sudo apt install g++-mingw-w64-x86-64
    fi
fi

echo "Building Berkeley DB 4.8 ..."
./contrib/install_db4.sh `pwd`

echo "Building depencies ..."
./autogen.sh

if [[ $1 == "true" ]]
  then
    cd depends
    make HOST=x86_64-linux-gnu
    cd ..
fi

export BDB_PREFIX=`pwd`/db4

if [[ $1 == "true" ]]
  then
    echo "Configure --prefix=`pwd`/depends/x86_64-linux-gnu ..."
    ./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include" --prefix=`pwd`/depends/x86_64-linux-gnu --without-gui
  else
    echo "Configure with just Berkeley conf ..."
    ./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include" --without-gui
fi

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
