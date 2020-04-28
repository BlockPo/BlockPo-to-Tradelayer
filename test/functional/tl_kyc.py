#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test KYC and Attestation."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class KYCBasicsTest (BitcoinTestFramework):
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
        accounts = ["john", "doe", "another", "mark"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        # self.log.info(addresses)


        self.log.info("Funding addresses with LTC")
        amount = 10
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating KYC of Tradelayer ")
        params = str([addresses[0], "TradeLayer.org","TradeLayer registrars"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_new_id_registration",params)
        self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the KYC ")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_kyc",params)
        self.log.info(out)

        assert_equal(out['result']['result: '],'enabled')

        self.log.info("Checking the KYC list ")
        out = tradelayer_HTTP(conn, headers, False, "tl_listkyc")
        # self.log.info(out)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['name'], 'TradeLayer registrars')
        assert_equal(out['result'][0]['website'], 'TradeLayer.org')
        assert_equal(out['result'][0]['block'], 202)
        assert_equal(out['result'][0]['kyc id'], 1)


        self.log.info("Creating another KYC for Tradelayer ")
        params = str([addresses[1], "TradeLayer.org","TradeLayer registrars"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_new_id_registration",params)
        self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the KYC ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_kyc",params)
        self.log.info(out)

        assert_equal(out['result']['result: '],'enabled')

        self.log.info("Checking the KYC list ")
        out = tradelayer_HTTP(conn, headers, False, "tl_listkyc")
        # self.log.info(out)

        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['name'], 'TradeLayer registrars')
        assert_equal(out['result'][0]['website'], 'TradeLayer.org')
        assert_equal(out['result'][0]['block'], 203)
        assert_equal(out['result'][0]['kyc id'], 2)

        assert(False)
        
        # self.log.info("Creating new tokens (sendissuancefixed)")
        # array = [0]
        # params = str([addresses[2],2,0,"lihki","","","90000000",array]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # # self.log.info(out)

        # self.log.info("Self Attestation for addresses")
        # tradelayer_selfAttestation(addresses,conn, headers)

        # TODO:
        # self.log.info("Checking attestations")


        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    KYCBasicsTest ().main ()
