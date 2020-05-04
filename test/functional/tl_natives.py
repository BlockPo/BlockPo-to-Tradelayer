#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test ContractDEx functions (natives)."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class NativesBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

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

        # TODO:
        # self.log.info("Checking attestations")


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


        self.log.info("Creating native Contract")
        array = [0]
        params = str([addresses[0], 1, 4, "ALL/Lhk", 5000, "1", 4, "0.1", 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_createcontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the native contract")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['name'],'ALL/Lhk')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '5000')
        assert_equal(out['result']['inverse quoted'], '0')


        #NOTE: we need to test this for all leverages
        self.log.info("Buying contracts")
        params = str([addresses[1], "ALL/Lhk", "1000", "780.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")
        # self.log.info(hash)

        self.nodes[0].generate(1)


        self.log.info("Checking colateral balance now in sender address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'49900.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 5)
        assert_equal(out['result'][0]['amountforsale'], '1000.00000000')
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '780.50000000')
        assert_equal(out['result'][0]['block'], 206)


        self.log.info("Canceling Contract order")
        address = '"'+addresses[1]+'"'
        params = str([addresses[1], hash]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendcancel_contract_order",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])

        #NOTE: we need to test this for all leverages
        self.log.info("Checking restored colateral in sender address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'50000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Buying contracts again")
        params = str([addresses[1], "ALL/Lhk", "1000", "980.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")

        self.nodes[0].generate(1)

        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 5)
        assert_equal(out['result'][0]['amountforsale'], '1000.00000000')
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '980.50000000')
        # assert_equal(out['result'][0]['block'], 206)


        self.log.info("Another address selling contracts")
        params = str([addresses[0], "ALL/Lhk", "1000", "980.5", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])


        self.log.info("Checking position in first address")
        params = str([addresses[1], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'],'1000.00000000')
        assert_equal(out['result']['shortPosition'],'0.00000000')


        self.log.info("Checking position in second address")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['longPosition'],'0.00000000')
        assert_equal(out['result']['shortPosition'],'1000.00000000')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    NativesBasicsTest ().main ()
