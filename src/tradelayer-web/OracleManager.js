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
var baclupKey2 = '' //the idea of a backup address is it's single sig but cold (multisig is viable but largely redundant), 
//we even (ideally) pre-sign a tx to save time on deploying the backup in the event of breach

var mode = 'backup'// also mode can be 'new', 'transfer', build backup 'restore' tx, build backup 'close' tx 
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
    return
  }
  if(data!=undefined||data!=null){
  	thisAdminAddress = data
  }
})

keyExport = {}

function createBackupMultisig(cb){
	//creates and returns a singlesig address that can be exported (for use on cold storage machines)
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
									return cb(data)
								})
							})

					})
			})
		})
}

function createOracle(params){
	tl.createOracle(params.address,params.url, params.numeratorid,params.title, params.duration, params.notional,params.denominatorCollateralId, params.marginReq, thisBackupAddress,function(data){
		return console.log(data)
	}){
		//tl_create_oraclecontract ${ADDR} 1 4 "OIL dUSD" 1000 1 4 0.5 ${ADDR2}
		//multisig address must be funded, generate
}

var txExport =''
var redeemkey = ''
//the array of inputs
if(mode=="backup"){
	createBackupMultisig(function(data){
			var string = JSON.stringify(obj)
					fs.writeFile('coldBackup',string,function(err,data,resheaders){
						if(err){console.log(err)}
						return data
					})
		})
}

if(mode=="new"){
	var params = {}
	createOracle(params)
}

if(mode=="transfer"){

}

if(mode=="restore"){

}

if(mode=="close"){

}

function updateAdminAddress(oldAddress, newAddress,contractTitle,cb){
	tl.changeOracleRef(oldAddress,newAddress,contractTitle,function(data){
		return cb(console.log("tx status: "+data))
	})
}

function sendData(adminAddress,contractTitle, high, low, close, cb){
	tl.publishOracleData(adminAddress,contractTitle,high,low,close,function(data){
		return cb(data)
	}
}