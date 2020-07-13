const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)
var serverURI = 'http://192.155.93.12:76/api/'
//early draft on the OO design of this module

var myChannels = []
var Channel = {'multisigAddr':'','counterparty':{},'positions':[],collateralid:1,'myMargin':0,'myPNL':0,'counterpartyMargin':0}
var counterpartyProfiles = []
var Profile = {'alias':'','regulatoryStatus':'unregulated','myHistoricalVolume':0,'avgSignBackTime':300,'cancelRate':0.02,'RepRating':70}

var channelManager = {}

channelManager.queryDealers = function(minAvgLatency, maxRejectionRate, maxRejectionTime, minVolume){
	//calls server to return data with the above params
	var thisURI = serverURI+'/'
	rest.get(thisURI+minAvgLatency+'/'+maxRejectionRate+'/'+maxRejectionTime+'/'+minVolume).on('complete',function(data){
		var table = data.dealers
	})
}

channelManager.sendIndicatorOfInterest= function(contract, buySell,amount, price, specialType, secondSigner=true)

//Replaces order api call.

channelManager.getBook= function(contract, amount, reputationMin, volumeMin){

	//Returns an orderbook snapshot just like people are used to.
	var thisURI = serverURI+'/'
	rest.get(thisURI+minAvgLatency+'/'+maxRejectionRate+'/'+maxRejectionTime+'/'+minVolume).on('complete',function(data){
		var table = data.quotes

	})
}


channelManager.takeIndicatorOfInterest= function(contract, buySell, amount, price, specialType, firstSigner, pubkey)

//Liquidity taking, does carry risk of getting squeezed on the option value, but usually will come back confirming quickly. Pubkey is needed if no pre-existing channel has been established with the given offeror. But how is a user to know? The logic inside the wallet has to check its pre-established inventory, see if the quote lines up with any existing channels, transfer capital to a channel or otherwise establish if it not pre-existing, and then form/sign the proposed quote-match and send to the dealer. The particular atomicity of these quote-interactions, ideally, will be obscured by a two-click process.

channelManager.submitCoSignedTrade= function(txString)

//This passes the signed trade to the server to relay a confirmation to the liquidity taker and broadcast to the blockchain.

channelManager.submitPartSignedTrade= function(txString)

//This is the initial part of the handshake that the submitCoSignedTrade completes.

channelMananger.proposeChannel = function(pubkey)

channelManager.buildTransfer = function(cacheAddress){

}

channelManager.buildCommit = function(fromAddress,toAddress){}

channelManager.buildWithdraw = function(originalAddress,channelAddress){}

channelManager.buildWithdrawLocalWallet = function(addressSet)

channelManager.buildTokenToTokenTrade = function(channeladdress, id1,amount, id2, amount, secondSigner=true)

channelManager.buildUXTOToTokenTrade = function(channeladdress, tokenSellerAddress, id1=0,amount, id2, amount2, blockheight_expiry, secondSigner=true, multisigInput,purchaseInput){
	tl.getBlock(null,function(data){
		var height = data.height+3
		tl.createpayload_instant_trade(id1, amount, height, id2, amount2, function(payload){
			tl.buildRaw(payload,multisigInput,[0],tokenSellerAddress,UXTOAmount, function(txString){
				tl.fundRawTransaction(txstring,{'replaceable':true},function(data){
					return data
				})
			})
		})
	}
} 

channelManager.buildContractTrade = function(channeladdress, contractid,amount, refAddrIsBuyer, secondSigner=true)

//Prompts to initiate creation of a new multisig channel via the server relay with a new counterparty. This would flow in the wallet, such that the client code would pingback information about the quote the user selected, then ping this call, then returning a new channel, propose the part-signed tx to the channel.
