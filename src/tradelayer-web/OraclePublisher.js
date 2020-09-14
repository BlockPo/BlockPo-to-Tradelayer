const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')

var pass = ''
var user = ''
var noSign = true
var testnet = true
var client = tl.init(user, pass, '', testnet)

//this program simply loops, pings a source of data, and relays that data into a hot-keyed RPC to send the data out to Bitcoin.
//it reads the same local text file to reference an admin address as the Oracle Manager script.
//In the future we'll support automatically selling token income for BTC/LTC to keep the address well maintained, 
//a regular sweep address to send some % of revenue, and alerts if the address is low on miner fee and not seeing adequete revenue.
//It is possible for these tx to be priced cheaply in fees, but if there is no sensitivity to general miner cost then it's possible settlements
//could end up with insufficient confirmed prints to average the correct TWAP. Therefore a professional oracle must operate a 2-3 block conf.
//target tx firing, so that prints are timely. To compensate for this, it's possible to run infrequently and pay for less total data.
//Remember: traders can still see your index on your website updating, and only the TWAP of the last several blocks every settlement
//period are what settles the contract. Dynamic fee estimates are another future feature, for now LTC fees are low, here are constants:

const period = (60*60*4*1000)/100 //number of milliseconds by which the program loops
const publishEveryNPeriods = 10
const txFePerByte = 0.00000001 //LTC per KB in the Tx, typically 2 outputs, one input, about 250 bytes.
const estimatedPeriodsUntilSettlement = 100
const adminAddress = '' //it's assumed you have the hotkey for this address in the local node. 
const dataURI = 'https://api.coinbase.com/v2/exchange-rates'
const contractTitle = "Coinbase BTC/USD Swap"
//If you lose the key, or it is compromised, restore to the cold back-up address.

//We'll add an RPC to get the next block where a protocol settlement is set to occur, 
//and use that to calibrate a rapid burst of publishing before that time

var loops = 0
var recentSamples = []

function publishingLoop(cb){
	setTimeout(function(){
		loops+=1
		console.log('Loops: '+loops + ' recent API Prices'+recentSamples)
		rest.get(dataURI).on('complete',function(data){
			var price = 1/data.data.rates.BTC
			recentSamples.push(price)
			if(loops%publishEveryNPeriods){
				var low = Math.Min(recentSamples)
				var high = Math.Max(recentSamples)
				var close = price
						client.publishOracleData(adminAddress,contractTitle,high,low,close,function(data){
							recentSamples = []
							return cb(data)
						}	
			}
		})
	},period)
}

publishingLoop()