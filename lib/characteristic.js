var attribute = require('./attribute');
var gatt = require('./gatt');
var UUID = require('./uuid');
var btle = require('./btle');

// Characteristic Property bit field
module.exports.Properties = Properties = {
  BROADCAST          : 0x01,
  READ               : 0x02,
  WRITE_WITHOUT_RESP : 0x04,
  WRITE              : 0x08,
  NOTIFY             : 0x10,
  INDICATE           : 0x20,
  AUTH               : 0x40,
  EXT_PROPER         : 0x80
}

function Characteristic(handle, properties, uuid, value, descriptors) {
  if (!properties) throw new TypeError('Properties must be specified');
  if (!uuid) throw new TypeError('UUID must be specified');

  if (handle) this.endHandle = handle + 1 + (descriptors ? descriptors.length : 0);

  // Construct the value for the declaration
  uuid = UUID.getUUID(uuid);
  var declValue = new Buffer(uuid.length() + 3);
  declValue[0] = properties;
  if (handle) declValue.writeUInt16LE(handle+1, 1);
  uuid.toBuffer().copy(declValue, 3);

  // The declaration
  this.declaration = attribute.create(handle, gatt.AttributeTypes.CHARACTERISTIC, declValue, this.endHandle);

  // The value
  this.value = attribute.create(handle && handle+1, uuid, value);

  // The descriptors
  this.descriptors = descriptors || [];
  for (var i = 0; i < this.descriptors.length; ++i) {
    this.descriptors.handle = handle && handle+2+i;
  }
}

module.exports.create = function(handle, properties, uuid, value, descriptors) {
  return new Characteristic(handle, properties, uuid, value, descriptors);
}

Characteristic.prototype.setHandle = function(handle) {
  this.declaration.handle = handle;
  this.value.handle = handle+1;
  // Write the handle value to the declaration
  this.declaration.value.writeUInt16LE(this.value.handle);
  for (var i = 0; i < this.descriptors.length; ++i) {
    this.descriptors.handle = handle+2+i;
  }
  this.endHandle = this.declaration.endHandle = handle + 1 + (this.descriptors ? this.descriptors.length : 0);
}

Characteristic.prototype.getAttributes = function() {
  var ret = [];
  ret.push(this.declaration);
  ret.push(this.value);
  ret = ret.concat(this.descriptors);

  return ret;
}

// Populate the value field
Characteristic.prototype.readValue = function(callback) {
  var self = this;
  // Only attempt to read if the read property is set
  if (this.properties & Properties.READ) {
    this.service.device.readHandle(this.declaration.valueHandle, function(err, value) {
      try {
        if (!err) {
          self.value.value = value;
        }
        return callback(err, self);
      } catch (e) {
        return callback(e, null);
      }
    });
  } else {
    return callback(new Error('Characteristic is not readable'), this);
  }
}

Characteristic.prototype.writeValue = function(buffer, callback) {
  if (this.properties & Properties.WRITE_WITHOUT_RESP ||
      this.properties & Properties.WRITE) {
    this.service.device.writeCommand(this.valueHandle, buffer, callback);
  } else {
    return callback(new Error('Characteristic is not writable'));
  }
}

// Get the descriptors
Characteristic.prototype.readDescriptors = function(callback) {
  var self = this;
  if (this.endHandle > this.valueHandle) {
    this.service.device.findInformation(this.valueHandle+1, this.endHandle, function(err, list) {
      try {
        if (err) return callback(err, null);
        self.descriptors = [];
        var item = list.shift();
        while (item) {
          self.descriptors.push(new Descriptor(self.service, item.handle, item.type));
          item = list.shift();
        }
        return callback(err, self);
      } catch (e) {
        return callback(e, null);
      }
    });
  } else {
    return callback(null, self);
  }
}

Characteristic.prototype.listenForNotifications = function(callback) {
  var self = this;
  this.service.device.addNotificationListener(this.valueHandle, function(err, value) {
    if (!err) self.value = value;
    return callback(err, self);
  });
}
