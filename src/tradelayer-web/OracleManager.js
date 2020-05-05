const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)

var myAddresses = {}
var thisAdminAddress = '' 
var thisBackupAddress = ''
var backupKey1 = ''
var backupKey2 = '' //the idea of a backup address is it's single sig but cold (multisig is viable but largely redundant), 
//we even (ideally) pre-sign a tx to save time on deploying the backup in the event of breach

var mode = 'backup'// also mode can be 'new', 
                   // 'transfer'
                   //  build 'backup'
                   // 'createRestoreTX',
                   // 'sendRestoreTX'
                   // 'createCloseTX'
                   // 'sign' 
// - close is very serious because it may displace a valuable piece of digital real estate (the contract id number and its graphical adherents)
// but if you did close an Oracle out, and wanted to go back and create a new one and try to hold onto your business, simply engaging
// your technical collaborators 
// 'new' runs a new Oracle contract creation with chosen parameters

const contractTitle = '' //if running in new mode, the title will be in the parameters object below. If running in transfer mode, must be supplied
                         //in a future version we'll get this value via RPC in any mode.
const parameters = {}    //must be populated for 'new' mode

fs.readFile('adminAddress.json', (err, data) => {
  if (err) {
    console.error(err)
    return data
  }
  if(data!=undefined||data!=null){
  	thisAdminAddress = data
  }
})

fs.readFile('coldBackup.txt', (err, data) => {
  if (err) {
    console.error(err)
    return data
  }
  if(data!=undefined||data!=null){
  	thisBackupAddress = data
  }
})
keyExport = {}

function createBackupMultisig(cb){
	//creates and returns a singlesig address that can be exported (for use on cold storage machines)
	//we're going to go ahead and build/sign a tx to restore an oracle contract to this back-up address, so that it may rest
	//on paper, until such time as a security compromise emergency may require its rapid broadcast to the network.
	client.getNewAddress(null, function(data){
			var firstAddress = data
			keyExport.firstAddress=firstAddress
			client.dumpPrivKey(firstAddress,function(data){
				var firstKey = data
				keyExport.firstKey=firstKey
				client.getNewAddress(null, function(data){
					var secondAddress = data
					keyExport.secondAddress = secondAddress
					client.dumpPrivKey(secondAddress,function(data){
						var secondKey=data
						keyExport.secondKey = secondKey
						client.getAddressInfo(keyExport.firstAddress,function(data){
							keyExport.firstPubKey= data.scriptPubKey
							client.getAddressInfo(keyExport.secondAddress,function(data){
								keyExport.secondPubKey=data.scriptPubKey
								client.addMultsigAddress(2,[keyExport.firstPubKey,keyExport.secondPubKey,function(data){
									keyExport.address = data.address
									keyExport.redeemkey = data.redeemkey
									thisBackupAddress= {address:data.address,redeemkey:data.redeemkey,type:'backup'}
										return cb(thisBackupAddress)
									})
								})
							})
						})		
					})
		        })
		    })
}


function createOracle(params){
	client.createOracle(params.address,params.url,params.numeratorid,params.title,params.duration,params.notional,
		params.denominatorCollateralId,params.marginReq,thisBackupAddress,function(data){
		return console.log(data)
	})
}
	//tl_create_oraclecontract ${ADDR} 1 4 "OIL dUSD" 1000 1 4 0.5 ${ADDR2}
		//multisig address must be funded

var txExport =''
var redeemkey = ''
//the array of inputs
if(mode=="backup"){
	createBackupMultisig(function(data){
			 var string = JSON.stringify(obj)
					fs.writeFile('coldBackup.txt',string,function(err,data,resheaders){
							if(err){console.log(err)}
							return data
						fs.writeFile('keys.txt',keyExport,function(err,data,resheaders){
							return data
						})
					})
	})
}

if(mode=="new"){
	var params = {'address':thisAdminAddress,'url':'',
				  'numeratorid':1,'title':'BTC/USD','duration':2000,
				  'notional':1,'denominatorCollateralId':3,
				  'marginReq':0.1,'thisBackupAddress':thisBackupAddress}

	createOracle(params)
}

if(mode=="transfer"){
	var newAdminAddress = ""
	client.changeOracleRef(thisAdminAddress,newAdminAddress,contractTitle,function(data){
		return(data)
	})
}

if(mode=="createRestoreTX"){
	//this assumes you have funded the multisig address
	client.getpayload_oraclebackup(contractTitle,function(op_payload){
										rest.get('https://blockchain.info/rawaddr/'+thisBackupAddress).on('complete',function(data){
											var amount = data.final_balance //note this is an integer in Satoshis
											var input = data.tx[0] //this only works if the multisig has just 1 input, will make this more resilient
											client.buildRaw(op_payload,input,[0],'',amount, function(tx_payload){//inputs are missing from param 2
												var string = JSON.stringify({'payload':tx_payload,'input':input})
												fs.writeFile(string,'unsignedRestoreTx.txt',function(data){
													return(data)
												})
											})	
										})
	})
}

if(mode=="sendtx"){
   //broadcast a raw.tx from a local file containing, presumably, a signed multisig tx from the backup address to restore to that addr
	fs.readFile('signedtx.txt', (err, data) => {
		 	if (err){
		    	console.error(err)
			}
			if(data!=undefined||data!=null){
  				tl.sendRawTransaction(data,function(data){
   	  				console.log(data)
   	  				return data
   				})
  			}
    })
}

if(mode=="createCloseTX"){
	client.getpayload_closeOracle(contractTitle,function(op_payload){
										rest.get('https://blockchain.info/rawaddr/'+thisBackupAddress).on('complete',function(data){
											var amount = data.final_balance //note this is an integer in Satoshis
											var input = data.tx[0] //this only works if the multisig has just 1 input, will make this more resilient
											
											client.buildRaw(op_payload,input,[0],'',amount, function(tx_payload){//inputs are missing from param 2
												var string = JSON.stringify({'payload':tx_payload,'input':input})
												fs.writeFile(string,'unsignedCloseTx.txt',function(data){
													return(data)
												})
											})	
										})
	})
}
var close = false
if(mode=='sign'){
	fs.readFile('keys.txt',(err,data) => {
		var firstKey = data.firstKey
		var secondKey = data.secondKey
		var stringSource = 'unsignedRestoreTx.txt'
		var redeemkey = data.redeemkey
		if(close==true){stringSource='unsignedCloseTx.txt'}
		fs.readFile(stringSource, (err, data) => {
			var input = data.input
			var tx_payload = data.payload
			rest.get('https://blockchain.info/rawtx/'+input).on('complete',function(data){
				var pubscript = data.out.script
			 	if (err){
			    	console.error(err)
				}
				if(data!=undefined||data!=null){
	  				client.signRaw(tx_payload,firstKey,redeemkey,input,0,pubscript,function(data){
	  					fs.writeFile(data,'signedtx.txt',function(err,data){
	  						console.log('signed!')
	  					})
	  				})
	  			}
	  		})
	    })
	})
}

function updateAdminAddress(oldAddress, newAddress,contractTitle,cb){
	client.changeOracleRef(oldAddress,newAddress,contractTitle,function(data){
		return cb(console.log("tx status: "+data))
	})
}

function sendData(adminAddress,contractTitle, high, low, close, cb){
	client.publishOracleData(adminAddress,contractTitle,high,low,close,function(data){
		return cb(data)
	}
}