const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')
const express = require('express')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)
var counterpartyIP = ""
var BTCUSD = 1

var id = 8 //property id of the token that this party wants to trade, let's assume this is USD
//yes we need to create a mirrored function to balance inventories and this bot will just trade away its inventory to the other for now
var amount = 10 //let's assume this is USD and a mirror trade would want a fraction of a propertyid 7 token
				//if BTCUSD is 18000 then this is 10/18000= 0.00055 desired of property 7
				//the reciprocal pricing of token to token trades is annoying here because if you're buying 7 with 8, it's BTC pricing
				//if you buy 8 with 7 then the pricings are like 0.000055 per USD, we'll fix this at the end
const ws = new websocket(counterpartyIP,{
  perMessageDeflate: false
});

var WSPackets = 0

var channelSingleSigAddress = ""
var myChannelPubkey = ""
var myChannelMultisig =""
var proposedMultisig = ""

client.getNewAddress(null,function(address){
	channelComponent = address
	client.validateAddress(address,function(data){
		myChannelPubkey =data.scriptPubKey
		return myChannelPubkey
	})
})

var dataSub = 
    {"jsonrpc": "2.0",
     "method": "public/get_index_price",
     "id": 42,
     "params": {
        "index_name": "btc_usd"}
    };
var ws = new WebSocket('wss://test.deribit.com/ws/api/v2');

ws.onopen = function(){ //this should define listen blocks for the 3 levels of input: 
						//new address received -> create multisig
						//proposed partially-signed tx -> pass to function to decode, verify the commit is there, co-sign, return message
	ws.send(JSON.stringify(dataSub)); //this gets our listening server ready to quote around the Bitcoin price
	//we're going to assume for this exercise that token A is Bitcoin and token B is a USD stablecoin
	var msg1 = myChannelPubkey
	ws.send(JSON.stringify(msg1))
};


ws.onmessage = function (e) {
    // do something with the response...
    if(typeof e ==Object){
    	//presumably the Deribit JSON, other receiver submissions are in string form
    	BTCUSD = e.result.index_price
    }
    if(e.length>=36){//probably a pubkey, we'll use it to make a multisig
    	client.addMultiSigAddress(2,[e,channelSingleSigAddress],function(data){
    		myChannelMultisig = data
    		ws.send({'multisig':myChannelMultisig,'pubKeyUsed':myChannelPubKey}) //sends back the confirmation for receiver to test
    	}) 																	     //I'm unsure how an expess server or other library handles 
    }
    if(e.length>74){
    	shouldCoSign(e,function(sign,e){
    		if(sign==true){
    			tl.simpleSign(e,function(data){
    				//send this back to the Receiver
    			}
    		}
    	})
    }                                                                             //various channels and if ws.send works or not to manage it

    //0250863ad64a87ae8a2fe83c1af1a8403cb53f53e486d8511dad8a04887e5b2352
    //1PMycacnJaSqwwJqjawXBErnLsZ7RkXUAs
};

var spread = 5
var denomAmount = 10 
var desiredAmount = 10
var desiredPropertyId =

function generateIoI(){
	//proposes prices to trade that are $5 away from the market
	var bid = BTCUSD-5
	var ask = BTCUSD+5
	var newBuyIoI = {'buy':bid,'sell':ask,'bidSize':10,'offerSize':10}
	//submits this to the feeds of WS subscribers
}

function shouldCoSign(rawstring,cb){
	tl.decodeRawTransaction(rawstring,null,0,function(data){
			/*if(
				data.OMNI.amount==
				&&data.OMNI.propertyid==desiredtx.propertyid
				&&data.OMNI.referenceaddress==desiredtx.referenceaddress
				&&data.OMNI.type==desiredtx.type    //note: update the formal tx type names to fit the encoding for txInventory
				&&data.OMNI.amount==desiredtx.amount){
				return cb(true)

				//this needs conform to the JSON you get back from decoding a Trade Channel token trade tx
			}else{
				return cb(false)
			}
			*/
			//to keep it simple, the Receiver is always signing a tx in advance, the Listener always submits the signed tx
			//and the Receiver always decides to instantly send it and publish it. 
	})
}

//create multisig
//create token commit tx
//create and co-sign trades
