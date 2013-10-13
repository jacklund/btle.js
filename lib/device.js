var btle = require('./btle');
var events = require('events');
var gatt = require('./gatt');
var Service = require('./service');
var util = require('util');
var uuid = require('./uuid');

function Device(destination) {
  this.destination = destination;
  events.EventEmitter.call(this);
}

util.inherits(Device, events.EventEmitter);

// Connect to a device
module.exports.connect = function(destination, connectCb) {
  btle.connect(destination, function(err, connection) {
    if (err) return connectCb(err, null);

    var device = new Device(destination);

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

    // Find a service on the device
    device.findService = function(type, callback) {
      if (type == null) {
        // Find all services
        connection.readByGroupType(0x001, 0xFFFF, gatt.Attributes.PRIMARY_SERVICE_UUID.shortString, function(err, list) {
          if (err) {
            callback(err, null);
          } else {
            var ret = new Array(list.length);
            for (var i = 0; i < list.length; ++i) {
              ret[i] = new Service(connection, list[i].handle, list[i].groupEndHandle, uuid.getUUID(list[i].data));
            }
            callback(err, ret);
          }
        });
      } else {
        var u = uuid.getUUID(type);
        connection.findByTypeValue(0x001, 0xFFFF, gatt.Attributes.PRIMARY_SERVICE_UUID.shortString,
            uuid.getUUID(type).buffer, function(err, list) {
          if (err) {
            callback(err, null);
          } else {
            var ret = new Array(list.length);
            for (var i = 0; i < list.length; ++i) {
              ret[i] = new Service(connection, list[i].handle, list[i].groupEndHandle, uuid.getUUID(type));
            }
            callback(err, ret);
          }
        });
      }
    }

    // Attach the connection methods to the device object
    device.close = function() {
      connection.close();
    }
    device.findInformation = function(start, end, callback) {
      connection.findInformation(start, end, callback);
    }
    device.findByTypeValue = function(startHandle, endHandle, uuidVal, data, callback) {
      var uuid = uuid.getUUID(uuidVal);
      if (uuid == null) {
        callback("Invalid UUID");
      } else {
        connection.findByTypeValue(startHandle, endHandle, uuid.longString, data, callback);
      }
    }
    device.readByType = function(startHandle, endHandle, uuidVal, callback) {
      var uuid = uuid.getUUID(uuidVal);
      if (uuid == null) {
        callback("Invalid UUID");
      } else {
        connection.readByType(startHandle, endHandle, uuid.longString, callback);
      }
    }
    device.readByGroupType = function(startHandle, endHandle, uuidVal, callback) {
      var uuid = uuid.getUUID(uuidVal);
      if (uuid == null) {
        callback("Invalid UUID");
      } else {
        connection.readByGroupType(startHandle, endHandle, uuid.longString, callback);
      }
    }
    device.readHandle = function(handle, callback) {
      connection.readHandle(handle, callback);
    }
    device.addNotificationListener = function(handle, callback) {
      connection.addNotificationListener(handle, callback);
    }
    device.writeCommand = function(handle, data, callback) {
      if (callback) {
        connection.writeCommand(handle, data, callback);
      } else {
        connection.writeCommand(handle, data);
      }
    }
    connectCb(null, device);
  });
}
