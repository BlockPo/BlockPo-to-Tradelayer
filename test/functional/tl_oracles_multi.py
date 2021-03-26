#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test ContractDEx oracles (2 different contracts)."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class OraclesMultiBasicsTest (BitcoinTestFramework):
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
        # mining 200 blocks
        self.nodes[0].generate(200)

        ################################################################################
        # Checking RPC tl_sendtrade (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 1.1
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating new tokens  (lihki)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","100000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


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

        assert_equal(result, [True, True, True])

        self.log.info("Checking the property: lihki")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'100000.00000000')


        self.log.info("Checking tokens balance in lihki's owner ")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'100000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 50000 tokens to second address")
        params = str([addresses[0], addresses[1], 4, "50000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'50000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Creating oracles Contract")
        array = [0]
        params = str([addresses[0], "Oracle 1", 10000, "1", 4, "0.1", addresses[2], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the oracle contract")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['name'],'Oracle 1')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '0')
        assert_equal(out['result']['low price'], '0')
        assert_equal(out['result']['last close price'], '0')


        self.log.info("Creating oracles Contract")
        array = [0]
        params = str([addresses[0], "Oracle 2", 10000, "1", 4, "0.1", addresses[2], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the oracle contract")
        params = str([6])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'], 6)
        assert_equal(out['result']['name'],'Oracle 2')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '0')
        assert_equal(out['result']['low price'], '0')
        assert_equal(out['result']['last close price'], '0')


        self.log.info("Setting oracle prices")
        params = str([addresses[0], "Oracle 1", "602.1", "450.6", "500.1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_setoracle",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        params = str([addresses[0], "Oracle 2", "502.1", "350.6", "400.1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_setoracle",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking the prices in oracle")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['hight price'], '602.1')
        assert_equal(out['result']['low price'], '450.6')
        assert_equal(out['result']['last close price'], '500.1')

        params = str([6])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['hight price'], '502.1')
        assert_equal(out['result']['low price'], '350.6')
        assert_equal(out['result']['last close price'], '400.1')


        #NOTE: we need to test this for all leverages
        self.log.info("Buying contracts")
        params = str([addresses[1], "Oracle 1", "1000", "780.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")
        # self.log.info(hash)

        self.nodes[0].generate(1)

        params = str([addresses[1], "Oracle 2", "2000", "680.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")
        # self.log.info(hash)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook")
        params = str(["Oracle 1", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 5)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '780.50000000')
        # assert_equal(out['result'][0]['block'], 209)


        params = str(["Oracle 2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 6)
        assert_equal(out['result'][0]['amountforsale'], 2000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '680.50000000')
        assert_equal(out['result'][0]['block'], 210)


        self.log.info("Another address selling contracts")
        params = str([addresses[0], "Oracle 1", "1000", "780.5", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)

        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        params = str([addresses[0], "Oracle 2", "2000", "680.5", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)

        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook")
        params = str(["Oracle 1", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])


        params = str(["Oracle 2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])

        self.log.info("Checking position in first address")
        params = str([addresses[1], "Oracle 1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'], 1000)
        assert_equal(out['result']['shortPosition'], 0)


        params = str([addresses[1], "Oracle 2"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'], 2000)
        assert_equal(out['result']['shortPosition'], 0)


        self.log.info("Checking position in second address")
        params = str([addresses[0], "Oracle 1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'], 0)
        assert_equal(out['result']['shortPosition'], 1000)

        params = str([addresses[0], "Oracle 2"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'], 0)
        assert_equal(out['result']['shortPosition'], 2000)


        self.log.info("Checking the open interest")
        params = str(["Oracle 1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 1000)



        params = str([addresses[0], "Oracle 2", "500", "480.5", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)

        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        params = str([addresses[1], "Oracle 2", "500", "480.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)

        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        params = str(["Oracle 2"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 2500)

        self.log.info("Checking position in first address")
        params = str([addresses[1], "Oracle 2"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'], 2500)
        assert_equal(out['result']['shortPosition'], 0)


        self.log.info("Checking position in second address")
        params = str([addresses[0], "Oracle 2"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'], 0)
        assert_equal(out['result']['shortPosition'], 2500)


        out = tradelayer_HTTP(conn, headers, True, "tl_getinfo")
        # self.log.info(out)
        assert_equal(out['error'], None)
        # assert_equal(out['result']['block'], 212)


        # mining 100 blocks towards settlement
        self.nodes[0].generate(100)

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    OraclesMultiBasicsTest ().main ()
