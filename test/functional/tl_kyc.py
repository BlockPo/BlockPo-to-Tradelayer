#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test KYC and Attestation."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class KYCBasicsTest (BitcoinTestFramework):
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
        accounts = ["john", "doe", "another", "mark", "tango"]

        #NOTE: admin address = addresses[0]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        # self.log.info(addresses)


        self.log.info("Funding addresses with LTC")
        amount = 10
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)

        self.log.info("Creating KYC of Tradelayer ")
        params = str([addresses[0], "TradeLayer.org","TradeLayer register"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_new_id_registration",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the KYC ")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_kyc",params)
        # self.log.info(out)

        assert_equal(out['result']['result: '],'enabled')

        self.log.info("Checking the KYC list ")
        out = tradelayer_HTTP(conn, headers, False, "tl_listkyc")
        # self.log.info(out)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['name'], 'TradeLayer register')
        assert_equal(out['result'][0]['website'], 'TradeLayer.org')
        assert_equal(out['result'][0]['block'], 202)
        assert_equal(out['result'][0]['kyc id'], 1)


        self.log.info("Creating another KYC for Tradelayer ")
        params = str([addresses[1], "TradeLayer.org","TradeLayer register"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_new_id_registration",params)
        # self.log.info(out)

        self.log.info("Checking the KYC before")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_kyc",params)
        # self.log.info(out)

        assert_equal(out['result']['result: '],'disabled')


        self.nodes[0].generate(1)

        self.log.info("Checking the KYC after")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_kyc",params)
        # self.log.info(out)

        assert_equal(out['result']['result: '],'enabled')

        self.log.info("Checking the KYC list ")
        out = tradelayer_HTTP(conn, headers, False, "tl_listkyc")
        # self.log.info(out)

        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['name'], 'TradeLayer register')
        assert_equal(out['result'][0]['website'], 'TradeLayer.org')
        assert_equal(out['result'][0]['block'], 203)
        assert_equal(out['result'][0]['kyc id'], 2)

        self.log.info("Self Attestation for address 2")
        params = str([addresses[2], addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_attestation",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['att sender'], addresses[2])
        assert_equal(out['result'][0]['att receiver'], addresses[2])
        assert_equal(out['result'][0]['kyc_id'], 0)

        self.log.info("Creating new tokens Dcoin(sendissuancefixed)")
        array = [0]
        params = str([addresses[2],2,0,"dCoin","","","90000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)


        self.nodes[0].generate(1)

        self.log.info("Creating new tokens Ecoin(sendissuancefixed)")
        array = [2]
        params = str([addresses[2],2,0,"eCoin","","","90000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the property 4")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'dCoin')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'90000000.00000000')


        self.log.info("Checking the property 5")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['name'],'eCoin')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'90000000.00000000')


        self.log.info("Creating oracle Contract 1")
        array = [0]
        params = str([addresses[2], "Oracle 1", 10000, "1", 4, "0.1", addresses[3], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the oracle contract")
        params = '["1"]'
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['contractid'],1)
        assert_equal(out['result']['name'],'Oracle 1')
        assert_equal(out['result']['admin'], addresses[2])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '0')
        assert_equal(out['result']['low price'], '0')
        assert_equal(out['result']['last close price'], '0')
        assert_equal(out['result']['kyc_ids allowed'], '[0]')


        self.log.info("Creating oracle Contract 2")
        array = [1]
        params = str([addresses[2], "Oracle 2", 10000, "1", 4, "0.1", addresses[3], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the oracle contract")
        params = '["2"]'
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['contractid'],2)
        assert_equal(out['result']['name'],'Oracle 2')
        assert_equal(out['result']['admin'], addresses[2])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '0')
        assert_equal(out['result']['low price'], '0')
        assert_equal(out['result']['last close price'], '0')
        assert_equal(out['result']['kyc_ids allowed'], '[0,1]')

        self.log.info("Creating oracle Contract 3")
        array = [1,2]
        params = str([addresses[2], "Oracle 3", 10000, "1", 4, "0.1", addresses[3], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the oracle contract")
        params = '["3"]'
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['contractid'], 3)
        assert_equal(out['result']['name'],'Oracle 3')
        assert_equal(out['result']['admin'], addresses[2])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '0')
        assert_equal(out['result']['low price'], '0')
        assert_equal(out['result']['last close price'], '0')
        assert_equal(out['result']['kyc_ids allowed'], '[0,1,2]')

        self.log.info("Sending attestation from admin address")
        params = str([addresses[0], addresses[3]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_attestation",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['att sender'], addresses[0])
        assert_equal(out['result'][0]['att receiver'], addresses[3])
        assert_equal(out['result'][0]['kyc_id'], 1)

        assert_equal(out['result'][1]['att sender'], addresses[2])
        assert_equal(out['result'][1]['att receiver'], addresses[2])
        assert_equal(out['result'][1]['kyc_id'], 0)


        self.log.info("Creating new address")
        params = str(['other']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getnewaddress", params)
        foreignAddr = out['result']
        # self.log.info(out)

        self.log.info("Trying to send 3456 dCoins to new address (must fail)")
        params = str([addresses[2], addresses[4], 4, "3456"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_send",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking tokens balances in new address ")
        params = str([addresses[4], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'0.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Revoking attestation")
        params = str([addresses[0], addresses[3]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_revoke_attestation",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['att sender'], addresses[2])
        assert_equal(out['result'][0]['att receiver'], addresses[2])
        assert_equal(out['result'][0]['kyc_id'], 0)


        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    KYCBasicsTest ().main ()
