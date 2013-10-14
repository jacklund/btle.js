var UUID = require('./uuid');
var btle = require('./btle');
var util = require('util');

// Constructor for a characteristic
function Characteristic(service, decHandle, endHandle, properties, valHandle, uuid) {
  this.service = service;
  this.properties = properties;
  this.declarationHandle = decHandle;
  this.endHandle = endHandle;
  this.valueHandle = valHandle;
  this.uuid = UUID.getUUID(uuid);
}

// Create a characteristic
module.exports.createCharacteristic = function(service, decHandle, endHandle, properties, valHandle, uuid, callback) {
  var ch = new Characteristic(service, decHandle, endHandle, properties, valHandle, uuid);

  // If the read property is set, try to read the value
  if (properties & btle.BtCharProperty.ATT_CHAR_PROPER_READ) {
    ch.readValue(function(err, ch) {
      if (err) return callback(err, null);
      ch.readDescriptors(callback);
    });
  } else {
    ch.readDescriptors(callback);
  }
}

// Populate the value field
Characteristic.prototype.readValue = function(callback) {
  var self = this;
  // Only attempt to read if the read property is set
  if (this.properties & btle.BtCharProperty.ATT_CHAR_PROPER_READ) {
    this.service.device.readHandle(this.valueHandle, function(err, value) {
      try {
        if (!err) self.value = value;
        return callback(err, self);
      } catch (e) {
        return callback(e, null);
      }
    });
  } else {
    return callback(null, this);
  }
}

Characteristic.prototype.writeValue = function(buffer) {
  this.service.device.writeCommand(this.valueHandle, buffer);
}

// Get the descriptors
Characteristic.prototype.readDescriptors = function(callback) {
  var self = this;
  if (this.endHandle > this.valueHandle) {
    this.service.device.findInformation(this.valueHandle+1, this.endHandle, function(err, list) {
      try {
        if (err) return callback(err, null);
        self.descriptors = [];
        var item = list.shift();
        while (item) {
          self.descriptors.push(new Descriptor(self.service, item.handle, item.type));
          item = list.shift();
        }
        return callback(err, self);
      } catch (e) {
        return callback(e, null);
      }
    });
  } else {
    return callback(null, self);
  }
}

Characteristic.prototype.listenForNotifications = function(callback) {
  var self = this;
  this.service.device.addNotificationListener(this.valueHandle, function(err, value) {
    if (!err) self.value = value;
    return callback(err, self);
  });
}

// Constructor for a characteristic descriptor
function Descriptor(service, handle, uuid) {
  this.service = service;
  this.handle = handle;
  this.uuid = uuid;
  var self = this;
}

Descriptor.prototype.readValue = function(callback) {
  var self = this;
  this.service.device.readHandle(this.handle, function(err, value) {
    if (!err) self.value = value;
    callback(err, self);
  });
}

Descriptor.prototype.writeValue = function(buffer) {
  this.service.device.writeCommand(this.handle, buffer);
}
