const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')
const express = require('express')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)
var counterpartyIP = "45.79.203.91"

const ws = new websocket(counterpartyIP,{
  perMessageDeflate: false
});

var WSPackets = 0

var tokenAddress ="" //the address where our tokens start the journey, this is used to build a commit
var id = 7 //property id of the token that this party wants to trade
var amount = 10

var channelComponent = ""
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

ws.onopen = function(){
	
	var msg1 = myChannelPubkey
	ws.send(JSON.stringify(msg1))//submits address to start the process
};


ws.onmessage = function (e) {
    // do something with the response...
    if(e.length>=36){//probably a key, we'll use it to make a multisig
    	client.addMultiSigAddress(2,[e,myChannelPubkey],function(data){
    		myChannelMultisig = data
    	}) 
	}else{//if it's address-length then it's probably a multisig
		proposedMultisig = data
		legitMultisig(proposedMultisig)
	}

	// if block for messages that are indicators of interest, passes to function that decides to built tx or not
	if(typeof e==Object){
		var trade = shouldTrade(e)
		if(trade==true){buildTrade(e)}
	} 

    //0250863ad64a87ae8a2fe83c1af1a8403cb53f53e486d8511dad8a04887e5b2352
    //1PMycacnJaSqwwJqjawXBErnLsZ7RkXUAs
};

function legitMultisig(e){
	var legit = false
	//takes the Listener's response, addsMultisig, compares them, returns true 
	if(legit==true){
		myChannelMultisig=e
		tl.commitToChannel(tokenAddress, e,id,amount, function(data){
			return console.log(data)
		})
	}else(return console.log('The server tried to scam with a bad multisig'))
}

function shouldTrade(e) //this will be a simple function at first that uses randomness to decide

function buildTrade(e){
	//takes IoI parameters and passes to buildRaw
}

function sanityCheck(e){
	//
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