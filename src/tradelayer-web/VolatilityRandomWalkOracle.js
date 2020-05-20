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
const estimatedPeriodsUntilSettlement = 100
const adminAddress = '' //it's assumed you have the hotkey for this address in the local node. 
const contractTitle = "RandomWalk Swap"
const startingPrice = 1000
const range = 0.01
const rangeModifier = 1
const volSpikeFrequency = 3
const modHalfLife = 4

var price = startingPrice

var loops = 0
var recentSamples = []

function getRandomArbitrary(min, max) {
  return Math.random() * (max - min) + min;
}
function publishingLoop(cb){
	setTimeout(function(){
		loops+=1
		console.log('Loops: '+loops + ' recent print: '+price)
		price *= Math.round(1+getRandomArbitrary(range*-1*rangeModifier,range*rangeModifier)*100)/100
		if(loops%volSpikeFrequency){
			rangeModifier += Math.random()
		}
		rangeModifier *=1-(1/modHalfLife)

			if(loops%publishEveryNPeriods){
						client.publishOracleData(adminAddress,contractTitle,price,price,price,function(data){
							return cb(data)
						})
			}
	},period)
}

publishingLoop()