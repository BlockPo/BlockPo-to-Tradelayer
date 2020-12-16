const PORT = 9876;
const listener = require('socket.io')(PORT);
const tl = require('./TradeLayerRPCAPI').tl

listener.on('connection', (io) => {
    console.log(`New Connection! ID: ${io.id}`)
    // code here 

    let dataSub = {
    "jsonrpc": "2.0",
     "method": "public/get_index_price",
     "id": 42,
     "params": {
        "index_name": "btc_usd"}
    };

    let channelComponent = ''
    let myChannelPubkey = ''

    tl.getnewaddress(null, (data) => {
            channelComponent = data
        tl.validateaddress(data, (d) => {
            myChannelPubkey = d.scriptPubKey
            io.emit('channelPubKey', myChannelPubkey)

        })
    });
    io.emit('dataSub', dataSub)


    io.on('deribitJSON', (data) => {
        BTCUSD = data.result.index_price
    })

    io.on('pubKey', (data) => {
        tl.addmultisigaddress(2, [data, channelSingleSigAddress], (data) => {
            myChannelMultisig = data
            const multisig = {
                'multisig': data,
                'pubKeyUsed': myChannelPubKey
            }
            io.emit('multisig', multisig)
        })
    })

    io.on('multisig', (data) => {
        shouldCoSign(data, (sign, data) => {
            if (sign === true) {
                tl.simplesign(data, (data) => {
                    io.emit('something', data)
                })
            }
        })
    })
    //end code here 
})

