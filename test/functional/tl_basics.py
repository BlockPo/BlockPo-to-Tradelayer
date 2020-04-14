#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test basic data RPCs ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import str_to_b64str, assert_equal

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

        self.log.info("Testing tl_getinfo")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getinfo"}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        assert_equal(resp.status, 200)
        assert_equal(out['result']['tradelayer_version_int'], 4000)
        assert_equal(out['result']['tradelayer_coreversion'], "0.0.4")
        assert_equal(out['result']['litecoinversion'], "0.16.3")
        assert_equal(out['result']['blocktransactions'], 0)
        assert_equal(out['result']['block'], 100)


        self.log.info("Testing tl_listproperties")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_listproperties"}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(resp.status, 200)

        # Checking the first property in the list
        assert_equal(out['result'][0]['propertyid'], 1)
        assert_equal(out['result'][0]['name'], "ALL")
        assert_equal(out['result'][0]['data'], "")
        assert_equal(out['result'][0]['url'], "")
        assert_equal(out['result'][0]['divisible'], True)
        assert_equal(out['result'][0]['category'], "N/A")
        assert_equal(out['result'][0]['subcategory'], "N/A")


        self.log.info("Testing tl_getbalance")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getbalance", "params": ["mgrNNyDCdAWeYfkvcarsQKRzMhEFQiDmnH", 1]}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(resp.status, 200)
        assert_equal(out['result']['balance'],'1500000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Testing tl_getallbalancesforid")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getallbalancesforid", "params": [3]}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(resp.status, 200)
        assert_equal(out['result'][0]['address'],'mgrNNyDCdAWeYfkvcarsQKRzMhEFQiDmnH')
        assert_equal(out['result'][0]['balance'],'1500000.00000000')
        assert_equal(out['result'][0]['reserve'],'0.00000000')


        self.log.info("Testing tl_getallprice")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getallprice"}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['result']['unitprice'],'0.00000000')


        self.log.info("Testing tl_getdexvolume")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getdexvolume", "params": [1,1,100]}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['result']['volume'],'0.00000000')
        assert_equal(out['result']['blockheigh'],'100')


        self.log.info("Testing tl_getmdexvolume")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getmdexvolume", "params": [1,2,1,100]}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['result']['volume'],'0.00000000')
        assert_equal(out['result']['blockheigh'],'100')


        self.log.info("Testing tl_getproperty")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getproperty", "params": [1]}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],1)
        assert_equal(out['result']['name'],'ALL')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'1500000.00000000')

        conn.request('POST', '/', '{"method": "tl_getproperty", "params": [3]}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],3)
        assert_equal(out['result']['name'],'Vesting Tokens')
        assert_equal(out['result']['data'],'Divisible Tokens')
        assert_equal(out['result']['url'],'www.tradelayer.org')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'1500000.00000000')
        assert_equal(out['result']['creation block'],100)


        self.log.info("Testing tl_getvesting_supply")
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "tl_getvesting_supply"}', headers)
        resp = conn.getresponse()
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['result']['supply'],'0.00000000')
        assert_equal(out['result']['blockheight'],'100')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    HTTPBasicsTest ().main ()
