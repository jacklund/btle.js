var btle = require('./btle');
var events = require('events');
var gatt = require('./gatt');
var Service = require('./service');
var util = require('util');
var UUID = require('./uuid');

function Device(connection, destination) {
  this.connection = connection;
  this.destination = destination;
  events.EventEmitter.call(this);
}

util.inherits(Device, events.EventEmitter);

// Connect to a device
module.exports.connect = function(destination, connectCb) {
  btle.connect(destination, function(err, connection) {
    if (err) return connectCb(err, null);

    var device = new Device(connection, destination);

    // Re-emit the connection events
    connection.on('error', function(err, c) {
      device.emit('error', err, device);
    });
    connection.on('connect', function(c) {
      device.emit('connect', device);
    });
    connection.on('close', function() {
      device.emit('close', device);
    });

    connectCb(null, device);
  });
}

// Find a service on the device
Device.prototype.findService = function(type, callback) {
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
Device.prototype.close = function() {
  this.connection.close();
}
Device.prototype.findInformation = function(start, end, callback) {
  this.connection.findInformation(start, end, callback);
}
Device.prototype.findByTypeValue = function(startHandle, endHandle, uuidVal, data, callback) {
  var uuid = UUID.getUUID(uuidVal);
  if (uuid == null) {
    callback("Invalid UUID");
  } else {
    this.connection.findByTypeValue(startHandle, endHandle, uuid.longString, data, callback);
  }
}
Device.prototype.readByType = function(startHandle, endHandle, uuidVal, callback) {
  var uuid = UUID.getUUID(uuidVal);
  if (uuid == null) {
    callback("Invalid UUID");
  } else {
    this.connection.readByType(startHandle, endHandle, uuid.longString, callback);
  }
}
Device.prototype.readByGroupType = function(startHandle, endHandle, uuidVal, callback) {
  var uuid = UUID.getUUID(uuidVal);
  if (uuid == null) {
    callback("Invalid UUID");
  } else {
    this.connection.readByGroupType(startHandle, endHandle, uuid.longString, callback);
  }
}
Device.prototype.readHandle = function(handle, callback) {
  this.connection.readHandle(handle, callback);
}
Device.prototype.addNotificationListener = function(handle, callback) {
  this.connection.addNotificationListener(handle, callback);
}
Device.prototype.writeCommand = function(handle, data, callback) {
  if (callback) {
    this.connection.writeCommand(handle, data, callback);
  } else {
    this.connection.writeCommand(handle, data);
  }
}
