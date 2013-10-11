var UUID = require('./uuid');
var btle = require('./btle');

// Constructor for a characteristic
function Characteristic(decHandle, endHandle, properties, valHandle, uuid) {
  this.properties = properties;
  this.declarationHandle = decHandle;
  this.endHandle = endHandle;
  this.valueHandle = valHandle;
  this.uuid = UUID.getUUID(uuid);
}

// Constructor for a characteristic descriptor
function Descriptor(handle, uuid) {
  this.handle = handle;
  this.uuid = uuid;
}

// Create a characteristic
module.exports.createCharacteristic = function(connection, decHandle, endHandle, properties, valHandle, uuid, callback) {
  var ch = new Characteristic(decHandle, endHandle, properties, valHandle, uuid);

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

  // Get the descriptors
  function readDescriptors(ch, callback) {
    if (ch.endHandle > ch.declarationHandle + 1) {
      connection.findInformation(ch.declarationHandle+2, ch.endHandle, function(err, list) {
        if (err) callback(err, ch);
        ch.descriptors = [];
        var item = list.shift();
        while (item) {
          ch.descriptors.push(new Descriptor(item.handle, item.type));
          item = list.shift();
        }
        // If the read property is set, try to read the value, otherwise just return it
        if (properties & btle.BtCharProperty.ATT_CHAR_PROPER_READ) readValue(ch, callback);
        else callback(null, ch);
      });
    } else {
        // If the read property is set, try to read the value, otherwise just return it
        if (properties & btle.BtCharProperty.ATT_CHAR_PROPER_READ) readValue(ch, callback);
        else callback(null, ch);
    }
  }

  readDescriptors(ch, callback);
}
