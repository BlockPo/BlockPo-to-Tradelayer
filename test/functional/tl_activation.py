#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test basic for Features activation ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class ActivationBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-txindex=1", "-tlactivationallowsender=QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb"]]

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
        adminAddress = 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb'
        privkey = 'cTkpBcU7YzbJBi7U59whwahAMcYwKT78yjZ2zZCbLsCZ32Qp5Wta'

        self.log.info("importing admin address")
        params = str([privkey]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "importprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)


        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)
        addresses.append(adminAddress)


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

        assert_equal(result, [True, True, True, True])

        # deactivation here to write 999999999 in the MSC_SP_BLOCK param
        params = str([adminAddress, 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddeactivation",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Creating new tokens (must be rejected)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","3000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the property (doesn't exist)")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['error']['message'], 'Property identifier does not exist')


        self.log.info("Testing tl_sendactivation")

        # adminAddress, activation number 1, in block 400, minim tl version = 1.
        params = str([adminAddress, 1, 400, 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendactivation",params)
        # self.log.info(out)

        self.nodes[0].generate(210)

        self.log.info("Creating new tokens")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","3000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

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

        self.log.info("Testing tl_senddeactivation")

        # adminAddress, activation number 1, in block 400, minim tl version = 1.
        params = str([adminAddress, 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddeactivation",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Creating new tokens (must be rejected)")
        array = [0]
        params = str([addresses[0],2,0,"dan","","","4000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the property (doesn't exist)")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['error']['message'], 'Property identifier does not exist')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    ActivationBasicsTest ().main ()
