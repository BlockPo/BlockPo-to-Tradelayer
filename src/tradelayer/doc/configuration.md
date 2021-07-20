Configuration
=============

TradeLayer can be configured by providing one or more optional command-line arguments:
```bash
$ litecoind -setting=value -setting=value
```

All settings can alternatively also be configured via the `litecoin.conf`.

Depending on the operating system, the default locations for the configuration file are:

- Unix systems: `$HOME/.litecoin/litecoin.conf`
- Mac OS X: `$HOME/Library/Application Support/Litecoin/litecoin.conf`
- Microsoft Windows: `%APPDATA%/Litecoin/litecoin.conf`

A typical `litecoin.conf` may include:
```
server=1
txindex=1
rpcuser=user
rpcpassword=whyDidTheBulletProofElephantCrossTheRoad
rpcport=9432
```

## Optional settings

To run and use TradeLayer, no explicit configuration is necessary.

#### General options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `startclean`                 | boolean      | `0`            | clear all persistence files on startup; triggers reparsing of Tradelayer transactions |
| `tltxcache`                | number       | `500000`       | the maximum number of transactions in the input transaction cache               |

#### Log options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `tllogfile`                | string       | `trdelayer.log` | the path of the log file (in the data directory per default)                    |
| `tldebug`                  | multi string | `""`           | enable or disable log categories, can be `"all"`, `"none"`                      |

#### Transaction options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `autocommit`                 | boolean      | `1`            | enable or disable broadcasting of transactions, when creating transactions      |
| `datacarrier`                | boolean      | `1`            | if disabled, payloads are embedded multisig, and not in `OP_RETURN` scripts     |
| `datacarriersize`            | number       | `80`           | the maximum size in byte of payloads embedded in `OP_RETURN` scripts            |

**Note:** the options `-datacarrier` and `datacarriersize` affect the global relay policies of transactions with `OP_RETURN` scripts.

#### RPC server options:

| Name                         | Type         | Default        | Description                                                                     |
|------------------------------|--------------|----------------|---------------------------------------------------------------------------------|
| `rpcforceutf8`               | boolean      | `1`            | replace invalid UTF-8 encoded characters with question marks in RPC responses   |
