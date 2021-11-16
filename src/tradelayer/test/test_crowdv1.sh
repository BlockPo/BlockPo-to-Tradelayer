#!/bin/bash

PASS=0
FAIL=0
clear
#sudo rm graphInfo*
printf "Preparing a test environment...\n"
printf "   * Starting a fresh regtest daemon\n"
rm -r ~/.litecoin/regtest
./src/litecoind --regtest --server --daemon --tlactivationallowsender=any --tldebug=all >nul
sleep 5
printf "   * Preparing some mature testnet LTC\n"
./src/litecoin-cli --regtest generate 102 >null
printf "   * Obtaining a master address to work with\n"
ADDR=$(./src/litecoin-cli --regtest getnewaddress TLAccount)
printf "   * Funding the address with some testnet LTC for fees\n"
./src/litecoin-cli --regtest sendtoaddress $ADDR 20 >null
./src/litecoin-cli --regtest generate 1 >null
printf "   * Participating in the Exodus crowdsale to obtain some <ALL> token\n"
JSON="{\"moneyqMan7uh8FqdCA2BV5yZ8qVrc9ikLP\":10,\""$ADDR"\":4}"
./src/litecoin-cli --regtest sendmany TLAccount $JSON >null
./src/litecoin-cli --regtest generate 1 >null
printf "   * Generating addresses to use\n"
ADDRESS=()
for i in {1..10}
do
   ADDRESS=("${ADDRESS[@]}" $(./src/litecoin-cli --regtest getnewaddress))
done
printf "\nTesting a crowdsale using LTC before activation...\n"
printf "   * Creating an indivisible test property and opening a crowdsale\n"

TXID=$(./src/litecoin-cli --regtest tl_sendissuancecrowdsale $ADDR 1 0 "Z_TestCat" "Z_TestSubCat" "Z_IndivisTestProperty" "Z_TestURL" "Z_TestData" 0 10 1667190670 0 0)
./src/litecoin-cli --regtest generate 1 >null
printf "     # Checking the transaction was invalid... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep -w valid | cut -c12-)
if [ $RESULT == "false," ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "\nActivating v2 crowdsales (allow LTC)... \n"
printf "   * Sending the activation\n"
BLOCKS=$(./src/litecoin-cli --regtest getblockcount)
TXID=$(./src/litecoin-cli --regtest tl_sendactivation $ADDR 32 $(($BLOCKS + 8)) 999)
./src/litecoin-cli --regtest generate 1 >null
printf "     # Checking the activation transaction was valid... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep -w valid | cut -c12-)
if [ $RESULT == "true," ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "   * Mining 10 blocks to forward past the activation block\n"
./src/litecoin-cli --regtest generate 10 >null
printf "     # Checking the activation went live as expected... "
FEATUREID=$(./src/litecoin-cli --regtest tl_getactivations | grep -A 10 completed | grep featureid | cut -c20-21)
if [ $FEATUREID == "32" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $FEATUREID
    FAIL=$((FAIL+1))
fi
printf "\nTesting crowdsales using LTC after activation...\n"
printf "\nTesting an indivisible LTC crowdsale...\n"
printf "   * Creating an indivisible test property and opening a crowdsale\n"
CROWDTXID=$(./src/litecoin-cli --regtest tl_sendissuancecrowdsale $ADDR 1 0 "Z_TestCat" "Z_TestSubCat" "Z_IndivisTestProperty" "Z_TestURL" "Z_TestData" 0 1000 1667190670 0 0)
./src/litecoin-cli --regtest generate 1 >null
printf "     # Checking the transaction was valid... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $CROWDTXID | grep -w valid | cut -c12-)
if [ $RESULT == "true," ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "\nTesting sending a LTC payment to the crowdsale...\n"
printf "   * Sending some LTC to %s\n" ${ADDRESS[1]}
./src/litecoin-cli --regtest sendtoaddress ${ADDRESS[1]} 0.2 >null
printf "   * Sending some LTC from %s to the crowdsale\n" ${ADDRESS[1]}
TXID=$(./src/litecoin-cli --regtest tl_sendltcpayment ${ADDRESS[1]} $ADDR $CROWDTXID 0.1)
./src/litecoin-cli --regtest generate 1 >null
printf "     # Checking the transaction was valid... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep -w valid | cut -c12-)
if [ $RESULT == "true," ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the payment was linked to the correct transaction... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep $CROWDTXID | wc -l)
if [ $RESULT == "1" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the payment was linked to the correct recipient... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep recipient | grep $ADDR | wc -l)
if [ $RESULT == "1" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the payment amount was 0.1 LTC... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep amount | cut -c21-30)
if [ $RESULT == "0.10000000" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the sending address now has 100 of property 4... "
BALANCE=$(./src/litecoin-cli --regtest tl_getbalance ${ADDRESS[1]} 4 | grep balance | cut -d '"' -f4)
if [ $BALANCE == "100" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $BALANCE  
    FAIL=$((FAIL+1))
fi
printf "     # Checking the transaction is listed in crowdsale participants... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 4 true | grep $TXID | wc -l)
if [ $RESULT == "1" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the crowdsale amount raised is now 0.1 LTC... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 4 true | grep amountraised | cut -d '"' -f4)
if [ $RESULT == "0.10000000" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the amount of tokens issued to users is now 100... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 4 true | grep tokensissued | cut -d '"' -f4)
if [ $RESULT == "100" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the amount of tokens issued to the issser is still 0... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 4 true | grep addedissuertokens | cut -d '"' -f4)
if [ $RESULT == "0" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "\nTesting a divisible LTC crowdsale...\n"
printf "   * Sending some LTC to %s\n" ${ADDRESS[3]}
./src/litecoin-cli --regtest sendtoaddress ${ADDRESS[3]} 0.2 >null
printf "   * Sending some LTC to %s\n" ${ADDRESS[4]}
./src/litecoin-cli --regtest sendtoaddress ${ADDRESS[4]} 0.2 >null
./src/litecoin-cli --regtest generate 1 >null
printf "   * Creating a divisible test property and opening a crowdsale\n"
CROWDTXID=$(./src/litecoin-cli --regtest tl_sendissuancecrowdsale ${ADDRESS[3]} 2 0 "Z_TestCat" "Z_TestSubCat" "Z_DivisTestProperty" "Z_TestURL" "Z_TestDt" 0 1000 1667190670 0 0)
./src/litecoin-cli --regtest generate 1 >null
printf "     # Checking the transaction was valid... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $CROWDTXID | grep valid | cut -c12-)
if [ $RESULT == "true," ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "   * Sending some LTC from %s to the crowdsale\n" ${ADDRESS[4]}
TXID=$(./src/litecoin-cli --regtest tl_sendltcpayment ${ADDRESS[4]} ${ADDRESS[3]} $CROWDTXID 0.1)
./src/litecoin-cli --regtest generate 1 >null
printf "     # Checking the transaction was valid... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep valid | cut -c12-)
if [ $RESULT == "true," ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the payment was linked to the correct transaction... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep $CROWDTXID | wc -l)
if [ $RESULT == "1" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the payment was linked to the correct recipient... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep recipient | grep ${ADDRESS[3]} | wc -l)
if [ $RESULT == "1" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the payment amount was 0.1 LTC... "
RESULT=$(./src/litecoin-cli --regtest tl_gettransaction $TXID | grep amount | cut -c21-30)
if [ $RESULT == "0.10000000" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the sending address now has 100.0 of property 5... "
BALANCE=$(./src/litecoin-cli --regtest tl_getbalance ${ADDRESS[4]} 5 | grep balance | cut -d '"' -f4)
if [ $BALANCE == "100.00000000" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $BALANCE
    FAIL=$((FAIL+1))
fi
printf "     # Checking the transaction is listed in crowdsale participants... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 5 true | grep $TXID | wc -l)
if [ $RESULT == "1" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the crowdsale amount raised is now 0.1 LTC... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 5 true | grep amountraised | cut -d '"' -f4)
if [ $RESULT == "0.10000000" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the amount of tokens issued to users is now 100.0... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 5 true | grep tokensissued | cut -d '"' -f4)
if [ $RESULT == "100.00000000" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi
printf "     # Checking the amount of tokens issued to the issuer is still 0... "
RESULT=$(./src/litecoin-cli --regtest tl_getcrowdsale 5 true | grep addedissuertokens | cut -d '"' -f4)
if [ $RESULT == "0.00000000" ]
  then
    printf "PASS\n"
    PASS=$((PASS+1))
  else
    printf "FAIL (result:%s)\n" $RESULT
    FAIL=$((FAIL+1))
fi

printf "\n"
printf "####################\n"
printf "#  Summary:        #\n"
printf "#    Passed = %d   #\n" $PASS
printf "#    Failed = %d    #\n" $FAIL
printf "####################\n"
printf "\n"

./src/litecoin-cli --regtest stop