Starting on Tradelayer
======================

(We are following instructions in order to work on Linux environment. If your system is macOS or Windows, the best strategy is use a Virtual Machine with Linux)

Installing Dependencies
---------------------
These dependencies are required:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 libboost    | Utility          | Library for threading, data structures, etc
 libevent    | Networking       | OS independent asynchronous networking

Optional dependencies:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 miniupnpc   | UPnP Support     | Firewall-jumping support
 libdb4.8    | Berkeley DB      | Wallet storage (only needed when wallet enabled)
 qt          | GUI              | GUI toolkit (only needed when GUI enabled)
 libqrencode | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)
 univalue    | Utility          | JSON parsing and encoding (bundled version will be used unless --with-system-univalue passed to configure)
 libzmq3     | ZMQ notification | Optional, allows generating ZMQ notifications (requires ZMQ version >= 4.0.0)

Build requirements:
```bash
sudo apt-get install build-essential libtool autotools-dev automake pkg-config bsdmainutils python3
```
Install the required dependencies:

```bash
sudo apt-get install libevent-dev libboost-system-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev
```
BerkeleyDB is required for the wallet.

Ubuntu and Debian have their own `libdb-dev` and `libdb++-dev` packages, but these will install
BerkeleyDB 5.1 or later. This will break binary wallet compatibility with the distributed executables, which
are based on BerkeleyDB 4.8. If you do not care about wallet compatibility,
pass `--with-incompatible-bdb` to configure.

Optional (see `--with-miniupnpc` and `--enable-upnp-default`):
```bash
sudo apt-get install libminiupnpc-dev
```
ZMQ dependencies (provides ZMQ API):
```bash
sudo apt-get install libzmq3-dev
```
libqrencode (optional) can be installed with:

```bash
sudo apt-get install libqrencode-dev
```


To Build
---------------------
Download the branch lihki-settlement:

```bash
git clone --branch lihki-settlement https://github.com/BlockPo/BlockPo-to-Tradelayer
```

Inside the working directory (BlockPo-to-Tradelayer) :

```bash
./autogen.sh
./configure
make
make install # optional
```

Configuration
=============
TradeLayer can be configured by providing one or more optional command-line arguments:
```bash
$ litecoind -setting=value -setting=value
```

All settings can alternatively also be configured via the `litecoin.conf`.

Depending on the operating system, the default locations for the configuration file are (NOTE: you should create the folder ./litecoin in your HOME folder):

- Unix systems: `$HOME/.litecoin/bitcoin.conf`

.litecoin folder contains important things, for example:

tradelayer.log : this is where is locate de output for PrintToLog functions. Very important thing when we are debugging.

debug.log : this log is for litecoin proccess: status in chain, pairs info, and more.

While most normal litecoin core config options can be utilized there are a few critical pieces integrators will need to enable, including txindex and server options.

A typical `litecoin.conf` may include:
```
server=1  #we are receiving rpc calls
rpcuser=user
rpcpassword=password
rpcallowip=127.0.0.1
rpcport=9432
txindex=1  #saving all blockchain
regtest=1 #regtest mode: working with an small blockchain in our local machine
addresstype=legacy
```

Starting in regtest mode
========================
In order to interact with the node, we can go to BlockPo-to-Tradelayer/src folder, and then run:

```bash
./litecoind -server -daemon
```

We should see the starting message, now we have our server (litecoind) running and waiting for calls, we use litecoin-cli to send rpc calls.

If we can interact with server , we can retrieve some data using this commands:

```bash
./litecoin-cli tl_getinfo
```
or if we need more details about blockchain:

```bash
./litecoin-cli getblockchaininfo
```

Note: it's a litecoin server, so if you need all list of rpc, you can type:

```bash
./litecoin-cli help
```

To stop the server we just type:

```bash
./litecoin-cli stop
```


Starting functional test
========================
Bitcoin and Litecoin have an python framework in order to test all rpcs calls that we made to the server (and more). Tradelayer has their own functional test in folder  /test/functional, their start with 'tl_' , for example: tl_basics.py . You can run it without start anything previously (the scripts do all the job):


```bash
./tl_basics.py  --nocleanup  
```
No cleanup flag is used to not delete the log folder ( the ./litecoin folder for this instance, which the struct : /tmp/_somerandom_name_here) where you can find all information of the proccess.

In order to see all outputs for settlement algorithm you should use:

```bash
./tl_settlement_oracles.py  --nocleanup  
```

You should see all *.txt outputs (that are created by functions located in tradelayer.cpp) in folder /test/functions. And the other file logs in the /tmp/_somerandom_name_here that the instance create for that specific test.
(Each time you run a script, you build a new one folder ./litecoin en /tmp folder).

Txt outputs names:

```bash
-graphInfoSixth.txt #all graph
-globalPNLALL_DUSD.txt #the final profit and loss for the entire system, it should be zero most of the time
-globalVolumeALL_DUSD.txt #all volume traded
-SettlementRows.txt #rows of M_file
```
Important log files in /tmp/_somerandom_name_here  ( specific ./litecoin folder for each test innstance) :

```bash
-tradelayer.log #any PrintToLog function you can add is gonna print here
-debug.log #litecoin level logs
```bash
