TradeLayer v0.2.0
================

v0.2.0 is the second version of TradeLayer protocol. Is based on Omnicore-lite (credits to https://github.com/zathras-crypto) working on top of Litecoin Core (v0.16.3).


Table of contents
=================

- [TradeLayer v0.2.0]
- [Compatibility with Litecoin Core](#compatibility-with-litecoin-core)
- [Notable changes](#notable-changes)
  - [Contract DEx](#contract-dex)
  - [Pegged Currency](#pegged-currency)
  - [DEx improvements](#dex-improvements)
  - [MetaDex](#metadex)
  - [RPCs](#rpcs)
  - [Settlement Algorithm](#settlement-algorithm)
  - [Unit Testing](#unit-testing)
- [Credits](#credits)


Compatibility with Litecoin Core
-------------------------------

TradeLayer can be used as replacement for Litecoin Core v0.16.3 because it's an enhancement. In inheritance, Tradelayer has all the notables improvements of this Litecoin version.

Notable changes from OmniLayer
=============== 

Contract DEx
-----------------------------------------
One of the major features of TradeLayer is the Decentralized Exchange for Futures Contracts. For now it's contains four contracts (ALL/dUSD, LTC/ALL)  for weekly and montly expiration. Also we integrate the ALL token, which is TradeLayer's token used for contract's colateralization.
Most of the development was made in x_Trade function. Changes related with the updating process for balances, trade amount and amount still for sale that is added in the order book with the respective address if was not found a match. New status for the buyer and seller depending on the updated balance after a trade. A new function for loop bidirectionals was built in order to look for matchs in the order book in different directions regarding of trading action (SELL or BUY).


Pegged Currency
-----------------------------------------------------
Other mayor feature of TradeLayer is pegged currency. The basis for pegged currency are related with the possibility to create a Hedge position in futures contracts and have an stable value equivalent. That equivalent value is represented as a new token (the pegged currency) but covered with the hedge.


DEx improvements
-----------------------------------------------------
Omni Layer's DEx is used now to sell/buy Litecoins in exchange for metacoins (like ALLs) who lives in TradeLayer Protocol. Old version of DEx makes possible the sell token offer, that later gets accepted by takers who want to buy them. The most important improvement for DEx is that is possible post a buying token offer.


MetaDEx
-----------------------------------------------------
Omni Layer's MetaDex is now ported into Tradelayer for trade all kinds of metacoins including the ALL token, and pegged currencies.


RPCs
-----------------------------------------------------
New RPCs are added in order to make Contract DEx trade, and retrieve different kind of data related.
Slight modifications were made to the existing RPC calls in order to have them
working with Litecoin Core v0.16.3 and some another news needed to the logic of Futures Contracts were added.


Settlement Algorithm
-----------------------------------------------------
This algorithm consider like source edges from every match line of the
vector of vector of maps built in recordMatchedTrade having two nodes opening or increasing
his positions two of them. We look for matched events for this two addresses using FIFO and
also for those nodes opening or increasing positions. When the
FIFO algorithm finishes opened contracts are added as ghost nodes in a graph structure, that builds paths
following netting events with source being the new contracts created. By using a piecewise
function obtained by solving some equation we compute the respective exit price
required to have a global difference between PNL for long and short positions equal to zero.


Unit Testing
-----------------------------------------------------
Inside the repo src/ some shell scripts to test the project were built and unit tests based
in some usual Boost libraries inside src/omnicore/test/.



Notable changes from 0.1.0
===============

TradeLayer v0.2.0 through 0.2.n repesent near-launch versions of the more mature protocol vs. the working prototype version that was pushed in October 2018. 

Contract DEx
-----------------------------------------

Expanded the Dex to include Oracle-based contracts. Expanded the type of contracts to include perpetual contracts with interest rate formulas. Insurance caches and fees deter wash trading, direct the cashflow to back-up contracts and deploy in cases where settlements would be otherwise insolvent. Oracles can earn revenue by publishing a reliable feed consistently and cultivating a popular usage of their contracts.

Node Reward
-----------------------------------------

It's possible to start from a given block to be able to publish Node Reward transactions, these transactions include a cipher based on correctly parsing and calculating the layer's consensus hash. Valid transactions split a Node Reward. The reward incrementally increases per value each block for about six months and then decays, until many decades in the future it stabilizes at a fixed number per block. It's designed to provide an egalitarian faucet for uncapitalized persons in the future, see the money supply of ALL in an organic way, and encourage long-term subsidy of non-miners operating full validating nodes.

Founder Reward
-----------------------------------------

A special kind of Vesting Token exists that can only be moved once a year, converts to ALL based on cumulative volume, and otherwise shows in reserve and carries with it when transferred, the yet-to-be-vested ALL associated with a balance. Vesting is linked to cumulative volume of all trading pairs on TradeLayer, measured in Litecoin, and requires many millions of cumulative LTC in volume to go through an S-curve like vesting function.

Revised Margin System
-----------------------------------------

We thoroughly delved into contingent logic and minute balance adjustments relating to the margining of decentralized derivatives. We came up with a model where users are incrementally liquidated below a cross-margined maintainence threshold. Due to the discrete complexity in the decentralized settlement, unrealized and pre-settlement realized profits cannot be used to offset losses in other assets, portfolio margining style. But the system is fairly robust for cross-margin, the incremental liquidation thresholds were tweaked to encourage highly leveraged traders to drop size before going broke, in favor of systemic anti-fragility.

Revised Settlement Algorithms
-----------------------------------------

A lot of re-working had to be done, arriving at a semi-distinct alternative implementation, of a perpetual settlement algorithms. Unlike an earlier vision where everyone would be settled stochastically on a rolling basis, this model implemented a much less experimental periodic settlement where the entire graph is reduced to a set of counterparties with open positions. Given this nature of the graph rewrite algorithm, it's very opportune to apply whatever interest rate function to those pairings at that time.

Coming soon in 0.2.x

Trade Channels
----------------------

Build atomic transactions using multisig channels and a commit/withdrawal block-height nonce scheme, co-sign them for instant execution, manage channels to limit the need to publish to the base chain and save on miner fees.

Liquidity Reward
-----------------------------------------

On-chain orders that match and clear in xTrade and involve native ecosystem trading pairs are subsidized with a perpetual inflation function. The rate of inflation is 10% per doubling past a fixed number of LTC in cumulative volume for those pairs. Below that fixed number, the inflation is more generous in favor of early liquidity providers. Oracle contracts and issued smart property tokens are not considered to be part of the native ecosystem.

Identity Module
----------------------

Get into the identity business as a 3rd party registrar, emit attestations, include registrars in a whitelist of who you would be comfortable:  

               1) trading with on chain
               2) having trade the oracle contract you operate
               3) having trade or hold the security token or bank coin you issued/operate

Standardizing dCurrency
-----------------------

Revised RPC parameters to make dCurrency more usable and standardized in the native ecosystem.


Credits
=======
Thanks to everyone who contributed to this release, and especially the Litecoin Core developers and Omni Core developers for providing the foundation for TradeLayer Protocol.


Litecoin Core integration/staging tree
=====================================

[![Build Status](https://travis-ci.org/litecoin-project/litecoin.svg?branch=master)](https://travis-ci.org/litecoin-project/litecoin)

https://litecoin.org

What is Litecoin?
----------------

Litecoin is an experimental digital currency that enables instant payments to
anyone, anywhere in the world. Litecoin uses peer-to-peer technology to operate
with no central authority: managing transactions and issuing money are carried
out collectively by the network. Litecoin Core is the name of open source
software which enables the use of this currency.

For more information, as well as an immediately useable, binary version of
the Litecoin Core software, see [https://litecoin.org](https://litecoin.org).

License
-------

Litecoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/litecoin-project/litecoin/tags) are created
regularly to indicate new official, stable release versions of Litecoin Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

The developer [mailing list](https://groups.google.com/forum/#!forum/litecoin-dev)
should be used to discuss complicated or controversial changes before working
on a patch set.

Developer IRC can be found on Freenode at #litecoin-dev.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

We only accept translation fixes that are submitted through [Bitcoin Core's Transifex page](https://www.transifex.com/projects/p/bitcoin/).
Translations are converted to Litecoin periodically.

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.
