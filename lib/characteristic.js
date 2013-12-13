var attribute = require('./attribute');
var gatt = require('./gatt');
var UUID = require('./uuid');
var btle = require('./btle');

function Declaration(attribute) {
  if (attribute.type && attribute.type != gatt.AttributeTypes.CHARACTERISTIC) {
    throw new TypeError('Characteristic declaration must have characteristic attribute type');
  }
  this.handle = attribute.handle;
  this.type = attribute.type;
  this.value = attribute.value;
  this.properties = attribute.value.readUInt8(0);
  this.valueHandle = attribute.value.readUInt16LE(1);
  this.uuid = UUID.getUUID(attribute.value.slice(3));
}

function createDeclaration(attribute) {
  return new Declaration(attribute);
}

function Characteristic(declaration, value, descriptors, endHandle, service) {
  if (declaration.constructor.name == 'Declaration') {
    this.declaration = declaration;
  } else {
    this.declaration = createDeclaration(declaration);
  }
  if (value) {
    this.value = value;
  } else {
    this.value = attribute.create(this.declaration.valueHandle, this.declaration.uuid);
  }
  this.descriptors = descriptors;
  this.service = service;
  this.properties = this.declaration.properties;
  if (endHandle) {
    this.endHandle = endHandle;
  } else if (descriptors) {
    this.endHandle = this.declaration.handle + descriptors.length + 1;
  }
}

module.exports.create = function(declaration, value, descriptors, endHandle, service) {
  return new Characteristic(declaration, value, descriptors, endHandle, service);
}

// Populate the value field
Characteristic.prototype.readValue = function(callback) {
  var self = this;
  // Only attempt to read if the read property is set
  if (this.properties & btle.BtCharProperty.ATT_CHAR_PROPER_READ) {
    this.service.device.readHandle(this.declaration.valueHandle, function(err, value) {
      try {
        if (!err) self.value.value = value;
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
  if (this.properties & btle.BtCharProperty.ATT_CHAR_PROPER_WRITE_WITHOUT_RESP ||
      this.properties & btle.BtCharProperty.ATT_CHAR_PROPER_WRITE) {
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
