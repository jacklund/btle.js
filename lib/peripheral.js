var att = require('./att');
var btle = require('./btle');
var CentralInterface = btle.CentralInterface;
var HCI = btle.HCI;

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

var hci = null;

function Peripheral(services) {
  if (services && services.constructor.name != 'Array') {
    throw new TypeError('Services parameter must be an array of Service objects');
  }

  this.mtu = att.DEFAULT_MTU;
  this.services = services ? services : [];
  this.attributes = [];
  this.handles = {}
  this.types16 = {}
  this.types128 = {}
  for (var i = 0; i < services.length; ++i) {
    var attributes = services[i].getAttributes();
    this.attributes = this.attributes.concat(attributes);
    for (var j = 0; j < attributes.length; ++j) {
      this.handles[attributes[j].handle] = attributes[j];
      var types = attributes[j].type.isLongUUID() ? this.types128 : this.types16;
      if (types[attributes[j].type] == undefined) types[attributes[j].type] = [];
      types[attributes[j].type].push(attributes[j]);
    }
  }
}

module.exports.create = function(services) {
  return new Peripheral(services);
}

function handleError(peripheral, central, data) {
}

function handleMTU(peripheral, central, data) {
  console.log("MTU Exchange");
  var requestedMTU = data.readUInt16LE(0);
  var this.mtu = central.getMTU();
  if (requestedMTU < att.DEFAULT_MTU) {
    // TODO:Respond with error
  }
  var pdu = new Buffer(3);
  pdu[0] = att.Opcodes.MTU_RESPONSE;
  pdu.writeUInt16LE(1, this.mtu);
  central.write(pdu);
  if (requestedMTU < this.mtu) {
    this.mtu = requestedMTU;
    central.setMTU(this.mtu);
  }
}

function handleFindInfo(peripheral, central, data) {
  console.log("Find Info");
}

function handleFindByType(peripheral, central, data) {
}

function handleReadByType(peripheral, central, data) {
}

function handleRead(peripheral, central, data) {
}

function handleReadBlob(peripheral, central, data) {
}

function handleReadMulti(peripheral, central, data) {
}

function handleReadByGroup(peripheral, central, data) {
}

function handleWrite(peripheral, central, data) {
}

function handleWriteCmd(peripheral, central, data) {
}

function handlePrepWrite(peripheral, central, data) {
}

function handleExecWrite(peripheral, central, data) {
}

function handleNotify(peripheral, central, data) {
}

var handlers = {
  att.Opcodes.ERROR: handleError,
  att.Opcodes.MTU_REQUEST: handleMTU,
  att.Opcodes.FIND_INFO_REQUEST: handleFindInfo,
  att.Opcodes.FIND_BY_TYPE_REQUEST: handleFindByType,
  att.Opcodes.READ_BY_TYPE_REQUEST: handleReadByType,
  att.Opcodes.READ_REQUEST: handleRead,
  att.Opcodes.READ_BLOB_REQUEST: handleReadBlob,
  att.Opcodes.READ_MULTI_REQUEST: handleReadMulti,
  att.Opcodes.READ_BY_GROUP_REQUEST: handleReadByGroup,
  att.Opcodes.WRITE_REQUEST: handleWrite,
  att.Opcodes.WRITE_CMD: handleWriteCmd,
  att.Opcodes.PREP_WRITE_REQUEST: handlePrepWrite,
  att.Opcodes.EXEC_WRITE_REQUEST: handleExecWrite,
  att.Opcodes.HANDLE_NOTIFY: handleNotify,
  att.Opcodes.HANDLE_INDICATE: handleIndicate,
  att.Opcodes.HANDLE_VALUE_CONFIRM: handleValueConfirm,
  att.Opcodes.SIGNED_WRITE_CMD: handleSignedWrite
}

Peripheral.prototype.listen = function(opts, callback) {
  var central = new CentralInterface();
  var peripheral = this;
  if (typeof opts == 'function') {
    callback = opts;
    opts = null;
  }
  central.listen(opts, function(err, c) {
    if (err) {
      return callback(err, c);
    }
    
    // If there are no registered data listeners,
    // handle requests ourselves
    if (c.listeners('data').length == 0) {
      c.on('data', function(data) {
        var opcode = data[0];
        handlers[opcode](peripheral, c, data.slice(1));
      });
    }

    return callback(err, c);
  });
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
