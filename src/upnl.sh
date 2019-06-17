#!/bin/bash
# short position, NO active orders. upnl loss greater than 80%, margin call
SRCDIR=/root/dev-margin-domake/src
DATADIR=/root/chain-lihki
NUL=/dev/null
printf "\n//////////////////////////////////////////\n"
printf "Cleaning the regtest folder\n"

rm -r /root/chain-lihki/regtest

printf "\n________________________________________\n"
printf "Preparing a test environment...\n"
$SRCDIR/litecoind -datadir=$DATADIR --cleancache --startclear -regtest -server -daemon

printf "\n________________________________________\n"
printf "Preparing some mature regtest LTC: Mining the first 101 blocks\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  -rpcwait=1 generate 201 #> $NUL # Es importante agregar el rpcwait que espera que el nodo e$
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance ""
printf " Creating the address ..."
ADDR=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress ale)
ADDR2=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress daniel)
ADDR3=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress cumpita)
ADDR4=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress lihki)
echo $ADDR
echo $ADDR2
echo $ADDR3
echo $ADDR4
##################################################################
printf "\n________________________________________\n"
printf " Funding the address with some testnet LTC for fees: \n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom "" ${ADDR} 4000  # enviamos 3 BTC a ADDR
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom "" ${ADDR2} 500 # enviamos 3 BTC a ADDR2
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom ale ${ADDR3} 500  # enviamos 3 BTC a ADDR3
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom ale ${ADDR4} 500 # enviamos 3 BTC a ADDR3
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1
##################################################################
printf "\n________________________________________\n"
printf "Checking balance for ale account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance ale
printf "Checking balance for daniel account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance daniel
printf "Checking balance for cumpita account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance cumpita
printf "Checking balance for lihki account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance lihki
##################################################################
printf "\n________________________________________\n"
printf "Creating ALLs:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendissuancemanaged ${ADDR} 1 2 0 "LIHKI" "data1" "data2"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regrest tl_getproperty 4

##################################################################
printf "\n________________________________________\n"
printf "Sending ALLs:\n"
TRA1=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDR} ${ADDR} 4 100000)
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
TRA2=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDR} ${ADDR2} 4 100000)
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
TRA3=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDR} ${ADDR3} 4 100000)
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
TRA4=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDR} ${ADDR4} 4 100000)
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
# ##################################################################
printf "\n________________________________________\n"
printf "Checking ALLs balances:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR} 4
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR2} 4
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR3} 4
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR4} 4

printf "\n________________________________________\n"
printf "Checking ALLs reserve balance:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getmargin ${ADDR} 4


printf "\n________________________________________\n"
printf "Checking ALLs balances for ADDR2:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR2} 4
#
# ##################################################################
printf "\n________________________________________\n"
printf "Creating Contract 1:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_createcontract ${ADDR} 1 4 "ALL dUSD" 100 1 4 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1

##################################################################
printf "\n________________________________________\n"
printf "Checking balance for ale account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance ale
printf "Checking balance for daniel account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance daniel
printf "Checking balance for cumpita account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance cumpita
printf "Checking balance for lihki account:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance lihki

printf "\n________________________________________\n"
printf "Creating short position:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_tradecontract ${ADDR} "ALL dUSD" 1000 1.2 2 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_tradecontract ${ADDR2} "ALL dUSD" 1000 1.2 1 1
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1

#
# printf "\n________________________________________\n"
# printf "Checking ALLs balances:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR} 4
# printf "\n________________________________________\n"
# printf "Checking ALLs reserve balance:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getmargin ${ADDR} 4
#
# printf "Adding a trade in the buy side:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_tradecontract ${ADDR4} "ALL dUSD" 1000 3000 1 1
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
#
# printf "Checking the buy side of orderbook:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getcontract_orderbook "ALL dUSD" 1
#

# printf "More trading:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_tradecontract ${ADDR3} "ALL dUSD" 1000 10000 2 1
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_tradecontract ${ADDR4} "ALL dUSD" 1000 10000 1 1
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 2
#
# printf "\n________________________________________\n"
# printf "Checking position in contract ALL/dUSD:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getposition ${ADDR} 5
#
# printf "\n________________________________________\n"
# printf "Checking ALLs balances:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDR} 4
# printf "\n________________________________________\n"
# printf "Checking ALLs reserve balance:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getmargin ${ADDR} 4
#
#
# printf "\n________________________________________\n"
# printf "Checking ALLs reserve balance:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getsum_upnl ${ADDR}
#
# printf "Checking the sell side of orderbook:\n"
# $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getcontract_orderbook "ALL dUSD" 2


printf "\n//////////////////////////////////////////\n"
printf "Stoping omnicored and litecoin-cli:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest stop
