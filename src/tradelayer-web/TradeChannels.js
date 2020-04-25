const tl = require('./TradeLayerRPCAPI.js')
const fs = require('fs')
const rest = require('restler')

var pass = ''
var user = ''
var noSign = true
var client = tl.init(user, pass, '', true)

//early draft on the OO design of this module

var myChannels = []
var Channel = {'multisigAddr':'','counterparty':{},'positions':[],collateralid:1,'myMargin':0,'myPNL':0,'counterpartyMargin':0}
var counterpartyProfiles = []
var Profile = {'alias':'','regulatoryStatus':'unregulated','myHistoricalVolume':0,'avgSignBackTime':300,'cancelRate':0.02,'RepRating':70}
