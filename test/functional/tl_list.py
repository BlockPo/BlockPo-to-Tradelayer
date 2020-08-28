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
        accounts = ["john", "doe", "another"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 2.1
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating new tokens  (lihki)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","100000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Creating new tokens  (dan)")
        params = str([addresses[1],2,0,"dan","","","200000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

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

        assert_equal(result, [True, True, True])

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
        assert_equal(out['result']['totaltokens'],'100000.00000000')


        self.log.info("Checking tokens balance in lihki's owner ")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'100000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 50000 tokens to second address")
        params = str([addresses[0], addresses[1], 4, "50000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'50000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Creating oracles Contract")
        array = [0]
        params = str([addresses[0], "Oracle 1", 10000, "1", 4, "0.1", addresses[2], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Testing tl_listproperties")
        out = tradelayer_HTTP(conn, headers, True, "tl_listproperties")
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 1)
        assert_equal(out['result'][0]['name'], 'ALL')
        assert_equal(out['result'][0]['data'], '')
        assert_equal(out['result'][0]['url'], '')
        assert_equal(out['result'][0]['divisible'], True)
        assert_equal(out['result'][0]['category'], 'N/A')
        assert_equal(out['result'][0]['subcategory'], 'N/A')
        assert_equal(out['result'][0]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][1]['propertyid'], 2)
        assert_equal(out['result'][1]['name'], 'sLTC')
        assert_equal(out['result'][1]['data'], '')
        assert_equal(out['result'][1]['url'], '')
        assert_equal(out['result'][1]['divisible'], True)
        assert_equal(out['result'][1]['category'], 'N/A')
        assert_equal(out['result'][1]['subcategory'], 'N/A')
        assert_equal(out['result'][1]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][2]['propertyid'], 3)
        assert_equal(out['result'][2]['name'], 'Vesting Tokens')
        assert_equal(out['result'][2]['data'], 'Divisible Tokens')
        assert_equal(out['result'][2]['url'], 'www.tradelayer.org')
        assert_equal(out['result'][2]['divisible'], True)
        assert_equal(out['result'][2]['category'], 'N/A')
        assert_equal(out['result'][2]['subcategory'], 'N/A')
        assert_equal(out['result'][2]['kyc_ids allowed'], '[]')

        assert_equal(out['result'][3]['propertyid'], 4)
        assert_equal(out['result'][3]['name'], 'lihki')
        assert_equal(out['result'][3]['data'], '')
        assert_equal(out['result'][3]['url'], '')
        assert_equal(out['result'][3]['divisible'], True)
        assert_equal(out['result'][3]['category'], '')
        assert_equal(out['result'][3]['subcategory'], '')
        assert_equal(out['result'][3]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][4]['propertyid'], 5)
        assert_equal(out['result'][4]['name'], 'dan')
        assert_equal(out['result'][4]['data'], '')
        assert_equal(out['result'][4]['url'], '')
        assert_equal(out['result'][4]['divisible'], True)
        assert_equal(out['result'][4]['category'], '')
        assert_equal(out['result'][4]['subcategory'], '')
        assert_equal(out['result'][4]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][5]['propertyid'], 6)
        assert_equal(out['result'][5]['name'], 'Oracle 1')
        assert_equal(out['result'][5]['data'], '')
        assert_equal(out['result'][5]['url'], '')
        assert_equal(out['result'][5]['divisible'], False)
        assert_equal(out['result'][5]['category'], '')
        assert_equal(out['result'][5]['subcategory'], '')
        assert_equal(out['result'][5]['kyc_ids allowed'], '[0]')


        self.log.info("Sending a DEx sell tokens offer")
        params = str([addresses[1], 4, "1000", "1", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer in DEx")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[1])
        assert_equal(out['result'][0]['ltcsdesired'], '1.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00100000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')


        self.log.info("Accepting the full offer")
        params = str([addresses[2], addresses[1], 4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer status")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[1])

        assert_equal(out['result'][0]['accepts'][0]['buyer'], addresses[2])
        assert_equal(out['result'][0]['accepts'][0]['amountdesired'], '1000.00000000')
        assert_equal(out['result'][0]['accepts'][0]['ltcstopay'], '1.00000000')

        self.log.info("Paying the tokens")
        params = str([addresses[2], addresses[1], "1.0"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send_dex_payment",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking token balance in buyer address")
        params = str([addresses[2], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '1000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Testing tl_listproperties (with verbose 1)")
        params = str([1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_listproperties", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 1)
        assert_equal(out['result'][0]['name'], 'ALL')
        assert_equal(out['result'][0]['data'], '')
        assert_equal(out['result'][0]['url'], '')
        assert_equal(out['result'][0]['divisible'], True)
        assert_equal(out['result'][0]['category'], 'N/A')
        assert_equal(out['result'][0]['subcategory'], 'N/A')
        assert_equal(out['result'][0]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][0]['issuer'], '')
        assert_equal(out['result'][0]['fixedissuance'], False)
        assert_equal(out['result'][0]['creation block'], 0)
        assert_equal(out['result'][0]['totaltokens'], '1500000.00000000')
        assert_equal(out['result'][0]['last 24h LTC volume'], '0.00000000')
        assert_equal(out['result'][0]['last 24h Token volume'], '0.00000000')

        assert_equal(out['result'][1]['propertyid'], 2)
        assert_equal(out['result'][1]['name'], 'sLTC')
        assert_equal(out['result'][1]['data'], '')
        assert_equal(out['result'][1]['url'], '')
        assert_equal(out['result'][1]['divisible'], True)
        assert_equal(out['result'][1]['category'], 'N/A')
        assert_equal(out['result'][1]['subcategory'], 'N/A')
        assert_equal(out['result'][1]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][1]['issuer'], '')
        assert_equal(out['result'][1]['fixedissuance'], False)
        assert_equal(out['result'][1]['creation block'], 0)
        assert_equal(out['result'][1]['totaltokens'], '0.00000000')
        assert_equal(out['result'][1]['last 24h LTC volume'], '0.00000000')
        assert_equal(out['result'][1]['last 24h Token volume'], '0.00000000')

        assert_equal(out['result'][2]['propertyid'], 3)
        assert_equal(out['result'][2]['name'], 'Vesting Tokens')
        assert_equal(out['result'][2]['data'], 'Divisible Tokens')
        assert_equal(out['result'][2]['url'], 'www.tradelayer.org')
        assert_equal(out['result'][2]['divisible'], True)
        assert_equal(out['result'][2]['category'], 'N/A')
        assert_equal(out['result'][2]['subcategory'], 'N/A')
        assert_equal(out['result'][2]['kyc_ids allowed'], '[]')
        assert_equal(out['result'][2]['issuer'], 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb')
        assert_equal(out['result'][2]['fixedissuance'], False)
        assert_equal(out['result'][2]['creation block'], 100)
        assert_equal(out['result'][2]['totaltokens'], '1500000.00000000')
        assert_equal(out['result'][2]['last 24h LTC volume'], '0.00000000')
        assert_equal(out['result'][2]['last 24h Token volume'], '0.00000000')

        assert_equal(out['result'][3]['propertyid'], 4)
        assert_equal(out['result'][3]['name'], 'lihki')
        assert_equal(out['result'][3]['data'], '')
        assert_equal(out['result'][3]['url'], '')
        assert_equal(out['result'][3]['divisible'], True)
        assert_equal(out['result'][3]['category'], '')
        assert_equal(out['result'][3]['subcategory'], '')
        assert_equal(out['result'][3]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][3]['issuer'], addresses[0])
        assert_equal(out['result'][3]['fixedissuance'], True)
        assert_equal(out['result'][3]['creation block'], 202)
        assert_equal(out['result'][3]['totaltokens'], '100000.00000000')
        assert_equal(out['result'][3]['last 24h LTC volume'], '1.00000000')
        assert_equal(out['result'][3]['last 24h Token volume'], '1000.00000000')

        assert_equal(out['result'][4]['propertyid'], 5)
        assert_equal(out['result'][4]['name'], 'dan')
        assert_equal(out['result'][4]['data'], '')
        assert_equal(out['result'][4]['url'], '')
        assert_equal(out['result'][4]['divisible'], True)
        assert_equal(out['result'][4]['category'], '')
        assert_equal(out['result'][4]['subcategory'], '')
        assert_equal(out['result'][4]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][4]['issuer'], addresses[1])
        assert_equal(out['result'][4]['fixedissuance'], True)
        assert_equal(out['result'][4]['creation block'], 203)
        assert_equal(out['result'][4]['totaltokens'], '200000.00000000')
        assert_equal(out['result'][4]['last 24h LTC volume'], '0.00000000')
        assert_equal(out['result'][4]['last 24h Token volume'], '0.00000000')

        assert_equal(out['result'][5]['propertyid'], 6)
        assert_equal(out['result'][5]['name'], 'Oracle 1')
        assert_equal(out['result'][5]['data'], '')
        assert_equal(out['result'][5]['url'], '')
        assert_equal(out['result'][5]['divisible'], False)
        assert_equal(out['result'][5]['category'], '')
        assert_equal(out['result'][5]['subcategory'], '')
        assert_equal(out['result'][5]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][5]['issuer'], addresses[0])
        assert_equal(out['result'][5]['fixedissuance'], False)
        assert_equal(out['result'][5]['creation block'], 206)
        assert_equal(out['result'][5]['notional size'], '1')
        assert_equal(out['result'][5]['collateral currency'], '4')
        assert_equal(out['result'][5]['margin requirement'], '0.1')
        assert_equal(out['result'][5]['blocks until expiration'], '10000')
        assert_equal(out['result'][5]['backup address'], addresses[2])
        assert_equal(out['result'][5]['hight price'], '0')
        assert_equal(out['result'][5]['low price'], '0')
        assert_equal(out['result'][5]['last close price'], '0')
        assert_equal(out['result'][5]['inverse quoted'], '0')
        assert_equal(out['result'][5]['open interest'], '0.00000000')
        assert_equal(out['result'][5]['last traded price'], '0.00000000')
        assert_equal(out['result'][5]['insurance fund balance'], '0.00000000')

        self.log.info("Testing tl_listproperties (with verbose 0)")
        params = str([0]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_listproperties", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 1)
        assert_equal(out['result'][0]['name'], 'ALL')
        assert_equal(out['result'][0]['data'], '')
        assert_equal(out['result'][0]['url'], '')
        assert_equal(out['result'][0]['divisible'], True)
        assert_equal(out['result'][0]['category'], 'N/A')
        assert_equal(out['result'][0]['subcategory'], 'N/A')
        assert_equal(out['result'][0]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][1]['propertyid'], 2)
        assert_equal(out['result'][1]['name'], 'sLTC')
        assert_equal(out['result'][1]['data'], '')
        assert_equal(out['result'][1]['url'], '')
        assert_equal(out['result'][1]['divisible'], True)
        assert_equal(out['result'][1]['category'], 'N/A')
        assert_equal(out['result'][1]['subcategory'], 'N/A')
        assert_equal(out['result'][1]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][2]['propertyid'], 3)
        assert_equal(out['result'][2]['name'], 'Vesting Tokens')
        assert_equal(out['result'][2]['data'], 'Divisible Tokens')
        assert_equal(out['result'][2]['url'], 'www.tradelayer.org')
        assert_equal(out['result'][2]['divisible'], True)
        assert_equal(out['result'][2]['category'], 'N/A')
        assert_equal(out['result'][2]['subcategory'], 'N/A')
        assert_equal(out['result'][2]['kyc_ids allowed'], '[]')

        assert_equal(out['result'][3]['propertyid'], 4)
        assert_equal(out['result'][3]['name'], 'lihki')
        assert_equal(out['result'][3]['data'], '')
        assert_equal(out['result'][3]['url'], '')
        assert_equal(out['result'][3]['divisible'], True)
        assert_equal(out['result'][3]['category'], '')
        assert_equal(out['result'][3]['subcategory'], '')
        assert_equal(out['result'][3]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][4]['propertyid'], 5)
        assert_equal(out['result'][4]['name'], 'dan')
        assert_equal(out['result'][4]['data'], '')
        assert_equal(out['result'][4]['url'], '')
        assert_equal(out['result'][4]['divisible'], True)
        assert_equal(out['result'][4]['category'], '')
        assert_equal(out['result'][4]['subcategory'], '')
        assert_equal(out['result'][4]['kyc_ids allowed'], '[0]')

        assert_equal(out['result'][5]['propertyid'], 6)
        assert_equal(out['result'][5]['name'], 'Oracle 1')
        assert_equal(out['result'][5]['data'], '')
        assert_equal(out['result'][5]['url'], '')
        assert_equal(out['result'][5]['divisible'], False)
        assert_equal(out['result'][5]['category'], '')
        assert_equal(out['result'][5]['subcategory'], '')
        assert_equal(out['result'][5]['kyc_ids allowed'], '[0]')

        self.log.info("Sending another DEx sell tokens offer")
        params = str([addresses[1], 5, "2000", "1", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Accepting the full offer")
        params = str([addresses[2], addresses[1], 5, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer status")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 5)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[1])

        assert_equal(out['result'][0]['accepts'][0]['buyer'], addresses[2])
        assert_equal(out['result'][0]['accepts'][0]['amountdesired'], '2000.00000000')
        assert_equal(out['result'][0]['accepts'][0]['ltcstopay'], '1.00000000')

        self.log.info("Paying the tokens")
        params = str([addresses[2], addresses[1], "0.5"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send_dex_payment",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking token balance in buyer address")
        params = str([addresses[2], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '1000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Testing tl_listproperties (with verbose 1)")
        params = str([1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_listproperties", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 1)
        assert_equal(out['result'][0]['name'], 'ALL')
        assert_equal(out['result'][0]['data'], '')
        assert_equal(out['result'][0]['url'], '')
        assert_equal(out['result'][0]['divisible'], True)
        assert_equal(out['result'][0]['category'], 'N/A')
        assert_equal(out['result'][0]['subcategory'], 'N/A')
        assert_equal(out['result'][0]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][0]['issuer'], '')
        assert_equal(out['result'][0]['fixedissuance'], False)
        assert_equal(out['result'][0]['creation block'], 0)
        assert_equal(out['result'][0]['totaltokens'], '1500000.00000000')
        assert_equal(out['result'][0]['last 24h LTC volume'], '0.00000000')
        assert_equal(out['result'][0]['last 24h Token volume'], '0.00000000')

        assert_equal(out['result'][1]['propertyid'], 2)
        assert_equal(out['result'][1]['name'], 'sLTC')
        assert_equal(out['result'][1]['data'], '')
        assert_equal(out['result'][1]['url'], '')
        assert_equal(out['result'][1]['divisible'], True)
        assert_equal(out['result'][1]['category'], 'N/A')
        assert_equal(out['result'][1]['subcategory'], 'N/A')
        assert_equal(out['result'][1]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][1]['issuer'], '')
        assert_equal(out['result'][1]['fixedissuance'], False)
        assert_equal(out['result'][1]['creation block'], 0)
        assert_equal(out['result'][1]['totaltokens'], '0.00000000')
        assert_equal(out['result'][1]['last 24h LTC volume'], '0.00000000')
        assert_equal(out['result'][1]['last 24h Token volume'], '0.00000000')

        assert_equal(out['result'][2]['propertyid'], 3)
        assert_equal(out['result'][2]['name'], 'Vesting Tokens')
        assert_equal(out['result'][2]['data'], 'Divisible Tokens')
        assert_equal(out['result'][2]['url'], 'www.tradelayer.org')
        assert_equal(out['result'][2]['divisible'], True)
        assert_equal(out['result'][2]['category'], 'N/A')
        assert_equal(out['result'][2]['subcategory'], 'N/A')
        assert_equal(out['result'][2]['kyc_ids allowed'], '[]')
        assert_equal(out['result'][2]['issuer'], 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb')
        assert_equal(out['result'][2]['fixedissuance'], False)
        assert_equal(out['result'][2]['creation block'], 100)
        assert_equal(out['result'][2]['totaltokens'], '1500000.00000000')
        assert_equal(out['result'][2]['last 24h LTC volume'], '0.00000000')
        assert_equal(out['result'][2]['last 24h Token volume'], '0.00000000')

        assert_equal(out['result'][3]['propertyid'], 4)
        assert_equal(out['result'][3]['name'], 'lihki')
        assert_equal(out['result'][3]['data'], '')
        assert_equal(out['result'][3]['url'], '')
        assert_equal(out['result'][3]['divisible'], True)
        assert_equal(out['result'][3]['category'], '')
        assert_equal(out['result'][3]['subcategory'], '')
        assert_equal(out['result'][3]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][3]['issuer'], addresses[0])
        assert_equal(out['result'][3]['fixedissuance'], True)
        assert_equal(out['result'][3]['creation block'], 202)
        assert_equal(out['result'][3]['totaltokens'], '100000.00000000')
        assert_equal(out['result'][3]['last 24h LTC volume'], '1.00000000')
        assert_equal(out['result'][3]['last 24h Token volume'], '1000.00000000')

        assert_equal(out['result'][4]['propertyid'], 5)
        assert_equal(out['result'][4]['name'], 'dan')
        assert_equal(out['result'][4]['data'], '')
        assert_equal(out['result'][4]['url'], '')
        assert_equal(out['result'][4]['divisible'], True)
        assert_equal(out['result'][4]['category'], '')
        assert_equal(out['result'][4]['subcategory'], '')
        assert_equal(out['result'][4]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][4]['issuer'], addresses[1])
        assert_equal(out['result'][4]['fixedissuance'], True)
        assert_equal(out['result'][4]['creation block'], 203)
        assert_equal(out['result'][4]['totaltokens'], '200000.00000000')
        assert_equal(out['result'][4]['last 24h LTC volume'], '0.50000000')
        assert_equal(out['result'][4]['last 24h Token volume'], '1000.00000000')

        assert_equal(out['result'][5]['propertyid'], 6)
        assert_equal(out['result'][5]['name'], 'Oracle 1')
        assert_equal(out['result'][5]['data'], '')
        assert_equal(out['result'][5]['url'], '')
        assert_equal(out['result'][5]['divisible'], False)
        assert_equal(out['result'][5]['category'], '')
        assert_equal(out['result'][5]['subcategory'], '')
        assert_equal(out['result'][5]['kyc_ids allowed'], '[0]')
        assert_equal(out['result'][5]['issuer'], addresses[0])
        assert_equal(out['result'][5]['fixedissuance'], False)
        assert_equal(out['result'][5]['creation block'], 206)
        assert_equal(out['result'][5]['notional size'], '1')
        assert_equal(out['result'][5]['collateral currency'], '4')
        assert_equal(out['result'][5]['margin requirement'], '0.1')
        assert_equal(out['result'][5]['blocks until expiration'], '10000')
        assert_equal(out['result'][5]['backup address'], addresses[2])
        assert_equal(out['result'][5]['hight price'], '0')
        assert_equal(out['result'][5]['low price'], '0')
        assert_equal(out['result'][5]['last close price'], '0')
        assert_equal(out['result'][5]['inverse quoted'], '0')
        assert_equal(out['result'][5]['open interest'], '0.00000000')
        assert_equal(out['result'][5]['last traded price'], '0.00000000')
        assert_equal(out['result'][5]['insurance fund balance'], '0.00000000')


        self.log.info("Testing tl_getproperty (5)")
        params = str([5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty", params)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'], 5)
        assert_equal(out['result']['name'], 'dan')
        assert_equal(out['result']['data'], '')
        assert_equal(out['result']['url'], '')
        assert_equal(out['result']['divisible'], True)
        assert_equal(out['result']['category'], '')
        assert_equal(out['result']['subcategory'], '')
        assert_equal(out['result']['kyc_ids allowed'], '[0]')
        assert_equal(out['result']['issuer'], addresses[1])
        assert_equal(out['result']['fixedissuance'], True)
        assert_equal(out['result']['creation block'], 203)
        assert_equal(out['result']['totaltokens'], '200000.00000000')
        assert_equal(out['result']['last 24h LTC volume'], '0.50000000')
        assert_equal(out['result']['last 24h Token volume'], '1000.00000000')

        conn.close()
        self.stop_nodes()

if __name__ == '__main__':
    HTTPBasicsTest ().main ()
