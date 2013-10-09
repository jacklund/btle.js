// Constructor for a Bluetooth UUID
function UUID(value) {
  var buffer;
  if (typeof(value) == 'string') {
    buffer = stringToBuffer(value);
    // Note: Need to do this to coerce it into the 'long' form
    this.string = bufferToString(buffer);
  } else if (typeof(value) == 'number') {
    // Shove the number into a buffer
    buffer = new Buffer(2);
    buffer.writeUInt16LE(value, 0);
    this.string = bufferToString(buffer);
  } else if (Buffer.isBuffer(value)) {
    buffer = value;
    this.string = bufferToString(value);
  } else {
    throw new TypeError("Argument must be either a String or integer or a Buffer");
  }
}

// UUID Equality
UUID.prototype.isEqual = function(uuid) {
  if (uuid.constructor.name == 'UUID') {
    return uuid.string == this.string;
  } else if (uuid.constructor.name == 'string') {
    return uuid == this.string;
  } else if (Buffer.isBuffer(uuid)) {
    var tmp = UUID.createUUID(uuid);
    return tmp.string == this.string;
  } else {
    return false;
  }
}

// Factory method
module.exports.createUUID = function(uuid) {
  if (uuid.constructor.name == 'UUID') {
    return uuid;
  } else {
    return new UUID(uuid);
  }
}

// Flip a buffer, used below
reverseBuffer = function(buffer) {
  var tmp = new Buffer(buffer.length);
  for (var j = 0; j < buffer.length; ++j) {
    tmp[j] = buffer[buffer.length-j-1];
  }
  return tmp;
}

// Convert a UUID in a buffer into a string
bufferToString = function(buffer) {
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

// Convert a UUID string to a buffer
stringToBuffer = function(str) {
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

UUID.prototype.toString = function() {
  if (this.string == null) {
    this.string = bufferToString(this.buffer);
  }
  return this.string;
}

UUID.prototype.toBuffer = function() {
  return this.buffer;
}
