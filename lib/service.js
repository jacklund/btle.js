var attribute = require('./attribute');
var btle = require('./btle');
var UUID = require('./uuid');
var characteristic = require('./characteristic');
var gatt = require('./gatt');
var util = require('util');

function Service(handle, serviceUUID, includes, characteristics, peripheral) {
  this.uuid = UUID.getUUID(serviceUUID);
  this.declaration = attribute.create(handle, gatt.AttributeTypes.PRIMARY_SERVICE, this.uuid);
  this.includes = includes || [];
  this.characteristics = characteristics || [];
  var myHandle = handle+1;
  for (var i = 0; i < this.includes.length; ++i) {
    this.includes.handle = myHandle++;
  }
  for (var i = 0; i < this.characteristics.length; ++i) {
    this.characteristics[i].setHandle(myHandle);
    myHandle = this.characteristics[i].endHandle+1;
  }
  this.endHandle = this.declaration.endHandle = myHandle;
  this.peripheral = peripheral;
}

Service.prototype.toString = function() {
  return util.format('Service %s', this.uuid.toString());
}

Service.prototype.setHandle = function(handle) {
  this.declaration.handle = handle++;
  for (var i = 0; i < this.includes.length; ++i) {
    this.includes[i].setHandle(handle);
    handle = this.includes[i].endHandle + 1;
  }
  for (var i = 0; i < this.characteristics.length; ++i) {
    this.characteristics[i].setHandle(handle);
    handle = this.characteristics[i].endHandle + 1;
  }
  this.endHandle = this.declaration.endHandle = handle - 1;
}

Service.prototype.getAttributes = function() {
  var ret = [];
  ret.push(this.declaration);
  for (var i = 0; i < this.includes.length; ++i) {
    ret.push(includes[i].declaration);
  }
  for (var i = 0; i < this.characteristics.length; ++i) {
    ret = ret.concat(this.characteristics[i].getAttributes());
  }

  return ret;
}

module.exports.create = function(handle, serviceUUID, includes, characteristics, peripheral) {
  return new Service(handle, serviceUUID, includes, characteristics, peripheral);
}

// Find the characteristics for this service
Service.prototype.findCharacteristics = function(uuidIn, callback) {
  var attributes = [];  // List of attributes

  getChars(this.handle, this, this.characteristics, attributes, uuidIn, callback);
}

// Recursive function to iterate through list and populate
// the characteristic, finally calling the callback when done
function createChars(item, endHandle, uuid, service, chars, attributes, callback) {
  if (item) {
    var uuidVal = UUID.getUUID(item.data.readUInt16LE(3));
    // Filter by UUID if specified
    if (uuid) {
      if (uuidVal != uuid) {
        processAttributes(uuid, service, chars, attributes, callback);
      }
    }
    if (!uuid || uuidVal == uuid) {
      chars.push(characteristic.create(item.handle, characteristic.Properties.READ, uuidVal));
      processAttributes(uuid, service, chars, attributes, callback);
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
