#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test MetaDEx functions."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class MetaDExBasicsTest (BitcoinTestFramework):
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
        accounts = ["john", "doe", "another", "mark"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 1.1
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
        params = str([addresses[1],2,0,"dan","","","100001",array]).replace("'",'"')
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
        assert_equal(out['result']['totaltokens'],'100001.00000000')


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
        assert_equal(out['result']['balance'],'100001.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending a trade in MetaDEx")
        params = str([addresses[0], 4, "500", 5, "2000"]).replace("'",'"')
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
        assert_equal(out['result'][0]['amountforsale'],'500.00000000')
        assert_equal(out['result'][0]['propertyiddesired'],5)
        assert_equal(out['result'][0]['amountdesired'],'2000.00000000')


        self.log.info("Cancelling all trades in MetaDEx")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendcancelalltrades",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the active orders")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])

        self.log.info("Sending a new  trade in MetaDEx")
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


        self.log.info("Checking tokens balance for first address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'89999999000.40000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        params = str([addresses[0], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking tokens balance for second address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'999.50000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        params = str([addresses[1], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'98001.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking trade history of first address")
        params = str([addresses[0], 100, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getmdextradehistoryforaddress",params)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['type_int'], 25)
        assert_equal(out['result'][0]['type'], 'Metadex Order')
        assert_equal(out['result'][0]['propertyId'], 4)
        assert_equal(out['result'][0]['propertyname'], 'lihki')
        assert_equal(out['result'][0]['amount'], '1000.00000000')
        assert_equal(out['result'][0]['desire property'], 5)
        assert_equal(out['result'][0]['desired value'], '2000.00000000')

        assert_equal(out['result'][1]['type_int'], 25)
        assert_equal(out['result'][1]['type'], 'Metadex Order')
        assert_equal(out['result'][1]['propertyId'], 4)
        assert_equal(out['result'][1]['propertyname'], 'lihki')
        assert_equal(out['result'][1]['amount'], '500.00000000')
        assert_equal(out['result'][1]['desire property'], 5)
        assert_equal(out['result'][1]['desired value'], '2000.00000000')


        self.log.info("Checking trade for property pair")
        params = str([4, 5, 10]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getmdextradehistoryforpair",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result'][0]['block'], 208)
        assert_equal(out['result'][0]['unitprice'], '2.00000000000000000000000000000000000000000000000000')
        assert_equal(out['result'][0]['inverseprice'], '0.50000000000000000000000000000000000000000000000000')
        assert_equal(out['result'][0]['amountsold'], '1000.00000000')
        assert_equal(out['result'][0]['amountreceived'], '2000.00000000')


        self.log.info("Sending 20000000000 lihki tokens to fourth address")
        params = str([addresses[0], addresses[3], 4, "20000000000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'20000000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending 1 dan token to third address")
        params = str([addresses[1], addresses[2], 5, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking dan tokens in receiver address")
        params = str([addresses[2], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending a big trade in MetaDEx")
        params = str([addresses[3], 4, "10000000000", 5, "0.00000002"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendtrade",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        txid = out['result']

        self.nodes[0].generate(1)


        self.log.info("Sending a small trade in MetaDEx")
        params = str([addresses[2], 5, "0.00000001", 4, "5000000000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendtrade",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking lihki tokens in third address")
        params = str([addresses[2], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'4997500000.00000000') # 5000000000 minus fees
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking dan tokens in fourth address")
        params = str([addresses[3], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'0.00000001')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking lihki tokens in fourth address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'10002000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Cancel specific order MetaDEx")
        params = str([addresses[3], txid]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendcancel_order",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trade in orderbook")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])


        self.log.info("Checking lihki tokens in fourth address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'15002000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending the trade again")
        params = str([addresses[3], 4, "2000000", 5, "4000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendtrade",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trade in orderbook")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[3])
        assert_equal(out['result'][0]['propertyidforsale'], 4)
        assert_equal(out['result'][0]['amountforsale'], '2000000.00000000')
        assert_equal(out['result'][0]['propertyiddesired'], 5)
        assert_equal(out['result'][0]['amountdesired'], '4000.00000000')
        txid = out['result'][0]['txid']


        self.log.info("Cancel by pair")
        params = str([addresses[3], 4, 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendcanceltradesbypair", params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trade in orderbook")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])


        self.log.info("Checking lihki tokens in fourth address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'15002000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending another trade again")
        params = str([addresses[3], 4, "2000000", 5, "4000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendtrade",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trade in orderbook")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[3])
        assert_equal(out['result'][0]['propertyidforsale'], 4)
        assert_equal(out['result'][0]['amountforsale'], '2000000.00000000')
        assert_equal(out['result'][0]['propertyiddesired'], 5)
        assert_equal(out['result'][0]['amountdesired'], '4000.00000000')
        txid = out['result'][0]['txid']


        self.log.info("Cancel by price")
        params = str([addresses[3], 4, "2000000", 5, "4000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendcanceltradesbyprice", params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trade in orderbook")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])


        self.log.info("Checking trade for property pair again")
        params = str([4, 5, 10]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getmdextradehistoryforpair",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.log.info("Checking lihki tokens in fourth address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'15002000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking fee restrictions")
        params = str([addresses[3], 4, "14997499400", 5, "4000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendtrade",params)
        # self.log.info(out)
        assert_equal(out['error']['message'], 'Sender has insufficient balance')

        conn.close()
        self.stop_nodes()

if __name__ == '__main__':
    MetaDExBasicsTest ().main ()
