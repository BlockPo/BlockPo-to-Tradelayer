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


        self.log.info("Checking tokens in issuer address")
        params = str([addresses[0], 4]).replace("'",'"')
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


        self.log.info("Sending a DEx sell tokens offer")
        params = str([addresses[0], 4, "1000", "1", 250, "0.00001", "2", 1]).replace("'",'"')
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
        assert_equal(out['result'][0]['ltcsdesired'], '1.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000.00000000')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.00100000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')


        self.log.info("Checking tokens in issuer address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'19999999000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("1st Restart for the node")
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
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['propertyid'],4)
        assert_equal(out['result']['issuer'],addresses[0])
        assert_equal(out['result']['name'],'lihki')
        assert_equal(out['result']['data'],'')
        assert_equal(out['result']['url'],'')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'20000000000.00000000')

        self.log.info("Persistence: checking tokens in issuer address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'19999999000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Persistence: checking attestations")
        out = tradelayer_HTTP(conn, headers, False, "tl_list_attestation")
        # self.log.info(out)

        result2 = []
        registers = out['result']
        for addr in addresses:
            for i in registers:
                if i['att sender'] == addr and i['att receiver'] == addr and i['kyc_id'] == 0:
                     result2.append(True)

        for i in registers:
            if i['att sender'] == notaryAddr and i['kyc_id'] == 1:
                result2.append(True)

        assert_equal(result2, [True, True, True, True])

        self.log.info("Persistence: checking the offer in DEx")
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

        self.nodes[0].generate(1)

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
        assert_equal(out['result'][0]['accepts'][0]['blocksleft'], 250)

        self.log.info("2nd Restart for the node")
        self.restart_node(0) #2nd restart


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Persistence: checking the offer status")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2 )
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
        assert_equal(out['result'][0]['accepts'][0]['blocksleft'], 250)


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
        assert_equal(out['result']['balance'], '19999999000.00000000')
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


        self.log.info("3rd Restart for the node")
        self.restart_node(0) #3rd restart


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        self.log.info("Persistence: checking token balance in seller address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '19999999000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Persistence: checking token balance in buyer address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '1000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Persistence: checking the offer status")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'],[])

        self.log.info("Creating new tokens  (dan)")
        array = [0]
        params = str([addresses[1],2,0,"dan","","","10000",array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendissuancefixed",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

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
        assert_equal(out['result']['totaltokens'],'10000.00000000')

        self.log.info("Checking tokens balance in dan's owner ")
        params = str([addresses[1], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'10000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Sending a trade in MetaDEx")
        params = str([addresses[0], 4, "100", 5, "200"]).replace("'",'"')
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
        assert_equal(out['result'][0]['amountforsale'],'100.00000000')
        assert_equal(out['result'][0]['propertyiddesired'],5)
        assert_equal(out['result'][0]['amountdesired'],'200.00000000')


        self.log.info("4th Restart for the node")
        self.restart_node(0)


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


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
        assert_equal(out['result']['totaltokens'],'10000.00000000')

        self.log.info("Persistence: checking tokens balance in dan's owner ")
        params = str([addresses[1], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'10000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Persistence: checking the trade in orderbook")
        params = str([4])
        out = tradelayer_HTTP(conn, headers, True, "tl_getorderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[0])
        assert_equal(out['result'][0]['propertyidforsale'],4)
        assert_equal(out['result'][0]['amountforsale'],'100.00000000')
        assert_equal(out['result'][0]['propertyiddesired'],5)
        assert_equal(out['result'][0]['amountdesired'],'200.00000000')


        self.log.info("Sending a second trade in MetaDEx")
        params = str([addresses[1], 5, "200", 4, "100"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_sendtrade",params)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking tokens balance for first address")
        params = str([addresses[0], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'19999998900.04000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        params = str([addresses[0], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'200.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking tokens balance for second address")
        params = str([addresses[1], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1099.95000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        params = str([addresses[1], 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'9800.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


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
        # assert_equal(out['result']['expiry block'],1205)
        assert_equal(out['result']['status'], 'active')


        self.log.info("Commiting to trade channel")
        params = str([addresses[0], multisig, 4, '100']).replace("'",'"')
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
        assert_equal(out['result'][0]['amount'], '100.00000000')
        # assert_equal(out['result'][0]['block'],206)


        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '100.00000000')

        self.log.info("5th Restart for the node")
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
        assert_equal(out['result']['second address'], addresses[1])
        # assert_equal(out['result']['expiry block'],1205)
        assert_equal(out['result']['status'], 'active')


        self.log.info("Persistence: checking the commit")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_check_commits",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['sender'], addresses[0])
        assert_equal(out['result'][0]['propertyId'], '4')
        assert_equal(out['result'][0]['amount'], '100.00000000')
        # assert_equal(out['result'][0]['block'],206)


        self.log.info("Persistence: checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '100.00000000')


        self.log.info("Withdrawal from channel ")
        params = str([addresses[0], multisig, 4, '100']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_withdrawal_fromchannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '100.00000000')

        self.log.info("6th Restart for the node")
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
        assert_equal(out['result']['channel reserve'], '100.00000000')

        self.log.info("mining 7 blocks")
        self.nodes[0].generate(7)


        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '0.00000000')

        self.log.info("Creating native Contract")
        array = [0]
        params = str([addresses[0], 1, 4, "ALL/Lhk", 5000, "1", 4, "0.1", 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_createcontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the native contract")
        params = str([6])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],6)
        assert_equal(out['result']['name'],'ALL/Lhk')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '5000')
        assert_equal(out['result']['inverse quoted'], '0')

        self.log.info("Buying contracts")
        params = str([addresses[1], "ALL/Lhk", "10", "780.5", 1, "1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_tradecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        hash = str(out['result']).replace("'","")
        # self.log.info(hash)

        self.nodes[0].generate(1)


        self.log.info("Checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 6)
        assert_equal(out['result'][0]['amountforsale'], '10.00000000')
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '780.50000000')
        assert_equal(out['result'][0]['block'], 224)

        self.log.info("7th Restart for the node")
        self.restart_node(0)


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Persistence: checking the native contract")
        params = str([6])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],6)
        assert_equal(out['result']['name'],'ALL/Lhk')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '5000')
        assert_equal(out['result']['inverse quoted'], '0')


        self.log.info("Persistence: checking orderbook")
        params = str(["ALL/Lhk", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract_orderbook",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['address'], addresses[1])
        assert_equal(out['result'][0]['contractid'], 6)
        assert_equal(out['result'][0]['amountforsale'], '10.00000000')
        assert_equal(out['result'][0]['tradingaction'], 1)
        assert_equal(out['result'][0]['effectiveprice'], '780.50000000')
        assert_equal(out['result'][0]['block'], 224)

        self.log.info("Persistence: checking LTC volume in DEx")
        params = str([4, 1, 300]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getdexvolume",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['volume'],'1.00000000')

        self.log.info("Creating oracles Contract")
        array = [0]
        params = str([addresses[0], "Oracle 1", 10000, "1", 4, "0.1", addresses[2], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking the oracle contract")
        params = str([7])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'],7)
        assert_equal(out['result']['name'],'Oracle 1')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '0')
        assert_equal(out['result']['low price'], '0')
        assert_equal(out['result']['last close price'], '0')


        self.log.info("Setting oracle prices")
        params = str([addresses[0], "Oracle 1", "602.1", "450.6", "500.1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_setoracle",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking the prices in oracle")
        params = str([7])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        assert_equal(out['result']['hight price'], '602.1')
        assert_equal(out['result']['low price'], '450.6')
        assert_equal(out['result']['last close price'], '500.1')

        self.log.info("8th Restart for the node")
        self.restart_node(0)


        url = urllib.parse.urlparse(self.nodes[0].url)
        #New authpair
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("Checking the oracle contract")
        params = str([7])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['propertyid'],7)
        assert_equal(out['result']['name'],'Oracle 1')
        assert_equal(out['result']['issuer'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '602.1')
        assert_equal(out['result']['low price'], '450.6')
        assert_equal(out['result']['last close price'], '500.1')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    PersistenceBasicsTest ().main ()
