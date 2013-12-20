var att = require('./att');
var btle = require('./btle');
require('buffertools');
var CentralInterface = btle.CentralInterface;
var characteristic = require('./characteristic');
var EventEmitter = require('events').EventEmitter;
var gap = require('./gap');
var gatt = require('./gatt');
var HCI = btle.HCI;
var util = require('util');
var UUID = require('./uuid');

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

function printHandle(handle) {
  return toHexString(handle, 4);
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

  this.mtu = att.DEFAULT_MTU;
  this.attributes = [];
  this.handles = []
  var handle = 1;
  for (var i = 0; i < services.length; ++i) {
    services[i].setHandle(handle);
    handle = services[i].endHandle + 1;
    services[i].peripheral = this;
    var attributes = services[i].getAttributes();
    this.attributes = this.attributes.concat(attributes);
    for (var j = 0; j < attributes.length; ++j) {
      this.handles[attributes[j].handle] = attributes[j];
    }
  }

  for (var i = 0; i < this.attributes.length; ++i) {
    console.log(this.attributes[i]);
  }

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

Peripheral.prototype.close = function() {
  this.central.close();
}

Peripheral.prototype.get = function(handle) {
  return this.handles[handle];
}

// Send an Invalid Handle error
function sendError(central, error, op, handle) {
  var response = new Buffer(5);
  response[0] = att.Opcodes.ERROR;
  response[1] = op;
  if (handle) response.writeUInt16LE(handle, 2);
  response[4] = error;
  central.write(response);
}

// Find a set of attributes from a handles list based on
// type and/or value (or neither)
function findAttributes(handles, start, end, type, value) {
  var attributes = [];
  for (var i = start; i < end; ++i) {
    if (handles[i] && (!type || handles[i].type == type) &&
        (!value || handles[i].value.equals(value))) {
      attributes.push(handles[i]);
    }
  }

  return attributes;
}

function handleError(peripheral, central, data) {
  var opcode = data[1];
  var handle = data.readUInt16LE(2);
  var errCode = data[4];
  if (btle.debug()) console.log("Error response, opcode = %d, handle = %s, error code = %d",
      opcode, printHandle(handle), errCode);
  var error = new Error('Received error from central device');
  error.opcode = opcode;
  error.handle = handle;
  error.errCode = errCode;
  peripheral.emit('remoteError', error);
}

function handleMTU(peripheral, central, data) {
  if (btle.debug()) console.log("MTU Exchange");
  var requestedMTU = data.readUInt16LE(0);
  this.mtu = central.getMTU();
  if (btle.debug()) console.log("Requested MTU = %d, my MTU = %d", requestedMTU, this.mtu);
  var pdu = new Buffer(3);
  pdu[0] = att.Opcodes.MTU_RESPONSE;
  pdu.writeUInt16LE(this.mtu, 1);
  central.write(pdu);
  if (requestedMTU < att.DEFAULT_MTU) {
    this.mtu = att.DEFAULT_MTU;
    central.setMTU(this.mtu);
  } else if (requestedMTU < this.mtu) {
    this.mtu = requestedMTU;
    central.setMTU(this.mtu);
  }
}

function handleFindInfo(peripheral, central, data) {
  // Get parameters
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  if (btle.debug()) console.log("Find Info, start=%s, end=%s", printHandle(start), printHandle(end));

  // Validity checks
  if (start == 0x0000 || start > end)
    return sendError(central, att.ErrorCodes.INVALID_HANDLE, att.Opcodes.FIND_INFO_REQUEST, start);

  // Get the attributes
  var attributes = findAttributes(peripheral.handles, start, end);

  // No attributes found
  if (attributes.length == 0)
    return sendError(central, att.ErrorCodes.ATTR_NOT_FOUND, att.Opcodes.FIND_INFO_REQUEST, start);

  // Construct the response
  var response = new Buffer(peripheral.mtu);
  response[0] = att.Opcodes.FIND_INFO_RESPONSE;
  var len = null;
  var offset = 2;
  for (var i = 0; i < attributes.length; ++i) {
    if (!len) {
      len = attributes[i].uuid.length();
      response[1] = len == 2 ? att.FindInfoResp.FMT_16BIT : att.FindInfoResp.FMT_128BIT;
    }
    if (attributes[i].uuid.length() != len) break;
    if (offset + attributes[i].uuid.length() + 2 > peripheral.mtu) break;
    response.writeUInt16LE(i, offset);
    offset += 2;
    var uuidBuf = attributes[i].uuid.toBuffer();
    uuidBuf.copy(response, offset);
    offset += uuidBuf.length;
  }
  central.write(response.slice(0, offset));
}

function handleFindByType(peripheral, central, data) {
  // Get params
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  var type = UUID.getUUID(data.readUInt16LE(4));
  var value = data.slice(6);
  if (btle.debug()) console.log("Find by Type Value, start=%s, end=%s, type=%s, value=%s",
      printHandle(start), printHandle(end), type, value);

  // Validate args
  if (start == 0x0000 || start > end)
    return sendError(central, att.ErrorCodes.INVALID_HANDLE, att.Opcodes.FIND_BY_TYPE_REQUEST, start);

  // Get the attributes
  var attributes = findAttributes(peripheral.handles, start, end, type, value);

  // If no match, send Attribute Not Found
  if (attributes.length == 0)
    return sendError(central, att.ErrorCodes.ATTR_NOT_FOUND, att.Opcodes.FIND_BY_TYPE_REQUEST, start);

  // Send the response
  var response = new Buffer(peripheral.mtu);
  response[0] = att.Opcodes.FIND_BY_TYPE_RESPONSE;
  var offset = 1;
  for (var i = 0; i < attributes.length; ++i) {
    if (offset + 4 > peripheral.mtu) break;
    if (attributes[i].uuid == type &&
        attributes[i].value.equals(value)) {
      response.writeUInt16LE(i, offset);
      offset += 2;
      response.writeUInt16LE(attributes[i].endHandle, offset);
      offset += 2;
    }
  }
  central.write(response.slice(0, offset));
}

function handleReadByType(peripheral, central, data) {
  // Get args
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  var type = UUID.getUUID(data.readUInt16LE(4));
  if (btle.debug()) console.log("Read by Type, start=%s, end=%s, type=%s",
      printHandle(start), printHandle(end), type);

  // Find the attributes
  var attributes = findAttributes(peripheral.handles, start, end, type);

  // If no matches, send Attribute Not Found
  if (attributes.length == 0)
    return sendError(central, att.ErrorCodes.ATTR_NOT_FOUND, att.Opcodes.READ_BY_TYPE_REQUEST, start);

  // Send the response
  var response = new Buffer(peripheral.mtu);
  response[0] = att.Opcodes.READ_BY_TYPE_RESPONSE;
  var len = attributes[0].value.length;
  response[1] = len + 2;
  var offset = 2;
  for (var i = 0; i < attributes.length; ++i) {
    // All returned attribute values must be same length
    if (attributes[i].value.length != len) break;
    // Don't exceed the MTU size
    if (offset + attributes[i].value.length + 4 > peripheral.mtu) break;
    response.writeUInt16LE(attributes[i].handle, offset);
    offset += 2;
    attributes[i].value.copy(response, offset);
    offset += attributes[i].value.length;
  }
  central.write(response.slice(0, offset));
}

function handleRead(peripheral, central, data) {
  // Get args
  var handle = data.readUInt16LE(0);
  if (btle.debug()) console.log("Read, handle = %s", printHandle(handle));

  // Get the attribute
  var attrib = peripheral.handles[handle];

  // No attribute found
  if (!attrib) return sendError(central, att.ErrorCodes.ATTR_NOT_FOUND, att.Opcodes.READ_REQUEST, handle);

  // Send the response
  var response = new Buffer(peripheral.mtu);
  if (attrib && attrib.value) {
    response[0] = att.Opcodes.READ_RESPONSE;
    attrib.value.copy(response, 1);
    central.write(response.slice(0, attrib.value.length+1));
  }
}

function handleReadBlob(peripheral, central, data) {
  var handle = data.readUInt16LE(0);
  var offset = data.readUInt16LE(2);
  if (btle.debug()) console.log("ReadBlob, handle = %s, offset = %d", toHexString(handle), offset);
}

function handleReadMulti(peripheral, central, data) {
  var handles = [];
  var offset = 0;
  while (offset < data.length) {
    handles.push(data.readUInt16LE(offset));
    offset += 2;
  }
  if (btle.debug()) console.log("ReadMultiple, %d handles", handles.length);
}

function handleReadByGroup(peripheral, central, data) {
  // Get args
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  var type = UUID.getUUID(data.slice(4));
  if (btle.debug()) console.log("Read by Group Type, start = %s, end = %s, type = %s",
      printHandle(start), printHandle(end), type);

  // Get the attributes
  var attributes = findAttributes(peripheral.handles, start, end, type);

  // No attributes
  if (attributes.length == 0) return sendError(central, att.ErrorCodes.ATTR_NOT_FOUND, att.Opcodes.READ_BY_GROUP_REQUEST, start);

  // Write the response
  var response = new Buffer(peripheral.mtu);
  response[0] = att.Opcodes.READ_BY_GROUP_RESPONSE;
  response[1] = attributes[0].value.length + 4;
  var offset = 2;
  for (var i = 0; i < attributes.length; ++i) {
    if (offset + attributes[i].value.length + 4 > peripheral.mtu) break;
    response.writeUInt16LE(attributes[i].handle, offset);
    offset += 2;
    response.writeUInt16LE(attributes[i].endHandle, offset);
    offset += 2;
    attributes[i].value.copy(response, offset);
    offset += attributes[i].value.length;
  }
  central.write(response.slice(0, offset));
}

function doWrite(peripheral, central, handle, value, respond) {
  var attribute = peripheral.get(handle);
  if (attribute) {
    if ((respond && (attribute.properties & characteristic.Properties.WRITE)) ||
        (!respond && (attribute.properties & characteristic.Properties.WRITE_WITHOUT_RESP))) {
      attribute.value = value;

      // If there's a write callback, call it
      if (attribute.writeCallback) {
        attribute.writeCallback();
      }

      if (respond) {
        // Write the response
        var response = new Buffer(1);
        response[0] = att.Opcodes.WRITE_RESPONSE;
        central.write(response);
      }
    } else {
      sendError(central, att.ErrorCodes.WRITE_NOT_PERMITTED, att.Opcodes.WRITE_REQUEST, handle);
    }
  } else {
    sendError(central, att.ErrorCodes.INVALID_HANDLE, att.Opcodes.WRITE_REQUEST, handle);
  }
}

function handleWrite(peripheral, central, data) {
  var handle = data.readUInt16LE(0);
  var value = data.slice(2);
  if (btle.debug()) console.log("Write, handle = %s, value = %s", printHandle(handle), value);
  doWrite(peripheral, central, handle, value, true);
}

function handleWriteCmd(peripheral, central, data) {
  var handle = data.readUInt16LE(0);
  var value = data.slice(2);
  if (btle.debug()) console.log("Write Command, handle = %s, value = %s", printHandle(handle), value);
  doWrite(peripheral, central, handle, value, false);
}

function handlePrepWrite(peripheral, central, data) {
}

function handleExecWrite(peripheral, central, data) {
}

function handleValueConfirm(peripheral, central, data) {
  // TODO: Should resend indication if we don't receive this
  if (btle.debug()) console.log("Handle Value Confirmation");
}

function handleSignedWrite(peripheral, central, data) {
}

var handlers = {}
handlers[att.Opcodes.ERROR] = handleError,
handlers[att.Opcodes.MTU_REQUEST] = handleMTU,
handlers[att.Opcodes.FIND_INFO_REQUEST] = handleFindInfo,
handlers[att.Opcodes.FIND_BY_TYPE_REQUEST] = handleFindByType,
handlers[att.Opcodes.READ_BY_TYPE_REQUEST] = handleReadByType,
handlers[att.Opcodes.READ_REQUEST] = handleRead,
handlers[att.Opcodes.READ_BLOB_REQUEST] = handleReadBlob,
handlers[att.Opcodes.READ_MULTI_REQUEST] = handleReadMulti,
handlers[att.Opcodes.READ_BY_GROUP_REQUEST] = handleReadByGroup,
handlers[att.Opcodes.WRITE_REQUEST] = handleWrite,
handlers[att.Opcodes.WRITE_CMD] = handleWriteCmd,
//handlers[att.Opcodes.PREP_WRITE_REQUEST] = handlePrepWrite,
//handlers[att.Opcodes.EXEC_WRITE_REQUEST] = handleExecWrite,
handlers[att.Opcodes.HANDLE_VALUE_CONFIRM] = handleValueConfirm //,
//handlers[att.Opcodes.SIGNED_WRITE_CMD] = handleSignedWrite

Peripheral.prototype.listen = function(opts) {
  var peripheral = this;
  if (!opts) opts = {};
  if (!opts.source) opts.source = 'hci0';
  this.central.on('connect', function(c) {
    c.on('data', function(data) {
      var opcode = data[0];
      if (btle.debug()) console.log('Data handler, opcode = %s', toHexString(opcode, 2));
      if (handlers[opcode]) {
        handlers[opcode](peripheral, c, data.slice(1));
      } else {
        console.error('Got unknown opcode %s', toHexString(opcode, 2));
        sendError(c, att.ErrorCodes.REQ_NOT_SUPPORTED, opcode, 0x0000);
      }
    });
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

var hci = null;

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

Peripheral.prototype.stopAdvertising = function() {
  hci.stopAdvertising();
}
