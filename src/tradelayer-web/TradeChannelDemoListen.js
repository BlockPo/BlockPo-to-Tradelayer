const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')
const express = require('express')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)
var counterpartyIP = ""

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

ws.onopen = function(){
	
	var msg1 = myChannelPubkey
	ws.send(JSON.stringify(msg1))
};


ws.onmessage = function (e) {
    // do something with the response...
    if(e.length>=36){//probably a key, we'll use it to make a multisig
    	client.addMultiSigAddress(2,[e,myChannelPubkey],function(data){
    		myChannelMultisig = data
    	}) 
	}else{//if it's address-length then it's probably a multisig
		proposedMultisig = data
	}
    //0250863ad64a87ae8a2fe83c1af1a8403cb53f53e486d8511dad8a04887e5b2352
    //1PMycacnJaSqwwJqjawXBErnLsZ7RkXUAs
};

//create multisig
//create token commit tx
//create and co-sign trades
