JSON-RPC API
============

Tradelayer is a fork of Litecoin Core, with Tradelayer Protocol feature support added as a new layer of functionality on top. As such interacting with the API is done in the same manner (JSON-RPC) as Litecoin Core, simply with additional RPCs available for utilizing Omni Protocol features.

As all existing Litecoin Core functionality is inherent to Tradelayer, the RPC port by default remains as `9432` as per Bitcoin Core.  If you wish to run Tradelayer in tandem with Litecoin Core (eg. via a separate datadir) you may utilize the `-rpcport<port>` option to nominate an alternative port number.

All available commands can be listed with `"help"`, and information about a specific command can be retrieved with `"help <command>"`.

*Please note: this document may not always be up-to-date. There may be errors, omissions or inaccuracies present.*


## Table of contents

- [Futures Contracts](#futures-contracts)
  - [tl_createcontract](#tl_createcontract)
  - [tl_tradecontract](#tl_tradecontract)
  - [tl_sendcancel_contract_order](#tl_sendcancel_contract_order)
  - [tl_cancelallcontractsbyaddress](#tl_cancelallcontractsbyaddress)
  - [tl_closeposition](#tl_closeposition)
  - [tl_create_oraclecontract](#tl_create_oraclecontract)
  - [tl_setoracle](#tl_setoracle)
  - [tl_change_oracleadm](#tl_change_oracleadm)
  - [tl_oraclebackup](#tl_oraclebackup)
  - [tl_closeoracle](#tl_closeoracle)
  - [tl_getposition](#tl_getposition)
  - [tl_getfullposition](#tl_getfullposition)
  - [tl_getcontract_orderbook](#tl_getcontract_orderbook)
  - [tl_gettradehistory](#tl_gettradehistory)
  - [tl_gettradehistory_unfiltered](#tl_gettradehistory_unfiltered)
  - [tl_getupnl](#tl_getupnl)
  - [tl_getpnl](#tl_getpnl)
  - [tl_getreserve](#tl_getreserve)
  - [tl_getmarketprice](#tl_getmarketprice)
  - [tl_list_natives](#tl_list_natives)
  - [tl_list_oracles](#tl_list_oracles)
  - [tl_getoraclecache](#tl_getoraclecache)
  - [tl_getcontract](#tl_getcontract)

- [General data retrieval](#data-retrieval)
  - [tl_getinfo](#tl_getinfo)
  - [tl_getbalance](#tl_getbalance)

## Futures Contracts

The RPCs for Futures Contract  can be used to create new derivatives contracts, trade and watch data related.

### tl_createcontract

Create new native Future Contract. Native is a contract that is living just in the protocol (it doesn't require external prices)

**Arguments:**

1. fromaddress               (string, required) the address to send from
2. numerator                 (number, required) propertyId (Asset)
3. denominator               (number, required) propertyId of denominator
4. name                      (string, required) the name of the new tokens to create
5. blocks until expiration   (number, required) life of contract, in blocks
6. notional size             (number, required) notional size
7. collateral currency       (number, required) collateral currency
8. margin requirement        (number, required) margin requirement
9. quoting                   (number, required) 0: inverse quoting contract, 1: normal quoting

10. kyc options              (array, required) A json with the kyc allowed

**Result:**
```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ ./litecoin-cli tl_createcontract "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 2 1 "Contract1" 2560 1 3 "0.1" 0 [1,2,4]
```

---

### tl_tradecontract

Place a trade offer on the distributed Futures Contracts exchange.

**Arguments:**

1. fromaddress          (string, required) the address to trade with
2. name or id           (string, required) the name or the identifier of the contract
3. amountforsale        (number, required) the amount of contracts to trade
4. effective price      (number, required) limit price desired in exchange
5. trading action       (number, required) 1 to BUY contracts, 2 to SELL contracts
6. leverage             (number, required) leverage (2x, 3x, ... 10x)

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**

```bash
$ ./litecoin-cli tl_tradecontract "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 31 1200 "150.0" 1 3
```
---

### tl_sendcancel_contract_order

Cancel specific contract order.

**Arguments:**

1. address       (string, required) sender address
2. txid          (string, required) transaction hash of the order

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_sendcancel_contract_order "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" "e8a97a4ab61389d6a4174beb8c0dbd52d094132d64d2a0bc813ab4942050f036"
```

---

### tl_cancelallcontractsbyaddress

Cancel all offers on a given Futures Contract.

**Arguments:**

1. fromaddress          (string, required) the address to trade with
2. name or id           (string, required) the name (or id) of Future Contract

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_cancelallcontractsbyaddress "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 1
```

---

### tl_closeposition

Close the position on a given Futures Contract. Sell/Buy all contracts in position.

**Arguments:**

1. fromaddress          (string, required) the address to trade with
2. contractid           (number, required) the Id of Future Contract

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_closeposition "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" 1
```

---

### tl_create_oraclecontract

Create new Oracle Future Contract.

**Arguments:**

1. oracle address            (string, required) the address to send from (admin)
2. name                      (string, required) the name of the new tokens to create
3. blocks until expiration   (number, required) life of contract, in blocks
4. notional size             (number, required) notional size
5. collateral currency       (number, required) collateral currency
6. margin requirement        (number, required) margin requirement
7. backup address            (string, required) backup admin address contract
8. quoting                   (number, required) 0: inverse quoting contract, 1: normal
9. kyc options               (array, required) A json with the kyc allowed

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_create_oraclecontract "3BydPiSLPP3DR5cf726hDQ89fpqWLxPKLR" "Contract1" 2560 1 3 1 "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P" 0 [1,2,4]
```

---

### tl_setoracle

Set the price for an oracle address. Oracles contracts require prices each block

**Arguments:**

1. fromaddress          (string, required) the oracle address for the Future Contract
2. contract name        (string, required) the name of the Future Contract
3. high price           (number, required) the highest price of the asset
4. low price            (number, required) the lowest price of the asset
5. close price          (number, required) the close price of the asset

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_setoracle "1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu"  "Contract 1" "900.2" "400.0" "600.5"
```

---

### tl_change_oracleadm

Change the admin on record of the Oracle Future Contract.

**Arguments:**

1. fromaddress (string, required) the address associated with the oracle Future Contract
2. toaddress  (string, required) the address to transfer administrative control to
3. name or id (string, required) the name (or id) of the Future Contract

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_change_oracleadm "1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu" "3HTHRxu3aSDV4de+akjC7VmsiUp7c6dfbvs" "Contract 1"
```

---

### tl_oraclebackup

Activation of backup address (backup is now the new oracle address).

**Arguments:**

1. backup address (string, required) the address associated with the oracle Contract
2. contract name (string, required) the name of the Future Contract

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_oraclebackup "1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu" "Contract 1"
```

---

### tl_closeoracle

Close an Oracle Future Contract. Make Oracle expire from admininstrator.

**Arguments:**

1. backup address (string, required) the backup address associated with the oracle.
2. name or id (string, required) the name of the Oracle Future Contract

**Result:**

```js
"hash"  // (string) the hex-encoded transaction hash
```

**Example:**
---

```bash
$ ./litecoin-cli tl_closeoracle "1ARjWDkZ7kT9fwjPrjcQyvbXDkEySzKHwu"  "Contract 1"
```

---

### tl_getposition

Returns the position for the future contract for a given address and contractId.

**Arguments:**

1. address     (string, required) the address
2. name or id  (string, required) the future contract name or id

**Result:**

```js
"{
  "shortPosition" : "n.nnnnnnnn",   (string) short position of the address
  "longPosition" : "n.nnnnnnnn",  (string) long position of the address
}"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getposition "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P" 1
```

---

### tl_getfullposition

Returns the full position for the future contract for a given address and contract id.

**Arguments:**

1. address              (string, required) the address
2. name or id           (string, required) the future contract name or identifier

**Result:**

```js
"{
  "symbol" : "n.nnnnnnnn",   (string) short position of the address
  "notional_size" : "n.nnnnnnnn"  (string) long position of the address
  "shortPosition" : "n.nnnnnnnn",   (string) short position of the address
  "longPosition" : "n.nnnnnnnn"  (string) long position of the address
  "liquidationPrice" : "n.nnnnnnnn"  (string) long position of the address
  "positiveupnl" : "n.nnnnnnnn",   (string) short position of the address
  "negativeupnl" : "n.nnnnnnnn"  (string) long position of the address
  "positivepnl" : "n.nnnnnnnn",   (string) short position of the address
  "negativepnl" : "n.nnnnnnnn"  (string) long position of the address
}"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getfullposition "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P" 1
```


---

### tl_getcontract_orderbook

List active offers on the distributed futures contracts exchange.

**Arguments:**

1. name or id    (string, required) filter orders by contract name or identifier for sale
2. tradingaction (number, required) filter orders by trading action (Buy = 1, Sell = 2)

**Result:**

```js
"[                                              (array of JSON objects)
  {
    "address" : "address",                         (string) the Litecoin address of the trader
    "txid" : "hash",                               (string) the hex-encoded hash of the transaction of the order
    "contractidforsale" : n,                       (number) the identifier of the contract market
    "amountforsale" : "n.nnnnnnnn",                (string) the amount of contracts initially offered
    "block" : nnnnnn,                              (number) the index of the block that contains the transaction
    "blocktime" : nnnnnnnnnn                       (number) the timestamp of the block that contains the transaction
  },
 ]"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getcontract_orderbook 2 1
```

---

### tl_gettradehistory

Retrieves the history of trades on the distributed contract exchange for the specified market.

**Arguments:**

1. name or id  (string, required) the name of future contract

**Result:**

```js
"[                                      (array of JSON objects)
  {
    "block" : nnnnnn,                      (number) the index of the block that contains the trade match
    "price" : "n.nnnnnnnnnnn..." ,     (string) the price used to execute this trade (received/sold)
    "sellertxid" : "hash",                 (string) the hash of the transaction of the seller
    "address" : "address",                 (string) the Litecoin address of the seller
    "amountsold" : "n.nnnnnnnn",           (string) the number of contracts sold in this trade
    "matchingtxid" : "hash",               (string) the hash of the transaction that was matched against
    "matchingaddress" : "address"          (string) the Litecoin address of the other party of this trade
  },
  ....
]"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_gettradehistory 1
```

---

### tl_gettradehistory_unfiltered

Retrieves the history of trades on the distributed contract exchange for the specified market.

**Arguments:**

1. name or id  (string, required) the name (or id) of future contract

**Result:**

```js
"[                                      (array of JSON objects)
  {
    "block" : nnnnnn,                      (number) the index of the block that contains the trade match
    "price" : "n.nnnnnnnnnnn..." ,     (string) the price used to execute this trade (received/sold)
    "sellertxid" : "hash",                 (string) the hash of the transaction of the seller
    "address" : "address",                 (string) the Litecoin address of the seller
    "amountsold" : "n.nnnnnnnn",           (string) the number of contracts sold in this trade
    "matchingtxid" : "hash",               (string) the hash of the transaction that was matched against
    "matchingaddress" : "address"          (string) the Litecoin address of the other party of this trade
  },
  ....
]"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_gettradehistory_unfiltered 3
```

---

### tl_getupnl

Retrieves the unrealized PNL for trades on the distributed contract exchange for a given adress in the specified market


**Arguments:**

1. address           (string, required) address of owner
2. name or id        (string, required) the name (or id number) of future contract
3. verbose           (number, optional) 1 : if you need the list of matched trades

**Result:**

```js
"{
  "upnl":  nnnnnn,                 (number) amount of gain or loss (unrealized)
} "
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getupnl "13framr31ziZmV8HLjE5Gs8KRVw894P2eT" "Contract 1" 1
```

---

### tl_getpnl

Retrieves the last realized PNL for trades on the distributed contract exchange for the specified market


**Arguments:**

1. address           (string, required) address of owner
2. name or id        (string, required) the name (or id number) of future contract

**Result:**

```js
"{
  "pnl":  nnnnnn,                 (number) amount of gain or loss
} "
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getpnl "13framr31ziZmV8HLjE5Gs8KRVw894P2eT" "Contract 1" 1
```

---

### tl_getreserve

Returns the token reserve account using in futures contracts, for a given address and contract id.

**Arguments:**

1. address              (string, required) the address
2. propertyid           (number, required) the contract identifier

**Result:**

```js
"{
    "reserved" : "n.nnnnnnnn"   (string) the amount reserved by sell offers and accepts
}"

```

**Example:**
---

```bash
$ ./litecoin-cli tl_getreserve "1EXoDusjGwvnjZUyKkxZ4UHEf77z6A5S4P" 1
```

---

### tl_getmarketprice

Retrieves the last match price on the distributed contract exchange for the specified market.

**Arguments:**

1. contractid           (number, required) the id of future contract


**Result:**

```js
"{
   "block\" : nnnnnn,                      (number) the index of the block that contain last trade.
   "price\" : "n.nnnnnnnnnnn...",        (string) last price
  ...
}"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getmarketprice 2
```

---

### tl_list_natives

Lists all native contracts availables.

**Arguments:**

None

**Result:**

```js
"[                                  (array of JSON objects)
  {
    "contractid" : n,                (number) the identifier of the contract
    "name" : "name",                 (string) the name of the contract
    "data" : "information",          (string) additional information or a description
    "url" : "uri",                   (string) an URI, for example pointing to a website
  },
  ...
]"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_list_natives
```

---

### tl_list_oracles

Lists all oracle contracts availables.

**Arguments:**

None

**Result:**

```js
"[                                  (array of JSON objects)
  {
    "contractid" : n,                (number) the identifier of the contract
    "name" : "name",                 (string) the name of the contract
    "data" : "information",          (string) additional information or a description
    "url" : "uri",                   (string) an URI, for example pointing to a website
  },
  ...
]"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_list_oracles
```

---

### tl_getoraclecache

Returns the oracle fee cache for a given token id.

**Arguments:**
1. collateral                   (number, required) the contract collateral

**Result:**

```js
"{
  "amount" : "n.nnnnnnnn",  (number) the available balance in the oracle cache for the property
}"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getcache 1
```

---

### tl_getcontract

Returns details for about the future contract.

**Arguments:**

1. name or id   (string, required) the name  of the future contract, or the number id

**Result:**

```js
"{
  "contractid" : n,                 (number) the identifier
  "name" : "name",                 (string) the name of the contract
  "data" : "information",          (string) additional information or a description
  "url" : "uri",                   (string) an URI, for example pointing to a website
  "issuer" : "address",           (string) the Litcoin address of the issuer on record
  "creationtxid" : "hash",         (string) the hex-encoded creation transaction hash
}"
```

**Example:**
---

```bash
$ ./litecoin-cli tl_getcontract "Contract1"
```
