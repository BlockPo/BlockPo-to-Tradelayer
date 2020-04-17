#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Creating fixed tokens."""

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
        accounts = ["john", "doe", "another"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating sender address")

        for i in accounts:
            # self.log.info(i)
            conn.request('POST', '/', '{"method": "getnewaddress", "params":["'+str(i)+'"]}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 200)
            input = (resp.read().decode('utf-8'))
            out = json.loads(input)
            # self.log.info(out)
            addresses.append(out['result'])


        self.log.info("Funding addresses with LTC")

        for addr in addresses:
             params = str([addr, 0.1]).replace("'",'"')
             # self.log.info(params)
             conn.request('POST', '/', '{"method": "sendtoaddress", "params":'+params+'}', headers)
             resp = conn.getresponse()
             assert_equal(resp.status, 200)
             input = (resp.read().decode('utf-8'))
             out = json.loads(input)
             # self.log.info(out)
             assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the LTC balance in every address")
        for addr in addresses:
            conn.request('POST', '/', '{"method": "getbalance", "params":["john"]}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 200)
            input = (resp.read().decode('utf-8'))
            out = json.loads(input)
            # self.log.info(out)
            assert_equal(out['error'], None)
            assert_equal(out['result'],0.1)


        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","3000",array]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_sendissuancefixed", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        assert_equal(out['error'], None)
        # self.log.info(out)



        self.log.info("Self Attestation for addresses")
        for addr in addresses:
            params = str([addr,addr,""]).replace("'",'"')
            # self.log.info(params)
            conn.request('POST', '/', '{"method": "tl_attestation", "params":'+params+'}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 200)
            input = (resp.read().decode('utf-8'))
            out = json.loads(input)
            assert_equal(out['error'], None)
            # self.log.info(out)

        self.nodes[0].generate(1)

        # TODO:
        # self.log.info("Checking attestations")

        self.log.info("Checking the property")
        conn.request('POST', '/', '{"method": "tl_getproperty", "params": [4]}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'3000.00000000')

        self.log.info("Checking token balance in every address")
        for addr in addresses:
            params = str([addr, 4]).replace("'",'"')
            conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 200)
            input = (resp.read().decode('utf-8'))
            out = json.loads(input)
            # self.log.info(out)
            amount = ""
            assert_equal(out['error'], None)
            if addr == addresses[0]:
                 amount = '3000.00000000'
            else:
                 amount = '0.00000000'

            assert_equal(out['result']['balance'], amount)
            assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 2000 tokens to receiver (tl_send)")
        params = str([addresses[0], addresses[1], 4, "2000"]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_send", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 4]).replace("'",'"')
        conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 1000 tokens to from receiver to last address (using tl_send)")
        params = str([addresses[1], addresses[2], 4,"1000"]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_send", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address and last address")
        for i in range(2):
            params = str([addresses[i], 4]).replace("'",'"')
            # self.log.info(params)
            conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
            resp = conn.getresponse()
            assert_equal(resp.status, 200)
            input = (resp.read().decode('utf-8'))
            out = json.loads(input)
            # self.log.info(out)
            assert_equal(out['error'], None)
            assert_equal(out['result']['balance'],'1000.00000000')
            assert_equal(out['result']['reserve'],'0.00000000')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    HTTPBasicsTest ().main ()
