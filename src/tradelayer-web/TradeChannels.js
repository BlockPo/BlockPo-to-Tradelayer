const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')
const express = require('express')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)
var serverURI = 'http://192.155.93.12:76/api/'
//early draft on the OO design of this module

var Channel = {'multisig':'','counterparty':{},'positions':[],'collateralid':1,'myMargin':0,'myPNL':0,'counterpartyMargin':0}
var counterpartyProfiles = []
var Profile = {'alias':'','regulatoryStatus':'unregulated','myHistoricalVolume':0,'avgSignBackTime':300,'cancelRate':0.02,'RepRating':70}

var channelManager = {}
var channelManager.WSchannels ={}
var channelManager.multisigChannels = {}

channelManager.setWSChannels = function(dealers){
	for(var i=0; i<dealers.length;i++){
		if(channelManager.WSchannels.contains(dealers[i].ip)==false){
			//subscribes to indicator of interest websocket channel with dealer
			//generates nonce-like name
			//triggers setUpWSServer function to create new channel with a nonce-like name
			//rest or WS post call to trigger a process on dealer's side that 
			//causes them to subscribe to a specialized ws.feed with the parameter's URI name
			//and also to create a new websocket channel based on that name, and save that
			//then we save in channelManager.WSchannels an object containing these URIs
			//{'ip':'','incomingChannelName':'nonce name','outChannelName':'handshake param'}
		}
	}

}

channelManager.setUpWSServer = function(){
	//this sets up a new WS subscription for specialized channels
}

channelManager.maintainWSServer = function(){
	//this checks heartbeat on WS channels we're hosting/live subs and ideally returns a snapshot data object
}

channelMananger.listenFirstChannelProposal = function(address1,WSChannel,cb){
	//this creates a new multisig and returns it and the 2nd party's pubkey to the 1st party
	tl.getNewAddress(null,function(address2){
		tl.addMultisigAddress(2,[address1,address2],function(multisig){
			var obj = {'multisig': multisig, 'address1':address1, 'address2':address2,'WSChannelName':WSChannel,'myAddr1':false}
			//WS send-back obj
			channelMananger.multisigChannels.push(obj)
			cb(obj)
		})
	})
	
}

channelManager.listenChannelConfirmation = function(multisig, myAddr, address2,WSchannel){
	if(channelManager.multisigChannels.contains(WSchannel)&&myAddr==null){
		var myAddr = WSchannel.myAddr
	}else if(myAddr==null){
		//send WS message indicating an err, triggers flush of channel
	}
	tl.getAddressInfo(multisig,function(data){
		if(data.pubkeys.contains(address2)==true&&data.pubkeys.contains(myAddr){
			var obj= {'multisig':multisig,'address1':myAddr,'address2':address2,'WSchannel':WSchannel, 'myAddr1':true}
			channelMananger.multisigChannels.push(obj)
			cb(obj)
		}
	}
}

channelMananger.listenChannelErr = function(errMessage, multisigChannelObj, WSchannel){
	//flush multisig channel
}

channelManager.sendChannelErr= function(errMessage, multisigChannelObj, WSchannel)

channelMananger.messageToggleAvailability = function(availableBool, multisigChannelObj, WSchannel){
	//sends a true or false to indicate that the other trade should not expect a live quote, or that this is no longer the case
	//for instance market makers may run out of inventory temporarily and this helps alleviate expectations for liquidity
}

channelManager.queryDealers = function(minAvgLatency, maxRejectionRate, maxRejectionTime, minVolume, cb){
	//calls server to return data with the above params
	var thisURI = serverURI
	var wholeURI=thisURI+minAvgLatency+'/'+maxRejectionRate+'/'+maxReject
	rest.get(wholeURI).on('complete',function(data){
		channelManager.dealers = data
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
	
	//Liquidity taking, does carry risk of getting squeezed on the option value, but usually will come back confirming quickly. Pubkey is needed if no pre-existing channel has been established with the given offeror. But how is a user to know? The logic inside the wallet has to check its pre-established inventory, see if the quote lines up with any existing channels, transfer capital to a channel or otherwise establish if it not pre-existing, and then form/sign the proposed quote-match and send to the dealer. The particular atomicity of these quote-interactions, ideally, will be obscured by a two-click process.

	var thisURI = serverURI
	var wholeURI=thisURI+"takeIoI"
	if(specialType=='null'){specialType='limit'}
	var params = {'contract':contract,'buySell':buySell,'amount':amount,'price':price,'specialType':specialType,'firstSigner':firstSigner}
	rest.post(wholeURI,params).on('complete',function(data){
		return cb(data)
	})
}

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
	}else{
		var wholeURI = ipaddress

		//ws call to other ip with pubkey parameter
	}
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

channelManager.sendRaw= function(txString){
	tl.sendRawTransaction(txstring,function(txid){
		console.log(txid)
	})
}

channelManager.scanCommitsWithdrawals = function(multisigChannelObj,cb){
	var channelFlowData = {firstAddressPendingCommits:[], firstAddressPendingWithdrawals:[],
						   secondAddressPendingCommits:[], secondAddressPendingWithdrawals:[],
						   firstAddressHistoricalCommits:[], firstAddressHistoricalWithdrawals:[],
						   secondAddressHistoricalCommits:[], secondAddressHistoricalWithdrawals: []
						   }
	tl.getChannelInfo(multisigChannelObj.multisig, function(data1){
			var first = data.firstAddress
			var second = data.secondAddress
	
		tl.listPendingTransactions(multisigChannelObj.multisig,function(data2){
			if(data2.length>1){
				var txids = []
				for(var i = 0; i<data2.length; i++){
					txids.push(data2[i].txid)
				}
			    var txDetails =	decodeTransactions(txids,[],0)
			    	for(var t = 0; t<txDetails.length; t++){
			    		var tx = txDetail[t]
			    		if(tx.type =="commit"&&tx.referenceAddress==multisigChannelObj.multisig){
			    			if(tx.senderAddress==first){
			    				channelFlowData.firstAddressPendingCommits.push(tx)
			    			}else if(tx.senderAddress==second){
			    				channelFlowData.secondAddressPendingCommits.push(tx)
			    			}
			    			//fix the name and object details later when you see the JSON from the decode
			    		}else if(tx.type =="withdrawal"&&tx.referenceAddress==multisigChannelObj.multisig){
			    			if(tx.senderAddress==multisigChannelObj.firstAddress){
			    				channelFlowData.firstAddressPendingWithdrawals.push(tx)
			    			}else if(tx.senderAddress==multisigChannelObj.secondAddress){
			    				channelFlowData.secondAddressPendingWithdrawals.push(tx)
			    			}
			    		}
			    	}
			}

				tl.getCommits(first,function(data3){
					channelFlowData.firstAddressHistoricalCommits.push(data3)
					tl.getWithdrawals(first,function(data4){
						channelFlowData.firstAddressHistoricalWithdrawals.push(data4)
						tl.getCommits(second,function(data5){
							channelFlowData.secondAddressHistoricalCommits.push(data5)
							tl.getWithdrawals(second,function(data){
									channelFlowData.secondAddressHistoricalWithdrawals.push(data)
									return cb(channelFlowData)
							})
						})
					})
				})
		})
	})
}

channelManager.reconcileFlowDataToChannelMap= function(channelFlowData){
	//loops through and updates values about inventory in ones active channels, the channelManager.multisigChannels array of objects
}

channelManager.decodeTransactions = function(txids,detailsArray,iterator){
	tl.getTransaction(txids[iterator],function(data){
		detailsArray.push(data)
		iterator+=1
		if(iterator=txids.length){
				return detailsArray
		}else{
				return channelMananger.decodeTransactions(txids,detailsArray,iterator)
		}
	})
}

channelManager.buildCommit = function(fromAddress,toAddress){
	tl.createpayload_commit()
}

channelManager.buildWithdraw = function(originalAddress,channelAddress){}

channelManager.buildWithdrawLocalWallet = function(addressSet){}

channelManager.buildTokenToTokenTrade = function(channeladdress, id1,amount, id2, amount, secondSigner=true,cb){
	tl.getBlock(null,function(data){
		var height = data.height+3
		tl.createpayload_instant_trade(id1, amount, height, id2, amount2, function(payload){
			tl.buildRaw(payload,multisigInput,[0],tokenSellerAddress,0.00000546, function(txString){
					return cb(data)
			})
		})
	}
}

channelManager.buildLTCTokenTrade = function(channeladdress, channelInput, tokenSellerAddress, propertyid, amountOfTokens, LTCPrice, blockheight_expiry, secondSigner=true,cb){
	tl.getBlock(null,function(data){
		var height = data.height+3
		tl.createpayload_instant_LTC_trade(propertyid, amount, blockheight_expiry, LTCPrice, function(payload){
			tl.buildRaw(payload,channelInput,[0],tokenSellerAddress,UXTOAmount, function(txString){
				tl.fundRawTransaction(txstring,{'replaceable':true},function(data){
					return cb(data)
				})
			})
		})
	}
} 

channelManager.buildContractTrade = function(channeladdress, channelInput, contractid,amount, buySell, secondSigner=true,cb){
	var leverage = 10
	if(buySell=='buy'){buySell=1}else if(buySell=='sell'){buySell=2}
	tl.getBlock(null,function(data){
		var height = data.height+3
		tl.createpayload_contract_instant_trade(contractid, amount, height, price, buySell, leverage, function(payload){
			tl.buildRaw(payload,channelInput,[0],null,0.00000546, function(txString){
				tl.fundRawTransaction(txstring,{'replaceable':true},function(data){
					return cb(data)
				})
			})
		})
	}
}

//Prompts to initiate creation of a new multisig channel via the server relay with a new counterparty. This would flow in the wallet, such that the client code would pingback information about the quote the user selected, then ping this call, then returning a new channel, propose the part-signed tx to the channel.
