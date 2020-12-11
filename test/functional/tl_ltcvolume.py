#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test MetaDEx token volume in LTC."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class LTCVolumeTest (BitcoinTestFramework):
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

    def run_test(self):

        self.log.info("Preparing the workspace...")

        # mining 400 blocks
        self.nodes[0].generate(400)

        ################################################################################
        # Checking RPC tl_sendtrade (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another", "mark"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("watching LTC general balance")
        params = str([""]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getbalance",params)
        self.log.info(out)
        assert_equal(out['error'], None)

        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 1001.1
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)

        self.log.info("Creating new tokens  (lihki)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","90000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Creating new tokens  (dan)")
        array = [0]
        params = str([addresses[1],2,0,"dan","","","1000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

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


        self.log.info("Checking the property: lihki")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'90000000000.00000000')


        self.log.info("Checking the property: dan ")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['name'],'dan')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'1000000000.00000000')


        self.log.info("Checking tokens balance in lihki's owner ")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'90000000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking tokens balance in dan's owner ")
        params = str([addresses[1], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1000000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Sending a DEx sell tokens offer")
        params = str([addresses[0], 4, "1000000", "1000", 200, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        self.nodes[0].generate(1)


        self.log.info("Checking the new offer in DEx")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[0])
        assert_equal(out['result'][0]['ltcsdesired'], '1000.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000000.00000000')


        self.log.info("Accepting the full offer")
        params = str([addresses[1], addresses[0], 4, "1000000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        self.nodes[0].generate(1)


        self.log.info("Paying the tokens")
        params = str([addresses[1], addresses[0], "1000.0"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send_dex_payment",params)
        # self.log.info(out)
        self.nodes[0].generate(1)


        self.log.info("Checking LTC Volume")
        params = str([4, 1, 30000]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_get_ltcvolume",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['volume'], '1000.00000000')


        self.log.info("Sending another DEx sell tokens offer")
        params = str([addresses[1], 5, "1000000", "1000", 200, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        self.nodes[0].generate(1)


        self.log.info("Checking the new offer in DEx")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 5)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[1])
        assert_equal(out['result'][0]['ltcsdesired'], '1000.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000000.00000000')


        self.log.info("Accepting the full offer")
        params = str([addresses[2], addresses[1], 5, "1000000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexaccept",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Paying the tokens")
        params = str([addresses[2], addresses[1], "1000.0"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send_dex_payment",params)
        # self.log.info(out)
        self.nodes[0].generate(1)


        self.log.info("Checking LTC Volume")
        params = str([5, 1, 30000]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_get_ltcvolume",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['volume'], '1000.00000000')


        self.log.info("Sending a trade in MetaDEx")
        params = str([addresses[0], 4, "1000", 5, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendtrade",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trade in orderbook")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['propertyidforsale'],4)
        assert_equal(out['result'][0]['amountforsale'],'1000.00000000')
        assert_equal(out['result'][0]['propertyiddesired'],5)
        assert_equal(out['result'][0]['amountdesired'],'2000.00000000')


        self.log.info("Sending a second trade in MetaDEx")
        params = str([addresses[1], 5, "2000", 4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendtrade",params)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking LTC Volume")
        params = str([4, 1, 30000]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_get_ltcvolume",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['volume'], '2000.00000000')

        conn.close()
        self.stop_nodes()

if __name__ == '__main__':
    LTCVolumeTest ().main ()
