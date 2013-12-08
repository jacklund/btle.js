var btle = require('../build/Release/btle.node');
var Connection = btle.Connection;
var HCI = btle.HCI;
var events = require('events');

// javascript shim that lets our object inherit from EventEmitter
inherits(Connection, events.EventEmitter);

// extend prototype
function inherits(target, source) {
  for (var k in source.prototype)
    target.prototype[k] = source.prototype[k];
}

// Connect function
module.exports.connect = function(destination, opts, callback) {
  var conn = new Connection();
  conn.connect(destination, opts, callback);
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

module.exports.ADV_TYPE = {
  CONNECTABLE_UNDIRECTED     : 0x00,
  CONNECTABLE_DIRECTED       : 0x01,
  SCANNABLE_UNDIRECTED       : 0x02,
  NON_CONNECTABLE_UNDIRECTED : 0x03
}

module.exports.ADV_FLAGS = {
  LIMITED_DISCOVERABLE : 0x01,
  GENERAL_DISCOVERABLE : 0x02,
  BR_EDR_NOT_SUPPORTED : 0x04,
  LE_BR_EDR_CONTROLLER : 0x08,
  LE_BR_EDR_HOST       : 0x10
}

module.exports.ADV_TYPES = ADV_TYPES = {
  FLAGS                         : 0x01,
  MORE_16BIT_SERVICE_UUIDS      : 0x02,
  COMPLETE_16BIT_SERVICE_UUIDS  : 0x03,
  MORE_32BIT_SERVICE_UUIDS      : 0x04,
  COMPLETE_32BIT_SERVICE_UUIDS  : 0x05,
  MORE_128BIT_SERVICE_UUIDS     : 0x06,
  COMPLETE_128BIT_SERVICE_UUIDS : 0x07,
  SHORTENED_LOCAL_NAME          : 0x08,
  COMPLETE_LOCAL_NAME           : 0x09,
  TX_POWER_LEVEL                : 0x0A,
  MFG_SPECIFIC_DATA             : 0xFF
}

module.exports.COMPANY_IDS = COMPANY_IDS = {
  APPLE : 0x004C
}

var hci = null;

module.exports.startAdvertising = function(options, data, scanData) {
  if (hci == null) {
    hci = new HCI();
  }

  hci.startAdvertising(options, data, scanData);
}

module.exports.stopAdvertising = function() {
  if (hci == null) {
    hci = new HCI();
  }

  hci.stopAdvertising();
}

// Encode the advertising flags
function encodeAdvFlags(flags) {
  var buffer = new Buffer(3);
  buffer[0] = 2; // length
  buffer[1] = ADV_TYPES.FLAGS; // type
  buffer[2] = flags;

  return buffer;
}

// Encode the service UUIDs
function encodeServiceUUIDs(serviceUUIDs) {
  var uuid16 = [];
  var uuid128 = [];
  var buffers = [];
  var length = 0;

  // Separate the UUIDs into 16 and 128 bit values
  for (var i = 0; i < serviceUUIDs.length; ++i) {
    var uuid = new Buffer(serviceUUIDs[i].match(/.{1,2}/g).reverse().join(''), 'hex');
    if (uuid.length == 2) {
      uuid16.push(uuid);
    } else {
      uuid128.push(uuid);
    }
  }
  if (uuid16.length > 0) {
    length += uuid16.length * 2 + 2; // length + type + uuids
    var buffer = new Buffer(2);
    buffer[0] = uuid16.length * 2 + 1; // length
    buffer[1] = ADV_TYPES.MORE_16_BIT_SERVICE_UUIDS; // type
    var len = uuid16.length * 2 + 2;
    uuid16.unshift(buffer);
    buffers.push(Buffer.concat(uuid16, len));
  }
  if (uuid128.length > 0) {
    length += uuid128.length * 16 + 2; // length + type + uuids
    var buffer = new Buffer(2);
    buffer[0] = uuid128.length * 16 + 1; // length
    buffer[1] = ADV_TYPES.MORE_128_BIT_SERVICE_UUIDS; // type
    var len = uuid128.length * 16 + 2;
    uuid128.unshift(buffer);
    buffers.push(Buffer.concat(uuid128, len));
  }

  return {buffers: buffers, length: length};
}

// Encode the {Short,Complete}Name
function encodeName(name, type) {
  var buffer = new Buffer(name.length + 2);
  buffer[0] = name.length + 1;
  buffer[1] = type;
  buffer.write(name, 2);

  return buffer;
}

// Encode the advertising data
function encodeAdvData(values, isScanResponse) {
  var length = 0;
  var buffers = [];

  if (!isScanResponse) {
    if (values.hasOwnProperty('flags')) {
      var buffer = encodeAdvFlags(values.flags);
      length += buffer.length;
      buffers.push(buffer);
    } else {
      // Throw exception
      throw new TypeError('Must specify flags in advertisement data');
    }
  }

  var uuid16 = [];
  var uuid128 = [];
  if (values.hasOwnProperty('service')) {
    if (typeof values.service != 'array') {
      // Throw exception
      throw new TypeError('Service value must be an array');
    }

    var ret = encodeServiceUUIDs(values.service);
    length += ret.length;
    buffers = buffers.concat(ret.buffers);
  }

  if (values.hasOwnProperty('completeName')) {
    if (typeof values.completeName != 'string') {
      // Throw exception
      throw new TypeError('completeName must be a string');
    }
    var buffer = encodeName(values.completeName, ADV_TYPES.COMPLETE_LOCAL_NAME);
    length += buffer.length;
    buffers.push(buffer);
  } else if (values.hasOwnProperty('shortName')) {
    if (typeof values.shortName != 'string') {
      // Throw exception
      throw new TypeError('shortName must be a string');
    }
    var buffer = encodeName(values.shortName, ADV_TYPES.SHORT_LOCAL_NAME);
    length += buffer.length;
    buffers.push(buffer);
  }

  if (values.hasOwnProperty('txPowerLevel')) {
    if (typeof values.txPowerLevel != 'number') {
      // Throw exception
      throw new TypeError('txPowerLevel must be an integer');
    } else if (values.txPowerLevel > 127 || values.txPowerLevel < -127) {
      // Throw exception
      throw new TypeError('txPowerLevel value must be between -127 and 127');
    }
    length += 3; // length + type + value
    var buffer = new Buffer(3);
    buffer[0] = 2; // length
    buffer[1] = ADV_TYPES.TX_POWER_LEVEL;
    buffer[2] = values.txPowerLevel;
  }

  if (values.hasOwnProperty('mfgSpecific')) {
    var buffer;
    if (typeof values.mfgSpecific == 'string') {
      buffer = new Buffer(values.mfgSpecific, 'hex');
    } else if (Buffer.isBuffer(values.mfgSpecific)) {
      buffer = values.mfgSpecific;
    } else if (typeof values.mfgSpecific == 'object') {
      buffer = Buffer.concat([new Buffer(values.mfgSpecific.companyID.match(/.{1,2}/g).reverse().join(''), 'hex'),
          new Buffer(values.mfgSpecific.data, 'hex')]);
    } else {
      // Throw exception
      throw new TypeError('mfgSpecific data must be a string, buffer, or object');
    }
    buffer = Buffer.concat([new Buffer([buffer.length+1, ADV_TYPES.MFG_SPECIFIC_DATA]), buffer], buffer.length+2);
    length += buffer.length; // length + type + buffer (as hex)
    buffers.push(buffer);
  }

  if (length > 31) {
    throw new Error("Advertising data too long");
  }

  return Buffer.concat(buffers, length);
}

module.exports.advertise = function(options, values, scanValues) {
  if (hci == null) {
    hci = new HCI();
  }

  if (options && !options.hasOwnProperty('advType') && !options.hasOwnProperty('minInterval') &&
      !options.hasOwnProperty('maxInterval') && !options.hasOwnProperty('chanMap')) {
    values = options;
    options = null;
  }

  var advBuffer = encodeAdvData(values, false);
  var scanBuffer = null;
  if (scanValues) {
    scanBuffer = encodeAdvData(scanValues, true);
  }

  hci.startAdvertising(options, advBuffer, scanBuffer);
}

// Get a number as a four-digit hex string
function getShortHex(num) {
  num = parseInt(num);
  return ('0' + (num >> 8).toString(16)).substr(-2) + ('0' + (num & 0xFF).toString(16)).substr(-2);
}

// Get a number as a 1-byte signed hex string
function getSignedHex(num) {
  num = parseInt(num);
  return num < 0 ? (0xFF + num + 1).toString(16) : num.toString(16);
}

module.exports.iBeacon = function(companyID, uuid, major, minor, power) {
  var data = uuid + getShortHex(major) + getShortHex(minor) + getSignedHex(power);
  data = '02' + (data.length / 2).toString(16) + data;
  var values = {
    flags: 0x1a,
    mfgSpecific: {
      companyID: getShortHex(companyID),
      data: data
    }
  };
  module.exports.advertise(null, values);
}
