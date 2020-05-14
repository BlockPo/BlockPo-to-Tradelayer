#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test basic for Creating tokens ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class ManagedBasicsTest (BitcoinTestFramework):
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
        # Checking RPC tl_sendissuancemanaged and tl_sendgrant (in the first 200 blocks of the chain) #
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
        amount = 0.1
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

        assert_equal(result, [True, True, True])

        self.log.info("Creating new tokens (sendissuancemanaged)")
        array = [0]
        params = str([addresses[0], 2, 0, "lihki", "", "", array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancemanaged",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the property")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'0.00000000')


        self.log.info("Checking token balance equal zero in every address")
        for addr in addresses:
            params = str([addr, 4]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
            # self.log.info(out)
            assert_equal(out['error'], None)
            assert_equal(out['result']['balance'],'0.00000000')
            assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 2000 tokens to receiver (sendgrant)")
        params = str([addresses[0], addresses[1], 4, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendgrant",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 1000 tokens to from receiver to last address (using tl_send)")
        params = str([addresses[1], addresses[2], 4,"1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address and last address")
        for i in range(2,3):
            params = str([addresses[i], 4]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
            # self.log.info(out)
            assert_equal(out['error'], None)
            assert_equal(out['result']['balance'],'1000.00000000')
            assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Trying to change the issuer")
        params = str([addresses[0], addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendchangeissuer",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the property (with new issuer)")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['issuer'], addresses[1])
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'2000.00000000')


        self.log.info("Sending 264 tokens from issuer to himself (tl_sendgrant)")
        params = str([addresses[1], addresses[1], 4, "264"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendgrant",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking issuer's balance")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1264.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 888 tokens from issuer to himself (tl_send)")
        params = str([addresses[1], addresses[1], 4, "888"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        # self.log.info(out)
        assert_equal(out['error']['message'], 'Property identifier does not refer to a managed property')

        self.nodes[0].generate(1)


        self.log.info("Checking issuer's balance now")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1264.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    ManagedBasicsTest ().main ()
