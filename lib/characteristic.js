var attribute = require('./attribute');
var btle = require('./btle');
var EventEmitter = require('events').EventEmitter;
var gatt = require('./gatt');
var util = require('util');
var UUID = require('./uuid');

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
  EventEmitter.call(this);
  if (!properties) throw new TypeError('Properties must be specified');
  if (!uuid) throw new TypeError('UUID must be specified');

  if (handle) this.endHandle = handle + 1 + (descriptors ? descriptors.length : 0);

  // Construct the value for the declaration
  uuid = UUID.getUUID(uuid);
  var declValue = new Buffer(uuid.length() + 3);
  declValue[0] = properties;
  if (handle) declValue.writeUInt16LE(handle+1, 1);
  uuid.toBuffer().copy(declValue, 3);

  this.attributes = [];
  this.properties = properties;

  // The declaration
  var declaration = attribute.create(handle, gatt.AttributeTypes.CHARACTERISTIC, declValue, this.endHandle);
  declaration.properties = Properties.READ;
  this.attributes.push(declaration);

  // The value
  var valAttr = attribute.create(handle && handle+1, uuid, value);
  valAttr.properties = properties;
  this.attributes.push(valAttr);

  // Set a write callback so we can emit a 'write' event
  // when this characteristic value is written
  if (properties & (Properties.WRITE | Properties.WRITE_WITHOUT_RESP)) {
    var self = this;
    this.attributes[1].writeCallback = function() {
      console.log(self.listeners('write'));
      self.emit('write', self);
    }
  }

  // The descriptors
  if (descriptors) {
    for (var i = 0; i < descriptors.length; ++i) {
      descriptors[i].handle = handle && handle+2+i;
      descriptors[i].properties = Properties.READ;
      this.attributes.push(descriptors[i]);
    }
  }
}

util.inherits(Characteristic, EventEmitter);

Object.defineProperty(Characteristic.prototype, 'value', {
  get: function() {
    return this.attributes[1].value;
  },
  set: function(value) {
    this.attributes[1].value = value;
  }
});

Object.defineProperty(Characteristic.prototype, 'uuid', {
  get: function() {
    return this.attributes[1].type;
  },
  set: function(value) {
    this.attributes[1].type = UUID.getUUID(value);
  }
});

module.exports.create = function(handle, properties, uuid, value, descriptors) {
  return new Characteristic(handle, properties, uuid, value, descriptors);
}

Characteristic.prototype.setHandle = function(handle) {
  // Write the handle value to the declaration
  this.attributes[0].value.writeUInt16LE(handle+1, 1);
  for (var i = 0; i < this.attributes.length; ++i) {
    this.attributes[i].handle = handle+i;
  }
  this.endHandle = this.attributes[0].endHandle = handle + this.attributes.length - 1;
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
