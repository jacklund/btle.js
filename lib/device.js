var btle = require('./btle');
var gatt = require('./gatt');

function Device(destination) {
  this.destination = destination;
}

module.exports.createDevice = function(connection, destination, callback) {
  var device = new Device(destination);
  console.log(gatt.Services.GAP_SERVICE_UUID);
  connection.findService(gatt.Services.GAP_SERVICE_UUID, function(err, list) {
    if (err) callback(err, null);
    list[0].findCharacteristics(gatt.CharacteristicTypes.DEVICE_NAME, function(err, list) {
      if (err) callback(err, null);
      if (list[0].value) {
        device.name = list[0].value.toString('utf-8');
      } else {
        device.name = null;
      }
      callback(null, device);
    });
  });
}
