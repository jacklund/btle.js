var attribute = require('./lib/attribute');
var btle = require('./lib/btle');
var characteristic = require('./lib/characteristic');
var gap = require('./lib/gap');
var gatt = require('./lib/gatt');
var Peripheral = require('./lib/peripheral');
var service = require('./lib/service');
var UUID = require('./lib/uuid');

btle.setDebug(true);

var data = {flags: Peripheral.AdvertisementFlags.LIMITED_DISCOVERABLE | Peripheral.AdvertisementFlags.BR_EDR_NOT_SUPPORTED,
            completeName: 'foo'};
var services = [];

// GAP service
services.push(gap.createGAPService('My BLE Device'));

// GATT service
services.push(gatt.createGATTService());

var peripheral = Peripheral.create(services);
peripheral.advertise(data, data);
peripheral.listen({source: 'hci0'}, function(err, central) {
  if (err) {
    return console.log(err);
  }

  console.log("Got connection!!!");
  /*
  central.on('data', function(data) {
    console.log(data);
  });
  */
});
