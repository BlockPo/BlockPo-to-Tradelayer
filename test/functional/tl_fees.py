#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Fees."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse
import time

class FeesBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # Use self.extra_args to change command-line arguments for the nodes
        self.extra_args = [["-txindex=1"]]  # NOTE: EXTREMELY IMPORTANT in order to use the persistence!

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
        # Checking the persistence in db (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)


        self.log.info("Funding addresses with LTC")
        amount = 1.1
        tradelayer_fundingAddresses(addresses, amount, conn, headers)


        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating new tokens  (lihki)")
        array = [0,1]
        params = str([addresses[0],2,0,"lihki","","","20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Creating new tokens  (dan)")
        array = [0,1]
        params = str([addresses[0], 2, 0, "dan", "", "", "20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the property: lihki")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['issuer'],addresses[0])
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'20000000000.00000000')


        self.log.info("Checking tokens in issuer address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'20000000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking the property: lihki")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'], 5)
        assert_equal(out['result']['issuer'],addresses[0])
        assert_equal(out['result']['name'],'dan')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'20000000000.00000000')


        self.log.info("Checking tokens in issuer address")
        params = str([addresses[0], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'20000000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


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

        self.log.info("Sending 2000 tokens to second address")
        params = str([addresses[0], addresses[1], 4, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking lihki tokens balance in lihki's owner ")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        oldbalance = float(out['result']['balance'])
        assert_equal(out['result']['balance'],'19999998000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking lihki tokens balance second address ")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')
        oldbalance1 = float(out['result']['balance'])

        self.log.info("Creating native Contract")
        array = [0]
        params = str([addresses[0], 1, 4, "ALL/Lhk", 1000, "1", 4, "0.1", 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_createcontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the native contract")
        params = str([6])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'], 6)
        assert_equal(out['result']['name'],'ALL/Lhk')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '1000')
        assert_equal(out['result']['inverse quoted'], '0')

        params = str([4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getcache",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['amount'], '0.00000000')

        self.log.info("Trading contract")
        params = str([addresses[0], "ALL/Lhk", "1000", "400.1", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking balance and reserve ")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        balance = float(out['result']['balance'])
        reserve = float(out['result']['reserve'])
        assert_equal(out['result']['balance'],'19999997900.00000000')
        assert_equal(out['result']['reserve'],'100.00000000')
        assert_equal(oldbalance, balance + reserve)

        self.log.info("Trading contract")
        params = str([addresses[1], "ALL/Lhk", "1000", "400.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        balance = float(out['result']['balance'])
        reserve = float(out['result']['reserve'])
        assert_equal(out['result']['balance'],'19999997900.00500000')
        assert_equal(out['result']['reserve'],'100.00000000')

        # 0.5 basis point
        rebate = 0.00005 * reserve
        assert_equal(oldbalance , balance - rebate + reserve)


        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        balance1 = float(out['result']['balance'])
        reserve1 = float(out['result']['reserve'])
        assert_equal(out['result']['balance'],'1900.00000000')
        assert_equal(out['result']['reserve'],'99.99000000')

        # 1 basis point
        fee = 0.0001 * reserve
        assert_equal(oldbalance1 , balance1 + fee + reserve1)


        params = str([4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getcache",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['amount'], '0.00500000')

        # checking for different leverages

        # then check in the DEx1

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    FeesBasicsTest ().main ()
