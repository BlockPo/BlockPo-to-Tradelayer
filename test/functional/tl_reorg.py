#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test token balance after reorg."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class ReorgTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-txindex=1"],["-txindex=1"]]

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

        # mining 100 blocks
        self.nodes[0].generate(201)

        url = urllib.parse.urlparse(self.nodes[0].url)
        url2 = urllib.parse.urlparse(self.nodes[1].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "marks"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 0.2
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)

        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","3000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.log.info("Self Attestation for addresses")
        tradelayer_selfAttestation(addresses,conn, headers)

        self.sync_all()

        self.log.info("Checking the property")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'3000.00000000')

        self.log.info("Sending 2000 tokens to receiver (tl_send)")
        params = str([addresses[0], addresses[1], 4, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        # self.log.info(out)

        # Stop second node
        self.stop_node(1)

        # Mine block at height 204 on first node (including tl_send transaction)
        self.nodes[0].generatetoaddress(1, addresses[0])

        # checking last block height
        out = tradelayer_HTTP(conn, headers, True, "tl_getinfo")
        height = out['result']['block']
        # self.log.info(height)

        # Stop first node and start second node
        self.stop_node(0)
        self.start_node(1)

        # Mine block at height 204 on second node
        self.nodes[1].generatetoaddress(1, addresses[1])
        self.start_node(0)

        # Reconnect nodes and make sure they're fully sycnced
        connect_nodes(self.nodes[0], 1)

        # Make sure nodes are at the expected height and hashes differ
        assert_equal(self.nodes[0].getblockcount(), height)
        assert_equal(self.nodes[1].getblockcount(), height)

        if self.nodes[0].getblockhash(height) == self.nodes[1].getblockhash(height):
            raise AssertionError("reorg test expects block hash to differ between nodes at the same height")

        # fist node building chain with more work
        self.nodes[0].generatetoaddress(1, addresses[0])
        self.sync_all()

        # Make sure nodes are at the expected height (205) and hashes now the same
        assert_equal(self.nodes[0].getblockcount(), height + 1)
        assert_equal(self.nodes[1].getblockcount(), height + 1)
        assert_equal(self.nodes[0].getblockhash(height + 1), self.nodes[1].getblockhash(height + 1))


        url = urllib.parse.urlparse(self.nodes[0].url)
        # #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Checking balance of receiver in node 0")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        conn.close()

        url = urllib.parse.urlparse(self.nodes[1].url)
        # #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Checking balance of receiver in node 1")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    ReorgTest ().main ()
