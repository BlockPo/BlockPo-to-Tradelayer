#!/bin/bash

SRCDIR=/home/ale/Escritorio/master/src
DATADIR=/home/ale/.litecoin
NUL=/dev/null
printf "\n//////////////////////////////////////////\n"
printf "Cleaning the regtest folder\n"

rm -r /home/ale/.litecoin/regtest

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
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom "" ${ADDR} 3  # enviamos 3 BTC a ADDR
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom "" ${ADDR2} 3  # enviamos 3 BTC a ADDR
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
##################################################################
printf "\n________________________________________\n"
printf "Creating ALLs:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendissuancemanaged ${ADDR} 1 2 0 "ALL" "data1" "data2"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1

##################################################################
printf "\n________________________________________\n"
printf "Sending ALLs:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDR} ${ADDR} 3 100000
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDR} ${ADDR2} 3 100000
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
##################################################################
printf "\n________________________________________\n"
printf "Checking ALLs balances:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR} 3
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR2} 3
##################################################################
printf "\n________________________________________\n"
printf "Creating Contract:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_createcontract ${ADDR} 1 4 "LTC/dUSD" 2 1 3 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regrest tl_listproperties

##################################################################
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDR} 4 1000 50 2 #SHORT POSITION
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1

$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDR2} 4 1000 50 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1

printf "\n Cheking the  orderbok (sellside):\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook 4 2

printf "\n Cheking the  orderbok (buyside):\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook 4 1
##################################################################
printf "\n Putting some buy orders:\n"

$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDR2} 4 50 40 2
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDR2} 4 1000 30 2
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1


# printf "\n Cheking the  orderbok (sellside):\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook ${CONTRACT} 2

printf "\n Cheking the  orderbok (buyside):\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook 4 1 # BUYING 1000 CONTRACTS TO MARKET PRICE

##################################################################
printf "\n Checking ADDR position:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getposition ${ADDR} 4

##################################################################
printf "\n Creating Pegged Currency:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_sendissuance_pegged ${ADDR} 1 2 0 "dUSD" 3 4 100
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regrest tl_listproperties
# ##################################################################
# printf "\n Creating Pegged Currency:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_sendissuance_pegged ${ADDR} 1 2 0 "dUSD" 3 4 100
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regrest tl_listproperties
##################################################################
printf "\n Checking pegged balance for ADDR:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getbalance ${ADDR} 5
##################################################################
printf "\n Checking contracts in reserve:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_reserve ${ADDR} 4
##################################################################
# printf "\n Sending ALLs to ADDR2:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_send ${ADDR} ${ADDR2} 5 50
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
# ##################################################################
printf "\n Checking pegged balance for ADDR:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getbalance ${ADDR} 5
printf "\n Checking pegged balance for ADDR2:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getbalance ${ADDR2} 5
##################################################################
printf "\n Checking margin account for ADDR:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getmargin ${ADDR} 3
printf "\n Checking margin account for ADDR2:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getmargin ${ADDR2} 3
##################################################################
# printf "\n Pegged history:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getpeggedhistory 5
# ##################################################################
# printf "\n List transaction:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_listtransactions
##################################################################
printf "\n Making redemption for ADDR:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_redemption_pegged ${ADDR} 5 100 4
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
##################################################################
printf "\n Checking contracts in reserve:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_reserve ${ADDR} 4
##################################################################
printf "\n Checking position of ADDR:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getposition ${ADDR} 4
printf "\n//////////////////////////////////////////\n"
printf "Stoping tradelayer and litecoin-cli:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest stop
# /home/lihki/Documentos/tradelayer-litecoin-local/src/litecoin-cli -datadir=/home/lihki/.litecoin --regtest stop
