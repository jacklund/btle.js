var btle = require('./btle');
var UUID = require('./uuid');
var characteristic = require('./characteristic');
var gatt = require('./gatt');
var util = require('util');

function Service(peripheral, handle, groupEndHandle, uuid) {
  this.peripheral = peripheral;
  this.handle = handle;
  this.groupEndHandle = groupEndHandle;
  this.uuid = uuid;
}

// Find the characteristics for this service
Service.prototype.findCharacteristics = function(uuidIn, callback) {
  var chars = [];       // List of characteristics
  var attributes = [];  // List of attributes

  getChars(this.handle, this, chars, attributes, uuidIn, callback);
}

// Recursive function to iterate through list and populate
// the characteristic, finally calling the callback when done
function createChars(item, endHandle, uuid, service, chars, attributes, callback) {
  if (item) {
    // Filter by UUID if specified
    if (uuid) {
      var uuidVal = UUID.getUUID(item.data.readUInt16LE(3));
      if (uuidVal != uuid) {
        processAttributes(uuid, service, chars, attributes, callback);
      }
    }
    if (!uuid || uuidVal == uuid) {
      characteristic.createCharacteristic(service, item.handle, endHandle, item.data.readUInt8(0),
          item.data.readUInt16LE(1), item.data.readUInt16LE(3), function(err, ch) {
        if (err) return callback(err, null);
        chars.push(ch);
        processAttributes(uuid, service, chars, attributes, callback);
      });
    }
  } else {
    return callback(null, chars);
  }
}

// Function to compute the end handle for a characteristic,
// and then call createChars() to process it, which then
// calls processAttributes(), recursively until the list is
// empty
function processAttributes(uuid, service, chars, attributes, callback) {
  next = attributes.shift();
  if (attributes.length > 0) h = attributes[0].handle-1;
  else h = service.groupEndHandle;
  createChars(next, h, uuid, service, chars, attributes, callback);
}

// Go to the peripheral and request (possibly repeatedly) the characteristics
// for the service
function getChars(startHandle, service, chars, attributes, uuidIn, callback) {
  service.peripheral.readByType(startHandle, service.groupEndHandle,
      gatt.Attributes.CHARACTERISTIC_UUID.shortString, function(err, list) {

    // Create the input UUID, if there
    var type = null;
    if (uuidIn) type = UUID.getUUID(uuidIn);

    // "Attribute not found" error signals we've finished getting the characteristics
    if (err && err.errorCode == btle.BtError.ATT_ECODE_ATTR_NOT_FOUND) {
      // finished
      processAttributes(type, service, chars, attributes, callback);
    } else if (err) {
      // Abort on any other error
      return callback(err, null);
    } else {
      // Compile the list of all returned attributes
      attributes = attributes.concat(list);
      var last = list[list.length-1].handle;
      if (last < service.groupEndHandle) {
        // Still more to get
        getChars(last+1, service, chars, attributes, uuidIn, callback);
      } else {
        // finished
        processAttributes(type, service, chars, attributes, callback);
      }
    }
  });
}

module.exports = Service;
