var Attribute = require('./attribute').Attribute;
var btle = require('../build/Release/btle.node');
var Connection = btle.Connection;
var HCI = btle.HCI;
var device = require('./device');
var events = require('events');
var gatt = require('./gatt');
var Service = require('./service');
var util = require('util');
var uuid = require('./uuid');

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

var hci = null;

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

module.exports.startAdvertising = function(data, scanData, options) {
  if (hci == null) {
    hci = new HCI();
  }

  hci.startAdvertising(data, scanData, options);
}

module.exports.stopAdvertising = function() {
  if (hci == null) {
    hci = new HCI();
  }

  hci.stopAdvertising();
}

module.exports.advertise = function(advType, values, scanAlso) {
  if (hci == null) {
    hci = new HCI();
  }

  if (enable) {
    var advLength = 3;
    var scanLength = 0;
    var flags = 0x04; // BR/EDR Not Supported
    if (limited) {
      flags |= 0x01; // Limited Discoverable Mode
    } else {
      flags |= 0x02; // General Discoverable Mode
    }
    if (power) {
      power = parseFloat(power);
      if (isNan(power)) {
        // TODO: Throw exception
      } else if (power > 127 || power < -127) {
        // TODO: Throw exception
      }
      advLength += 3;
      scanLength += 3;
    }
    if (name) {
      advLength += name.length + 2;
      scanLength += name.length + 2;
    }

    var serviceUUIDs16Bit = [];
    var serviceUUIDs128Bit = [];
    if (serviceUUIDs) {
      // Separate the UUIDs into 16 and 128 bit values
      for (var i = 0; i < serviceUUIDs.length; ++i) {
        var uuid = new Buffer(serviceUUIDs[i].match(/.{1,2}/g).reverse().join(''), 'hex');
        if (uuid.length == 2) {
          scanLength += 2;
          advLength += 2;
          serviceUUIDs16Bit.push(uuid);
        } else {
          scanLength += 16;
          advLength += 16;
          serviceUUIDs128Bit.push(uuid);
        }
      }
      if (advLength > 31 || scanLength > 31) {
        throw new Error("Advertising data too long");
      }
    }
    var advData = new Buffer(advLength);
    var scanData = new Buffer(scanLength);

    // Flags
    advData.writeUInt8(2, 0);
    advData.writeUInt8(0x01, 1);
    advData.writeUInt8(flags, 2);

    var scanOffset = 0;
    var advOffset = 3;
    if (name) {
      advData.writeUInt8(name.length+1, advOffset++);
      advData.writeUInt8(0x08, advOffset++);
      advData.write(name, advOffset, name.length);
      advOffset += name.length;

      scanData.writeUInt8(name.length+1, scanOffset++);
      scanData.writeUInt8(0x08, scanOffset++);
      scanData.write(name, scanOffset, name.length);
      scanOffset += name.length;
    }

    if (serviceUUIDs) {
      if (serviceUUIDs16Bit.length > 0) {
        advData.writeUInt8(1 + 2 * serviceUUIDs16Bit.length, advOffset++);
        scanData.writeUInt8(1 + 2 * serviceUUIDs16Bit.length, scanOffset++);
        advData.writeUInt8(0x02, advOffset++);
        scanData.writeUInt8(0x02, scanOffset++);
        for (var i = 0; i < serviceUUIDs16Bit.length; ++i) {
          advData.writeUInt16(serviceUUIDs16Bit[i], advOffset);
          advOffset += 2;
          scanData.writeUInt16(serviceUUIDs16Bit[i], scanOffset);
          scanOffset += 2;
        }
      }
      if (serviceUUIDs128Bit.length > 0) {
        advData.writeUInt8(1 + 16 * serviceUUIDs16Bit.length, advOffset++);
        scanData.writeUInt8(1 + 16 * serviceUUIDs16Bit.length, scanOffset++);
        advData.writeUInt8(0x06, advOffset++);
        scanData.writeUInt8(0x06, scanOffset++);
        for (var i = 0; i < serviceUUIDs128Bit.length; ++i) {
          serviceUUIDs128Bit[i].copy(advData, advOffset);
          advOffset += 16;
          serviceUUIDs128Bit[i].copy(scanData, scanOffset);
          scanOffset += 16;
        }
      }
    }

    if (!advType) advType = 0;
    hci.startAdvertising(advData, scanData, {advType: advType});
  } else {
    hci.stopAdvertising();
  }
}

module.exports.iBeacon = function(enable) {
  if (enable) {
    module.exports.startAdvertising(new Buffer(
      [0x02, 0x01, 0x1a, 0x1a, 0xff, 0x4c, 0x00, 0x02, 0x15, 0xe2, 0xc5,
       0x6d, 0xb5, 0xdf, 0xfb, 0x48, 0xd2, 0xb0, 0x60, 0xd0, 0xf5, 0xa7, 0x10,
       0x96, 0xe0, 0x00, 0x00, 0x00, 0x00, 0xc5]
    ));
  } else {
    module.exports.stopAdvertising();
  }
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
