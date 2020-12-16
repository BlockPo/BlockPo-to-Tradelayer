const URL = 'http://localhost'
const PORT = 9876
const client = require('socket.io-client');

const io = client.connect(`${URL}:${PORT}`);

io.on('channelPubKey', (data) => {
    console.log(`channelPubKey: ${data}`)
});

io.on('dataSub', (data) => {
    console.log(`dataSub: ${data}`)
});
