#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Consensus Hash ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class ConsensusTest (BitcoinTestFramework):
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

    # NOTE: this is the basis, of course, we can build more objects and classes
    # in order to do a better work

    def run_test(self):

        self.log.info("Preparing the workspace...")

        # mining 200 blocks
        self.nodes[0].generate(200)

        ################################################################################
        # Checking RPC tl_senddexoffer and tl_getactivedexsells (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = ["mvUXLCy5nKnMtrfYwbZRSvuKyBRFEkZdJg", "mvLbyaSrZg8hBPg1oxnNAYApbY825DfPgA", "mrkjviVhf9FSCpto1nu63ByxgQswqyE6Aw", "mypj6fL9AXLgNwjyqPYLkXRmrxV9LSuZr5", "QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb"]
        privkeys = ["cMw2dpyVeiQbpiTxoqCzLHFYJKwyLMRBanawQ1QnEYq4yom2RMfd", "cTCxScsXSNdP4tCiidHBtQvStnAjmaV299Bqx6qJfpuG7xdNwL9s", "cQuQra5K7b4Rd8zWbQ4ncooEoT3ZuZWMusuhGazLX1s5cEy41tuK", "cUZ8sikuPdycEnVudPPjybi8zBSPw4HMDh8c7Lhtv614bxsoq1eb", "cTkpBcU7YzbJBi7U59whwahAMcYwKT78yjZ2zZCbLsCZ32Qp5Wta"]


        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("importing privkeys of addresses")
        for priv in privkeys:
            params = str([priv]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, True, "importprivkey",params)
            # self.log.info(out)
            assert_equal(out['error'], None)

        self.log.info("Funding addresses with LTC")
        amount = 3
        tradelayer_fundingAddresses(addresses, amount, conn, headers)


        self.log.info("Checking consensus hash")
        out = tradelayer_HTTP(conn, headers, False, "tl_getcurrentconsensushash")
        # self.log.info(out)
        assert_equal (out['result']['consensushash'],"3746ff2e2cbbbeaf593fd79109cf7076ce78934cc69cf5d285b299c50baad053")

        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[0], 2, 0,"lihki","","","90000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking consensus hash")
        out = tradelayer_HTTP(conn, headers, False, "tl_getcurrentconsensushash")
        # self.log.info(out)
        assert_equal (out['result']['consensushash'], "ea2ab2227852b38f726a57157c093d939ec75ca50db7b29fa6fea8002ba5c09c")


        self.log.info("Self Attestation for addresses")
        for addr in addresses:
            params = str([addr,addr,""]).replace("'",'"')
            tradelayer_HTTP(conn, headers, False, "tl_attestation", params)
            self.nodes[0].generate(1)

        self.log.info("Checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        # self.log.info(out)

        result = []
        registers = out['result']

        for addr in addresses:
            for i in registers:
                if i['att sender'] == addr and i['att receiver'] == addr and i['kyc_id'] == 0:
                     result.append(True)

        assert_equal(result, [True, True, True, True, True])

        self.log.info("Checking the property lihki")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'90000000.00000000')

        self.log.info("Checking consensus hash")
        out = tradelayer_HTTP(conn, headers, False, "tl_getcurrentconsensushash")
        # self.log.info(out)
        assert_equal (out['result']['consensushash'],"1df8801c4473825c803a1f26be16fdabd12d522fd07b7004a0d95fbb3501daa3")


        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[1], 2, 0,"dan","","","123456789",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the property dan")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        assert_equal(out['error'], None)
        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['name'],'dan')

        self.log.info("Checking consensus hash")
        out = tradelayer_HTTP(conn, headers, False, "tl_getcurrentconsensushash")
        # self.log.info(out)
        assert_equal (out['result']['consensushash'],"4e0ef1396e76d1aab2f7eeb6f6bfd4aab29d7cb796f155dfe805faa2351a8d9d")
        
        conn.close()
        self.stop_nodes()

if __name__ == '__main__':
    ConsensusTest ().main ()
