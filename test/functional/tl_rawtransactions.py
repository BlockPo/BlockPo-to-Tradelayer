#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Raw transactions for Channels."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class RawTransactionBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # Use self.extra_args to change command-line arguments for the nodes
        # self.extra_args = [["-deprecatedrpc=signrawtransaction"]]
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
        # Checking all tl rpcs related with tradechannels (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another", "marks"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Getting private keys")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "dumpprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        privatekey0 = out['result']

        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "dumpprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        privatekey1 = out['result']


        self.log.info("Creating multisig address")
        params = str([2, [addresses[0], addresses[1]]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "addmultisigaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        multisig = out['result']['address']
        redeemScript = out['result']['redeemScript']
        self.log.info("multisig:"+multisig)
        self.log.info("address0:"+addresses[0])
        self.log.info("address1:"+addresses[1])

        self.log.info("Checking multisig")
        params = str([multisig]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "validateaddress",params)
        # self.log.info(out)
        scriptPubKey = out['result']['scriptPubKey']
        assert_equal(out['result']['embedded']['script'],'multisig')


        self.log.info("Funding addresses with LTC")
        amount = 0.5
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        params = str([addresses[1], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress", params)
        # self.log.info(out)

        # NOTE: this is important
        txid = out['result']
        self.log.info("txid:"+txid)

        self.log.info("Checking the transaction")
        params = str([txid]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "gettransaction", params)
        self.log.info(out)
        vout = out['result']['details'][0]['vout']
        self.log.info('vout:'+str(vout))

        self.nodes[0].generate(1)

        # self.log.info("Creating rawtx transaction")
        # params = '[[{"txid":"'+txid+'","vout":'+str(vout1)+'}],{"'+multisig+'":0.98, "'+multisig1+'":0.01}, 0, true]'
        # out = tradelayer_HTTP(conn, headers, False, "createrawtransaction", params)
        # assert_equal(out['error'], None)
        # self.log.info(out)
        # hex = out['result']

        self.log.info("Creating raw input")
        params = str(['', txid, vout]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_input",params)
        self.log.info(out)

        self.stop_nodes()

if __name__ == '__main__':
    RawTransactionBasicsTest ().main ()
