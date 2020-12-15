const bitcoin = require('bitcoinjs-lib')
address = bitcoin.address.fromBech32('bc1q6aurm54nk7ucgtul2ud3p70gqjdrskkrf8sm4ykxu0f85el8ksvs4gdngk')
base58 = bitcoin.address.toBase58Check(address.data, bitcoin.networks.mainnet.pubKeyHash)

console.log(base58)