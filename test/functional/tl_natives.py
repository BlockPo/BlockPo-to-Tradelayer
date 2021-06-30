#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test ContractDEx functions (natives)."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class NativesBasicsTest (BitcoinTestFramework):
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
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 1.1
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


        self.log.info("Creating native Contract")
        array = [0]
        params = str([addresses[0], 1, 4, "ALL/Lhk", 1000, "1", 4, "0.1", 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_createcontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the native contract")
        params = '["1"]'
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['contractid'], 1)
        assert_equal(out['result']['name'],'ALL/Lhk')
        assert_equal(out['result']['admin'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '1000')
        assert_equal(out['result']['inverse quoted'], '0')


        #NOTE: we need to test this for all leverages
        self.log.info("Buying contracts")
        params = str([addresses[1], "ALL/Lhk", "1000", "780.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")
        # self.log.info(hash)

        self.nodes[0].generate(1)


        self.log.info("Checking colateral balance now in sender address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getbalance",params)
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'49799.99000000')
        assert_equal(out['result']['reserve'],'200.01000000')


        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '780.50000000')
        assert_equal(out['result'][0]['block'], 206)


        self.log.info("Canceling Contract order")
        address = '"'+addresses[1]+'"'
        params = str([addresses[1], hash]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendcancel_contract_order",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])

        #NOTE: we need to test this for all leverages
        self.log.info("Checking restored colateral in sender address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'50000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Buying contracts again")
        params = str([addresses[1], "ALL/Lhk", "1000", "980.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")

        self.nodes[0].generate(1)

        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '980.50000000')
        # assert_equal(out['result'][0]['block'], 206)


        self.log.info("Another address selling contracts")
        params = str([addresses[0], "ALL/Lhk", "1000", "980.5", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        # we need to se here the margin for addresses[0] in Contract
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['reserve'],'100.00000000')

        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])


        self.log.info("Checking position in first address")
        params = str([addresses[1], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 1000)


        self.log.info("Checking position in second address")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -1000)


        self.log.info("Checking the open interest")
        params = str(["ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 1000)


        self.log.info("Checking when the price does not match")


        self.log.info("Buying contracts at price 100.3")
        params = str([addresses[1], "ALL/Lhk", "1000", "100.3", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '100.30000000')
        # assert_equal(out['result'][0]['block'], 206)


        self.log.info("Another address selling contracts at 900.1")
        params = str([addresses[0], "ALL/Lhk", "1000", "900.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        # we need to see here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['reserve'],'100.00000000')


        self.log.info("Checking orderbook (sell side)")
        params = str(["ALL/Lhk", 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 2)
        assert_equal(out['result'][0]['effectiveprice'], '900.10000000')
        # assert_equal(out['result'][0]['block'], 206)


        self.log.info("Cancel orders using tl_cancelallcontractsbyaddress")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_cancelallcontractsbyaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        params = str([addresses[1], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_cancelallcontractsbyaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['reserve'],'100.00000000')


        self.log.info("Checking orderbook (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])

        self.log.info("Checking orderbook (sell side)")
        params = str(["ALL/Lhk", 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])


        self.log.info("Sending a new buy order")
        params = str([addresses[1], "ALL/Lhk", "1000", "100.9", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")

        self.nodes[0].generate(1)

        self.log.info("Checking orderbook (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '100.90000000')
        assert_equal(out['result'][0]['block'], 213)
        assert_equal(out['result'][0]['idx'], 1)

        idx = out['result'][0]['idx']
        block = out['result'][0]['block']

        self.log.info("Canceling using tl_cancelorderbyblock")
        params = str([addresses[1], block, idx]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_cancelorderbyblock",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking orderbook (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])

        self.log.info("Checking the partial fill")
        self.log.info("Sending buy order")
        params = str([addresses[0], "ALL/Lhk", "1000", "800.1", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['reserve'],'100.00000000')


        self.log.info("Checking orderbook (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '800.10000000')
        assert_equal(out['result'][0]['block'], 215)
        assert_equal(out['result'][0]['idx'], 1)

        self.log.info("Sending sell order")
        params = str([addresses[1], "ALL/Lhk", "500", "800.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking position in addresses")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -500)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['reserve'],'100.00000000')

        params = str([addresses[1], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 500)


        self.log.info("Checking the open interest")
        params = str(["ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 500)

        self.log.info("Checking orderbook again (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 500)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '800.10000000')
        assert_equal(out['result'][0]['block'], 215)
        assert_equal(out['result'][0]['idx'], 1)

        self.log.info("Putting order without collateral")

        self.log.info("Sending sell order")
        params = str([addresses[2], "ALL/Lhk", "300", "800.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error']['message'], 'Sender has insufficient balance for collateral')

        self.log.info("Sending sell order with more than max leverage")
        params = str([addresses[1], "ALL/Lhk", "500", "800.1", 2, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error']['message'], 'Leverage out of range')

        self.log.info("Checking trading against yourself")
        self.log.info("Sending buy sell")
        params = str([addresses[0], "ALL/Lhk", "500", "800.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract",params)
        # self.log.info(out)
        # assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook again (sell side)")
        params = str(["ALL/Lhk", 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 500)
        assert_equal(out['result'][0]['tradingaction'], 2)
        assert_equal(out['result'][0]['effectiveprice'], '800.10000000')
        assert_equal(out['result'][0]['block'], 217)
        assert_equal(out['result'][0]['idx'], 1)

        self.log.info("Cancel orders using tl_cancelallcontractsbyaddress")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_cancelallcontractsbyaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['reserve'],'100.00000000')


        self.log.info("Checking orderbook again (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['result'], [])

        self.log.info("Checking orderbook again (sell side)")
        params = str(["ALL/Lhk", 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['result'], [])


        self.log.info("Closing positions")
        self.log.info("Preparing the orderbook")
        params = str([addresses[1], "ALL/Lhk", "500", "500.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -500)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['reserve'],'100.00000000')


        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_closeposition",params)
        # self.log.info(out)
        # assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 0)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        self.log.info(out)
        assert_equal(out['error'], None)
        # NOTE: we need to change the logic in margin to balance (closing positions)
        # assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking margins for short and long position")
        params = str([addresses[0], "ALL/Lhk", "500", "500.1", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        params = str([addresses[1], "ALL/Lhk", "500", "500.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 500)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        # assert_equal(out['result']['reserve'],'50.00500000')

        params = str([addresses[1], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -500)

        params = str([addresses[2], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 0)


        self.log.info("Checking the open interest")
        params = str(["ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 500)

        self.log.info("Sending 2000 tokens to third address")
        params = str([addresses[0], addresses[2], 4, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        params = str([addresses[2], "ALL/Lhk", "1000", "400.1", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("First address from long to short position")

        params = str([addresses[0], "ALL/Lhk", "1000", "400.1", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -500)

        # we need to se here the margin for addresses[0]
        params = str([addresses[0], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        # assert_equal(out['result']['reserve'],'150.00500000')


        params = str([addresses[2], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 1000)


        self.log.info("Checking the open interest")
        params = str(["ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 1000)


        self.log.info("Checking orderbook again (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['result'], [])

        self.log.info("Checking orderbook again (sell side)")
        params = str(["ALL/Lhk", 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['result'], [])

        self.log.info("Testing order stack (different blocks)")

        for i in range(5):
             params = str([addresses[0], "ALL/Lhk", "1000", "100.2", 1, "1"]).replace("'",'"')
             out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
             assert_equal(out['error'], None)

             self.nodes[0].generate(1)


        self.log.info("Checking orderbook again (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'],  1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][0]['block'], 226)
        assert_equal(out['result'][0]['idx'], 1)

        assert_equal(out['result'][1]['address'], addresses[0])
        assert_equal(out['result'][1]['contractid'], 1)
        assert_equal(out['result'][1]['amountforsale'],  1000)
        assert_equal(out['result'][1]['tradingaction'], 1)
        assert_equal(out['result'][1]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][1]['block'], 227)
        assert_equal(out['result'][1]['idx'], 1)

        assert_equal(out['result'][2]['address'], addresses[0])
        assert_equal(out['result'][2]['contractid'], 1)
        assert_equal(out['result'][2]['amountforsale'], 1000)
        assert_equal(out['result'][2]['tradingaction'], 1)
        assert_equal(out['result'][2]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][2]['block'], 228)
        assert_equal(out['result'][2]['idx'], 1)

        assert_equal(out['result'][3]['address'], addresses[0])
        assert_equal(out['result'][3]['contractid'], 1)
        assert_equal(out['result'][3]['amountforsale'], 1000)
        assert_equal(out['result'][3]['tradingaction'], 1)
        assert_equal(out['result'][3]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][3]['block'], 229)
        assert_equal(out['result'][3]['idx'], 1)

        assert_equal(out['result'][4]['address'], addresses[0])
        assert_equal(out['result'][4]['contractid'], 1)
        assert_equal(out['result'][4]['amountforsale'], 1000)
        assert_equal(out['result'][4]['tradingaction'], 1)
        assert_equal(out['result'][4]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][4]['block'], 230)
        assert_equal(out['result'][4]['idx'], 1)

        params = str([addresses[1], "ALL/Lhk", "1000", "100.2", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking the open interest")
        params = str(["ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 1500)


        self.log.info("Checking orderbook again (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)

        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][0]['block'], 227)
        assert_equal(out['result'][0]['idx'], 1)

        assert_equal(out['result'][1]['address'], addresses[0])
        assert_equal(out['result'][1]['contractid'], 1)
        assert_equal(out['result'][1]['amountforsale'], 1000)
        assert_equal(out['result'][1]['tradingaction'], 1)
        assert_equal(out['result'][1]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][1]['block'], 228)
        assert_equal(out['result'][1]['idx'], 1)

        assert_equal(out['result'][2]['address'], addresses[0])
        assert_equal(out['result'][2]['contractid'], 1)
        assert_equal(out['result'][2]['amountforsale'], 1000)
        assert_equal(out['result'][2]['tradingaction'], 1)
        assert_equal(out['result'][2]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][2]['block'], 229)
        assert_equal(out['result'][2]['idx'], 1)

        assert_equal(out['result'][3]['address'], addresses[0])
        assert_equal(out['result'][3]['contractid'], 1)
        assert_equal(out['result'][3]['amountforsale'], 1000)
        assert_equal(out['result'][3]['tradingaction'], 1)
        assert_equal(out['result'][3]['effectiveprice'], '100.20000000')
        assert_equal(out['result'][3]['block'], 230)
        assert_equal(out['result'][3]['idx'], 1)

        self.log.info("Cleaning orderbook")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_cancelallcontractsbyaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook again")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['result'], [])

        self.log.info("Testing order stack (different idx)")

        for i in range(5):
             params = str([addresses[0], "ALL/Lhk", "1000", "200.3", 1, "1"]).replace("'",'"')
             out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
             assert_equal(out['error'], None)


        self.nodes[0].generate(1)

        self.log.info("Checking orderbook again (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][0]['block'], 233)
        assert_equal(out['result'][0]['idx'], 1)

        assert_equal(out['result'][1]['address'], addresses[0])
        assert_equal(out['result'][1]['contractid'], 1)
        assert_equal(out['result'][1]['amountforsale'], 1000)
        assert_equal(out['result'][1]['tradingaction'], 1)
        assert_equal(out['result'][1]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][1]['block'], 233)
        assert_equal(out['result'][1]['idx'], 2)

        assert_equal(out['result'][2]['address'], addresses[0])
        assert_equal(out['result'][2]['contractid'], 1)
        assert_equal(out['result'][2]['amountforsale'], 1000)
        assert_equal(out['result'][2]['tradingaction'], 1)
        assert_equal(out['result'][2]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][2]['block'], 233)
        assert_equal(out['result'][2]['idx'], 3)

        assert_equal(out['result'][3]['address'], addresses[0])
        assert_equal(out['result'][3]['contractid'], 1)
        assert_equal(out['result'][3]['amountforsale'], 1000)
        assert_equal(out['result'][3]['tradingaction'], 1)
        assert_equal(out['result'][3]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][3]['block'], 233)
        assert_equal(out['result'][3]['idx'], 4)

        assert_equal(out['result'][4]['address'], addresses[0])
        assert_equal(out['result'][4]['contractid'], 1)
        assert_equal(out['result'][4]['amountforsale'], 1000)
        assert_equal(out['result'][4]['tradingaction'], 1)
        assert_equal(out['result'][4]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][4]['block'], 233)
        assert_equal(out['result'][4]['idx'], 5)

        params = str([addresses[1], "ALL/Lhk", "1000", "200.3", 2, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook again (buy side)")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)

        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['contractid'], 1)
        assert_equal(out['result'][0]['amountforsale'], 1000)
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][0]['block'], 233)
        assert_equal(out['result'][0]['idx'], 2)

        assert_equal(out['result'][1]['address'], addresses[0])
        assert_equal(out['result'][1]['contractid'], 1)
        assert_equal(out['result'][1]['amountforsale'], 1000)
        assert_equal(out['result'][1]['tradingaction'], 1)
        assert_equal(out['result'][1]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][1]['block'], 233)
        assert_equal(out['result'][1]['idx'], 3)

        assert_equal(out['result'][2]['address'], addresses[0])
        assert_equal(out['result'][2]['contractid'], 1)
        assert_equal(out['result'][2]['amountforsale'], 1000)
        assert_equal(out['result'][2]['tradingaction'], 1)
        assert_equal(out['result'][2]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][2]['block'], 233)
        assert_equal(out['result'][2]['idx'], 4)

        assert_equal(out['result'][3]['address'], addresses[0])
        assert_equal(out['result'][3]['contractid'], 1)
        assert_equal(out['result'][3]['amountforsale'], 1000)
        assert_equal(out['result'][3]['tradingaction'], 1)
        assert_equal(out['result'][3]['effectiveprice'], '200.30000000')
        assert_equal(out['result'][3]['block'], 233)
        assert_equal(out['result'][3]['idx'], 5)

        self.log.info("Cleaning orderbook")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_cancelallcontractsbyaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook again")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['result'], [])

        self.log.info("Testing trades after contract deadline")

        for i in range(5):
             self.nodes[0].generate(200)


        params = str([addresses[0], "ALL/Lhk", "1000", "200.3", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking orderbook empty")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['result'], [])

        self.log.info("Checking all positions")
        params = str([addresses[0], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 1500)


        params = str([addresses[1], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -2500)


        params = str([addresses[2], "ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 1000)

        self.log.info("Checking the open interest")
        params = str(["ALL/Lhk"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getopen_interest",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['totalLives'], 2500)

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    NativesBasicsTest ().main ()
