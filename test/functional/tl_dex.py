#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test DEx functions ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class DExBasicsTest (BitcoinTestFramework):
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

    # NOTE: this is the basis, of course, we can build more objects and classes
    # in order to do a better work

    def run_test(self):

        self.log.info("Preparing the workspace...")

        # mining 200 blocks
        self.nodes[0].generate(200)

        ################################################################################
        # Checking RPC tl_senddexoffer and tl_getactivedexsells (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another", "marks"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating sender address")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        self.log.info("Funding addresses with LTC")
        amount = 2
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)

        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","90000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        # self.log.info(out)

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

        self.log.info("Checking the property")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'90000000.00000000')

        self.log.info("Checking token balance in every address")
        for addr in addresses:
            params = str([addr, 4]).replace("'",'"')
            out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
            # self.log.info(out)
            amount = ""
            assert_equal(out['error'], None)
            if addr == addresses[0]:
                 amount = '90000000.00000000'
            else:
                 amount = '0.00000000'

            assert_equal(out['result']['balance'], amount)
            assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Sending 1000 tokens to second address")
        params = str([addresses[0], addresses[3], 4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Sending a DEx sell tokens offer")
        params = str([addresses[0], 4, "1000", "20", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[0])
        assert_equal(out['result'][0]['ltcsdesired'], '20.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000.00000000')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.02000000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        self.log.info("Cancelling the DEx offer")
        params = str([addresses[0], 4, "1000", "1", 250, "0.00001", "2", 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer in DEx again")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])


        self.log.info("Putting a new DEx sell tokens offer")
        params = str([addresses[0], 4, "1000", "30", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
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
        assert_equal(out['result'][0]['ltcsdesired'], '30.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000.00000000')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.03000000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')


        self.log.info("Changing the new DEx sell tokens offer")
        params = str([addresses[0], 4, "1000", "1", 250, "0.00001", "2", 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
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
        assert_equal(out['result'][0]['ltcsdesired'], '1.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000.00000000')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00100000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')


        self.log.info("Accepting the full offer")
        params = str([addresses[1], addresses[0], 4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(10)

        self.log.info("Checking the offer status")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[0])
        assert_equal(out['result'][0]['ltcsdesired'], '0.00000000')
        assert_equal(out['result'][0]['amountavailable'], '0.00000000')
        assert_equal(out['result'][0]['amountoffered'], '1000.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00100000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        assert_equal(out['result'][0]['accepts'][0]['buyer'], addresses[1])
        assert_equal(out['result'][0]['accepts'][0]['amountdesired'], '1000.00000000')
        assert_equal(out['result'][0]['accepts'][0]['ltcstopay'], '1.00000000')
        assert_equal(out['result'][0]['accepts'][0]['block'], 208)
        assert_equal(out['result'][0]['accepts'][0]['blocksleft'], 241)

        self.log.info("Paying the tokens")
        params = str([addresses[1], addresses[0], "1.0"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send_dex_payment",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking token balance in seller address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '89998000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking token balance in buyer address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '1000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking the offer status")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])


        self.log.info("Sending a DEx buy tokens offer")
        params = str([addresses[2], 4, "1000", "1", 250, "0.00001", "1", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx ")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 1)
        assert_equal(out['result'][0]['buyer'], addresses[2])
        assert_equal(out['result'][0]['ltcstopay'], '1.00000000')
        assert_equal(out['result'][0]['amountdesired'], '1000.00000000')
        assert_equal(out['result'][0]['amountaccepted'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00100000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        self.log.info("Cancelling the DEx offer")
        params = str([addresses[2], 4, "1000", "1", 250, "0.00001", "1", 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx ")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])


        self.log.info("Sending a new buy token offer")
        params = str([addresses[2], 4, "1000", "1", 250, "0.00001", "1", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx ")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 1)
        assert_equal(out['result'][0]['buyer'], addresses[2])
        assert_equal(out['result'][0]['ltcstopay'], '1.00000000')
        assert_equal(out['result'][0]['amountdesired'], '1000.00000000')
        assert_equal(out['result'][0]['amountaccepted'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00100000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')


        self.log.info("Accepting the full offer")
        params = str([addresses[3], addresses[2], 4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(10)

        self.log.info("Checking the offer status")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 1)
        assert_equal(out['result'][0]['buyer'], addresses[2])
        # assert_equal(out['result'][0]['ltcstopay'], '1.00000000')

        assert_equal(out['result'][0]['amountdesired'], '0.00000000')
        assert_equal(out['result'][0]['amountaccepted'], '1000.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00100000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        assert_equal(out['result'][0]['accepts'][0]['seller'], addresses[3])
        assert_equal(out['result'][0]['accepts'][0]['amountoffered'], '1000.00000000')
        assert_equal(out['result'][0]['accepts'][0]['ltcstoreceive'], '1.00000000')
        assert_equal(out['result'][0]['accepts'][0]['blocksleft'], 241)

        self.log.info("Paying the tokens")
        params = str([addresses[2], addresses[3], "1.0"]).replace("'",'"')
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


        self.log.info("Checking token balance in seller address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '0.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking a small token offer")
        params = str([addresses[0], 4, "0.00000001", "20", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[0])
        assert_equal(out['result'][0]['ltcsdesired'], '20.00000000')
        assert_equal(out['result'][0]['amountavailable'], '0.00000001')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '2000000000.00000000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        self.log.info("Checking a smaller token offer")
        params = str([addresses[0], 4, "0.000000001", "20", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        assert_equal(out['error']['code'], -3)
        assert_equal(out['error']['message'],'Invalid amount ???')
        # self.log.info(out)

        self.log.info("Sending 20000000 tokens to second address")
        params = str([addresses[0], addresses[1], 4, "20000000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'20001000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking a big token offer")
        params = str([addresses[1], 4, "10000000.98765432", "20", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[1])
        assert_equal(out['result'][0]['ltcsdesired'], '20.00000000')
        assert_equal(out['result'][0]['amountavailable'], '10000000.98765432')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00000200') # should be: 0.00000199999 (here we are rounding up)
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')


        self.log.info("Cancelling the DEx offer")
        params = str([addresses[1], 4, "10000000.98765432", "20", 250, "0.00001", "2", 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer in DEx ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], [])


        self.log.info("Testing the payment window")

        self.log.info("Sending a new DEx offer")
        params = str([addresses[1], 4, "600", "1", 10, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_senddexoffer",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[1])
        assert_equal(out['result'][0]['ltcsdesired'], '1.00000000')
        assert_equal(out['result'][0]['amountavailable'], '600.00000000')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00166667') # (here we are rounding up)
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        self.log.info("Accepting the full offer")
        params = str([addresses[3], addresses[1], 4, "600"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer in DEx ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)

        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[1])
        assert_equal(out['result'][0]['ltcsdesired'], '0.00000000')

        assert_equal(out['result'][0]['amountavailable'], '0.00000000')
        assert_equal(out['result'][0]['amountoffered'], '600.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00166667')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        assert_equal(out['result'][0]['accepts'][0]['buyer'], addresses[3])
        assert_equal(out['result'][0]['accepts'][0]['amountdesired'], '600.00000000')
        assert_equal(out['result'][0]['accepts'][0]['ltcstopay'], '1.00000000')
        assert_equal(out['result'][0]['accepts'][0]['blocksleft'], 10)

        self.log.info("Mining 10 blocks ")
        self.nodes[0].generate(10)

        self.log.info("Checking the offer in DEx again")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['accepts'], [])

        conn.close()
        self.stop_nodes()

if __name__ == '__main__':
    DExBasicsTest ().main ()
