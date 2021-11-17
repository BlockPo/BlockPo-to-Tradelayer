#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test basic data RPCs ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class HTTPBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-txindex=1"]]

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

        # mining 200 blocks
        self.nodes[0].generate(200)

        ################################################################################
        # Checking RPC calls for data retrieval (in the first 100 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "cloe"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 1.1
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

        self.log.info("Testing tl_sendactivation")

        # adminAddress, activation number 8, in block 400, minim tl version = 1.
        params = str([addresses[0], 8, 200, 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendactivation",params)
        # self.log.info(out)
        
        self.nodes[0].generate(110)

        # # john
        self.log.info("Creating new tokens (issuer:john)")
        array = [0]
        params = str([addresses[0],2,0,"TL1","","","3000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed", params)

        self.nodes[0].generate(1)

        self.log.info("Checking the property")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'TL1')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'3000.00000000')

        self.log.info("Checking token balance in every address")
        for addr in addresses:
            params = str([addr, 4]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
            # self.log.info(out)
            amount = ""
            assert_equal(out['error'], None)
            if addr == addresses[0]:
                 amount = '3000.00000000'
            else:
                 amount = '0.00000000'

            assert_equal(out['result']['balance'], amount)
            assert_equal(out['result']['reserve'],'0.00000000')

        #
        #out = tradelayer_HTTP(conn, headers, True, "tl_listtransactions")
        
        self.log.info("Sending 10 tokens to receiver (tl_send)")
        params = str([addresses[0], addresses[1], 4, "10"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)

        self.nodes[0].generate(1)

        self.log.info("Checking tokens balance in john's #1")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        assert_equal(out['result']['balance'], '2990.00000000')
        assert_equal(out['result']['reserve'], '0.00000000')
        
        self.log.info("Checking tokens balance in doe's #1")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        assert_equal(out['result']['balance'], '10.00000000')
        assert_equal(out['result']['reserve'], '0.00000000')

        #
        params = str([addresses[0], { addresses[1] : '10', addresses[2] : '10' }, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendmany", params)

        self.nodes[0].generate(1)
        time.sleep(1)

        self.log.info("Checking tokens balance in john's #2")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)  
        assert_equal(out['result']['balance'], '2970.00000000')
        assert_equal(out['result']['reserve'], '0.00000000')

        self.log.info("Checking tokens balance in doe's #2")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        assert_equal(out['result']['balance'], '20.00000000')
        assert_equal(out['result']['reserve'], '0.00000000')

        self.log.info("Checking tokens balance in cloe's #2")
        params = str([addresses[2], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        assert_equal(out['result']['balance'], '10.00000000')
        assert_equal(out['result']['reserve'], '0.00000000')
        
        #assert_equal(out['result'][0]['balance'],'2980.00000000')
        # assert_equal(out['result'][1]['propertyid'], 5)
        # assert_equal(out['result'][1]['balance'],'2000.00000000')
        # assert_equal(out['result'][2]['propertyid'], 6)
        # assert_equal(out['result'][2]['balance'],'3000.00000000')

        # out = tradelayer_HTTP(conn, headers, True, "listreceivedbyaddress")
        # b = 0.0
        # for addr in out['result']:  b += float(addr['amount'])
        # self.log.info("LTC_balance: " + str(b))

        # params = str([addresses[0]]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, True, "tl_getallbalancesforaddress", params)
        # assert_equal(out['error'], None)

        # out = tradelayer_HTTP(conn, headers, True, "tl_getwalletbalance")
        # assert(abs(out['result']['LTC_balance'] - b*100000000) < 0.00000001)

        conn.close()
        self.stop_nodes()

def tradelayer_selfAttestation(addresses,conn, headers):
    for addr in addresses:
        params = str([addr,addr,""]).replace("'",'"')
        tradelayer_HTTP(conn, headers, False, "tl_attestation", params)
    tradelayer_HTTP(conn, headers, True, "generate", str([1]))

if __name__ == '__main__':
    HTTPBasicsTest ().main ()