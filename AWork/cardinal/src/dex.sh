#!/bin/bash

SRCDIR=/home/lihkir/Documents/TradeLayer/tradelayer-litecoin-lihki-local/src
DATADIR=/home/lihkir/.litecoin
NUL=/dev/null
printf "\n//////////////////////////////////////////\n"
printf "Cleaning the regtest folder\n"

rm -r /home/lihkir/.litecoin/regtest

printf "\n________________________________________\n"
printf "Preparing a test environment...\n"
$SRCDIR/litecoind -datadir=$DATADIR --cleancache --startclear -regtest -server -daemon

printf "\n________________________________________\n"
printf "Preparing some mature regtest LTC: Mining the first 101 blocks\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  -rpcwait=1 generate 101 #> $NUL # Es importante agregar el rpcwait que espera que el nodo e$
printf " Creating the address ..."
ADDR=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress ale)
ADDR2=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress daniel)
##################################################################
printf "\n________________________________________\n"
printf " Funding the address with some testnet LTC for fees: \n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom "" ${ADDR} 4  # enviamos 3 BTC a ADDR
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom "" ${ADDR2} 4  # enviamos 3 BTC a ADDR
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
##################################################################
./litecoin-cli -datadir=$DATADIR -regrest tl_listproperties
printf "\n________________________________________\n"
printf "Creating ALLs:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendissuancemanaged ${ADDR} 1 2 0 "ALL" "data1" "data2"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regrest tl_getproperty 4
##################################################################
printf "\n________________________________________\n"
printf "Sending ALLs:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDR} ${ADDR} 4 200000
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_send ${ADDR} ${ADDR2} 4 100000
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1

printf "\n________________________________________\n"
printf "Checking ALLs balances:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR} 4
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR2} 4
#################################################################
printf "\n________________________________________\n"
printf "Sending DEX trade:\n"                                                     #blks
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_senddexoffer ${ADDR} 4 1000 20 10 0.00002980 2 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
##################################################################
printf "\n________________________________________\n"
printf "Checking the orderbook:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getactivedexsells ${ADDR}
printf "\n________________________________________\n"
printf "Accepting DEX offer:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_senddexaccept ${ADDR2} ${ADDR} 4 1000
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
#################################################################
printf "\n________________________________________\n"
printf "Checking the orderbook:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getactivedexsells ${ADDR}
#################################################################
printf "\n//////////////////////////////////////////\n"
printf "Stoping tradelayer and litecoin-cli:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest stop
# /home/lihki/Documentos/tradelayer-litecoin-local/src/litecoin-cli -datadir=/home/lihki/.litecoin --regtest stop
