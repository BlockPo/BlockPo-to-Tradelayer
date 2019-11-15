var user = ''
var pass = ''

/*
binary /home/blockpo/BlockPo-to-Tradelayer/src/
datadir /home/blockpo/.litecoin
*/


var litecoin = require('litecoin');

var tl = {}

tl.init = function(user, pass, otherip, test){
  var host
  if(otherip == null){host = 'localhost'}else{host=otherip}
  var port
  if(test == false || test == null){port = 9332}else{port=19336}  
  var client = new litecoin.Client({
    host: host,
    port: port,
    user: user,
    pass: pass
    timeout:30000
    ssl:false
  })
  
  return client
}

var client = tl.init(user, pass, '200.86.176.38', true)

tl.getnewaddress = function(account, cb){
    if(account == null|| account == undefined){
    client.cmd('getnewaddress',function(err,address,resHeaders){
      if (err) return console.log(err)
  console.log("created address:"+address)
  return cb(address)
    })}else{
        client.cmd('getnewaddress', account, function(err, address, resHeaders) {
              if (err) return console.log(err)
  console.log(address)
  return cb(address)
        })
    }
}

tl.listaccounts = function(cb){
    client.cmd('listaccounts',function(err,accounts,resHeaders){
      if (err) return console.log(err)
  console.log(accounts)
  return cb(accounts)
    })
}

tl.getaccountaddress = function(account, cb){
    if(account == null){
    client.cmd('getaccountaddress',function(err,data,resHeaders){
      if (err) return console.log(err)
  console.log("Account created"+data)
  return cb(data)
    })}else{
    client.cmd('getaccountaddress', account, function(err,data,resHeaders){
      if (err) return console.log(err)
    console.log("Account returned:"+data)
    return cb(data)
    })}
}

tl.getaddressesbyaccount = function(cb){
    client.cmd('getaddressesbyaccount',function(err,addresses,resHeaders){
      if (err) return console.log(err)
  console.log(addresses)
  return cb(data)
    })
}

tl.getaccount = function(address, cb){
     client.cmd('getaccount', address, function(err,account,resHeaders){
      if (err) return console.log(err)
  return cb(account)
    })
}

tl.getbalance = function(account, confirmations, cb){
  client.cmd('getbalance', account, 1, function(err, balance, resHeaders){
  if (err) return console.log(err)
  console.log('Bitcoin balance:', balance)
  return cb(balance)
  })
}

tl.getreceivedbyaddress = function(address, confirmations, cb){
     client.cmd('getreceivedbyaddress', address, confirmations, function(err, balance, resHeaders){
  if (err) return console.log(err)
  console.log('bitcoin received:', balance)
  return cb(balance)
  })
}

tl.getinfo =function(cb){
    client.cmd("getinfo", function(err, data, resHeaders){
  if (err) return console.log(err);
 
  })
  return cb(data)
}

//all parameters must be text

tl.getrawtransaction = function(txid, cb){
     client.cmd("getrawtransaction", txid , 1,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.getblockhash = function(block, cb){
     client.cmd("getblockhash", block,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
         
     })
}
 
tl.getblock = function(hash, cb){
     client.cmd("getblock", hash,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  }) 
}

tl.sendrawtransaction = function(tx, cb){
    try{
       client.cmd("sendrawtransaction", tx,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  })
    }catch(e){
      return e
    }
    return cb(data)
}

tl.validateaddress = function(addr, cb){
     client.cmd("validateaddress", addr,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.sendtoaddress = function(addr, amt, cb){
    client.cmd("sendtoaddress",addr,amt,function(err, data, resHeaders){
        console.log(data)
        return cb(data)
    })
}

tl.createrawtransaction = function(ins,outs, cb){
     client.cmd("createrawtransaction",ins,outs,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
    })
}

tl.decoderawtransaction = function(rawtx, cb){
     client.cmd("decoderawtransaction", rawtx,function(err, data, resHeaders){
  if (err) return console.log(err)
 
  return cb(data)
  })
}

// tllayer Specific RPC calls
tl.gettlbalance = function(addr, propertyid, cb){
     client.cmd("tl_getbalance", addr, propertyid,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.getallbalancesforaddress = function(addr, cb){
     client.cmd("tl_getallbalancesforaddress", addr,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}
    
tl.getallbalancesforid = function(propertyid, cb){
     client.cmd("tl_getallbalancesforid", propertyid,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.gettransaction = function(tx, cb){
     client.cmd("tl_gettransaction", tx,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.getorderbook = function(id1, id2, cb){
    if(id2 == null){
        client.cmd("tl_getorderbook", id1, function(err, data, resHeaders) {
            if(err) return console.log(err)
        return cb(data)
    })}else{client.cmd("tl_getorderbook", id1, id2, function(err, data, resHeaders) {
        if(err){console.log(err)}
        console.log('book'+data)
        //if(err) return console.log(err)
        return cb(data)
    })}
}

tl.listtransactions = function(txid, count, skip, startblock, endblock, cb){
    if(txid == null){txid ='*'}
    if(count == null){count =1}
    if(skip == null){skip =0}
    if(startblock == null){startblock=0}
    if(endblock == null){endblock=9999999}
     client.cmd("tl_listtransactions",txid, count, skip, startblock, endblock, function(err, data, resHeaders){
        if (err) return console.log(err)
 
        return cb(data)
  })
}

tl.listblocktransactions = function(height, cb){
     client.cmd("tl_listblocktransactions", height,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.getproperty= function(propertyid, cb){
     client.cmd("tl_getproperty", propertyid,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.listproperties=function(cb){
     client.cmd("tl_listproperties",function(err, data, resHeaders){
        if (err) return console.log(err)
        return cb(data)
  })
}

tl.getactivedexsells=function(cb){
        client.cmd("tl_getactivedexsells",function(err, data, resHeaders){
            if (err) return console.log(err);
 
            return cb(data)
        })
}

tl.gettrade = function(txid, cb){
    client.cmd("tl_gettrade", txid, function(err, da, resHeaders){
        return cb(data)
    })
}

tl.getsto = function(txid, recipients, cb){
    if(recipients = null){recipients = "*"}
    client.cmd("tl_getsto", txid, recipients, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.getdivisibleproperty = function(propertyid){
     client.cmd("tl_getproperty",'result','divisible', propertyid,function(err, data, resHeaders){
  if (err) return console.log(err);
 
  return cb(data)
  })
}

tl.getproperty = function(propertyid, cb){
     client.cmd("tl_getproperty", propertyid,function(err, data, resHeaders){
        if (err) return console.log(err);
 
        return cb(data)
    })
}

tl.gettradehistory= function(id1, id2, trades, cb){
    client.cmd("tl_gettradehistoryforpair", id1, id2, trades, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.gettradehistoryaddress= function(address, trades, propertyfilter, cb){
    
        if(propertyfilter == null){
            client.cmd("tl_gettradehistoryforaddress", address, trades, function(err, data, resHeaders){
                return cb(data)
            })
        }else{client.cmd("tl_gettradehistoryforaddress", address, trades, propertyfilter, function(err, data, resHeaders){
        return cb(data)
        })}
}

tl.listpendingtransactions= function(addressfilter, cb){
    if(addressfilter == null){addressfilter = ''}
    client.cmd("tl_listpendingtransactions", addressfilter, function(err, data, resHeaders) {
        return cb(data)
    })
}

tl.send = function(address, address2, id, amount, cb){
    client.cmd('tl_send', address, address2, id, amount, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.senddexsell = function(address, id, amount1, amount2, paymentwindow, fee, action, cb){  //action = (1 for new offers, 2 to update, 3 to cancel)
    client.cmd('tl_senddexsell', address, id, amount1, amount2, paymentwindow, fee, action,function(err, data, resHeaders){
        return cb(data)
    })
}

tl.senddexaccept = function(address, address2, id, amount, cb){  //action = (1 for new offers, 2 to update, 3 to cancel)
    client.cmd('tl_senddexsell', address, address2, id, amount, false,function(err, data, resHeaders){
        return cb(data)
    })
}
    
tl.sendcancelalltrades = function(address, ecosystem, cb){
    client.cmd('tl_sendcancelalltrades', address, ecosystem,function(err, data, resHeaders){
        return cb(data)
    })
}

tl.sendcanceltradesbyprice = function(address, id1, amount1, id2, amount2, cb){
    client.cmd('tl_sendcanceltradesbyprice', address, id1, amount1, id2, amount2, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.sendcanceltradesbypair = function(address, id1, id2, cb){
    client.cmd('tl_sendcanceltradesbypair', address,function(err, data, resHeaders){
        return cb(data)
    })
}

tl.sendchangeissuer = function(address1, address2, propertyid, cb){
    client.cmd('tl_sendchangeissuer', address1, address2, propertyid, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.sendtrade = function(address, id1, amount, id2, amount2, cb){
    client.cmd('tl_sendtrade', address, id1, amount.toString(), id2, amount2.toString(), function(err, data, resHeaders){
        if(err){data=err}
        return cb(data)
    })
}

tl.sendall = function(address1, address2, ecosystem, cb){
    client.cmd('tl_sendall', address1, address2, ecosystem, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.sendissuancefixed = function(params, cb){
    var address = params.fromaddress
    var ecosystem = params.ecosystem
    var type = params.type
    var previousid = params.previousid

    var category = params.category

    var subcategory = params.subcategory

    var name = params.name
    var url = params.url

    var data = params.data

    var amount = params.amount

    var tokensperunit = params.tokensperunit

    var deadline = params.deadline

    var earlybonus  = params.earlybonus
    var issuerpercentage = params.issuerpercentage
    client.cmd('tl_sendissuancefixed',address, ecosystem, type, previousid, category, subcategory, name, url, data, amount, function(err, data, resHeaders){
        return cb(data)
    })
} 

tl.sendrevoke = function(address, id, amount, note, cb){
    client.cmd('tl_sendrevoke', address, id, amount, note, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.sendgrant = function(address1, address2, id, amount, note, cb){
    client.cmd('tl_sendgrant', address1, address2, id, amount, note, function(err, data, resHeaders){
        return cb(data)
    })
}

tl.sendissuancemanaged = function(params, cb){
    var address = params.fromaddress
    var ecosystem = params.ecosystem
    var type = params.type
    var previousid = params.previousid
    var category = params.category
    var subcategory = params.subcategory
    var name = params.name
    var url = params.url
    var data = params.data
    client.cmd('tl_sendissuancemanaged', address, ecosystem, type, previousid, category, subcategory, name, url, data, function(err, data, resHeaders){
        console.log(data)
        return cb(data)
    })
} 

tl.sendContractTrade = function(params, cb){
	var address = params.address
    var contractcode = params.contractcode
    var quantity = params.quantity
    var price = params.price
    var tradetype = params.tradetype

    client.cmd('tl_tradecontract', address, contractcode, quantity, price, tradetype, function(err, data, resHeaders){
        console.log(data)
        return cb(data)
    })
}

tl.cancelAllContractsByAddress = function(address, ecosystem, contractid, cb){
	client.cmd('tl_cancelallcontractsbyaddress', address, ecosystem, contractid,function(err,data,resHeaders){
		console.log(data)
		return cb(data)
	})
}

var rawPubScripts = []

tl.buildRaw= function(payload, inputs, vOuts, refaddresses){
	var txstring = ""
	client.cmd('tl_createrawtx_input', txstring, inputs, vouts, function(err, data, resHeaders){
		if(err==null){
			txstring = data
		}else{return err}
		client.cmd('tl_createrawtx_reference', txstring, refaddresses, function(err, data, resHeaders){
			if(err==null){
				txstring = data
			}else{return err}
			client.cmd('tl_createrawtx_opreturn', txstring, payload, function(err, data, resHeaders){
				if(err==null){
				txstring = data
				}else{return err}
				    rest.get('https://blockchain.info/rawtx/'+inputs[0]).on('complete', function(data){
			    	/*
			    	{
				    "hash":"b6f6991d03df0e2e04dafffcd6bc418aac66049e2cd74b80f14ac86db1e3f0da",
				    "ver":1,
				    "vin_sz":1,
				    "vout_sz":2,
				    "lock_time":"Unavailable",
				    "size":258,
				    "relayed_by":"64.179.201.80",
				    "block_height, 12200,
				    "tx_index":"12563028",
				    "inputs":[


				            {
				                "prev_out":{
				                    "hash":"a3e2bcc9a5f776112497a32b05f4b9e5b2405ed9",
				                    "value":"100000000",
				                    "tx_index":"12554260",
				                    "n":"2"
				                },
				                "script":"76a914641ad5051edd97029a003fe9efb29359fcee409d88ac"
				            }

				        ],
				    "out":[

						        {
				                    "value":"98000000",
				                    "hash":"29d6a3540acfa0a950bef2bfdc75cd51c24390fd",
				                    "script":"76a914641ad5051edd97029a003fe9efb29359fcee409d88ac"
				                },

				                {
				                    "value":"2000000",
				                    "hash":"17b5038a413f5c5ee288caa64cfab35a0c01914e",
				                    "script":"76a914641ad5051edd97029a003fe9efb29359fcee409d88ac"
				                }

					        ]
					}
					*/
					var vOut = vOuts[0]
					rawPubScripts = []
					rawPubScripts.push(data.out[vOut].script)
					var fee = 0.00001*(inputs.length*0.11+0.04+0.038*3)
					if(inputs.length>1){
						rest.get('https://blockchain.info/rawtx/'+inputs[1]).on('complete', function(data){
						scripts.push(data.out[vOut].script)
						return tl.addChangeAddress(txstring, rawPubScripts, inputs, vOuts, changeAddress, amount,fee)
						}
					}else{
						return tl.addChangeAddress(txstring, rawPubScripts,inputs, vOuts, changeAddress, amount,fee)
					}
			    }
			})
		})
	}
}

tl.addChangeAddress = function(txstring, rawPubScripts,inputs, vOuts,changeAddress, amount){
	var data = "[{\'txid\': \'"+inputs+"\',\'vout\':"+vOuts+",\'scriptPubKey\':\'"+rawPubScripts+",\"value \":"+amount"}]"
	client.cmd('tl_createrawtx_change', txstring, data, changeAddress, fee, function(err, data, resHeaders){
		if(err == null){
			return {'tx':data,'rawPubScripts':rawPubScripts,'input':inputs,'vOuts':vOuts}
		}else{return err}
	})
}

tl.getContractInfo =  function(index, oracleContracts, nativeContracts){
	if(index==null){
		index=0
		nativeContracts = []
		oracleContracts = []
	}
	
	client.cmd("tl_listproperties",function(err, data, resHeaders){
        if (err) return console.log(err)     	
     		var thisData = data[i]
     		var propertyid = thisData.propertyid
     		if(index<=data.length){
     		client.cmd("tl_getproperty", propertyid, function(err,data,resHeaders){
     				if(err){
     					console.log(err)
     				}else if(data.type =="contract"){
     					if(data.backUpAddress!=undefined){
     						oracleContracts.push(data)
     					}else{
     						nativeContracts.push(data)
     					}
     				}
     				index+=1
     				tl.getContractInfo(index,oracleContracts,nativeContracts)
     		})   
     	}else{
     		return {'nativeContracts':nativeContracts,'oracleContracts':oracleContracts}
     	}
  	})
}

tl_createNativeContract= function(thisAddress, numeratorid, title, durationInBlocks, notional,denominatorCollateralid){
	////tl_createcontract ${ADDR} 1 4 "ALL/dUSD" 1000 1 4 0.5 #leverage = 10
	client.cmd('tl_createcontract', thisAddress, 1, numeratorid denominatorid, title, durationInBlocks, notional,denominatorCollateralid, marginReq, function(err,data,resHeaders){
		if(err == null){
			return data
		}else{return err}
	}) 
}

tl.createOracleContract = function(thisAddress, numeratorid, title, durationInBlocks, notional,denominatorCollateralid, backUpAddr){
	//tl_create_oraclecontract ${ADDR} 1 4 "OIL dUSD" 1000 1 4 0.5 ${ADDR2}
	client.cmd('tl_createoraclecontract', thisAddress, 1, numeratorid, title, durationInBlocks, notional,denominatorCollateralid, marginReq, backUpAddr, function(err,data,resHeaders){
		if(err == null){
			return data
		}else{return err}
	})
}

tl.signRaw = function(txstring, privkey, redeemkey, input, vout, pubscript){
	var data = "[{'txid':"+input+",'vout':"+vout+",'scriptPubKey':"+pubscript+",'redeemScript':"+redeemkey+"}]"
	if(redeemkey==null){data = "[{'txid':"+input+",'vout':"+vout+",'scriptPubKey':"+pubscript+"}]"}
	var sign = '['+privkey+']'
			client.cmd('signrawtransaction', data,sign,function(err, data, resHeaders){
				if(err == null){
					return data
				}else{return err}
			})
}

/* TODO: wrap these

        tl_getreserve //equivalent to margin on orders or under positions
        tl_getfullposition //full set of positions on an address
        tl_getposition //contract specific: propertyid, address
        tl_getcontractreserve //equivalent to open orders, propertyid, address
        tl_getcontractorderbook
        tl_gettradehistoryunfiltered
        tl_getpeggedhistory 
        tl_getallprice //this is the reference price of ALL to LTC that is used to convert ALL equivalent volumes into LTC totals for the purposes of vesting and liquidity reward

        tl_getupnl
        
        tl_getpnl //in tradelayer realized PNL isn’t transferable until a settlement event, at which point it moves from this column to the balance column.
    
        tl_getmarketprice
        
    tl_getexodus //hilariously we included a modular exodus address system as a makeshift way of punting on re-doing the UXTO Dex transactions with OP_Returns, which is a to-do for the backlog

    tl_getsum_upnl

    tl_create_oraclecontract           
    tl_setoracle
    tl_change_oracleref
    tl_oraclebackup
    tl_closeoracle
    tl_commit_tochannel
    tl_withdrawal_fromchannel
    tl_create_channel  
*/

tl.createpayload_createcontract = function(numeratorid, name, blocks, size, denominatorCollateralid, marginReq){
	/*
	ecosystem (string, required) the ecosystem to create the tokens in (1 for main ecosystem, 2 for test ecosystem)
	2. numerator (number, required) 4: ALL, 5: sLTC, 6: LTC

	3. name (string, required) the name of the new tokens to create

	4. blocks until expiration (number, required) life of contract, in blocks

	5. notional size (number, required) notional size

	6. collateral currency (number, required) collateral currency
	7. margin requirement

	*/

	client.cmd('tl_createpayload_createcontract', 1, numeratorid, name, blocks, size, denominatorCollateralid, marginReq, function(err,data, resHeaders){
		if(err){return err}else{return data}
	})
}


tl.createpayload_tradecontract = function(propertyid, amount, price, type, leverage){
 	/*tl_createpayload_tradecontract

		1. propertyidforsale (number, required) the identifier of the contract to list for trade

		2. amountforsale (number, required) the amount of contracts to trade

		3. effective price (number, required) limit price desired in exchange

		4. trading action (number, required) 1 to BUY contracts, 2 to SELL contracts

		5. leverage (number, required) leverage (2x, 3x, ... 10x)
 	*/
 	if(leverage==null){leverage = 1}
 	client.cmd('tl_createpayload_tradecontract', propertyid, amount, price, type, leverage, function(err,data, resHeaders){
		if(err){return err}else{return data}
	})       
} 


tl.createpayload_cancelallcontractsbyaddress = function(address, contractid){
/*
tl_createpayload_cancelallcontractsbyaddress
ecosystem (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem) 
contractId (number, required) the Id of Future Contract 
*/
	client.cmd('tl_createpayload_cancelallcontractsbyaddress', 1, contractid, function(err,data, resHeaders){
		if(err){return err}else{return data}
	}) 
}

tl.createpayload_closeposition = function(contractid){
/*
tl_createpayload_closeposition

ecosystem (number, required) the ecosystem of the offers to cancel (1 for main ecosystem, 2 for test ecosystem)
2. contractId
*/
	client.cmd('tl_createpayload_closeposition', 1, contractid, function(err,data, resHeaders){
		if(err){return err}else{return data}
	}) 
}

tl.createpayload_sendissuance_pegged = function(divisible, previousid, name, collateralid, contractid){
/*
tl_createpayload_sendissuance_pegged

1. ecosystem (string, required) the ecosystem to create the pegged currency in (1 for main ecosystem, 2 for test ecosystem)
2. type (number, required) the type of the pegged to create: (1 for indivisible tokens, 2 for divisible tokens)
3. previousid (number, required) an identifier of a predecessor token (use 0 for new tokens)

4. name (string, required) the name of the new pegged to create
"5. collateralcurrency (number, required) the collateral currency for the new pegged"

"6. future contract name (number, required) the future contract name for the new pegged"

7. amount of pegged (number, required) amount of pegged to create
*/
	client.cmd('tl_createpayload_closeposition', 1, contractid, function(err,data, resHeaders){
		if(err){return err}else{return data}
	}) 
}

tl.createpayload_send_pegged = function(namestring, amount){
	/*
	        tl_createpayload_send_pegged
			1. property name (string, required) the identifier of the tokens to send"         
    		"2. amount (string, required) the amount to send        
     */
	if(typeof amount == Number){amount = amount.toString()}
		client.cmd('tl_createpayload_send_pegged', namestring, amount, function(err,data, resHeaders){
		if(err){return err}else{return data}
	})
}

tl.createpayload_redemption_pegged = function(tokenNameString,amount, contractNameString){
	/*
	    tl_createpayload_redemption_pegged
	        "1. name of pegged (string, required) name of the tokens to redeem\n"
	"2. amount (number, required) the amount of pegged currency for redemption"
	 "3. name of contract (string, required) the identifier of the future contract involved\n"
	*/


	client.cmd('tl_createpayload_redemption_pegged', tokenNameString, amount, contractNameString, function(err,data, resHeaders){
		if(err){return err}else{return data}
	})
}

tl.createpayload_sendvesting = function(propertyid, amount){
/*
        tl_createpayload_sendvesting
        "1. propertyid (number, required) the identifier of the tokens to send\n"
            "2. amount (string, required) the amount of vesting tokens to send\n"
*/
	if(propertyid!=2){propertyid=2}
		client.cmd('tl_createpayload_sendvesting', propertyid, amount, function(err,data, resHeaders){
		if(err){return err}else{return data}
	})
}

    
exports.tl = tl