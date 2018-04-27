SRC=/home/ale/Desktop/omni-lite/src
DATADIR=/home/ale/.litecoin
CONT=2147483651
COL=2147483652
AMOUNT=1000
litecoins=5
par1="Future Contract 1"
par2=20 # blocks until expiration
par3=10 # Notional Size
par5=2 #Margin Requirement
rm -r -f $DATADIR/regtest
# $SRC/omnicore-cli --regtest --datadir=$DATADIR stop &>/dev/null
# sleep 1
printf "Preparing a test environment...\n"
$SRC/litecoind --cleancache --startclear --regtest --server --daemon
$SRC/litecoin-cli -datadir=$DATADIR --regtest  -rpcwait generate 101 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest  getinfo
printf "Creating some addresses ...\n"
ADDR=$($SRC/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress OMNIAccount)
ADDR2=$($SRC/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress OMNIAccount)
ADDR3=$($SRC/litecoin-cli -datadir=$DATADIR --regtest  getnewaddress OMNIAccount)
printf "${ADDR}\n"
printf "${ADDR2}\n"
printf "${ADDR3}\n"
JSON="{\""${ADDR}"\":${litecoins},\""${ADDR2}"\":${litecoins}}"
#Sending litecoins to all
$SRC/litecoin-cli -datadir=$DATADIR --regtest sendmany "" $JSON > /dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest  generate 1 > /dev/null
printf "Testing create payload rpc for createcontract: \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_createpayload_createcontract  2 2 "Derivaties" "Futures Contracts" "${par1}" 2 ${par2} ${par3} ${COL} ${par5}
printf "Testing rpc for createcontract: \n"
TRA=$($SRC/litecoin-cli -datadir=$DATADIR --regtest omni_createcontract ${ADDR} 2 2 "Derivatives" "Futures Contracts" "$par1" 2 ${par2} ${par3} ${COL} ${par5})
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA}
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_listproperties
printf "Testing create payload rpc for tradecontract: \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_createpayload_contract_trade 1 100 20 1
# # Creating Divisible tokens (ALLs)
printf "Creating ALLs:\n"   # TODO: see the indivisible/divisible troubles (amountToReserve < nBalance) in logicMath_Contra$
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_sendissuancemanaged ${ADDR} 2 2 0 "All" "All" "All"
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_listproperties

printf "Sending ALLs to addresses:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_sendgrant ${ADDR} ${ADDR} ${COL} 1000000
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_sendgrant ${ADDR} ${ADDR2} ${COL} 1000000
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
printf "Checking ALL balance of ADDR:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} ${COL}
printf "Checking ALL balance of ADDR2:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} ${COL}
printf "Testing rpc for trade contract: \n"
TRA1=$($SRC/litecoin-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR} ${CONT} 100 20 1)
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA1}
TRA2=$($SRC/litecoin-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR2} ${CONT} 200 20 2)
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA2}
printf "Checking the buy orderbook : \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getcontract_orderbook ${CONT} 1
printf "Checking the sell orderbook : \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getcontract_orderbook ${CONT} 2
printf "Checking the position of ADDR : \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getposition ${ADDR} ${CONT}
printf "Checking the position of ADDR2 : \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getposition ${ADDR2} ${CONT}
# # Default inputs in create contract RPC
# par1="Future Contract 1"
# par2=20 # blocks until expiration
# par3=10 # Notional Size
# par5=2 #Margin Requirement
# #Creo el futuro
# T=$($SRC/omnicore-cli -datadir=$DATADIR --regtest omni_createcontract ${ADDR} 2 3 1 "Derivaties" "Futures Contracts" "$par1" "www.tradelayer.org" "Futures Contracts Exchange on OmniLayer" 3 "1" 120 30 2 $par2 $par3 $COL $par5)
# $SRC/omnicore-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
#
# # Creating Divisible tokens ( Synthetic USD)
# #printf "Creating an Divisible Token:\n"   # TODO: see the indivisible/divisible troubles (amountToReserve < nBalance) in logicMath_Contra$
# $SRC/omnicore-cli --regtest omni_sendissuancemanaged ${ADDR} 2 2 0 "Tether" "Tether" "Tether" "" "" >/dev/null
# $SRC/omnicore-cli --regtest generate 1 >/dev/null
# #$SRC/omnicore-cli --regtest omni_gettransaction $TRA9
#
# #Sending synthetic USDs to all
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_sendgrant ${ADDR} ${ADDR} ${COL} 1000000 > /dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_sendgrant ${ADDR} ${ADDR2} ${COL} 1000000 > /dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 >/dev/null
#
# # Checking the balances
# printf "Balance of USDT for ADDR:\n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483652
# printf "Balance of USDT for ADDR2:\n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} 2147483652
#
# printf "Making some tradings ...\n"
# TRA1=$($SRC/omnicore-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR2} 2147483651 100 1 1 100 1) #>/dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 #>/dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA1}
# # ADDR going in short position
# TRA2=$($SRC/omnicore-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR} 2147483651 100 1 1 100 2) #>/dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 #>/dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA2}
# printf "Position in Contracts for ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getposition ${ADDR} 2147483651
# printf "Testing the RPC for  pegged currency createpayload\n"
# $SRC/omnicore-cli --regtest --datadir=$DATADIR omni_createpayload_issuance_pegged 2 2 0 "Currency" "Pegged Currency" "Lihki Coin" "Lihki" "Lihki" ${COL} ${CONT} ${AMOUNT}
# printf "Testing the RPC for  pegged currency Tx\n"
# TRA=$($SRC/omnicore-cli --regtest --datadir=$DATADIR omni_sendissuance_pegged ${ADDR} 2 2 0 "Currency" "Pegged Currency" "Lihki Coin" "Lihki" "Lihki" ${COL} ${CONT} ${AMOUNT})
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 > /dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA}
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_listproperties
# # Checking the balances
# printf "Balance of USDT for ADDR:\n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483652
# #$SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} 2147483652
# printf "Balance of Pegged Currency in ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483653
# printf "Position in Contracts for ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getposition ${ADDR} 2147483651
# printf "Making some tradings ...\n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR2} 2147483651 1 1 1 100 2 >/dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 #>/dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA1}
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR} 2147483651 1 1 1 100 1 >/dev/null
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 #>/dev/null
# printf "Position in Contracts for ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getposition ${ADDR} 2147483651
# printf " Sending pegged currency from ADDR to ADDR2 : \n"
# TRA3=$($SRC/omnicore-cli --regtest --datadir=$DATADIR omni_send_pegged ${ADDR} ${ADDR3} 2147483653 500)
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 >/dev/null
# printf "Balance of USDT for ADDR:\n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483652
# $SRC/omnicore-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA3}
# printf "Balance of Pegged Currency in ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483653
# printf "Balance of Pegged Currency in ADDR3: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR3} 2147483653
# printf "Checking RPC for Redemption of Peggeds : \n"
# TRA4=$($SRC/omnicore-cli --regtest --datadir=$DATADIR omni_redemption_pegged ${ADDR} 2147483653 200 ${CONT})
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 1 > /dev/null
# printf "Balance of USDT for ADDR:\n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483652
# printf "Balance of Pegged Currency in ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483653
# printf "Position in Contracts for ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getposition ${ADDR} 2147483651
# printf "Balance of Pegged Currency in ADDR2: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} 2147483653
# printf "Balance of Pegged Currency in ADDR3: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR3} 2147483653
# printf "Creating 12 blocks more:\n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  generate 12 > /dev/null
# printf "Balance of Pegged Currency in ADDR: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} 2147483653
# printf "Balance of Pegged Currency in ADDR2: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} 2147483653
# printf "Balance of Pegged Currency in ADDR3: \n"
# $SRC/omnicore-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR3} 2147483653
$SRC/litecoin-cli --regtest --datadir=$DATADIR stop
