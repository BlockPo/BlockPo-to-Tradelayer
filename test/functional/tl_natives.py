#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test ContractDEx functions (natives)."""

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
        # Checking RPC tl_sendtrade (in the first 200 blocks of the chain) #
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
             params = str([addr, 1.1]).replace("'",'"')
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
            assert_equal(out['result'],1.1)


        self.log.info("Creating new tokens  (lihki)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","100000",array]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_sendissuancefixed", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


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

        self.log.info("Checking the property: lihki")
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
        assert_equal(out['result']['totaltokens'],'100000.00000000')


        self.log.info("Checking tokens balance in lihki's owner ")
        params = str([addresses[0], 4]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'100000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')



        self.log.info("Creating native Contract")
        array = [0]
        params = str([addresses[0], 1, 4, "ALL/Lhk", 5000, "1", 4, "0.1", 1, array]).replace("'",'"')
        self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_createcontract", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking the native contract")
        conn.request('POST', '/', '{"method": "tl_getproperty", "params": [5]}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        assert_equal(out['error'], None)
        self.log.info(out)
        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['name'],'ALL/Lhk')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], 4)
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], 5000)
        assert_equal(out['result']['inverse quoted'], 1)

        assert(False)


        self.log.info("Checking the trade in orderbook")
        params = str([4])
        conn.request('POST', '/', '{"method": "tl_getorderbook", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['propertyidforsale'],4)
        assert_equal(out['result'][0]['amountforsale'],'500.00000000')
        assert_equal(out['result'][0]['propertyiddesired'],5)
        assert_equal(out['result'][0]['amountdesired'],'2000.00000000')


        self.log.info("Cancelling all trades in MetaDEx")
        params = str([addresses[0]]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_sendcancelalltrades", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trades in orderbook")
        params = str([4])
        conn.request('POST', '/', '{"method": "tl_getorderbook", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])

        self.log.info("Sending a new  trade in MetaDEx")
        params = str([addresses[0], 4, "1000", 5, "2000"]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_sendtrade", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking the trade in orderbook")
        params = str([4])
        conn.request('POST', '/', '{"method": "tl_getorderbook", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['propertyidforsale'],4)
        assert_equal(out['result'][0]['amountforsale'],'1000.00000000')
        assert_equal(out['result'][0]['propertyiddesired'],5)
        assert_equal(out['result'][0]['amountdesired'],'2000.00000000')


        self.log.info("Sending a second trade in MetaDEx")
        params = str([addresses[1], 5, "2000", 4, "1000"]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_sendtrade", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens balance for first address")
        params = str([addresses[0], 4]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'99000.40000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        params = str([addresses[0], 5]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking tokens balance for second address")
        params = str([addresses[1], 4]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        # assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'999.50000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        params = str([addresses[1], 5]).replace("'",'"')
        # self.log.info(params)
        conn.request('POST', '/', '{"method": "tl_getbalance", "params":'+params+'}', headers)
        resp = conn.getresponse()
        assert_equal(resp.status, 200)
        input = (resp.read().decode('utf-8'))
        out = json.loads(input)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'98000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    HTTPBasicsTest ().main ()
