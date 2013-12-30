var att = require('./att');
var btle = require('./btle');
require('buffertools');
var CentralInterface = btle.CentralInterface;
var EventEmitter = require('events').EventEmitter;
var gap = require('./gap');
var gatt = require('./gatt');
var HCI = btle.HCI;
var util = require('util');

module.exports.AdvertisementTypes = {
  CONNECTABLE_UNDIRECTED     : 0x00,
  CONNECTABLE_DIRECTED       : 0x01,
  SCANNABLE_UNDIRECTED       : 0x02,
  NON_CONNECTABLE_UNDIRECTED : 0x03
}

module.exports.AdvertisementFlags = AdvertisementFlags = {
  LIMITED_DISCOVERABLE : 0x01,
  GENERAL_DISCOVERABLE : 0x02,
  BR_EDR_NOT_SUPPORTED : 0x04,
  LE_BR_EDR_CONTROLLER : 0x08,
  LE_BR_EDR_HOST       : 0x10
}

module.exports.AdvertisingDataTypes = AdvertisingDataTypes = {
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

module.exports.CompanyIDs = CompanyIDs = {
  APPLE : 0x004C
}

// Convert a number to a hex string of the specified char width
function toHexString(num, width) {
  return '0x' + (num + (1 << width * 4)).toString(16).substr(-width).toUpperCase();
}

// Constructor
function Peripheral(name, options, services) {
  EventEmitter.call(this);

  // Check arguments
  if (options && options.constructor.name == 'Array' && !services) {
    services = options;
    options = null;
  }

  if (!name) {
    throw new TypeError('Device name must be specified');
  } else if (name.constructor.name != 'String' && name.constructor.name != 'Buffer') {
    throw new TypeError('Device name must be a string or buffer');
  }

  if (options && typeof options != 'Object') {
    throw new TypeError('Options must be an object');
  }

  if (services && services.constructor.name != 'Array') {
    throw new TypeError('Services parameter must be an array of Service objects');
  }

  this.services = services || [];

  // Make sure we have a GATT service
  if (services.length < 2 || services[1].uuid != gatt.Services.GATT) {
    var gattService = gatt.createGATTService(options && options.start, options && options.end);
    this.services.unshift(gattService);
  }

  // Make sure we have a GAP service
  if (services.length == 0 || services[0].uuid != gatt.Services.GAP) {
    var gapService = gap.createGAPService(name,
        options && options.appearance,
        options && options.ppf,
        options && options.reconnAddr,
        options && options.ppcp);
    this.services.unshift(gapService);
  }

  if (btle.debug()) console.log('Peripheral has %d services', services.length);

  this.central = new CentralInterface();

  // Forward event listeners to central
  this.on('newListener', function(event, listener) {
    this.central.on(event, listener);
  });

  this.on('removeListener', function(event, listener) {
    this.central.removeListener(event, listener);
  });
}

util.inherits(Peripheral, EventEmitter);

module.exports.create = function(name, options, services) {
  return new Peripheral(name, options, services);
}

// Only instantiate an HCI when we need it, because it requires
// root privs
Object.defineProperty(Peripheral.prototype, 'hci', {
  get: function() {
    if (!this._hci) {
      this._hci = new HCI();
    }
    return this._hci;
  }
}

Peripheral.prototype.close = function() {
  this.central.close();
}

Peripheral.prototype.listen = function(opts) {
  if (!opts) opts = {};
  if (!opts.source) opts.source = 'hci0';
  this.central.on('connect', function(c) {
    c.on('data', att.createHandler(c));
  });
  this.central.listen(opts);
}

Peripheral.prototype.iBeacon = function(companyID, uuid, major, minor, power) {
  var data = uuid + getShortHex(major) + getShortHex(minor) + getSignedHex(power);
  data = '02' + (data.length / 2).toString(16) + data;
  var values = {
    flags: AdvertisementFlags.LE_BR_EDR_HOST |
           AdvertisementFlags.LE_BR_EDR_CONTROLLER |
           AdvertisementFlags.GENERAL_DISCOVERABLE,
    mfgSpecific: {
      companyID: getShortHex(companyID),
      data: data
    }
  };
  module.exports.advertise(null, values);
}

Peripheral.prototype.advertise = function(options, values, scanValues) {
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

  this.hci.startAdvertising(options, advBuffer, scanBuffer);
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
    var buffer = encodeName(values.completeName, AdvertisingDataTypes.COMPLETE_LOCAL_NAME);
    length += buffer.length;
    buffers.push(buffer);
  } else if (values.hasOwnProperty('shortName')) {
    if (typeof values.shortName != 'string') {
      // Throw exception
      throw new TypeError('shortName must be a string');
    }
    var buffer = encodeName(values.shortName, AdvertisingDataTypes.SHORT_LOCAL_NAME);
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
    buffer[1] = AdvertisingDataTypes.TX_POWER_LEVEL;
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
    buffer = Buffer.concat([new Buffer([buffer.length+1, AdvertisingDataTypes.MFG_SPECIFIC_DATA]), buffer], buffer.length+2);
    length += buffer.length; // length + type + buffer (as hex)
    buffers.push(buffer);
  }

  if (length > 31) {
    throw new Error("Advertising data too long");
  }

  return Buffer.concat(buffers, length);
}

// Encode the advertising flags
function encodeAdvFlags(flags) {
  var buffer = new Buffer(3);
  buffer[0] = 2; // length
  buffer[1] = AdvertisingDataTypes.FLAGS; // type
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
    buffer[1] = AdvertisingDataTypes.MORE_16_BIT_SERVICE_UUIDS; // type
    var len = uuid16.length * 2 + 2;
    uuid16.unshift(buffer);
    buffers.push(Buffer.concat(uuid16, len));
  }
  if (uuid128.length > 0) {
    length += uuid128.length * 16 + 2; // length + type + uuids
    var buffer = new Buffer(2);
    buffer[0] = uuid128.length * 16 + 1; // length
    buffer[1] = AdvertisingDataTypes.MORE_128_BIT_SERVICE_UUIDS; // type
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

Peripheral.prototype.stopAdvertising = module.exports.stopAdvertising = function() {
  this.hci.stopAdvertising();
}
