var btle = require('./lib/btle');
var gatt = require('./lib/gatt');
var Peripheral = require('./lib/peripheral');

btle.setDebug(true);

// Create the peripheral
var services = [];
services.push(gatt.createDeviceInfoService('My MFG', '0', '0000-0000', 'v0.01', 'v0.01', 'v0.01', 'My BLE Device'));
var peripheral = Peripheral.create('My BLE Device', services);

// Connection handler
peripheral.on('connect', function(central) {
  console.log("Got connection!!!");
  peripheral.stopAdvertising();
});

peripheral.on('error', function(err) {
  console.log(err);
}

function advertiseAndListen() {
  // Start advertising
  var advOptions = {flags: Peripheral.AdvertisementFlags.LIMITED_DISCOVERABLE | Peripheral.AdvertisementFlags.BR_EDR_NOT_SUPPORTED,
              completeName: 'foo'};
  peripheral.advertise(advOptions, advOptions);

  // Listen for incoming connections
  peripheral.listen();
}

// When the central closes the connection, start advertising again
peripheral.on('close', advertiseAndListen());

advertiseAndListen();
