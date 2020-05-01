#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Trade Channels."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class ChannelsBasicsTest (BitcoinTestFramework):
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
        # Checking all tl rpcs related with tradechannels (in the first 200 blocks of the chain) #
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

        self.log.info("Creating multisig address")
        params = str([2, [addresses[0], addresses[1]]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "addmultisigaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        multisig = out['result']['address']
        redeemScript = out['result']['redeemScript']
        # self.log.info("multisig:"+multisig)
        # self.log.info("address0:"+addresses[0])
        # self.log.info("address1:"+addresses[1])


        self.log.info("Checking multisig")
        params = str([multisig]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "validateaddress",params)
        # self.log.info(out)
        assert_equal(out['result']['embedded']['script'],'multisig')

        self.log.info("Funding addresses with LTC")
        amount = 1.1
        tradelayer_fundingAddresses(addresses, amount, conn, headers)


        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)


        self.log.info("Creating new tokens  (lihki)")
        array = [0]
        params = str([addresses[0],2,0,"lihki","","","20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Creating new tokens  (dan)")
        array = [0]
        params = str([addresses[1],2,0,"dan","","","10000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Self Attestation for addresses")
        tradelayer_selfAttestation(addresses,conn, headers)

        # TODO:
        # self.log.info("Checking attestations")


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

        self.log.info("Checking the property: dan")
        params = str([5])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['issuer'],addresses[1])
        assert_equal(out['result']['name'],'dan')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'10000000000.00000000')


        self.log.info("Creating trade channel")
        params = str([addresses[0], addresses[1], multisig, 1000]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_create_channel",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the trade channel")
        params = str([multisig]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getchannel_info",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['multisig address'], multisig)
        assert_equal(out['result']['first address'], addresses[0])
        assert_equal(out['result']['second address'], addresses[1])
        assert_equal(out['result']['expiry block'],1205)
        assert_equal(out['result']['status'], 'active')


        self.log.info("Commiting to trade channel") #NOTE: we have to use a multisig in addresses[0]
        params = str([addresses[0], multisig, 4, '1000']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the commit")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_commits",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['sender'], addresses[0])
        assert_equal(out['result'][0]['propertyId'], '4')
        assert_equal(out['result'][0]['amount'], '1000.00000000')
        assert_equal(out['result'][0]['block'],206)

        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '1000.00000000')

        self.log.info("Commiting to trade channel again") #NOTE: we have to use a multisig in addresses[0]
        params = str([addresses[0], multisig, 4, '875']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the commit")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_commits",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        if out['result'][0]['block'] == 207:
            i = 0
        else:
            i = 1

        assert_equal(out['result'][i]['sender'], addresses[0])
        assert_equal(out['result'][i]['propertyId'], '4')
        assert_equal(out['result'][i]['amount'], '875.00000000')
        assert_equal(out['result'][1-i]['sender'], addresses[0])
        assert_equal(out['result'][1-i]['propertyId'], '4')
        assert_equal(out['result'][1-i]['amount'], '1000.00000000')
        assert_equal(out['result'][1-i]['block'],206)


        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '1875.00000000')


        self.log.info("Withdrawal from channel ")
        params = str([addresses[0], multisig, 4, '700']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_withdrawal_fromchannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '1875.00000000')


        self.log.info("mining 7 blocks")
        self.nodes[0].generate(7)


        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '1175.00000000')


        # addr0 making instant trade with addr1 (tokens for tokens)

        self.log.info("Funding the channel with 2 LTCs")
        params = str([multisig, 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress", params)
        # self.log.info(out)
        txid = out['result']

        self.nodes[0].generate(1)


        self.log.info("Checking the transaction")
        params = str([txid]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "gettransaction", params)
        # self.log.info(out)
        vout = out['result']['details'][0]['vout']
        # self.log.info(vout)


        self.log.info("Creating rawtx transaction")
        params = '[[{"txid":"'+txid+'","vout":'+str(vout)+'}],{"'+multisig+'":1.98, "'+addresses[1]+'":0.01}]'
        out = tradelayer_HTTP(conn, headers, False, "createrawtransaction", params)
        # assert_equal(out['error'], None)
        hex = out['result']
        # self.log.info(out)


        self.log.info("Creating payload for instant trade")
        params = str([4, '1000',300, 5, '2000']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_instant_trade", params)
        # assert_equal(out['error'], None)
        payload = out['result']
        # self.log.info(out)

        self.log.info("Adding the op return to transaction")
        params = str([hex,payload]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_opreturn", params)
        # assert_equal(out['error'], None)
        # self.log.info(out)
        raw = out['result']

        # NOTE: see multi.sh test
        # self.log.info("Signing raw transaction with address 0")
        # params = str([addreses[0]]).replace("'",'"')
        # out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_opreturn", params)
        # assert_equal(out['error'], None)
        # self.log.info(out)

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    ChannelsBasicsTest ().main ()
