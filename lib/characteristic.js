var UUID = require('./uuid');

// Constructor for a characteristic
function Characteristic(decHandle, properties, valHandle, uuid) {
  this.properties = properties;
  this.declarationHandle = decHandle;
  this.valueHandle = valHandle;
  this.uuid = UUID.createUUID(uuid);
}

// Create a characteristic
module.exports.createCharacteristic = function(connection, decHandle, properties, valHandle, uuid, callback) {
  var ch = new Characteristic(decHandle, properties, valHandle, uuid);
  // Populate the value field
  function readValue(ch, callback) {
    connection.readHandle(ch.valueHandle, function(err, value) {
      if (err) {
        callback(err, ch);
      } else {
        ch.value = value;
        callback(err, ch);
      }
    });
  }
  readValue(ch, callback);
}
