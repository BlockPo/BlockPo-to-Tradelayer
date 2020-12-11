#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test input cache."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import math
import http.client
import urllib.parse

class InputCacheTest (BitcoinTestFramework):
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

        self.nodes[0].generate(201)

        ################################################################################
        # Checking RPC tl_sendvesting and related (starting in block 1000)             #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another", "mark", "tango"]

        # for graphs for addresses[1]
        vested = []
        unvested = []
        volume_ltc = []

        #vested ALL for addresses[4]
        bvested = []

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("watching LTC general balance")
        params = str([""]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getbalance",params)
        self.log.info(out)
        assert_equal(out['error'], None)

        adminAddress = 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb'
        privkey = 'cTkpBcU7YzbJBi7U59whwahAMcYwKT78yjZ2zZCbLsCZ32Qp5Wta'


        self.log.info("importing admin address")
        params = str([privkey]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "importprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)


        self.log.info("watching private key of admin address")
        params = str([adminAddress]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "dumpprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)


        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        addresses.append(adminAddress)
        # self.log.info(addresses)


        self.log.info("Funding addresses with LTC")
        amount = 5
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.nodes[0].generate(1)


        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[2],2,0,"lihki","","","90000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.log.info("Self Attestation for addresses")
        tradelayer_selfAttestation(addresses,conn, headers)

        self.log.info("Checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        # self.log.info(out)

        self.log.info("Checking vesting tokens property")
        params = str([3])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],3)
        assert_equal(out['result']['name'],'Vesting Tokens')
        assert_equal(out['result']['data'],'Divisible Tokens')
        assert_equal(out['result']['url'],'www.tradelayer.org')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'1500000.00000000')


        self.log.info("Checking the property")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'90000000.00000000')

        for i in range (0, 600):
            self.log.info("sending tokens from  second to first address")
            params = str([addresses[2], addresses[0], 4, "1"]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, False, "tl_send",params)
            self.log.info(out)

            txid = out['result']

            assert_equal(out['error'], None)

            self.nodes[0].generate(1)

            self.log.info("Checking transaction invalidation")
            params = str([txid]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, False, "tl_gettransaction", params)
            self.log.info(out)

            params = str(["another"]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, False, "getbalance", params)
            self.log.info(out)

            self.log.info("-----------------------------------------")


        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    InputCacheTest ().main ()
