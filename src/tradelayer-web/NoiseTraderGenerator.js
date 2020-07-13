const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')

var pass = ''
var user = ''
var noSign = true
var testnet = true
var client = tl.init(user, pass, '', testnet)

//this program simulates a random walk/brownian motion change in price for an oracle

const period = (60*60*4*1000)/100 //number of milliseconds by which the program loops
const publishEveryNPeriods = 10
const txFePerByte = 0.00000001 //LTC per KB in the Tx, typically 2 outputs, one input, about 250 bytes.
const adminAddress = '' //it's assumed you have the hotkey for this address in the local node. 
const contractTitle = "RandomWalk Swap"
const traderQuantity = 100
const avgTraderSize = 1000
const sizePowerLaw = 10
const avgTradeFrequency = 5 //in blocks

const mode = "generate" //or trade, reading from a pre-existing file

var trader = {address:'',avgSize=1,avgFrequency:3,aggressionFactor:2}
var newAddresses = []

function generateAddressSet(index){
	client.getNewAddress(function(address){
		newAddresses.push(address)
		index+=1
		if(index<=traderQuantity){
		return generateAddressSet(index)
		}else{
			return newAddresses
		}
	})
}

function generateTraders(quantity){
	var array= []
	for(var q=0; q<=quantity;q++){
		var address= newAddresses[q]
		var avgSize = avgTraderSize*(sizePowerLaw*Math.random())*Math.random()
		var Frequency=Math.round(Math.random()*avgTradeFrequency*2)
		var aggressionFactor = Math.random()*10
		array.push({address:address,avgSize:avgSize,avgFrequency:avgFrequency,aggressionFactor:aggressionFactor})
	}
	return array
}

var traders = generateTraders(traderQuantity)

var loops = 0

if(adminAddress ==''){
	fs.readFile('adminAddress.json', (err, data) => {
	  if (err) {
	    console.error(err)
	    return data
	  }
	  if(data!=undefined||data!=null){
	  	thisAdminAddress = data
	  }
	})
}

fs.readFile('traders.txt', (err, data) => {
  if (err) {
    console.error(err)
    return data
  }
  if(data!=undefined||data!=null){
  	traders = data
  }
})


function getRandomArbitrary(min, max) {
  return Math.random() * (max - min) + min;
}
function tradingLoop(cb){
	tradingThisLoop= []
	setTimeout(function(){
		loops+=1
		
		for(var t = 0; t<traders.length; t++){
			var trader = traders[t]
			if(loops%trader.avgFrequency){
				tradingThisLoop.push(trader)
			}
		}
		return tradeGatlingGun(tradingThisLoop,0)
	},period)
}

tradingLoop()

function tradeGatlingGun(traders,index){
	var trader = traders[index]
	var flip = Math.random()
	if(flip>0.5){type=="buy"}else{type="sell"}
	price = 2000*Math.random()	
	var params {address:trader.address,contractCode:contractTitle,price:price,tradeType:type}
	tl.sendContractTrade(params,function(data){
		index+=1
		return tradeGatlingGun(traders,index)
	})
		
}