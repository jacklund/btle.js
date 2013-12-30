var EventEmitter = require('events').EventEmitter;
var util = require('util');

var DEFAULT_MTU = 23;

var mtuSize = DEFAULT_MTU;

module.exports.Opcodes = Opcodes = {
  ERROR                  : 0x01,
  MTU_REQUEST            : 0x02,
  MTU_RESPONSE           : 0x03,
  FIND_INFO_REQUEST      : 0x04,
  FIND_INFO_RESPONSE     : 0x05,
  FIND_BY_TYPE_REQUEST   : 0x06,
  FIND_BY_TYPE_RESPONSE  : 0x07,
  READ_BY_TYPE_REQUEST   : 0x08,
  READ_BY_TYPE_RESPONSE  : 0x09,
  READ_REQUEST           : 0x0A,
  READ_RESPONSE          : 0x0B,
  READ_BLOB_REQUEST      : 0x0C,
  READ_BLOB_RESPONSE     : 0x0D,
  READ_MULTI_REQUEST     : 0x0E,
  READ_MULTI_RESPONSE    : 0x0F,
  READ_BY_GROUP_REQUEST  : 0x10,
  READ_BY_GROUP_RESPONSE : 0x11,
  WRITE_REQUEST          : 0x12,
  WRITE_RESPONSE         : 0x13,
  WRITE_CMD              : 0x52,
  PREP_WRITE_REQUEST     : 0x16,
  PREP_WRITE_RESPONSE    : 0x17,
  EXEC_WRITE_REQUEST     : 0x18,
  EXEC_WRITE_RESPONSE    : 0x19,
  HANDLE_NOTIFY          : 0x1B,
  HANDLE_INDICATE        : 0x1D,
  HANDLE_VALUE_CONFIRM   : 0x1E,
  SIGNED_WRITE_CMD       : 0xD2
}

module.exports.ErrorCodes = ErrorCodes = {
  INVALID_HANDLE        : 0x01,
  READ_NOT_PERMITTED    : 0x02,
  WRITE_NOT_PERMITTED   : 0x03,
  INVALID_PDU           : 0x04,
  AUTHENTICATION        : 0x05,
  REQ_NOT_SUPPORTED     : 0x06,
  INVALID_OFFSET        : 0x07,
  AUTHORIZATION         : 0x08,
  PREP_QUEUE_FULL       : 0x09,
  ATTR_NOT_FOUND        : 0x0A,
  ATTR_NOT_LONG         : 0x0B,
  INSUFF_ENCR_KEY_SIZE  : 0x0C,
  INVAL_ATTR_VALUE_LEN  : 0x0D,
  UNLIKELY              : 0x0E,
  INSUFF_ENCRYPTION     : 0x0F,
  UNSUPP_GRP_TYPE       : 0x10,
  INSUFF_RESOURCES      : 0x11,
  IO                    : 0x80,
  TIMEOUT               : 0x81,
  ABORTED               : 0x82
}

FindInfoResp = {
  FMT_16BIT  : 0x01,
  FMT_128BIT : 0x02
}

var attributes = [];
var handles = [];

function printHandle(num) {
  return '0x' + (num + (1 << 16)).toString(16).substr(-4).toUpperCase();
}

function Attribute(handle, type, value, endHandle) {
  EventEmitter.call(this);

  this.handle = handle;
  this.type = type;
  this.value = value;
  this.endHandle = endHandle;
}

util.inherits(Attribute, EventEmitter);

Object.defineProperty(Attribute.prototype, 'handle', {
  get: function() {
    return this._handle;
  },
  set: function(handle) {
    if (handle) {
      if (handles[handle]) {
        throw new TypeError(util.format('Attribute with handle %s already exists', printHandle(handle)));
      }
      // Remove the old element
      if (this._handle != undefined && this._handle != null) handles.splice(this._handle, 1);
      // Add the new
      handles[handle] = this;
    }
    this._handle = handle;
  }
});

Object.defineProperty(Attribute.prototype, 'value', {
  get: function() {
    return this._value;
  },
  set: function(value) {
    // Make sure the value is always a Buffer
    if (value != null && value != undefined) {
      switch (value.constructor.name) {
        case 'UUID':
          this._value = value.toBuffer();
          break;

        case 'Buffer':
          this._value = value;
          break;

        case 'String':
          this._value = new Buffer(value);
          break;

        case 'Number':
          this._value = new Buffer([value]);
          break;

        case 'Array':
          this._value = new Buffer(value);
          break;

        default:
          throw new TypeError('Unknown value type "' + value.constructor.name + '"');
      }
      this.emit('valueChanged', this);
    }
  }
});

Attribute.prototype.enableNotifications = function(central) {
  if (this.indicationListener) {
    throw new Error('Cannot enable notifications and indications on the same attribute');
  }
  this.notificationListener = function(attribute) {
    var notificationSize = value.length + 3;
    var valueSize = value.length;
    // Write no more than can fit in a PDU
    if (notificationSize > mtuSize) {
      notificationSize = mtuSize;
      valueSize = mtuSize - 3;
    }
    var notification = new Buffer(notificationSize);
    notification[0] = Opcodes.HANDLE_NOTIFY;
    notification.writeUInt16LE(attribute.handle, 1);
    attribute.value.copy(notification, 3, 0, valueSize);
    central.write(notification);
  };
  this.addListener('valueChanged', this.notificationListener);
  // Send the first notification
  this.notificationListener(this);
}

Attribute.prototype.disableNotifications = function() {
  if (this.notificationListener) {
    this.removeListener('valueChanged', this.notificationListener);
    this.notificationListener = null;
  }
}

Attribute.prototype.enableIndications = function(central) {
  if (this.notificationListener) {
    throw new Error('Cannot enable notifications and indications on the same attribute');
  }
  this.indicationListener = function(attribute) {
    // Can't send indication until we've received confirmation on the last one
    if (central.waitingForConfirmation) return;
    var indicationSize = value.length + 3;
    var valueSize = value.length;
    // Write no more than can fit in a PDU
    if (indicationSize > mtuSize) {
      indicationSize = mtuSize;
      valueSize = mtuSize - 3;
    }
    var indication = new Buffer(indicationSize);
    indication[0] = Opcodes.HANDLE_INDICATE;
    indication.writeUInt16LE(attribute.handle, 1);
    attribute.value.copy(indication, 3, 0, valueSize);
    central.write(indication);
    central.waitingForConfirmation = true;
  };
  this.addListener('valueChanged', this.indicationListener);
  // Send the first indication
  this.indicationListener(this);
}

Attribute.prototype.disableIndications = function() {
  if (this.indicationListener) {
    this.removeListener('valueChanged', this.indicationListener);
    this.indicationListener = null;
  }
}

module.exports.createAttribute = function(type, value, handle, endHandle) {
  var attribute = new Attribute(handle, type, value, endHandle || handle);
  attributes.push(attribute);
  if (handle) handles[handle] = attribute;

  return attribute;
}

function getAttribute(handle) {
  return handles[handle];
}

function findAttributes(start, end, type, value) {
  var attributes = [];
  for (var i = start; i < end; ++i) {
    if (handles[i] && (!type || handles[i].type == type) &&
        (!value || handles[i].value.equals(value))) {
      attributes.push(handles[i]);
    }
  }

  return attributes;
}

// Send an error
function sendError(central, error, op, handle) {
  var response = new Buffer(5);
  response[0] = att.Opcodes.ERROR;
  response[1] = op;
  if (handle) response.writeUInt16LE(handle, 2);
  response[4] = error;
  central.write(response);
}

function handleError(central, data) {
  var opcode = data[1];
  var handle = data.readUInt16LE(2);
  var errCode = data[4];
  if (btle.debug()) console.log("Error response, opcode = %d, handle = %s, error code = %d",
      opcode, printHandle(handle), errCode);
  var error = new Error('Received error from central device');
  error.opcode = opcode;
  error.handle = handle;
  error.errCode = errCode;
  central.emit('remoteError', error);
}

function handleMTU(central, data) {
  if (btle.debug()) console.log("MTU Exchange");
  var requestedMTU = data.readUInt16LE(0);
  mtuSize = central.getMTU();
  if (btle.debug()) console.log("Requested MTU = %d, my MTU = %d", requestedMTU, mtuSize);
  var pdu = new Buffer(3);
  pdu[0] = Opcodes.MTU_RESPONSE;
  pdu.writeUInt16LE(mtuSize, 1);
  central.write(pdu);
  if (requestedMTU < DEFAULT_MTU) {
    mtuSize = DEFAULT_MTU;
    central.setMTU(mtuSize);
  } else if (requestedMTU < mtuSize) {
    mtuSize = requestedMTU;
    central.setMTU(mtuSize);
  }
}

function handleFindInfo(central, data) {
  // Get parameters
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  if (btle.debug()) console.log("Find Info, start=%s, end=%s", printHandle(start), printHandle(end));

  // Validity checks
  if (start == 0x0000 || start > end)
    return sendError(central, ErrorCodes.INVALID_HANDLE, Opcodes.FIND_INFO_REQUEST, start);

  // Get the attributes
  var attributes = findAttributes(start, end);

  // No attributes found
  if (attributes.length == 0)
    return sendError(central, ErrorCodes.ATTR_NOT_FOUND, Opcodes.FIND_INFO_REQUEST, start);

  // Construct the response
  var response = new Buffer(mtuSize);
  response[0] = Opcodes.FIND_INFO_RESPONSE;
  var len = null;
  var offset = 2;
  for (var i = 0; i < attributes.length; ++i) {
    if (!len) {
      len = attributes[i].uuid.length();
      response[1] = len == 2 ? FindInfoResp.FMT_16BIT : FindInfoResp.FMT_128BIT;
    }
    if (attributes[i].uuid.length() != len) break;
    if (offset + attributes[i].uuid.length() + 2 > mtuSize) break;
    response.writeUInt16LE(i, offset);
    offset += 2;
    var uuidBuf = attributes[i].uuid.toBuffer();
    uuidBuf.copy(response, offset);
    offset += uuidBuf.length;
  }
  central.write(response.slice(0, offset));
}

function handleFindByType(central, data) {
  // Get params
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  var type = UUID.getUUID(data.readUInt16LE(4));
  var value = data.slice(6);
  if (btle.debug()) console.log("Find by Type Value, start=%s, end=%s, type=%s, value=%s",
      printHandle(start), printHandle(end), type, value);

  // Validate args
  if (start == 0x0000 || start > end)
    return sendError(central, ErrorCodes.INVALID_HANDLE, Opcodes.FIND_BY_TYPE_REQUEST, start);

  // Get the attributes
  var attributes = findAttributes(start, end, type, value);

  // If no match, send Attribute Not Found
  if (attributes.length == 0)
    return sendError(central, ErrorCodes.ATTR_NOT_FOUND, Opcodes.FIND_BY_TYPE_REQUEST, start);

  // Send the response
  var response = new Buffer(mtuSize);
  response[0] = Opcodes.FIND_BY_TYPE_RESPONSE;
  var offset = 1;
  for (var i = 0; i < attributes.length; ++i) {
    if (offset + 4 > mtuSize) break;
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

function handleReadByType(central, data) {
  // Get args
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  var type = UUID.getUUID(data.readUInt16LE(4));
  if (btle.debug()) console.log("Read by Type, start=%s, end=%s, type=%s",
      printHandle(start), printHandle(end), type);

  // Find the attributes
  var attributes = findAttributes(start, end, type);

  // If no matches, send Attribute Not Found
  if (attributes.length == 0)
    return sendError(central, ErrorCodes.ATTR_NOT_FOUND, Opcodes.READ_BY_TYPE_REQUEST, start);

  // Send the response
  var response = new Buffer(mtuSize);
  response[0] = Opcodes.READ_BY_TYPE_RESPONSE;
  var len = attributes[0].value.length;
  response[1] = len + 2;
  var offset = 2;
  for (var i = 0; i < attributes.length; ++i) {
    // All returned attribute values must be same length
    if (attributes[i].value.length != len) break;
    // Don't exceed the MTU size
    if (offset + attributes[i].value.length + 4 > mtuSize) break;
    response.writeUInt16LE(attributes[i].handle, offset);
    offset += 2;
    attributes[i].value.copy(response, offset);
    offset += attributes[i].value.length;
  }
  central.write(response.slice(0, offset));
}

function handleRead(central, data) {
  // Get args
  var handle = data.readUInt16LE(0);
  if (btle.debug()) console.log("Read, handle = %s", printHandle(handle));

  // Get the attribute
  var attrib = getAttribute[handle];

  // No attribute found
  if (!attrib) return sendError(central, ErrorCodes.ATTR_NOT_FOUND, Opcodes.READ_REQUEST, handle);

  // Send the response
  var response = new Buffer(mtuSize);
  if (attrib && attrib.value) {
    response[0] = Opcodes.READ_RESPONSE;
    attrib.value.copy(response, 1);
    central.write(response.slice(0, attrib.value.length+1));
  }
}

function handleReadBlob(central, data) {
  var handle = data.readUInt16LE(0);
  var offset = data.readUInt16LE(2);
  if (btle.debug()) console.log("ReadBlob, handle = %s, offset = %d", toHexString(handle), offset);
}

function handleReadMulti(central, data) {
  var handles = [];
  var offset = 0;
  while (offset < data.length) {
    handles.push(data.readUInt16LE(offset));
    offset += 2;
  }
  if (btle.debug()) console.log("ReadMultiple, %d handles", handles.length);
}

function handleReadByGroup(central, data) {
  // Get args
  var start = data.readUInt16LE(0);
  var end = data.readUInt16LE(2);
  var type = UUID.getUUID(data.slice(4));
  if (btle.debug()) console.log("Read by Group Type, start = %s, end = %s, type = %s",
      printHandle(start), printHandle(end), type);

  // Get the attributes
  var attributes = findAttributes(start, end, type);

  // No attributes
  if (attributes.length == 0) return sendError(central, ErrorCodes.ATTR_NOT_FOUND, Opcodes.READ_BY_GROUP_REQUEST, start);

  // Write the response
  var response = new Buffer(mtuSize);
  response[0] = Opcodes.READ_BY_GROUP_RESPONSE;
  response[1] = attributes[0].value.length + 4;
  var offset = 2;
  for (var i = 0; i < attributes.length; ++i) {
    if (offset + attributes[i].value.length + 4 > mtuSize) break;
    response.writeUInt16LE(attributes[i].handle, offset);
    offset += 2;
    response.writeUInt16LE(attributes[i].endHandle, offset);
    offset += 2;
    attributes[i].value.copy(response, offset);
    offset += attributes[i].value.length;
  }
  central.write(response.slice(0, offset));
}

function doWrite(central, handle, value, respond) {
  var attribute = getAttribute(handle);
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
        response[0] = Opcodes.WRITE_RESPONSE;
        central.write(response);
      }
    } else {
      sendError(central, ErrorCodes.WRITE_NOT_PERMITTED, Opcodes.WRITE_REQUEST, handle);
    }
  } else {
    sendError(central, ErrorCodes.INVALID_HANDLE, Opcodes.WRITE_REQUEST, handle);
  }
}

function handleWrite(central, data) {
  var handle = data.readUInt16LE(0);
  var value = data.slice(2);
  if (btle.debug()) console.log("Write, handle = %s, value = %s", printHandle(handle), value);
  doWrite(central, handle, value, true);
}

function handleWriteCmd(central, data) {
  var handle = data.readUInt16LE(0);
  var value = data.slice(2);
  if (btle.debug()) console.log("Write Command, handle = %s, value = %s", printHandle(handle), value);
  doWrite(central, handle, value, false);
}

function handlePrepWrite(central, data) {
}

function handleExecWrite(central, data) {
}

function handleValueConfirm(central, data) {
  // TODO: Should resend indication if we don't receive this
  if (btle.debug()) console.log("Handle Value Confirmation");
  if (central.waitingForConfirmation) central.waitingForConfirmation = false;
}

function handleSignedWrite(central, data) {
}

var handlers = {}
handlers[Opcodes.ERROR] = handleError,
handlers[Opcodes.MTU_REQUEST] = handleMTU,
handlers[Opcodes.FIND_INFO_REQUEST] = handleFindInfo,
handlers[Opcodes.FIND_BY_TYPE_REQUEST] = handleFindByType,
handlers[Opcodes.READ_BY_TYPE_REQUEST] = handleReadByType,
handlers[Opcodes.READ_REQUEST] = handleRead,
handlers[Opcodes.READ_BLOB_REQUEST] = handleReadBlob,
handlers[Opcodes.READ_MULTI_REQUEST] = handleReadMulti,
handlers[Opcodes.READ_BY_GROUP_REQUEST] = handleReadByGroup,
handlers[Opcodes.WRITE_REQUEST] = handleWrite,
handlers[Opcodes.WRITE_CMD] = handleWriteCmd,
//handlers[Opcodes.PREP_WRITE_REQUEST] = handlePrepWrite,
//handlers[Opcodes.EXEC_WRITE_REQUEST] = handleExecWrite,
handlers[Opcodes.HANDLE_VALUE_CONFIRM] = handleValueConfirm //,
//handlers[Opcodes.SIGNED_WRITE_CMD] = handleSignedWrite

module.exports.createHandler = function(central) {
  return function(data) {
    var opcode = data[0];
    if (btle.debug()) console.log('Data handler, opcode = %s', toHexString(opcode, 2));
    if (handlers[opcode]) {
      handlers[opcode](central, data.slice(1));
    } else {
      console.error('Got unknown opcode %s', toHexString(opcode, 2));
      sendError(central, ErrorCodes.REQ_NOT_SUPPORTED, opcode, 0x0000);
    }
  }
}
