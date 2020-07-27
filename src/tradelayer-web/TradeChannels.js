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

channelManager.queryDealers = function(minAvgLatency, maxRejectionRate, maxRejectionTime, minVolume, cb){
	//calls server to return data with the above params
	var thisURI = serverURI
	var wholeURI=thisURI+minAvgLatency+'/'+maxRejectionRate+'/'+maxReject
	rest.get(wholeURI).on('complete',function(data){
		return cb(data)
	})
}

channelManager.sendIndicatorOfInterest= function(contract, buySell,amount, price, specialType, secondSigner=true,cb){
	var thisURI = serverURI
	var wholeURI=thisURI+"sendIoI"
	if(specialType=='null'){specialType='limit'}
	var params = {'contract':contract,'buySell':buySell,'amount':amount,'price':price,'specialType':specialType,'secondSigner':secondSigner}
	rest.post(wholeURI,params).on('complete',function(data){
		return cb(data)
	})
}

//Replaces order api call.

channelManager.getBook= function(contract, amount, reputationMin, volumeMin, cb){

	//Returns an orderbook snapshot just like people are used to.
	var thisURI = serverURI
	rest.get(thisURI+minAvgLatency+'/'+maxRejectionRate+'/'+maxRejectionTime+'/'+minVolume).on('complete',function(data){
		return cb(data)
	})
}


channelManager.takeIndicatorOfInterest= function(contract, buySell, amount, price, specialType, firstSigner=true, pubkey,cb){
	var thisURI = serverURI
	var wholeURI=thisURI+"takeIoI"
	if(specialType=='null'){specialType='limit'}
	var params = {'contract':contract,'buySell':buySell,'amount':amount,'price':price,'specialType':specialType,'firstSigner':firstSigner}
	rest.post(wholeURI,params).on('complete',function(data){
		return cb(data)
	})
}

//Liquidity taking, does carry risk of getting squeezed on the option value, but usually will come back confirming quickly. Pubkey is needed if no pre-existing channel has been established with the given offeror. But how is a user to know? The logic inside the wallet has to check its pre-established inventory, see if the quote lines up with any existing channels, transfer capital to a channel or otherwise establish if it not pre-existing, and then form/sign the proposed quote-match and send to the dealer. The particular atomicity of these quote-interactions, ideally, will be obscured by a two-click process.

channelManager.submitTX= function(txString,cb){
	var thisURI = serverURI
	var wholeURI=thisURI+"takeIoI"
	if(specialType=='null'){specialType='limit'}
	var params = {'contract':contract,'buySell':buySell,'amount':amount,'price':price,'specialType':specialType,'firstSigner':firstSigner}
	rest.post(wholeURI,params).on('complete',function(data){
		return cb(data)
	})
}

//This passes the string back and forth

channelMananger.proposeChannel = function(pubkey,ipaddress, dealerid, cb){
	if(ipaddress==null){
		ipaddress=serverURI 
		var wholeURI = serverURI+"proposeChannel"
	}else{var wholeURI = ipaddress}
	if(dealerid==null){
		dealerid = "route"	
	}
	var params = {'pubkey':pubkey,'ipaddress':ipaddress,'dealerid':dealerid}
	rest.post(wholeURI,params).on('complete',function(data){
		return cb(data)
	})
}

channelManager.buildTransfer = function(cacheAddress, channelAddress, cacheInput, inputAmount, propertyid, amount, cb){
	tl.createpayload_transfer(propertyid,amount,function(payload){
		tl.buildRaw(payload,cacheInput,[0],channelAddress,inputAmount,function(txString){
			tl.simpleSign(txString,function(data){
				return cb(txString)
			})
			
		})
	})
}

channelManager.buildCommit = function(fromAddress,toAddress){}

channelManager.buildWithdraw = function(originalAddress,channelAddress){}

channelManager.buildWithdrawLocalWallet = function(addressSet){}

channelManager.buildTokenToTokenTrade = function(channeladdress, id1,amount, id2, amount, secondSigner=true){}

channelManager.buildUXTOToTokenTrade = function(channeladdress, tokenSellerAddress, id1=0,amount, id2, amount2, blockheight_expiry, secondSigner=true, multisigInput,purchaseInput,cb){
	tl.getBlock(null,function(data){
		var height = data.height+3
		tl.createpayload_instant_trade(id1, amount, height, id2, amount2, function(payload){
			tl.buildRaw(payload,multisigInput,[0],tokenSellerAddress,UXTOAmount, function(txString){
				tl.fundRawTransaction(txstring,{'replaceable':true},function(data){
					return cb(data)
				})
			})
		})
	}
} 

channelManager.buildContractTrade = function(channeladdress, contractid,amount, refAddrIsBuyer, secondSigner=true)

//Prompts to initiate creation of a new multisig channel via the server relay with a new counterparty. This would flow in the wallet, such that the client code would pingback information about the quote the user selected, then ping this call, then returning a new channel, propose the part-signed tx to the channel.
