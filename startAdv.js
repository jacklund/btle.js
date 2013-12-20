var btle = require('./lib/btle');
var characteristic = require('./lib/characteristic');
var gatt = require('./lib/gatt');
var Peripheral = require('./lib/peripheral');
var service = require('./lib/service');
var util = require('util');

btle.setDebug(true);

// Create the services.
var services = [];

// Device Info service
services.push(gatt.createDeviceInfoService('My MFG', '0', '0000-0000', 'v0.01', 'v0.01', 'v0.01', 'My BLE Device'));

// Custom service
var characteristics = [];
// One read/write characteristic
var c = characteristic.create(null, characteristic.Properties.READ | characteristic.Properties.WRITE, 0xAA01, 0);

// Called whenever the characteristic value is written
c.on('write', function(c) {
  console.log("Characteristic %s written, new value = %s", c.uuid.toString(), util.inspect(c.value));
});
characteristics.push(c);

// Create the custom service
services.push(service.create(null, 0xAA00, null, characteristics, peripheral));
var peripheral = Peripheral.create('My BLE Device', services);

// Connection handler
peripheral.on('connect', function(central) {
  console.log("Got connection!!!");
  peripheral.stopAdvertising();
});

peripheral.on('error', function(err) {
  console.log(err);
  advertiseAndListen();
});

function advertiseAndListen() {
  // Start advertising
  var advOptions = {flags: Peripheral.AdvertisementFlags.LIMITED_DISCOVERABLE | Peripheral.AdvertisementFlags.BR_EDR_NOT_SUPPORTED,
              completeName: 'foo'};
  peripheral.advertise(advOptions, advOptions);

  // Listen for incoming connections
  peripheral.listen();
}

// When the central closes the connection, start advertising again
peripheral.on('close', function() {
  console.log('Got close event, readvertising');
  advertiseAndListen();
});

advertiseAndListen();
