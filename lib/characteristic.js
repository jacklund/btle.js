var UUID = require('./uuid');

// Constructor for a characteristic
function Characteristic(connection, decHandle, properties, valHandle, uuid) {
  this.properties = properties;
  this.declarationHandle = decHandle;
  this.valueHandle = valHandle;
  this.uuid = UUID.createUUID(uuid);
}

module.exports = Characteristic;
