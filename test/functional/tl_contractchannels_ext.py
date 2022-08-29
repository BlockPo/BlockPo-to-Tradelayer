#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Contract Trade in Channels."""

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
        # Use self.extra_args to change command-line arguments for the nodes
        # self.extra_args = [["-deprecatedrpc=signrawtransaction"]]
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
        # Checking all tl rpcs related with tradechannels (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another", "marks"]

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
        self.log.info("multisig:"+multisig)
        self.log.info("address0:"+addresses[0])
        self.log.info("address1:"+addresses[1])

        self.log.info("Checking multisig")
        params = str([multisig]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "validateaddress",params)
        # self.log.info(out)
        scriptPubKey = out['result']['scriptPubKey']


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

        self.log.info("Creating oracles Contract")
        array = [0]
        params = str([addresses[0], "Oracle 1", 10000, "1", 4, "0.1", addresses[2], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)
        self.log.info("Checking the oracle contract")
        params = '["1"]'
        out = tradelayer_HTTP(conn, headers, True, "tl_getcontract",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        assert_equal(out['result']['contractid'], 1)
        assert_equal(out['result']['name'],'Oracle 1')
        assert_equal(out['result']['admin'], addresses[0])
        assert_equal(out['result']['notional size'], '1')
        assert_equal(out['result']['collateral currency'], '4')
        assert_equal(out['result']['margin requirement'], '0.1')
        assert_equal(out['result']['blocks until expiration'], '10000')
        assert_equal(out['result']['inverse quoted'], '0')
        assert_equal(out['result']['hight price'], '0')
        assert_equal(out['result']['low price'], '0')
        assert_equal(out['result']['last close price'], '0')


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

        assert_equal(result, [True, True, True, True, True])

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


        self.log.info("Sending 2000 lihki cons to Address1 (tl_send)")
        params = str([addresses[0], addresses[1], 4, "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send",params)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Commiting to trade channel")
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
        # assert_equal(out['result'][0]['block'],206)

        self.log.info("Address 1 commiting to trade channel")
        params = str([addresses[1], multisig, 4, '1000']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_commit_tochannel",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking reserve in channel")
        params = str([multisig, 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_get_channelreserve",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['channel reserve'], '2000.00000000')


        # addr0 making instant trade with addr1 (contracts)
        self.log.info("Funding the multisig with 2 LTC")
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
        # self.log.info('vout:' + str(vout))


        self.log.info("Creating raw input")
        params = str(['', txid, vout]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_input",params)
        # self.log.info(out)
        hex = out['result']

       # LOGIC: sender(multisig), receiver(first address)

        # Destination here is addresses[0] first address
        self.log.info("Creating raw reference")
        params = str([hex, addresses[0], 0.1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_reference",params)
        # self.log.info(out)
        hex = out['result']

        # Destination here is addresses[1] second address
        self.log.info("Creating another raw reference")
        params = str([hex, addresses[1], 0.1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_reference",params)
        # self.log.info(out)
        hex = out['result']


        self.log.info("Creating payload for instant trade")
        # Args: contractId, amount, blockheight expiry, effective price, trading action (1: buy, 2: sell), leverage (1-10x)
        params = str([1, '1', 50000, '200.5', 1, '1']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_contract_instant_trade", params)
        assert_equal(out['error'], None)
        payload = out['result']
        self.log.info(payload)

        self.log.info("Adding the op return to transaction")
        params = str([hex,payload]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_opreturn", params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        hex = out['result']
        # self.log.info(hex)


        params = '["'+hex+'",[{"txid":"'+txid+'","vout":'+str(vout)+', "scriptPubKey":"'+scriptPubKey+'","redeemScript":"'+redeemScript+'","amount":2}],["'+privatekey0+'"]]'
        self.log.info("Signing raw transaction with address 0")
        # self.log.info(params)
        out = tradelayer_HTTP(conn, headers, False, "signrawtransaction",params)
        # assert_equal(out['error'], None)
        hex = out['result']['hex']
        # self.log.info(out)

        self.log.info("Signing raw transaction with address 1")
        params = '["'+hex+'",[{"txid":"'+txid+'","vout":'+str(vout)+', "scriptPubKey":"'+scriptPubKey+'","redeemScript":"'+redeemScript+'","amount":2}],["'+privatekey1+'"]]'
        out = tradelayer_HTTP(conn, headers, False, "signrawtransaction",params)
        assert_equal(out['error'], None)
        hex = out['result']['hex']
        # self.log.info(hex)


        self.log.info("Sending raw transaction")
        params = '["'+hex+'", true]'
        out = tradelayer_HTTP(conn, headers, False, "sendrawtransaction",params)
        # assert_equal(out['error'], None)
        # self.log.info(out)
        tx = out['result']

        # params = '["'+hex+'"]'
        # out = tradelayer_HTTP(conn, headers, False, "decoderawtransaction",params)
        # # self.log.info(out)

        # params = '["'+tx+'", true]'
        # out = tradelayer_HTTP(conn, headers, False, "getrawtransaction",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking position in first address")
        params = str([addresses[0], "Oracle 1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -1)

        self.log.info("Checking position in second address")
        params = str([addresses[1], "Oracle 1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 1)



        self.log.info("Funding the multisig with 1 LTC")
        params = str([multisig, 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "sendtoaddress", params)
        # self.log.info(out)
        txid = out['result']

        self.nodes[0].generate(1)


        self.log.info("Checking the transaction")
        params = str([txid]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "gettransaction", params)
        # self.log.info(out)
        vout = out['result']['details'][0]['vout']
        # self.log.info('vout:' + str(vout))



        self.log.info("Creating raw input")
        params = str(['', txid, vout]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_input",params)
        # self.log.info(out)
        hex = out['result']


        # Destination here is addresses[0] first address
        self.log.info("Creating raw reference")
        params = str([hex, addresses[0], 0.1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_reference",params)
        # self.log.info(out)
        hex = out['result']

        # Destination here is addresses[1] second address
        self.log.info("Creating another raw reference")
        params = str([hex, addresses[1], 0.1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_reference",params)
        # self.log.info(out)
        hex = out['result']


        self.log.info("Creating payload for instant trade")
        # Args: contractId, amount, blockheight expiry, effective price, trading action (1: buy, 2: sell), leverage (1-10x)
        params = str([1, '90', 50000, '200.5', 1, '1']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_contract_instant_trade", params)
        assert_equal(out['error'], None)
        payload = out['result']
        self.log.info(payload)

        self.log.info("Adding the op return to transaction")
        params = str([hex,payload]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createrawtx_opreturn", params)
        assert_equal(out['error'], None)
        # self.log.info(out)
        hex = out['result']
        # self.log.info(hex)


        params = '["'+hex+'",[{"txid":"'+txid+'","vout":'+str(vout)+', "scriptPubKey":"'+scriptPubKey+'","redeemScript":"'+redeemScript+'","amount":1}],["'+privatekey0+'"]]'
        self.log.info("Signing raw transaction with address 0")
        # self.log.info(params)
        out = tradelayer_HTTP(conn, headers, False, "signrawtransaction",params)
        # assert_equal(out['error'], None)
        hex = out['result']['hex']
        # self.log.info(out)

        self.log.info("Signing raw transaction with address 1")
        params = '["'+hex+'",[{"txid":"'+txid+'","vout":'+str(vout)+', "scriptPubKey":"'+scriptPubKey+'","redeemScript":"'+redeemScript+'","amount":1}],["'+privatekey1+'"]]'
        out = tradelayer_HTTP(conn, headers, False, "signrawtransaction",params)
        assert_equal(out['error'], None)
        hex = out['result']['hex']
        # self.log.info(hex)


        self.log.info("Sending raw transaction")
        params = '["'+hex+'", true]'
        out = tradelayer_HTTP(conn, headers, False, "sendrawtransaction",params)
        # assert_equal(out['error'], None)
        # self.log.info(out)
        tx = out['result']

        self.nodes[0].generate(1)


        self.log.info("Checking position in first address")
        params = str([addresses[0], "Oracle 1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], -91)

        self.log.info("Checking position in second address")
        params = str([addresses[1], "Oracle 1"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getposition",params)

        self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['position'], 91)


        self.stop_nodes()

if __name__ == '__main__':
    ChannelsBasicsTest ().main ()
