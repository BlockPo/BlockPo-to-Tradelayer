const PORT = 9876;
const listener = require('socket.io')(PORT);
const tl = require('./TradeLayerRPCAPI').tl 

var client = tl.init(user, pass, '', true)

var channelSingleSigAddress = ""
var myChannelPubkey = ""
var myChannelMultisig =""
var proposedMultisig = "" //we'll use these to ensure for this specific 1:1 channel set-up, the curtains match the drapes
var counterPartyPubkey = ""
var desiredtx={'propertyid':0,'amount':0,'propertyid'}
var propertyid = 7
var propertyid = 8
var amount =10
var amountDesired = 1

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
        counterPartyPubkey = data
        tl.addmultisigaddress(2, [data, channelComponent], (data) => {
            myChannelMultisig = data
            const multisig = {
                'multisig': data,
                'pubKeyUsed': myChannelPubKey
            }
            io.emit('multisig', multisig)
        })
    })

    io.on('multisig', (data) => {
        legitMultisig(data,function(data){
            if(legit==true){
                myChannelMultisig=e
                tl.commitToChannel(tokenAddress, e,id,amount, function(data){
                    return console.log(data)
                 })
            }else(return console.log('The client tried to scam with a bad multisig'))
        }
    })        
    
    io.on('partialtx', (data)=>){
        shouldCoSign(data, (sign, data) => {
            if (sign === true) {
                tl.simplesign(data, (data) => {
                    io.emit('signedtx', data)
                })
            }
        })
    }
})

function legitMultisig(e, cb){
    var legit = false
    tl.addmultisigaddress(2,[channelComponent,counterPartyPubkey],function(data){
        if(data==e){
            legit=true
            return cb(legit)
        }else{return cb(legit)}
    })
}
    
function shouldCoSign(rawstring,cb){
    tl.decodeRawTransaction(rawstring,null,0,function(data){
            if(
                data.OMNI.amount==desiredtx.amount
                &&data.OMNI.propertyid==desiredtx.propertyid
                &&data.OMNI.amountDesired==desiredtx.amount2
                &&data.OMNI.propertyDesired==desiredtx.propertyid2
                //&&data.OMNI.signed==true ...not sure what this looks like, will review and check
                ){
                return cb(true)

                //this needs conform to the JSON you get back from decoding a Trade Channel token trade tx
            }else{
                return cb(false)
            }
            //to keep it simple, the Receiver is always signing a tx in advance, the Listener always submits the signed tx
            //and the Receiver always decides to instantly send it and publish it. 
            //To support LTC trades, simply look for the type of trade indicated, if it's an LTC trade then if it checks out, co-sign
    })
}

