const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)

var myAddresses = {}
var thisAdminAddress = ''
var thisAuxAddress = '' //the idea of an Aux. address is it's single sig, we get it when we generate one of the pubkeys for the multisig
						//and you can use it to top up the multisig address if it falls below some balance
//called after hist. data load, loads trades
fs.readFile('adminAddress.json', (err, data) => {
  if (err) {
    console.error(err)
    return
  }
  myAddresses = data
})

function createMultisig(coldPubKeyArray,cb){
	//creates and returns a multisig address and the single signature address of the hot key used in the mix
	if(myAddresses.length<1){
		client.getnewaddress(null, function(data){
			client.getAddressInfo(data,function(data){
				myAddresses.push({address:data.address,pubkey:data.scriptPubKey,type:'aux'})
				coldPubKeyArray.push(data.scriptPubKey)
				var signers= coldPubKeyArray.length -1
				client.addMultsigAddress(signers,coldPubKeyArray,function(data){
					myAddresses.push({address:data.address,redeemkey:data.redeemScript,type:'admin'})
					thisAdminAddress= {address:data.address,redeemkey:data.redeemScript,type:'admin'}
					return cb(data)
				})
			})
		})
	}else{
		for(var i = 0; i<myAddresses.length; i++){
			if(myAddresses[i].type =='admin'){
				return cb(data)
			}
		}
	}
}
//example parameters for creating a token
/*
 "\nArguments:\n"
            "1. fromaddress          (string, required) the address to send from\n"
            "2. type                 (number, required) the type of the tokens to create: (1 for indivisible tokens, 2 for divisible tokens)\n"
            "3. previousid           (number, required) an identifier of a predecessor token (use 0 for new tokens)\n"
            "4. name                 (string, required) the name of the new tokens to create\n"
            "5. url                  (string, required) an URL for further information about the new tokens (can be \"\")\n"
            "6. data                 (string, required) a description for the new tokens (can be \"\")\n"
            "7. kyc options          (array, optional) A json with the kyc allowed.\n"
            "    [\n"
            "      2,3,5         (number) kyc id\n"
            "      ,...\n"
            "    ]\n"

            "\nResult:\n"
            "\"hash\"                  (string) the hex-encoded transaction hash\n"
 */
var parameters = {type:2,
			  previousid: 0,
			  name: "USDx",
			  url: "usdxissuer.org",
			  data: "Bank-backed Dollars.",
			  kyc: [2,3,5]
             }

function createPropertyWithMultisig(address,parameters,cb){
	client.createpayload_issuanceManaged(params,function(err, op_payload){
		rest.get('https://blockchain.info/rawaddr/'+address).on('complete',function(data){
			var amount = data.final_balance //note this is an integer in Satoshis
			var input = data.tx[0] //this only works if the multisig has just 1 input, will make this more resilient
			client.buildRaw(op_payload,input,[0],'',amount, function(tx_payload){//inputs are missing from param 2
				return cb(tx_payload)
			})	
		})
	})
	//multisig address must be funded, generate
}

var txExport =''
var redeemkey = ''
//the array of inputs
createMultisig(["",""],function(data){
	redeemkey = data.redeemkey
	createPropertyWithMultisig(data.address,params,function(tx){
		txExport=tx
		
		if(noSign==false){
			rest.get('https://blockchain.info/rawtx/+$tx_hash').on('complete',function(data){
				var privkey =''
				var input = ''//this is the tx id of the BTC or LTC funding transaction sent to the multisig
				  //if you fund the aux. address it will be able to automatically send and then cite that input
				var vout= 0 //it might not be 0, but isn't it nice when the vout *is* 0?
				var pubscript = ''//this can be found in the tx info of the funding tx that preceeds this one
				
				tl.signRaw(tx, privkey, redeemkey, input, vout, pubscript,function(data){
					return console.log(data)
				})	
			})
		}else{
			return tx
		}
	})
})

  fs.writeFile('txOut.txt', txExport, function(err,data){
    console.log('dahhh!')
  })

  var obj = JSON.stringify(myAddresses)
  fs.writeFile('adminAddress.json',obj,function(err,data){
  	console.log("updated adminAddress.json")
  })

function updateAdminAddress(oldAddress, newAddress,propertyid,cb){
	client.createpayload_changeIssuer(propertyid,function(op_payload){\
		rest.get('https://blockchain.info/rawaddr/'+oldAddress).on('complete',function(data){
			var input = data.tx[0]//we are assuming this first tx in the array is the most recent, otherwise will need a fix
			client.buildRaw(op_payload,input,[0],newAddress,amount, function(tx_payload){//inputs are missing from param 2
				return cb(tx_payload)
			})	
		})
	})
}

function sendGrant(adminAddress,targetAddress,propertyid, amount, cb){
	client.createpayload_sendGrant(propertyid,amount,function(op_payload){
		rest.get('https://blockchain.info/rawaddr/'+adminAddress).on('complete',function(data){
			var input = data.tx[0]//we are assuming this first tx in the array is the most recent, otherwise will need a fix
			client.buildRaw(op_payload,input,[0],targetAddress,'', function(tx_payload){//inputs are missing from param 2
				return cb(tx_payload)
			})	
		})
	})
}

function revoke(adminAddress,propertyid, amount, cb){
	client.createpayload_sendGrant(propertyid,amount,function(op_payload){
		rest.get('https://blockchain.info/rawaddr/'+adminAddress).on('complete',function(data){
			var input = data.tx[0]//we are assuming this first tx in the array is the most recent, otherwise will need a fix
			client.buildRaw(op_payload,input,[0],'','', function(tx_payload){//inputs are missing from param 2
				return cb(tx_payload)
			})	
		})
	})
}

/*
updateAdminAddress('','',123,function(tx){\
  fs.writeFile('txOut.txt', txExport, function(err,data){
  		  console.log('dahhh!')
  })
})
*/