#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Vesting tokens."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class VestingBasicsTest (BitcoinTestFramework):
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
        # Checking RPC tl_sendissuancefixed and tl_send (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        adminAddress = 'mgrNNyDCdAWeYfkvcarsQKRzMhEFQiDmn'
        privkey = 'cRs6niSYMs5pSpHT6kB7V1Urj3zqyE9sJwUiQYorfini1kw11C8z'

        self.log.info("importing admin address")
        params = str([privkey]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "importprivkey",params)
        self.log.info(out)
        assert_equal(out['error'], None)

        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        addresses.append(adminAddress)

        self.log.info(addresses)

        assert(False)

        self.log.info("Funding addresses with LTC")
        amount = 0.1
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","3000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.log.info("Self Attestation for addresses")
        tradelayer_selfAttestation(addresses,conn, headers)

        # TODO:
        # self.log.info("Checking attestations")

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

        self.log.info("sendvesting from  adminAddress to first address")
        params = str([adminAddress, addresses[0], 3, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendvesting",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')



        self.log.info("Checking the time lock of 1 year")
        self.log.info("sendvesting from first to second address")
        params = str([addresses[1], addresses[2], 3, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendvesting",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')
        
        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    VestingBasicsTest ().main ()
