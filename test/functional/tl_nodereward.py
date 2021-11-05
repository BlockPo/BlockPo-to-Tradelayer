#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test basic for Node Reward ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class NodeRewardTest (BitcoinTestFramework):
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

        # mining 200 blocks
        for i in range(0, 200):
            self.nodes[0].generate(1)
            # self.log.info("checking next reward")
            # out = tradelayer_HTTP(conn, headers, True, "tl_getnextreward")
            # self.log.info(out)

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


        self.log.info("Sending node reward address")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_submit_nodeaddress",params)
        self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Listing all node reward address")
        out = tradelayer_HTTP(conn, headers, True, "tl_listnodereward_address")
        self.log.info(out)

        self.log.info("Sending node reward address")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_claim_nodereward",params)
        self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Listing all node reward address")
        out = tradelayer_HTTP(conn, headers, True, "tl_listnodereward_address")
        self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Listing all node reward address")
        out = tradelayer_HTTP(conn, headers, True, "tl_listnodereward_address")
        self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("checking next reward")
        out = tradelayer_HTTP(conn, headers, True, "tl_getnextreward")
        self.log.info(out)

        self.log.info("get ALL balance in address0")
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getbalance",params)
        self.log.info(out)

        for i in range(0, 300):
            self.nodes[0].generate(1)
            self.log.info("checking next reward")
            out = tradelayer_HTTP(conn, headers, True, "tl_getnextreward")
            self.log.info(out)

        self.log.info("get ALL balance in address0 again")
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getbalance",params)
        self.log.info(out)

        conn.close()
        self.stop_nodes()

if __name__ == '__main__':
    NodeRewardTest().main ()
