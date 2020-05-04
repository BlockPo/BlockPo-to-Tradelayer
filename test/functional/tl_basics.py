#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test basic data RPCs ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import str_to_b64str, assert_equal, tradelayer_HTTP

import os
import json
import http.client
import urllib.parse

class HTTPBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def setup_chain(self):
        super().setup_chain()
        #Append rpcauth to bitcoin.conf before initialization
        rpcauth = "rpcauth=rt:93648e835a54c573682c2eb19f882535$7681e9c5b74bdd85e78166031d2058e1069b3ed7ed967c93fc63abba06f31144"
        # rpcauth2 = "rpcauth=rt2:f8607b1a88861fac29dfccf9b52ff9f$ff36a0c23c8c62b4846112e50fa888416e94c17bfd4c42f88fd8f55ec6a3137e"
        rpcuser = "rpcuser=rpcuser💻"
        rpcpassword = "rpcpassword=rpcpassword🔑"
        with open(os.path.join(self.options.tmpdir+"/node0", "litecoin.conf"), 'a', encoding='utf8') as f:
            f.write(rpcauth+"\n")

    def run_test(self):

        # mining 100 blocks
        self.nodes[0].generate(100)

        ################################################################################
        # Checking RPC calls for data retrieval (in the first 100 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)


        self.log.info("Testing tl_getinfo")
        out = tradelayer_HTTP(conn, headers, True, "tl_getinfo")
        assert_equal(out['result']['tradelayer_version_int'], 4000)
        assert_equal(out['result']['tradelayer_coreversion'], "0.0.4")
        assert_equal(out['result']['litecoinversion'], "0.16.3")
        assert_equal(out['result']['blocktransactions'], 0)
        assert_equal(out['result']['block'], 100)


        self.log.info("Testing tl_listproperties")
        out = tradelayer_HTTP(conn, headers, True, "tl_listproperties")
        # self.log.info(out)

        # Checking the first property in the list
        assert_equal(out['result'][0]['propertyid'], 1)
        assert_equal(out['result'][0]['name'], "ALL")
        assert_equal(out['result'][0]['data'], "")
        assert_equal(out['result'][0]['url'], "")
        assert_equal(out['result'][0]['divisible'], True)
        assert_equal(out['result'][0]['category'], "N/A")
        assert_equal(out['result'][0]['subcategory'], "N/A")


        self.log.info("Testing tl_getbalance")
        address = 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb'
        params = str([address, 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['result']['balance'],'1500000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Testing tl_getallbalancesforid")
        params = str([3])
        out = tradelayer_HTTP(conn, headers, True, "tl_getallbalancesforid",params)
        # self.log.info(out)
        assert_equal(out['result'][0]['address'], address)
        assert_equal(out['result'][0]['balance'],'1500000.00000000')
        assert_equal(out['result'][0]['reserve'],'0.00000000')


        self.log.info("Testing tl_getallprice")
        out = tradelayer_HTTP(conn, headers, True, "tl_getallprice",params)
        # self.log.info(out)
        assert_equal(out['result']['unitprice'],'0.00000000')


        self.log.info("Testing tl_getdexvolume")
        params = str([1,1,100])
        out = tradelayer_HTTP(conn, headers, True, "tl_getdexvolume",params)
        # self.log.info(out)
        assert_equal(out['result']['volume'],'0.00000000')
        assert_equal(out['result']['blockheigh'],'100')


        self.log.info("Testing tl_getmdexvolume")
        params = str([1,2,1,100])
        out = tradelayer_HTTP(conn, headers, True, "tl_getmdexvolume",params)
        # self.log.info(out)
        assert_equal(out['result']['volume'],'0.00000000')
        assert_equal(out['result']['blockheigh'],'100')


        self.log.info("Testing tl_getproperty")
        params = str([1])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],1)
        assert_equal(out['result']['name'],'ALL')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'1500000.00000000')

        params = str([3])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],3)
        assert_equal(out['result']['name'],'Vesting Tokens')
        assert_equal(out['result']['data'],'Divisible Tokens')
        assert_equal(out['result']['url'],'www.tradelayer.org')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'1500000.00000000')
        assert_equal(out['result']['creation block'],100)


        self.log.info("Testing tl_getvesting_supply")
        out = tradelayer_HTTP(conn, headers, True, "tl_getvesting_supply")
        # self.log.info(out)
        assert_equal(out['result']['supply'],'0.00000000')
        assert_equal(out['result']['blockheight'],'100')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    HTTPBasicsTest ().main ()
