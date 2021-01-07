#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test ContractDEx oracle settlement."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse
import random
import string
import sys
import time

try:
    os.remove("SettlementRows.txt")
except:
    print("SettlementRows.txt allready deleted")

VOWELS = "aeiou"
CONSONANTS = "".join(set(string.ascii_lowercase) - set(VOWELS))

def generate_word(length):
    word = ""
    for i in range(length):
        if i % 2 == 0:
            word += random.choice(CONSONANTS)
        else:
            word += random.choice(VOWELS)
    return word

class OracleSettlementTest (BitcoinTestFramework):
    
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-txindex=1"]]

    def setup_chain(self):
        super().setup_chain()
        #Append rpcauth to bitcoin.conf before initialization
        rpcauth = "rpcauth=rt:93648e835a54c573682c2eb19f882535$7681e9c5b74bdd85e78166031d2058e1069b3ed7ed967c93fc63abba06f31144"
        rpcuser = "rpcuser=rpcuser💻"
        rpcpassword = "rpcpassword=rpcpassword🔑"
        with open(os.path.join(self.options.tmpdir+"/node0", "litecoin.conf"), 'a', encoding='utf8') as f:
            f.write(rpcauth+"\n")

    def run_test(self):

        self.log.info("Preparing the workspace...")

        # mining 500 blocks
        self.nodes[0].generate(500)
        
        ################################################################################
        # Checking RPC tl_sendtrade (in the first 200 blocks of the chain) #
        ################################################################################
        
        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        addresses = []
        accounts = []

        n_accounts = 100 # Number of accounts created
        id_names = 0
        while (id_names < n_accounts):
            id_names = id_names + 1;
            accounts.append(generate_word(5));

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        adminAddress = 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb'
        privkey = 'cTkpBcU7YzbJBi7U59whwahAMcYwKT78yjZ2zZCbLsCZ32Qp5Wta'

        self.log.info("importing admin address")
        params = str([privkey]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "importprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)
        addresses.append(adminAddress)

        self.log.info("Funding addresses with LTC")
        amount = 3.0
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)

        self.log.info("Self Attestation for addresses")
        tradelayer_selfAttestation(addresses,conn, headers)

        self.log.info("Checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        # self.log.info(out)
        
        result = []
        registers = out['result']
        
        for addr in addresses:
            for i in registers:
                if i['att sender'] == addr and i['att receiver'] == addr and i['kyc_id'] == 0:
                     result.append(True)
                     
        self.log.info("Creating new tokens  (lihki)")
        array = [0]
        params = str([addresses[0], 2, 0,"dEUR","","","1000000", array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed", params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        self.nodes[0].generate(1)
        
        self.log.info("Checking the property: dEUR")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty", params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'], 4)
        assert_equal(out['result']['name'], 'dEUR')
        assert_equal(out['result']['data'], '')
        assert_equal(out['result']['url'], '')
        assert_equal(out['result']['divisible'], True)
        assert_equal(out['result']['totaltokens'],'1000000.00000000')

        self.log.info("Checking tokens balance in dEUR's owner ")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        ######################################################################

        for id_send in range(1, n_accounts):
            self.log.info("Sending 10000 to address[%d] from address[0]"%id_send)
            params = str([addresses[0], addresses[id_send], 4, "10000"]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
            assert_equal(out['error'], None)
            # self.log.info(out)
            self.nodes[0].generate(1)

            self.log.info("Checking tokens in addresses[%d]"%id_send)
            params = str([addresses[id_send], 4]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, True, "tl_getbalance", params)
            # self.log.info(out)
            assert_equal(out['error'], None)
            assert_equal(out['result']['balance'], '10000.00000000')
            assert_equal(out['result']['reserve'], '0.00000000')        

        ####################################################################
        
        self.log.info("Creating perpetual an oracle contract")
        params = str([addresses[0], "Oracle 1", 100000000, "1", 4, "0.1", addresses[2], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_create_oraclecontract", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        self.nodes[0].generate(1)

        self.log.info("Checking the oracle contract")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty", params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'], 5)
        assert_equal(out['result']['name'],'Oracle 1')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '100000000')
        
        self.log.info("Setting oracle prices")
        params = str([addresses[0], "Oracle 1", "610.5", "602.1", "605.1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_setoracle", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        self.nodes[0].generate(1)
        time.sleep(1)

        #########################################################################
        
        self.log.info("Bid side")
        id_bid = 0
        while (id_bid < int(n_accounts/2)):
            id_bid += 1
            price_h  = str(round(round(random.uniform(20000, 40000), 1000), 8))
            amount_h = str(round(random.randrange(10, 100), 90))
            params = str([addresses[id_bid], "Oracle 1", amount_h, price_h, 1, "1"]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract", params)
            # self.log.info(out)
            assert_equal(out['error'], None)
            hash = str(out['result']).replace("'","")
            self.nodes[0].generate(1)
            
        self.log.info("Checking orderbook")
        params = str(["Oracle 1", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getcontract_orderbook", params)
        self.log.info(out)

        # while (id_bid < n_accounts):
        #     id_bid += 1
        #     price_h  = str(round(round(random.uniform(20000, 40000), 1000), 8))
        #     amount_h = str(round(random.randrange(10, 100), 90))
        #     print("id_bid : %d | price_h: %s | amount_h: %s:" %(id_bid, price_h, amount_h))
        #     params = str([addresses[id_bid], "Oracle 1", amount_h, price_h, 2, "1"]).replace("'",'"')
        #     out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract", params)
        #     # self.log.info(out)
        #     assert_equal(out['error'], None)
        #     hash = str(out['result']).replace("'","")
        #     self.nodes[0].generate(1)

        
        # self.log.info("Another address selling contracts")
        # params = str([addresses[0], "Oracle 1", "1000", "980.5", 2, "1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract", params) # Trade giving problems
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # self.nodes[0].generate(1)
    
        # self.log.info("Checking orderbook")
        # params = str(["Oracle 1", 1]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result'],[])

        # self.log.info("Another address buying contracts")
        # params = str([addresses[2], "Oracle 1", "1000", "1961", 1, "1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)

        # self.nodes[0].generate(1)

        # self.log.info("Another address selling contracts")
        # params = str([addresses[3], "Oracle 1", "1000", "1961", 2, "1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)

        # self.nodes[0].generate(1)

        # self.log.info("Putting one sell order")
        # params = str([addresses[0], "Oracle 1", "1000", "500.2", 2, "1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)

        # self.nodes[0].generate(1)

        # self.log.info("Putting one buy order")
        # params = str([addresses[1], "Oracle 1", "1000", "200.6", 1, "1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)

        # self.nodes[0].generate(1)

        # self.log.info("Putting one sell order filling the half of order")
        # params = str([addresses[2], "Oracle 1", "500", "200.6", 2, "1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)

        # self.nodes[0].generate(1)

        # self.log.info("Checking orderbook")
        # params = str(["Oracle 1", 2]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result'][0]['address'], addresses[0])
        # assert_equal(out['result'][0]['contractid'], 5)
        # assert_equal(out['result'][0]['amountforsale'], 1000)
        # assert_equal(out['result'][0]['tradingaction'], 2)
        # assert_equal(out['result'][0]['effectiveprice'], '500.20000000')

        # params = str(["Oracle 1", 1]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result'][0]['address'], addresses[1])
        # assert_equal(out['result'][0]['contractid'], 5)
        # assert_equal(out['result'][0]['amountforsale'], 500)
        # assert_equal(out['result'][0]['tradingaction'], 1)
        # assert_equal(out['result'][0]['effectiveprice'], '200.60000000')

        # self.log.info("Checking position in addresses")
        # params = str([addresses[0], "Oracle 1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, False, "tl_getposition",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result']['longPosition'], 0)
        # assert_equal(out['result']['shortPosition'], 1000)

        # params = str([addresses[1], "Oracle 1"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result']['longPosition'], 1500)
        # assert_equal(out['result']['shortPosition'], 0)

        # params = str([addresses[2], "5"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result']['longPosition'], 500)
        # assert_equal(out['result']['shortPosition'], 0)

        # params = str([addresses[3], "5"]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # # self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result']['longPosition'], 0)
        # assert_equal(out['result']['shortPosition'], 1000)
        
        # self.log.info("Mining towards settlement")
        # self.nodes[0].generate(300)
        
        # conn.close()
        # self.stop_nodes()
        

if __name__ == '__main__':
    
    OracleSettlementTest().main()
