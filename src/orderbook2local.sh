#!/bin/bash

SRCDIR=/home/lihki/Documents/omnicore-litecoin-demo-local/src
DATADIR=/home/lihki/.litecoin
NUL=/dev/null
printf "\n//////////////////////////////////////////\n"
printf "Cleaning the regtest folder\n"

sudo rm -r /home/lihki/.litecoin/regtest
sudo rm graphInfo*

printf "\n________________________________________\n"
printf "Preparing a test environment...\n"
$SRCDIR/litecoind -datadir=$DATADIR --cleancache --startclear -regtest -server -daemon 

printf "\n________________________________________\n"
printf "Waiting three seconds to start the client...\n"

sleep 3

printf "\n________________________________________\n"
printf "Preparing some mature regtest BTC: Mining the first N blocks getting X Bitcoins\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  -rpcwait=1 generate 401 #> $NUL # Es importante agregar el rpcwait que espera que el nodo e$

printf "\n________________________________________\n"
printf "\n Checking the balance of block:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getbalance  # balance del bloque (50BTCs)

##################################################################
ADDRBase=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress Lihki)
printf "\n________________________________________\n"
printf "Base address to work with:\n"
printf $ADDRBase

N=100

amount_bitcoin=10
amountbitcoin_baseaddr=10
amountbitcoin_manyaddr=100
amountbitcoin_moneyaddr=10
notional_size=1
margin_requirement=1
amountusdts_manyaddr=200000000
blocks_until_expiration=225
CONTRACT=3
collateral=4

printf " Creating the addresses ..."

ADDRess=()
for (( i=1; i<=${N}; i++ ))
do
    ADDRess[$i]=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress OMNIAccount)
    echo ${ADDRess[$i]} >> graphInfoAddresses.txt
done

##################################################################
printf "\n________________________________________\n"
printf " Funding the address with some testnet LTC for fees\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  sendfrom "" ${ADDRBase} ${amount_bitcoin}  
printf "Generating one block\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  generate 1

##################################################################
printf "\n________________________________________\n"
printf "   * Participating in the Exodus crowdsale to obtain M OMNIs: To get OMNIs in the first address ADDR\n"

for (( i=1; i<=${N}; i++ ))
do
    StringJSON+=",\""${ADDRess[$i]}"\":${amountbitcoin_manyaddr}"
done

printf "\n________________________________________\n"
printf "Checking JSON String\n"
JSON="{\"moneyqMan7uh8FqdCA2BV5yZ8qVrc9ikLP\":${amountbitcoin_moneyaddr},\""${ADDRBase}"\":${amountbitcoin_baseaddr}$StringJSON}"
printf "${JSON}"

printf "\n________________________________________\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest sendmany "" $JSON #Sending Bitcoin to every address
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1 # Generating one block

##################################################################
printf "\n________________________________________\n"
printf "Creating Future Contract with base address: ${ADDRBase}\n"
TRACreate=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_createcontract $ADDRBase 1 1 "ALL F19" ${blocks_until_expiration} ${notional_size} ${collateral} ${margin_requirement})
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1

printf "\n________________________________________\n"
printf "Checking confirmation of Creating Future Contract #1:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_gettransaction $TRACreate
##################################################################
printf "\n________________________________________\n"
printf "Creating an Divisible Token USDT:\n"
TRAUSDT=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendissuancemanaged $ADDRBase 1 2 0 "Tether" "Tether" "")
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1

printf "\n________________________________________\n"
printf "Checking confirmation of transaction Token USDT:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction $TRAUSDT
./litecoin-cli -datadir=$DATADIR -regrest tl_listproperties
##################################################################
for (( i=1; i<=${N}; i++ ))
do
    printf "\n////////////////////////////////////////\n"
    printf "Sending USDTs from base address to the addresses #$i\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDRBase} ${ADDRess[$i]} 4 ${amountusdts_manyaddr}
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1 # Generating one block
    
    printf "\n________________________________________\n"
    printf "Checking USDT balances for the address #$i:\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDRess[$i]} 4
done
##################################################################
# for (( i=1; i<=${N}; i++ ))
# do
#     printf "\n________________________________________\n"
#     printf "Price for sale Seller #$i\n"
#     PRICE=$((RANDOM%100+3400))
#     printf "\nRandom Price:\n"
#     printf $PRICE
    
#     printf "\nAmount for sale Seller #$i\n"
#     AMOUNT=$((RANDOM%99+1))
#     printf "\nRandom Amount:\n"
#     printf $AMOUNT
#     printf "\n"
    
#     $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[$i]} ${CONTRACT} ${AMOUNT} ${PRICE} 1
#     $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1    
# done
# ##################################################################
# for (( i=1; i<=${N}; i++ ))
# do
#     printf "\n________________________________________\n"
#     printf "Price for sale Seller #$i\n"
#     PRICE=$((RANDOM%100+3400))
#     printf "\nRandom Price:\n"
#     printf $PRICE
    
#     printf "\nAmount for sale Seller #$i\n"
#     AMOUNT=$((RANDOM%99+1))
#     printf "\nRandom Amount:\n"
#     printf $AMOUNT
#     printf "\n"
    
#     $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[$i]} ${CONTRACT} ${AMOUNT} ${PRICE} 2
#     $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1    
# done
##################################################################
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[1]} ${CONTRACT} 1000 1000 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[2]} ${CONTRACT} 1000 1000 2
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1

$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[2]} ${CONTRACT} 1000 100 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[3]} ${CONTRACT} 1000 100 2
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
##################################################################

printf "\n Cheking the  orderbok (sellside):\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook ${CONTRACT} 2

printf "\n Cheking the  orderbok (buyside):\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook ${CONTRACT} 1
################################################################## 
printf "Stoping omnicored and litecoin-cli:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest stop
# /home/lihki/Documentos/omnicore-litecoin-local/src/litecoin-cli -datadir=/home/lihki/.litecoin --regtest stop
