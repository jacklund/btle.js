// javascript shim that lets our object inherit from EventEmitter
var Connection = require('../build/Debug/btle.node').Connection;
var Attribute = require('./attribute').Attribute;
var events = require('events');
var Service = require('./service');
var UUID = require('./uuid');
var util = require('util');

inherits(Connection, events.EventEmitter);
exports.Connection = Connection;
exports.connect = function(destination, opts, callback) {
  var conn = new Connection();
  conn.connect(destination, opts, callback);
  return conn;
}

// extend prototype
function inherits(target, source) {
  for (var k in source.prototype)
    target.prototype[k] = source.prototype[k];
}

module.exports.BtIOType = {
  BT_IO_L2CAP: 0,
  BT_IO_RFCOMM: 1,
  BT_IO_SCO: 2,
  BT_IO_INVALID: 3,
}

module.exports.BtAddrType = {
  BDADDR_BREDR      :    0,
  BDADDR_LE_PUBLIC  :    1,
  BDADDR_LE_RANDOM  :    2
}

module.exports.BtSecurityLevel = {
  BT_SECURITY_SDP     :    0,
  BT_SECURITY_LOW     :    1,
  BT_SECURITY_MEDIUM :    2,
  BT_SECURITY_HIGH   :    3
}

module.exports.BtMode = {
  L2CAP_MODE_BASIC      : 0,
  L2CAP_MODE_RETRANS    : 1,
  L2CAP_MODE_FLOWCTL    : 2,
  L2CAP_MODE_ERTM        : 3,
  L2CAP_MODE_STREAMING  : 4
}

BASE_UUID = '-0000-1000-8000-00805f9b34fb'

// Find a service by UUID. If type is null, return all services
Connection.prototype.findService = function(type, callback) {
  var PRIMARY_SERVICE_UUID = "0x2800";
  var self = this;

  if (type == null) {
    this.readByGroupType(0x001, 0xFFFF, PRIMARY_SERVICE_UUID, function(err, list) {
      if (err) {
        callback(err, null);
      } else {
        var ret = new Array(list.length);
        for (var i = 0; i < list.length; ++i) {
          ret[i] = new Service(self, list[i].handle, list[i].groupEndHandle, UUID.createUUID(list[i].data));
        }
        callback(err, ret);
      }
    });
  } else {
    this.findByTypeValue(0x001, 0xFFFF, PRIMARY_SERVICE_UUID, stringToUuid(type), function(err, list) {
      if (err) {
        callback(err, null);
      } else {
        var ret = new Array(list.length);
        for (var i = 0; i < list.length; ++i) {
          ret[i] = new Service(self, list[i].handle, list[i].groupEndHandle, UUID.createUUID(type));
        }
        callback(err, ret);
      }
    });
  }
}
