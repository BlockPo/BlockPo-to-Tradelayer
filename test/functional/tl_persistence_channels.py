#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Channel Persistence."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse
import time

class ChannelPersistenceBasicsTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # Use self.extra_args to change command-line arguments for the nodes
        self.extra_args = [["-txindex=1"]]  # NOTE: EXTREMELY IMPORTANT in order to use the persistence!

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


        self.log.info("Creating new tokens  (dan)")
        array = [0,1]
        params = str([addresses[0],2,0,"dan","","","20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Creating new tokens  (dav)")
        array = [0,1]
        params = str([addresses[1],2,0,"dav","","","20000000000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the property: lihki")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
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
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'],5)
        assert_equal(out['result']['issuer'],addresses[0])
        assert_equal(out['result']['name'],'dan')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'20000000000.00000000')


        self.log.info("Checking the property: dav")
        params = str([6])
        out = tradelayer_HTTP(conn, headers, False, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'], 6)
        assert_equal(out['result']['issuer'],addresses[1])
        assert_equal(out['result']['name'],'dav')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'20000000000.00000000')


        self.log.info("Checking tokens in issuer address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'20000000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        params = str([addresses[0], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'20000000000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


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
        # self.log.info(out)
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


        self.log.info("Sending 300 lihki tokens to receiver (sendgrant)")
        params = str([addresses[0], addresses[1], 4, "300"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'300.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Commiting to trade channel")
        params = str([addresses[0], multisig, 4, '100']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        params = str([addresses[0], multisig, 5, '200']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the commit")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_commits",params)
        self.log.info(out)
        # assert_equal(out['error'], None)
        # assert_equal(out['result'][0]['sender'], addresses[0])
        # assert_equal(out['result'][0]['propertyId'], '4')
        # assert_equal(out['result'][0]['amount'], '100.00000000')
        # assert_equal(out['result'][0]['block'],206)


        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '100.00000000')

        params = str([multisig, 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '200.00000000')

        self.log.info("1st Restart for the node")
        self.restart_node(0)


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Persistence: checking the trade channel")
        params = str([multisig]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getchannel_info",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['multisig address'], multisig)
        assert_equal(out['result']['first address'], addresses[0])
        assert_equal(out['result']['second address'], 'pending')
        # assert_equal(out['result']['expiry block'],1205)
        assert_equal(out['result']['status'], 'active')


        self.log.info("Persistence: checking the commit")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_commits",params)
        self.log.info(out)

        # assert_equal(out['error'], None)
        # assert_equal(out['result'][0]['sender'], addresses[0])
        # assert_equal(out['result'][0]['propertyId'], '4')
        # assert_equal(out['result'][0]['amount'], '100.00000000')
        # assert_equal(out['result'][0]['block'],206)


        self.log.info("Persistence: checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '100.00000000')


        params = str([multisig, 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '200.00000000')

        self.log.info("Commiting to trade channel (address 1)")
        params = str([addresses[1], multisig, 4, '300']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("2nd Restart for the node")
        self.restart_node(0)


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        out = tradelayer_HTTP(conn, headers, False, "tl_getinfo")
        # self.log.info(out)
        assert_equal(out['error'], None)
        block = out['result']['block']
        self.log.info("Blockheight: "+str(block))

        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '400.00000000')

        params = str([multisig, 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '200.00000000')

        self.log.info("Commiting to trade channel (address 1)")
        params = str([addresses[1], multisig, 6, '500']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("3nd Restart for the node")
        self.restart_node(0)


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '400.00000000')

        params = str([multisig, 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '200.00000000')


        params = str([multisig, 6]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '500.00000000')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    ChannelPersistenceBasicsTest ().main ()
