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
            f.write(rpcuser+"\n")
            f.write(rpcpassword+"\n")

    def run_test(self):

        self.log.info("Preparing the workspace...")

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        # mining 200 blocks
        for i in range(0, 20):
            self.nodes[0].generate(1)
            # self.log.info("checking next reward")
            # out = tradelayer_HTTP(conn, headers, True, "tl_getnextreward")
            # self.log.info(out)

        self.log.info("setting consensushash:")
        consensusHash = "cfc1a5fe9c43f36a468d9267a499171fb14710be4d4284a700b99dec276a"
        self.log.info(consensusHash)


        while True:
            out = tradelayer_HTTP(conn, headers, True, "getnewaddress")
            address = out['result']
            self.log.info("checking if address matches with consensushash")
            params = str([address, consensusHash]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, False, "tl_isaddresswinner", params)
            # self.log.info(out)
            if out['result']['result'] == "true":
                self.log.info("winner address found!!!:"+ address)
                break


        conn.close()
        self.stop_nodes()

if __name__ == '__main__':
    NodeRewardTest().main ()
