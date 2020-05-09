#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Persistence."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse
import time

class PersistenceBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # Use self.extra_args to change command-line arguments for the nodes
        self.extra_args = [["-dbcache=16000"]]

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
        # Checking the persistence in db (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)


        params = str(["bob"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "getnewaddress", params)
        notaryAddr = out['result']
        # self.log.info(notaryAddr)

        params = str([notaryAddr, '0.1']).replace("'",'"')
        tradelayer_HTTP(conn, headers, True, "sendtoaddress", params)

        self.nodes[0].generate(1)

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


        self.log.info("Checking multisig")
        params = str([multisig]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "validateaddress",params)
        # self.log.info(out)
        scriptPubKey = out['result']['scriptPubKey']
        assert_equal(out['result']['embedded']['script'],'multisig')


        self.log.info("Funding addresses with LTC")
        amount = 1.1
        tradelayer_fundingAddresses(addresses, amount, conn, headers)


        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating new tokens  (lihki)")
        array = [0,1]
        params = str([addresses[0],2,0,"lihki","","","20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the property: lihki")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['issuer'],addresses[0])
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'20000000000.00000000')

        self.log.info("Self Attestation for addresses")
        tradelayer_selfAttestation(addresses,conn, headers)


        self.log.info("Creating  KYC for Notary Address ")
        params = str([notaryAddr, "TradeLayer.org","TradeLayer registrars"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_new_id_registration",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Creating  Attestation from Notary Address to multisig")
        params = str([notaryAddr, multisig,""]).replace("'",'"')
        tradelayer_HTTP(conn, headers, True, "tl_attestation", params)

        self.nodes[0].generate(1)

        self.log.info("Checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        self.log.info(out)
        result = []
        registers = out['result']
        for addr in addresses:
            for i in registers:
                if i['att sender'] == addr and i['att receiver'] == addr and i['kyc_id'] == 0:
                     result.append(True)

        for i in registers:
            if i['att sender'] == notaryAddr and i['kyc_id'] == 1:
                result.append(True)

        assert_equal(result, [True, True, True, True])


        self.nodes[0].generate(1)

        self.log.info("Stopping and restaring the node")
        self.restart_node(0) #stop and start


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        self.log.info("Persistence: checking the property lihki")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        # assert_equal(out['error'], None)
        self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['issuer'],addresses[0])
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'20000000000.00000000')

        self.log.info("Persistence : checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        self.log.info(out)

        self.log.info("Persistence : checking tl_geinfo")
        out = tradelayer_HTTP(conn, headers, False, "tl_getinfo")
        self.log.info(out)

        # result2 = []
        # registers = out['result']
        # for addr in addresses:
        #     for i in registers:
        #         if i['att sender'] == addr and i['att receiver'] == addr and i['kyc_id'] == 0:
        #              result2.append(True)
        #
        # for i in registers:
        #     if i['att sender'] == notaryAddr and i['kyc_id'] == 1:
        #         result2.append(True)

        # assert_equal(result2, [True, True, True, True])


        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    PersistenceBasicsTest ().main ()