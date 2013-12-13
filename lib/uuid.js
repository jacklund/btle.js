var util = require('util');

/**
 * This creates UUID objects which are singletons, so that you can do uuid1 == uuid2, for instance.
 * UUIDs may be specified as either strings in either long format (e.g. '00002a00-0000-1000-8000-00805f9b34fb')
 * or short format (e.g. '0x2400'), or as a number (e.g. 0x2400), or as a Buffer (this is usually only when returned
 * from the C++ layer).
 */

// Base for short UUIDs
var BASE_UUID = '-0000-1000-8000-00805f9b34fb'

// Constructor for a Bluetooth UUID
function UUID(longString, shortString) {
  this.longString = longString;
  if (shortString) {
    this.shortString = shortString;
    this.buffer = stringToBuffer(shortString);
  } else {
    this.buffer = stringToBuffer(longString);
  }
}

UUID.prototype.size() {
  return isLongUUID() ? 128 : 16;
}

UUID.prototype.isLongUUID() {
  return this.shortString == null;
}

UUID.prototype.isShortUUID() {
  return this.shortString != null;
}

// Cached UUIDs
var uuids = {};

// Parse a long UUID string
function parseLongUUID(string) {
  if (string.indexOf('-') == -1) {
    return null;
  } else {
    var uuid = uuids[string];
    if (!uuid) {
      uuid = new UUID(string.toUpperCase());
      uuids[string] = uuid;
    }
    return uuid;
  }
}

// Convert a short UUID string to a long one
function shortToLong(string) {
  return util.format('0000%s%s', string.substr(2), BASE_UUID);
}

// Convert a long UUID string to a short one, if possible
function longToShort(string) {
  if (string.substr(0, 4) == '0000' && string.substr(8) == BASE_UUID) {
    return util.format('0x%s', string.substr(4, 4));
  } else {
    return null;
  }
}

// Parse a short UUID string
function parseShortUUID(string) {
  var patt = /^0x[0-9A-F]{4}$/i;
  if (patt.test(string)) {
    var longString = shortToLong(string.toUpperCase());
    var uuid = uuids[longString];
    if (!uuid) {
      uuid = new UUID(longString, string);
      uuids[longString] = uuid;
    }
    return uuid;
  } else {
    return null;
  }
}

// Parse a string as a UUID
function parseString(string) {
  if (string.length == 36) {
    return parseLongUUID(string);
  } else if (string.length <= 6) {
    return parseShortUUID(string);
  } else {
    return null;
  }
}

// Parse a number as a UUID
function parseNumber(num) {
  if (num > 0 && num <= 0xFFFF) {
    var shortString = '0x' + num.toString(16).toUpperCase();
    var longString = shortToLong(shortString);
    var uuid = uuids[longString];
    if (!uuid) {
      uuid = new UUID(longString, shortString);
      uuids[longString] = uuid;
    }
    return uuid;
  } else {
    return null;
  }
}

// Parse a buffer as a UUID
function parseBuffer(buf) {
  var longString = bufferToString(buf);
  if (!longString) return null;
  var uuid = uuids[longString];
  if (!uuid) {
    uuid = new UUID(longString, longToShort(longString));
    uuids[longString] = uuid;
  }
  return uuid;
}

// Get a UUID back given a string/numeric/buffer representation
module.exports.getUUID = function(value) {
  if (typeof(value) == 'string') {
    return parseString(value);
  } else if (typeof(value) == 'number') {
    return parseNumber(value);
  } else if (Buffer.isBuffer(value)) {
    return parseBuffer(value);
  } else if (value.constructor.name == 'UUID') {
    // This is already a UUID, check to see if we already have it
    if (value.longString) {
      var uuid = uuids[value.longString];
      if (!uuid) uuids[value.longString] = value;
      return value;
    } else {
      return null;
    }
  } else {
    return null;
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
