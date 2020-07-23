#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""KYC in Trade Channels."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class TradeChannelsKYCBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # Use self.extra_args to change command-line arguments for the nodes
        # self.extra_args = [["-deprecatedrpc=signrawtransaction"]]
        self.extra_args = [["-txindex=1"]] #, "-minrelaytxfee=0.00000000"

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
        accounts = ["john", "marks", "tango"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Creating addresses")
        params = str(["jhon"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getnewaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        addresses.append(out['result'])


        params = str(["marks"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getnewaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        addresses.append(out['result'])


        params = str(["tango"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getnewaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        addresses.append(out['result'])

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
        self.log.info("multisig:"+multisig)
        self.log.info("address0:"+addresses[0])
        self.log.info("address1:"+addresses[1])

        self.log.info("Checking multisig")
        params = str([multisig]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "validateaddress",params)
        # self.log.info(out)
        scriptPubKey = out['result']['scriptPubKey']
        assert_equal(out['result']['embedded']['script'],'multisig')


        self.log.info("Funding addresses with LTC")
        amount = 4.0
        params = str([addresses[0], amount]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)


        params = str([addresses[1], amount]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)


        self.nodes[0].generate(1)

        self.log.info("Self Attestation for address0")
        params = str([addresses[0],addresses[0],""]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_attestation", params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Creating new tokens  (lihki)")
        array = [0,1]
        params = str([addresses[0],2,0,"lihki","","","20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Creating new tokens  (dan)")
        array = [1]
        params = str([addresses[1],2,0,"dan","","","20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking LTC balances for addresses[0]")
        params = str(["jhon"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getbalance",params)
        # assert_equal(out['error'], None)
        self.log.info(out)


        self.log.info("Commiting to trade channel")
        params = str([addresses[0], multisig, 4, '1000']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        # assert_equal(out['error'], None)
        self.log.info(out)

        self.log.info("Checking LTC balances for addresses[0]")
        params = str(["jhon"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getbalance",params)
        # assert_equal(out['error'], None)
        self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking LTC balances for addresses[0]")
        params = str(["jhon"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getbalance",params)
        # assert_equal(out['error'], None)
        self.log.info(out)

        self.log.info("Checking the commit")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_commits",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['sender'], addresses[0])
        assert_equal(out['result'][0]['propertyId'], '4')
        assert_equal(out['result'][0]['amount'], '1000.00000000')

        params = str([addresses[1], 0.2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress", params)
        self.log.info(out)

        params = str([addresses[2], 0.2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        # tango is the admin for Tradelayer KYCs
        self.log.info("Creating KYC of Tradelayer ")
        params = str([addresses[2], "TradeLayer.org","TradeLayer registrars"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_new_id_registration",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Attestation for address1")
        params = str([addresses[2], addresses[1],""]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_attestation", params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Funding address1")
        params = str([addresses[1], 1.1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress", params)
        # self.log.info(out)
        txid = out['result']
        self.log.info("txid:"+txid)

        self.nodes[0].generate(1)

        self.log.info("Checking the transaction")
        params = str([txid]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "gettransaction", params)
        self.log.info(out)
        vout = out['result']['details'][0]['vout']
        self.log.info('vout:'+str(vout))

        self.log.info("Creating raw input")
        params = str(['', txid, vout]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_input",params)
        # self.log.info(out)
        hex = out['result']

        # Destination here is yourself (we are sending 1 LTC from addresses[1] to addresses[0])
        self.log.info("Creating raw reference")
        params = str([hex, addresses[0], 0.5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_reference",params)
        # self.log.info(out)

        hex = out['result']

        self.log.info("Creating payload for instant trade")
        params = str([4, '10',300, 5, '20']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_instant_trade", params)
        assert_equal(out['error'], None)
        payload = out['result']
        # self.log.info(payload)


        self.log.info("Adding the op return to transaction")
        params = str([hex,payload]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_opreturn", params)
        assert_equal(out['error'], None)
        self.log.info(out)
        hex = out['result']
        # self.log.info(hex)


        params = '["'+hex+'",[],["'+privatekey0+'"]]'
        self.log.info("Signing raw transaction with address 0")
        # self.log.info(params)
        out = tradelayer_HTTP(conn, headers, False, "signrawtransaction",params)
        assert_equal(out['error'], None)
        self.log.info(out)
        hex = out['result']['hex']
        # self.log.info(hex)


        params = '["'+hex+'",[],["'+privatekey1+'"]]'
        self.log.info("Signing raw transaction with address 1")
        # self.log.info(params)
        out = tradelayer_HTTP(conn, headers, False, "signrawtransaction",params)
        assert_equal(out['error'], None)
        self.log.info(out)
        hex = out['result']['hex']
        # self.log.info(hex)

        self.log.info("Sending raw transaction")
        params = '["'+hex+'", true]'
        out = tradelayer_HTTP(conn, headers, False, "sendrawtransaction",params)
        assert_equal(out['error'], None)
        self.log.info(out)
        tx = out['result']

        self.nodes[0].generate(1)

        self.log.info("Checking transaction")
        params = str([tx, 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getrawtransaction",params)
        self.log.info(out)

        # addresses[1] has now 0 tokens
        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'0.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.stop_nodes()

if __name__ == '__main__':
    TradeChannelsKYCBasicsTest ().main ()
