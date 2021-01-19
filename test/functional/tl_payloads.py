#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Payloads RPCs ."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class PayloadsBasicsTest (BitcoinTestFramework):
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

        self.log.info("Preparing the workspace...")

        # mining 200 blocks
        self.nodes[0].generate(200)
        ################################################################################
        # Checking RPC calls for payloads retrieval                                    #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()


        addresses = []
        accounts = ["john", "doe", "another"]

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


        self.log.info("Creating oracles Contract")
        array = [0]
        params = str([addresses[0], "Oracle 1", 10000, "1", 4, "0.1", addresses[2], 0, array]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_create_oraclecontract",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Testing tl_createpayload_simplesend")
        params = str([1, '2000']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_simplesend", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00000180a0b787e905')


        self.log.info("Testing tl_createpayload_sendall")
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendall")
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0004')


        self.log.info("Testing tl_createpayload_issuancefixed")
        params = str([2, 0, 'lihki', 'wwww.lihki.com', 'data', '2000000', [0,2,3]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_issuancefixed", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '003202006c69686b6900777777772e6c69686b692e636f6d0064617461008080d287e2bc2d000203')


        self.log.info("Testing tl_createpayload_issuancemanaged")
        params = str([1, 0, 'dan', 'wwww.dan.com', 'data', [0,2,3,5]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_issuancemanaged", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0036010064616e00777777772e64616e2e636f6d00646174610000020305')


        self.log.info("Testing tl_createpayload_sendgrant")
        params = str([1, '51236']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendgrant", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00370180c8eef28e9501')


        self.log.info("Testing tl_createpayload_sendrevoke")
        params = str([3, '1854263']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendrevoke", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00380380aef9e5ce942a')


        self.log.info("Testing tl_createpayload_changeissuer")
        params = str([1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_changeissuer", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '004601')


        self.log.info("Testing tl_createpayload_sendactivation")
        params = str([1, 370000, 999]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendactivation", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], 'ffff03feff0301d0ca16e707')


        self.log.info("Testing tl_createpayload_senddeactivation")
        params = str([1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_senddeactivation", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], 'ffff03fdff0301')


        self.log.info("Testing tl_createpayload_sendalert")
        params = str([1, 5, 'alert message']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendalert", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], 'ffff03ffff030105616c657274206d65737361676500')


        self.log.info("Testing tl_createpayload_sendtrade")
        params = str([1, '200', 3, '542']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendtrade", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0019018090dfc04a0380bcc9f4c901')


        self.log.info("Testing tl_createpayload_createcontract")
        params = str([1, 4, "ALL/Lhk", 5000, "1", 4, "0.1", 0, [0,2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_createcontract", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00280104414c4c2f4c686b00882780c2d72f0480ade204000002')


        self.log.info("Testing tl_createpayload_tradecontract")
        params = str(['ALL/Lhk', '1000', '780.5', 1, '1']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_tradecontract", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '001d414c4c2f4c686b00e80780f991e1a2020101')


        self.log.info("Testing tl_createpayload_cancelallcontractsbyaddress")
        params = str(['ALL/Lhk']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_cancelallcontractsbyaddress", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '002005')


        self.log.info("Testing tl_createpayload_closeposition")
        params = str([9]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_closeposition", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '002109')


        self.log.info("Testing tl_createpayload_sendissuance_pegged")
        params = str([2, 0, 'Pegged', 5, 'ALL/Lhk', '2541']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendissuance_pegged", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00640200506567676564000505ed13')


        self.log.info("Testing tl_createpayload_send_pegged")
        params = str(['Pegged', '50']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_send_pegged", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00660080e497d012')


        self.log.info("Testing tl_createpayload_redemption_pegged")
        params = str(['Pegged', '50', 'ALL/Lhk']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_redemption_pegged", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0065000580e497d012')


        self.log.info("Testing tl_createpayload_cancelorderbyblock")
        params = str([560, 2]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_cancelorderbyblock", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0022b00402')


        self.log.info("Testing tl_createpayload_dexoffer")
        params = str([ 4, "1000", "20", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_dexoffer", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '01140480d0dbc3f40280a8d6b907fa01e80701')


        self.log.info("Testing tl_createpayload_dexaccept")
        params = str([4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_dexaccept", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00160480d0dbc3f402')


        self.log.info("Testing tl_createpayload_sendvesting")
        params = str(["1000.06"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_sendvesting", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '000580ebc9c6f402')


        self.log.info("Testing tl_createpayload_instant_trade")
        params = str([4, '1000', 5,'2000', 300]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_instant_trade", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '006e0480d0dbc3f402ac0205d00f')


        self.log.info("Testing tl_createpayload_instant_ltc_trade")
        params = str([4, '1000','1.0', 5]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_instant_ltc_trade", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00710480d0dbc3f40280c2d72f05')


        self.log.info("Testing tl_createpayload_contract_instant_trade")
        params = str([4, '1000',300,'2000.6', 1, '1']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_contract_instant_trade", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00720480d0dbc3f402ac0280ae85a4e9050101')


        self.log.info("Testing tl_createpayload_pnl_update")
        params = str([4, '1000',300]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_pnl_update", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '006f0480d0dbc3f402ac02')


        self.log.info("Testing tl_createpayload_transfer")
        params = str([0, 4, '100']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_transfer", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0070000480c8afa025')


        self.log.info("Testing tl_createpayload_dex_payment")
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_dex_payment")
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0075')


        self.log.info("Testing tl_createpayload_change_oracleadm")
        params = str(['ALL/Lhk']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_change_oracleadm", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '006805')


        self.log.info("Testing tl_createpayload_create_oraclecontract")
        params = str(['Oracle 1', 10000, '1', 4, '0.1', 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb', 0, [0,1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_create_oraclecontract", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00674f7261636c65203100904e80c2d72f0480ade204000001')


        self.log.info("Testing tl_createpayload_setoracle")
        params = str(['Oracle 1', '400.3', '620.2', '586.7']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_setoracle", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00690680a7e58f950180b2b885e70180cf84c8da01')


        self.log.info("Testing tl_createpayload_closeoracle")
        params = str(['Oracle 1']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_closeoracle", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '006b06')


        self.log.info("Testing tl_createpayload_new_id_registration")
        params = str(['tradelayer.org', 'Tradelayer']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_new_id_registration", params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '007374726164656c617965722e6f72670054726164656c6179657200')


        self.log.info("Testing tl_createpayload_update_id_registration")
        params = str(['Oracle 1']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_update_id_registration")
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '0074')

        self.log.info("Testing tl_createpayload_attestation")
        # sha-1 hash (input: blockpo)
        params = str(['2507f85b9992c0d518f56c8e1a7cd43e1282c898']).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_createpayload_attestation",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'], '00763235303766383562393939326330643531386635366338653161376364343365313238326338393800')


        self.stop_nodes()

if __name__ == '__main__':
    PayloadsBasicsTest ().main ()
