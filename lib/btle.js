// javascript shim that lets our object inherit from EventEmitter
var Connection = require('../build/Debug/btle.node').Connection;
var Attribute = require('./attribute').Attribute;
var events = require('events');
var Service = require('./service');
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

reverseBuffer = function(buffer) {
  var tmp = new Buffer(buffer.length);
  for (var j = 0; j < buffer.length; ++j) {
    tmp[j] = buffer[buffer.length-j-1];
  }
  return tmp;
}

uuidToString = function(buffer) {
  var uuid;
  if (buffer.length == 2) {
    var tmp = new Buffer(4);
    tmp[0] = 0;
    tmp[1] = 0;
    tmp[2] = buffer[1];
    tmp[3] = buffer[0];
    uuid = tmp.toString('hex') + BASE_UUID;
  } else {
    var str = reverseBuffer(buffer).toString('hex');
    uuid = str.substr(0, 8) + '-' + str.substr(8, 4) + '-' +
      str.substr(12, 4) + '-' + str.substr(16, 4) + '-' + str.substr(20);
  }
  return uuid;
}

stringToUuid = function(str) {
  var buffer;
  if (str.length == 36) {
    buffer = new Buffer(str.substr(0,8) + str.substr(9,4) + str.substr(14,4) +
        str.substr(19,4) + str.substr(24), 'hex');
    return reverseBuffer(buffer);
  } else if (str.length <= 6) {
    buffer = new Buffer(2);
    buffer.writeUInt16LE(str, 0, 4, 'hex');
    return buffer;
  }
}

Connection.prototype.discoverServices = function(type, callback) {
  var PRIMARY_SERVICE_UUID = "0x2800";

  if (type == null) {
    this.readByGroupType(0x001, 0xFFFF, PRIMARY_SERVICE_UUID, function(err, list) {
      if (err) {
        callback(err, null);
      } else {
        var ret = new Array(list.length);
        for (var i = 0; i < list.length; ++i) {
          ret[i] = {'handle': list[i].handle,
                    'groupEndHandle': list[i].groupEndHandle,
                    'uuid': uuidToString(list[i].data)};
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
          ret[i] = {'handle': list[i].handle,
                    'groupEndHandle': list[i].groupEndHandle};
        }
        callback(err, ret);
      }
    });
  }
}
