var btle = require('./btle');
var Peripheral = btle.Peripheral;
var events = require('events');
var gatt = require('./gatt');
var Service = require('./service');
var util = require('util');
var UUID = require('./uuid');

util.inherits(Peripheral, events.EventEmitter);

// Find a service on the device
Peripheral.prototype.findService = function(type, callback) {
  var self = this;
  if (type == null) {
    // Find all services
    this.connection.readByGroupType(0x001, 0xFFFF, gatt.Attributes.PRIMARY_SERVICE_UUID.shortString, function(err, list) {
      if (err) {
        callback(err, null);
      } else {
        var ret = new Array(list.length);
        for (var i = 0; i < list.length; ++i) {
          ret[i] = new Service(self, list[i].handle, list[i].groupEndHandle, UUID.getUUID(list[i].data));
        }
        callback(err, ret);
      }
    });
  } else {
    var u = UUID.getUUID(type);
    this.connection.findByTypeValue(0x001, 0xFFFF, gatt.Attributes.PRIMARY_SERVICE_UUID.shortString,
        UUID.getUUID(type).buffer, function(err, list) {
      if (err) {
        callback(err, null);
      } else {
        var ret = new Array(list.length);
        for (var i = 0; i < list.length; ++i) {
          ret[i] = new Service(self, list[i].handle, list[i].groupEndHandle, UUID.getUUID(type));
        }
        callback(err, ret);
      }
    });
  }
}

// Attach the connection methods to the device object
Peripheral.prototype.close = function() {
  this.connection.close();
}
Peripheral.prototype.findInformation = function(start, end, callback) {
  this.connection.findInformation(start, end, callback);
}
Peripheral.prototype.findByTypeValue = function(startHandle, endHandle, uuidVal, data, callback) {
  var uuid = UUID.getUUID(uuidVal);
  if (uuid == null) {
    callback("Invalid UUID");
  } else {
    this.connection.findByTypeValue(startHandle, endHandle, uuid.longString, data, callback);
  }
}
Peripheral.prototype.readByType = function(startHandle, endHandle, uuidVal, callback) {
  var uuid = UUID.getUUID(uuidVal);
  if (uuid == null) {
    callback("Invalid UUID");
  } else {
    this.connection.readByType(startHandle, endHandle, uuid.longString, callback);
  }
}
Peripheral.prototype.readByGroupType = function(startHandle, endHandle, uuidVal, callback) {
  var uuid = UUID.getUUID(uuidVal);
  if (uuid == null) {
    callback("Invalid UUID");
  } else {
    this.connection.readByGroupType(startHandle, endHandle, uuid.longString, callback);
  }
}
Peripheral.prototype.readHandle = function(handle, callback) {
  this.connection.readHandle(handle, callback);
}
Peripheral.prototype.addNotificationListener = function(handle, callback) {
  this.connection.addNotificationListener(handle, callback);
}
Peripheral.prototype.writeCommand = function(handle, data, callback) {
  if (callback) {
    this.connection.writeCommand(handle, data, callback);
  } else {
    this.connection.writeCommand(handle, data);
  }
}
