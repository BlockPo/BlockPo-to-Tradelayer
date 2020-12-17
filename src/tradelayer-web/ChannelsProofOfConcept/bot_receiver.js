const URL = 'http://localhost' //replace this with the ip address of the listener, assumes a static ip
const PORT = 9876
const client = require('socket.io-client');

const io = client.connect(`${URL}:${PORT}`);

const tl = require('./TradeLayerRPCAPI').tl
const user = "" //enter the RPC username indicated in the litecoin.conf file of your node used to run this script
const pass = "" //same as above, but the password

var channelSingleSigAddress = ""
var myChannelPubkey = ""
var myChannelMultisig =""
var proposedMultisig = "" //we'll use these to ensure for this specific 1:1 channel set-up, the curtains match the drapes
var counterPartyPubkey = ""
var desiredtx={'propertyid':0,'amount':0,'propertyid'}
var propertyid = 7
var propertyid2 = 8
var amount =10
var amountDesired = 1
var tokenAddress = "" //insert address with tokens

io.on('connect', ()=>{
	var dataSub = 'channelProposal'
	io.emit('dataSub',dataSub)
})

io.on('channelPubKey', (data) => {

    console.log(`channelPubKey: ${data}`)
});

io.on('dataSub', (data) => {
	tl.getNewAddress(null,function(address){
	channelComponent = address
		tl.validateAddress(address,function(data){
			myChannelPubkey =data.scriptPubKey
            io.emit('pubKey', myChannelPubkey)
        })
	})
    console.log(`dataSub: ${data}`)
});

io.on('multisig', (multisig)=>{
		legitMultisig(multisig,function(legit){
            if(legit==true){
                myChannelMultisig=multisig
                tl.commitToChannel(tokenAddress, e,id,amount, function(data){
                    return console.log(data)
                    setTimeout(function(){
                    	//we're assuming 1000 seconds is long enough to get a commit confirmed from both sides to move this along
                    	buildTokenToTokenTrade(myChannelMultisig,propertyid,amount,propertyid2,amount2,true,function(data){
                    		io.emit('partialtx',data)
                  			return(console.log(data))
                    	})
                    },1000000)
                 })
            }else(return console.log('The client tried to scam with a bad multisig'))
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

function buildTokenToTokenTrade = function(channeladdress, id1,amount, id2, amount, secondSigner=true,cb){
	tl.getBlock(null,function(data){
		var height = data.height+3
		tl.createpayload_instant_trade(id1, amount, height, id2, amount2, function(payload){
			tl.buildRaw(payload,multisigInput,[0],tokenSellerAddress,0.00000546, function(txString){
					return cb(data)
			})
		})
	}
}