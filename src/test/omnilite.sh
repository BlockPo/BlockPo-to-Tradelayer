SRC=/home/ale/Desktop/omni-lite/src
DATADIR=/home/ale/.litecoin
CONT=4
COL=3
PEGGED=5
AMOUNT=1000
litecoins=5
par1="LTC/ALL"
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
# # Creating Divisible tokens (ALLs)
printf "Creating ALLs:\n"   # TODO: see the indivisible/divisible troubles (amountToReserve < nBalance) in logicMath_Contra$
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_sendissuancemanaged ${ADDR} 1 2 0 "All" "All" "All"
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_listproperties
#
printf "Sending ALLs to addresses:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_sendgrant ${ADDR} ${ADDR} ${COL} 1000000
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_sendgrant ${ADDR} ${ADDR2} ${COL} 1000000
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
printf "Checking ALL balance of ADDR:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR} ${COL}
printf "Checking ALL balance of ADDR2:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} ${COL}
printf "Testing create payload rpc for createcontract: \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_createcontract ${ADDR} 1 2 "Futures Contracts" "${par1}" ${par2} ${par3} ${COL} ${par5}
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_listproperties
printf "Testing rpc for trade contract: \n"
TRA1=$($SRC/litecoin-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR} ${CONT} 100 100 1)
$SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA1}
TRA2=$($SRC/litecoin-cli -datadir=$DATADIR --regtest omni_tradecontract ${ADDR2} ${CONT} 200 100 2)
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
printf "Testing the RPC for  pegged currency Tx\n"
$SRC/litecoin-cli --regtest --datadir=$DATADIR omni_sendissuance_pegged ${ADDR2} 1 2 0 "Pegged Currency" "USD" ${COL} ${CONT} ${AMOUNT}
$SRC/litecoin-cli -datadir=$DATADIR --regtest  generate 1 > /dev/null
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_listproperties
printf "Checking Pegged Currency balance of ADDR2 : \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${PEGGED}
printf "Checking ALL balance of ADDR2:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} ${COL}
printf "Checking Pegged balance ADDR2:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_redemption_pegged ${ADDR2} ${PEGGED} 1000 ${CONT}
$SRC/litecoin-cli -datadir=$DATADIR --regtest  generate 1
printf "Checking ALL balance of ADDR2:\n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_getbalance ${ADDR2} ${COL}
printf "Checking the position of ADDR2 : \n"
$SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getposition ${ADDR2} ${CONT}
# printf "Testing create payload rpc for createcontract: \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_createpayload_createcontract  2 2 "Derivaties" "Futures Contracts" "${par1}" 2 ${par2} ${par3} ${COL} ${par5}
# printf "Testing rpc for createcontract: \n"
# TRA=$($SRC/litecoin-cli -datadir=$DATADIR --regtest omni_createcontract ${ADDR} 2 2 "Derivatives" "Futures Contracts" "$par1" 2 ${par2} ${par3} ${COL} ${par5})
# $SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA}
# $SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_listproperties
#
#
# printf "Checking ALL balance of ADDR : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR} ${COL}
# printf "Checking ALL balance of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${COL}
# printf "Checking rpc payload for cancelallcontractsbyaddress \n"
# TRA3=$($SRC/litecoin-cli -datadir=$DATADIR --regtest omni_cancelallcontractsbyaddress  ${ADDR2} 2)
# $SRC/litecoin-cli -datadir=$DATADIR --regtest generate 1 >/dev/null
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA3}
# printf "Checking ALL balance of ADDR : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR} ${COL}
# printf "Checking ALL balance of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${COL}
# printf "Checking the buy orderbook : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getcontract_orderbook ${CONT} 1
# printf "Checking the sell orderbook : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getcontract_orderbook ${CONT} 2
# printf "Testing the RPC for  pegged currency Tx\n"
# $SRC/litecoin-cli --regtest --datadir=$DATADIR omni_sendissuance_pegged ${ADDR2} 2 2 0 "Currency" "Pegged Currency" "USD" "USD" "USD" ${COL} ${CONT} ${AMOUNT}
# $SRC/litecoin-cli -datadir=$DATADIR --regtest  generate 1 > /dev/null
# # $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_gettransaction ${TRA3}
# $SRC/litecoin-cli -datadir=$DATADIR --regtest  omni_listproperties
# printf "Checking Pegged Currency balance of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${PEGGED}
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_send_pegged ${ADDR2} ${ADDR3} ${PEGGED} 500
# $SRC/litecoin-cli -datadir=$DATADIR --regtest  generate 1 > /dev/null
# printf "Checking Pegged Currency balance of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${PEGGED}
# printf "Checking Pegged Currency balance of ADDR3 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR3} ${PEGGED}
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_redemption_pegged ${ADDR2} ${PEGGED} 100 ${CONT}
# $SRC/litecoin-cli -datadir=$DATADIR --regtest  generate 1 > /dev/null
# printf "Checking the position of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getposition ${ADDR2} ${CONT}
# printf "Checking Pegged Currency balance of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${PEGGED}
# printf "Checking ALL balance of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${COL}
# printf "Generating 13 blocks more: \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest  generate 13 > /dev/null
# printf "Checking ALL balance of ADDR2 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR2} ${COL}
# printf "Checking ALL balance of ADDR3 : \n"
# $SRC/litecoin-cli -datadir=$DATADIR --regtest omni_getbalance ${ADDR3} ${COL}
$SRC/litecoin-cli --regtest --datadir=$DATADIR stop
