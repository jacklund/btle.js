var Attribute = require('./attribute').Attribute;
var Connection = require('../build/Debug/btle.node').Connection;
var device = require('./device');
var events = require('events');
var gatt = require('./gatt');
var Service = require('./service');
var util = require('util');
var uuid = require('./uuid');

// javascript shim that lets our object inherit from EventEmitter
inherits(Connection, events.EventEmitter);

// Connect function
exports.connect = function(destination, opts, callback) {
  var conn = new Connection();
  conn.connect(destination, opts, callback);
}

// extend prototype
function inherits(target, source) {
  for (var k in source.prototype)
    target.prototype[k] = source.prototype[k];
}

// Bluetooth I/O types
module.exports.BtIOType = {
  BT_IO_L2CAP: 0,
  BT_IO_RFCOMM: 1,
  BT_IO_SCO: 2,
  BT_IO_INVALID: 3,
}

// Bluetooth address types
module.exports.BtAddrType = {
  BDADDR_BREDR      :    0,
  BDADDR_LE_PUBLIC  :    1,
  BDADDR_LE_RANDOM  :    2
}

// Bluetooth security levels
module.exports.BtSecurityLevel = {
  BT_SECURITY_SDP     :    0,
  BT_SECURITY_LOW     :    1,
  BT_SECURITY_MEDIUM :    2,
  BT_SECURITY_HIGH   :    3
}

// L2CAP modes
module.exports.BtMode = {
  L2CAP_MODE_BASIC      : 0,
  L2CAP_MODE_RETRANS    : 1,
  L2CAP_MODE_FLOWCTL    : 2,
  L2CAP_MODE_ERTM        : 3,
  L2CAP_MODE_STREAMING  : 4
}

// Error codes
module.exports.BtError = {
  ATT_ECODE_INVALID_HANDLE        : 0x01,
  ATT_ECODE_READ_NOT_PERM         : 0x02,
  ATT_ECODE_WRITE_NOT_PERM        : 0x03,
  ATT_ECODE_INVALID_PDU           : 0x04,
  ATT_ECODE_AUTHENTICATION        : 0x05,
  ATT_ECODE_REQ_NOT_SUPP          : 0x06,
  ATT_ECODE_INVALID_OFFSET        : 0x07,
  ATT_ECODE_AUTHORIZATION         : 0x08,
  ATT_ECODE_PREP_QUEUE_FULL       : 0x09,
  ATT_ECODE_ATTR_NOT_FOUND        : 0x0A,
  ATT_ECODE_ATTR_NOT_LONG         : 0x0B,
  ATT_ECODE_INSUFF_ENCR_KEY_SIZE  : 0x0C,
  ATT_ECODE_INVAL_ATTR_VALUE_LEN  : 0x0D,
  ATT_ECODE_UNLIKELY              : 0x0E,
  ATT_ECODE_INSUFF_ENC            : 0x0F,
  ATT_ECODE_UNSUPP_GRP_TYPE       : 0x10,
  ATT_ECODE_INSUFF_RESOURCES      : 0x11,
  ATT_ECODE_IO                    : 0x80,
  ATT_ECODE_TIMEOUT               : 0x81,
  ATT_ECODE_ABORTED               : 0x82
}

// Characteristic Property bit field
module.exports.BtCharProperty = {
  ATT_CHAR_PROPER_BROADCAST          : 0x01,
  ATT_CHAR_PROPER_READ               : 0x02,
  ATT_CHAR_PROPER_WRITE_WITHOUT_RESP : 0x04,
  ATT_CHAR_PROPER_WRITE              : 0x08,
  ATT_CHAR_PROPER_NOTIFY             : 0x10,
  ATT_CHAR_PROPER_INDICATE           : 0x20,
  ATT_CHAR_PROPER_AUTH               : 0x40,
  ATT_CHAR_PROPER_EXT_PROPER         : 0x80
}
