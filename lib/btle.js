var btle = require('../build/Debug/btle.node');
var Peripheral = btle.Peripheral;
var Central = btle.Central;
var HCI = btle.HCI;
var events = require('events');

// javascript shim that lets our object inherit from EventEmitter
inherits(Peripheral, events.EventEmitter);
inherits(Central, events.EventEmitter);

// extend prototype
function inherits(target, source) {
  for (var k in source.prototype)
    target.prototype[k] = source.prototype[k];
}

// Connect function
module.exports.connect = function(destination, opts, callback) {
  var peripheral = new Peripheral();
  peripheral.connect(destination, opts, callback);
}

// Listen function
module.exports.listen = function(opts, callback) {
  var central = new Central();
  central.listen(opts, callback);
}

// Bluetooth I/O types
module.exports.IOTypes = {
  L2CAP  : 0,
  RFCOMM : 1,
  SCO    : 2,
}

// Bluetooth address types
module.exports.AddrTypes = {
  BREDR      :    0,
  LE_PUBLIC  :    1,
  LE_RANDOM  :    2
}

// Bluetooth security levels
module.exports.SecurityLevel = {
  SDP     :    0,
  LOW     :    1,
  MEDIUM  :    2,
  HIGH    :    3
}

// L2CAP modes
module.exports.L2CAP_Mode = {
  BASIC      : 0,
  RETRANS    : 1,
  FLOWCTL    : 2,
  ERTM       : 3,
  STREAMING  : 4
}

// Error codes
module.exports.ATTErrorCodes = {
  INVALID_HANDLE        : 0x01,
  READ_NOT_PERM         : 0x02,
  WRITE_NOT_PERM        : 0x03,
  INVALID_PDU           : 0x04,
  AUTHENTICATION        : 0x05,
  REQ_NOT_SUPP          : 0x06,
  INVALID_OFFSET        : 0x07,
  AUTHORIZATION         : 0x08,
  PREP_QUEUE_FULL       : 0x09,
  ATTR_NOT_FOUND        : 0x0A,
  ATTR_NOT_LONG         : 0x0B,
  INSUFF_ENCR_KEY_SIZE  : 0x0C,
  INVAL_ATTR_VALUE_LEN  : 0x0D,
  UNLIKELY              : 0x0E,
  INSUFF_ENC            : 0x0F,
  UNSUPP_GRP_TYPE       : 0x10,
  INSUFF_RESOURCES      : 0x11,
  IO                    : 0x80,
  TIMEOUT               : 0x81,
  ABORTED               : 0x82
}

// Characteristic Property bit field
module.exports.CharProperties = {
  BROADCAST          : 0x01,
  READ               : 0x02,
  WRITE_WITHOUT_RESP : 0x04,
  WRITE              : 0x08,
  NOTIFY             : 0x10,
  INDICATE           : 0x20,
  AUTH               : 0x40,
  EXT_PROPER         : 0x80
}
