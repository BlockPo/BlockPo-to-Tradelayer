#!/usr/bin/env python3
# Copyright (c) 2015-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test Vesting tokens."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *

import os
import json
import http.client
import urllib.parse

class VestingBasicsTest (BitcoinTestFramework):
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

        # mining 1000 blocks, total budget: 14949.77187643 LTC
        for i in range(0,2):
            self.nodes[0].generate(500)
            blocks = 500*(i+1)
            self.log.info(str(blocks)+" blocks mined...")

        ################################################################################
        # Checking RPC tl_sendvesting and related (in the first 200 blocks of the chain) #
        ################################################################################

        url = urllib.parse.urlparse(self.nodes[0].url)

        #Old authpair
        authpair = url.username + ':' + url.password

        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        addresses = []
        accounts = ["john", "doe", "another", "mark"]

        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.connect()

        self.log.info("watching LTC general balance")
        params = str([""]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "getbalance",params)
        self.log.info(out)
        assert_equal(out['error'], None)

        adminAddress = 'QgKxFUBgR8y4xFy3s9ybpbDvYNKr4HTKPb'
        privkey = 'cTkpBcU7YzbJBi7U59whwahAMcYwKT78yjZ2zZCbLsCZ32Qp5Wta'

        self.log.info("importing admin address")
        params = str([privkey]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "importprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.log.info("watching private key of admin address")
        params = str([adminAddress]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "dumpprivkey",params)
        # self.log.info(out)
        assert_equal(out['error'], None)


        self.log.info("Creating addresses")
        addresses = tradelayer_createAddresses(accounts, conn, headers)

        addresses.append(adminAddress)

        # self.log.info(addresses)


        self.log.info("Funding addresses with LTC")
        amount = 2
        tradelayer_fundingAddresses(addresses, amount, conn, headers)

        self.nodes[0].generate(1)

        self.log.info("Checking the LTC balance in every account")
        tradelayer_checkingBalance(accounts, amount, conn, headers)

        self.log.info("Funding addresses[3] with 6000 LTC")
        amount = 1000
        params = str([addresses[3], amount]).replace("'",'"')
        for i in range(0,6):
            out = tradelayer_HTTP(conn, headers, False, "sendtoaddress",params)
            # self.log.info(out)
            assert_equal(out['error'], None)

            self.nodes[0].generate(1)

        self.log.info("Creating new tokens (sendissuancefixed)")
        array = [0]
        params = str([addresses[2],2,0,"lihki","","","90000000",array]).replace("'",'"')
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

        assert_equal(result, [True, True, True, True, True])


        self.log.info("Checking vesting tokens property")
        params = str([3])
        out = tradelayer_HTTP(conn, headers, True, "tl_getproperty",params)
        # self.log.info(out)
        assert_equal(out['result']['propertyid'],3)
        assert_equal(out['result']['name'],'Vesting Tokens')
        assert_equal(out['result']['data'],'Divisible Tokens')
        assert_equal(out['result']['url'],'www.tradelayer.org')
        assert_equal(out['result']['divisible'],True)
        assert_equal(out['result']['totaltokens'],'1500000.00000000')

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

        self.log.info("sendvesting from  adminAddress to first address")
        params = str([adminAddress, addresses[0], "2000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendvesting",params)
        # self.log.info(out)
        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking tokens in receiver address")
        params = str([addresses[0], 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')

        self.log.info("Checking unvested ALLs ")
        params = str([addresses[0]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getunvested",params)
        # self.log.info(out)
        assert_equal(out['result']['unvested'],'2000.00000000')


        self.log.info("Checking the time lock of one year")
        self.log.info("sendvesting from first to second address")
        params = str([addresses[0], addresses[1], "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendvesting",params)
        # self.log.info(out)

        assert_equal(out['error'], None)

        self.nodes[0].generate(1)

        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'0.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking unvested ALLs ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getunvested",params)
        # self.log.info(out)
        assert_equal(out['result']['unvested'],'0.00000000')

        out = tradelayer_HTTP(conn, headers, True, "tl_getinfo")
        block = out['result']['block']
        self.log.info(" block height :"+str(block))


        self.log.info("Waiting for one year")
        for i in range(20):
            self.nodes[0].generate(1)


        self.log.info("sendvesting from first to second address, again")
        params = str([addresses[0], addresses[1], "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_sendvesting",params)
        # self.log.info(out)

        assert_equal(out['error'], None)

        self.nodes[0].generate(1)


        self.log.info("Checking tokens in receiver address")
        params = str([addresses[1], 3]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'1000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking unvested ALLs ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getunvested",params)
        # self.log.info(out)
        assert_equal(out['result']['unvested'],'1000.00000000')

        # 200 LTC implies release 7.5% of ALLs from unvested to balance
        # NOTE: In regtest 200 LTC volume is equivalent to 20000 (x100) LTCs in testnet or mainnet
        self.log.info("Creating LTC volume in DEx")
        self.log.info("Sending a DEx sell tokens offer")
        params = str([addresses[2], 4, "1000", "200", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[2])
        assert_equal(out['result'][0]['ltcsdesired'], '200.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000.00000000')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.20000000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        self.log.info("Accepting the full offer")
        params = str([addresses[3], addresses[2], 4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer status")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[2])
        assert_equal(out['result'][0]['ltcsdesired'], '0.00000000')
        assert_equal(out['result'][0]['amountavailable'], '0.00000000')
        assert_equal(out['result'][0]['amountoffered'], '1000.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.20000000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        assert_equal(out['result'][0]['accepts'][0]['buyer'], addresses[3])
        assert_equal(out['result'][0]['accepts'][0]['amountdesired'], '1000.00000000')
        assert_equal(out['result'][0]['accepts'][0]['ltcstopay'], '200.00000000')
        # assert_equal(out['result'][0]['accepts'][0]['block'], 208)
        # assert_equal(out['result'][0]['accepts'][0]['blocksleft'], 241)


        self.log.info("Paying the tokens")
        params = str([addresses[3], addresses[2], "200"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send_dex_payment",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking token balance in buyer address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '1000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking LTC Volume")
        params = str([4, 1, 3000]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getdexvolume",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['volume'], '200.00000000')


        self.nodes[0].generate(1)


        self.log.info("Checking vesting in related address")
        params = str([addresses[1], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getbalance",params)
        self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'75.25749000')  # 7.5% of vesting (NOTE: check the round up)
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking unvested ALLs ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getunvested",params)
        self.log.info(out)
        assert_equal(out['result']['unvested'],'924.74251000')


        # 400 LTC implies release 15.05% of ALLs from unvested to balance
        # Remember: 400 LTCs in regtest are 40000 (x100) LTCs in testnet/mainnet
        self.log.info("Sending a DEx sell tokens offer")
        params = str([addresses[2], 4, "1000", "200", 250, "0.00001", "2", 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexoffer",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)

        self.log.info("Checking the offer in DEx")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[2])
        assert_equal(out['result'][0]['ltcsdesired'], '200.00000000')
        assert_equal(out['result'][0]['amountavailable'], '1000.00000000')
        assert_equal(out['result'][0]['amountoffered'], '0.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.20000000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        self.log.info("Accepting the full offer")
        params = str([addresses[3], addresses[2], 4, "1000"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_senddexaccept",params)
        assert_equal(out['error'], None)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking the offer status")
        params = str([addresses[2]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getactivedexsells",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result'][0]['propertyid'], 4)
        assert_equal(out['result'][0]['action'], 2)
        assert_equal(out['result'][0]['seller'], addresses[2])
        assert_equal(out['result'][0]['ltcsdesired'], '0.00000000')
        assert_equal(out['result'][0]['amountavailable'], '0.00000000')
        assert_equal(out['result'][0]['amountoffered'], '1000.00000000')
        assert_equal(out['result'][0]['unitprice'], '0.20000000')
        assert_equal(out['result'][0]['minimumfee'], '0.00001000')

        assert_equal(out['result'][0]['accepts'][0]['buyer'], addresses[3])
        assert_equal(out['result'][0]['accepts'][0]['amountdesired'], '1000.00000000')
        assert_equal(out['result'][0]['accepts'][0]['ltcstopay'], '200.00000000')
        # assert_equal(out['result'][0]['accepts'][0]['block'], 208)
        # assert_equal(out['result'][0]['accepts'][0]['blocksleft'], 241)


        self.log.info("Paying the tokens")
        params = str([addresses[3], addresses[2], "200"]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_send_dex_payment",params)
        # self.log.info(out)

        self.nodes[0].generate(1)


        self.log.info("Checking token balance in buyer address")
        params = str([addresses[3], 4]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'], '2000.00000000')
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking LTC Volume")
        params = str([4, 1, 99999]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, True, "tl_getdexvolume",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['volume'], '400.00000000')


        self.nodes[0].generate(2)


        self.log.info("Checking vesting in related address")
        params = str([addresses[1], 1]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getbalance",params)
        # self.log.info(out)
        assert_equal(out['error'], None)
        assert_equal(out['result']['balance'],'150.51498000')  # 15.05% of vesting (NOTE: check the round up)
        assert_equal(out['result']['reserve'],'0.00000000')


        self.log.info("Checking unvested ALLs ")
        params = str([addresses[1]]).replace("'",'"')
        out = tradelayer_HTTP(conn, headers, False, "tl_getunvested",params)
        # self.log.info(out)
        assert_equal(out['result']['unvested'],'899.65666820')

        conn.close()

        self.stop_nodes()

if __name__ == '__main__':
    VestingBasicsTest ().main ()
