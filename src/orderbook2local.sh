#!/bin/bash

SRCDIR=/home/lihki/Documents/TradeLayer/tradelayer-devlihki-todomake/src

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

$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  -rpcwait=1 generate 1000 #> $NUL # Es importante agregar el rpcwait que espera que el nodo e$

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
amountbitcoin_baseaddr=10000
amountbitcoin_manyaddr=10
amountbitcoin_moneyaddr=10
notional_size=2
margin_requirement=1
amountusdts_manyaddr=90000000
blocks_until_expiration=99999999
collateral=5
PAYMENTWINDOW=10
MINFEEACCEPTED=0.00002980

printf " Creating the addresses ..."
ADDRess=()
for (( i=1; i<=${N}; i++ ))
do
    ADDRess[$i]=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress OMNIAccount${i})
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

TRACreate=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_createcontract $ADDRBase 1 1 "ALL F18" ${blocks_until_expiration} ${notional_size} ${collateral} ${margin_requirement})

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
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_gettransaction $TRAUSDT
./litecoin-cli -datadir=$DATADIR -regrest tl_listproperties

for (( i=1; i<=${N}; i++ ))
do
    printf "\n////////////////////////////////////////\n"
    printf "Sending USDTs from base address to the addresses #$i\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDRBase} ${ADDRess[$i]} 5 ${amountusdts_manyaddr}
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1 # Generating one block
    
    printf "\n________________________________________\n"
    printf "Checking USDT balances for the address #$i:\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDRess[$i]} 5
done
##################################################################
printf "\n________________________________________\n"
printf "Creating an Divisible Token ALLs:\n"
TRAALL=$($SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendissuancemanaged $ADDRBase 1 2 0 "ALL" "ALL" "")
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1

printf "\n________________________________________\n"
printf "Checking confirmation of transaction Token ALLs:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_gettransaction $TRAALL
./litecoin-cli -datadir=$DATADIR -regrest tl_listproperties

for (( i=1; i<=${N}; i++ ))
do
    printf "\n////////////////////////////////////////\n"
    printf "Sending ALLs from base address to the addresses #$i\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendgrant ${ADDRBase} ${ADDRess[$i]} 6 ${amountusdts_manyaddr}
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1 # Generating one block
    
    printf "\n________________________________________\n"
    printf "Checking ALL balances for the address #$i:\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getbalance ${ADDRess[$i]} 6
done
##################################################################
# Begin of CMPContractDEx
for (( i=1; i<=${N}; i++ ))
do
    printf "\n________________________________________\n"
    printf "Price for sale Buyer #$i ContractDEx\n"
    PRICE=$((RANDOM%1000+6000))
    printf "\nRandom Price:\n"
    printf $PRICE
    
    printf "\nAmount for sale Buyer #$i ContractDEx\n"
    AMOUNT=$((RANDOM%1000+6000))
    printf "\nRandom Amount:\n"
    printf $AMOUNT
    printf "\n"
    
    $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[$i]} "ALL F18" ${AMOUNT} ${PRICE} 1
    $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
    
done
##################################################################
for (( i=1; i<=${N}-1; i++ ))
do
    ##################################################################
    # Begin DEx Traded
    printf "\n________________________________________\n"
    printf "Price for sale Buyer #$i DEx\n"
    PRICEDEx=$((RANDOM%80000+10000))
    printf "\nRandom Price DEx:\n"
    printf $PRICEDEx
    
    printf "\nAmount for sale Buyer #$i DEx\n"
    AMOUNTDEx=$((RANDOM%8+1))
    printf "\nRandom Amount DEx:\n"
    printf $AMOUNTDEx
    
    printf "\n________________________________________\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_senddexoffer ${ADDRess[$i]} 6 ${AMOUNTDEx} ${PRICEDEx} ${PAYMENTWINDOW} ${MINFEEACCEPTED} 2 1
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
    
    printf "\n________________________________________\n"
    printf "Checking the orderbook DEx:\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getactivedexsells ${ADDRess[$i]}
    
    printf "\n________________________________________\n"
    printf "Accepting DEX offer DEx:\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_senddexaccept ${ADDRess[$i+1]} ${ADDRess[$i]} 6 ${AMOUNTDEx}
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
    
    printf "\n________________________________________\n"
    printf "Checking the orderbook DEx:\n"
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_getactivedexsells ${ADDRess[$i]}
    
    # End DEx Traded
    #################################################################
    # Begin MetaDex traded
    
    printf "\n________________________________________\n"
    printf "Sending metadex trade:\n"
    
    printf "\nAmount ALL #$i\n"
    AMOUNTALL=$((RANDOM%1000+6000))
    printf "\nRandom Amount ALL:\n"
    printf $AMOUNTALL
    printf "\n"
    
    printf "\nAmount Tether #$i\n"
    AMOUNTTETHER=$((RANDOM%1000+6000))
    printf "\nRandom Amount Tether:\n"
    printf $AMOUNTTETHER
    printf "\n"
    
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendtrade ${ADDRess[$i+1]} 5 ${AMOUNTALL} 6 ${AMOUNTTETHER}
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
    
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest tl_sendtrade ${ADDRess[$i]} 6 ${AMOUNTTETHER} 5 ${AMOUNTALL}
    $SRCDIR/litecoin-cli -datadir=$DATADIR --regtest generate 1
    
    # End MetaDex traded
    # #################################################################
    # Match CMPCOntractDEx
    
    printf "\n________________________________________\n"
    printf "Price for sale Seller #$i ContractDEx match\n"
    PRICE=$((RANDOM%1000+6000))
    printf "\nRandom Price  ContractDEx match:\n"
    printf $PRICE
    
    printf "\nAmount for sale Seller #$i ContractDEx match\n"
    AMOUNT=$((RANDOM%1000+6000))
    printf "\nRandom Amount ContractDEx match:\n"
    printf $AMOUNT
    printf "\n"
    
    $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_tradecontract ${ADDRess[$i]} "ALL F18" ${AMOUNT} ${PRICE} 2
    $SRCDIR/litecoin-cli -datadir=$DATADIR -regtest generate 1
    
done
##################################################################
printf "\n Cheking the  orderbok (sellside):\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook "ALL F18" 2

printf "\n Cheking the  orderbok (buyside):\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR -regtest tl_getcontract_orderbook "ALL F18" 1
# End of CMPContractDEx
##################################################################
printf "Stoping omnicored and litecoin-cli:\n"
$SRCDIR/litecoin-cli -datadir=$DATADIR --regtest stop
# /home/lihki/Documentos/tradelayer-local/src/litecoin-cli -datadir=/home/lihki/.litecoin --regtest stop

