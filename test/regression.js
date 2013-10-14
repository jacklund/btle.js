var assert = require('assert');
var btle = require('../index');
var device = require('../lib/device');
var tester = require('test.js');

// Setup
device.connect('BC:6A:29:C3:52:A9', function(err, device) {
  process.on('SIGINT', function() {
    device.close();
  });

  assert.ifError(err);

  // Register an error callback
  device.on('error', function(err) {
    device.close();
    assert.ifError(err);
  });

  // Run the tests
  tester.runTests(device);
});

exports.testReadHandle = function(device) {
  device.readHandle(0x25, tester.createCallback(function(err, value) {
    assert.ifError(err);
    assert(Buffer.isBuffer(value));
    assert.equal(value.readUInt32LE(0), 0);
    return tester.nextTest();
  }));
}

exports.testWriteCommand = function(device) {
  // Write a 1 to handle 0x29 to turn on the thermometer
  var buffer = new Buffer([1]);
  device.writeCommand(0x29, buffer);

  // Wait for a second then read handle 0x25 again
  setTimeout(function() {
    device.readHandle(0x25, tester.createCallback(function(err, value) {
      assert.ifError(err);
      assert(Buffer.isBuffer(value));
      assert.notEqual(value.readUInt32LE(0), 0);
      return tester.nextTest();
    }));
  }, 1000);
}

exports.testReadNotifications = function(device) {
  function enableNotifications(val) {
    var buffer;
    if (val) buffer = new Buffer([1, 0]);
    else buffer = new Buffer([0, 0]);
    device.writeCommand(0x26, buffer);
  }

  // Listen for notifications
  device.addNotificationListener(0x25, tester.createCallback(function(err, value) {
    enableNotifications(false);
    assert.ifError(err);
    assert(Buffer.isBuffer(value));
    assert.notEqual(value.readUInt32LE(0), 0);
    return tester.nextTest();
  }));

  enableNotifications(true);
}

exports.testFindInformation = function(device) {
  device.findInformation(0x0001, 0xffff, tester.createCallback(function(err, list) {
    assert.ifError(err);
    assert.equal(list.constructor.name, 'Array');
    assert.equal(list.length, 124);
    assert(list[0].handle);
    assert(list[0].type);
    return tester.nextTest();
  }));
}

exports.testFindAllServices = function(device) {
  device.findService(null, tester.createCallback(function(err, list) {
    assert.ifError(err);
    assert.equal(list.constructor.name, 'Array');
    assert.equal(list.length, 13);
    assert(list[0].handle);
    assert(list[0].groupEndHandle);
    assert(list[0].uuid);
    return tester.nextTest();
  }));
}

exports.testFindServiceByShortUUID = function(device) {
  var uuid = '0x1801';
  device.findService(uuid, tester.createCallback(function(err, list) {
    assert.ifError(err);
    assert.equal(list.constructor.name, 'Array');
    assert.equal(list.length, 1);
    assert(list[0].handle);
    assert(list[0].groupEndHandle);
    assert(list[0].uuid);
    assert.equal(list[0].uuid.shortString, uuid);
    return tester.nextTest();
  }));
}

exports.testFindServiceByLongUUID = function(device) {
  var uuid = 'f000ffc0-0451-4000-b000-000000000000';
  device.findService(uuid, tester.createCallback(function(err, list) {
    assert.ifError(err);
    assert.equal(list.constructor.name, 'Array');
    assert.equal(list.length, 1);
    assert(list[0].handle);
    assert(list[0].groupEndHandle);
    assert(list[0].uuid);
    assert.equal(list[0].uuid.longString, uuid);
    return tester.nextTest();
  }));
}

exports.testFindAllCharacteristics = function(device) {
  var uuid = 'f000aa00-0451-4000-b000-000000000000';
  device.findService(uuid, function(err, list) {
    list[0].findCharacteristics(null, tester.createCallback(function(err, list) {
      assert.ifError(err);
      assert.equal(list.constructor.name, 'Array');
      assert.equal(list.length, 2);
      assert.equal(list[0].properties, 18);
      assert.equal(list[0].declarationHandle, 36);
      assert.equal(list[0].valueHandle, 37);
      assert.equal(list[0].endHandle, 39);
      assert.equal(list[0].uuid, null); // UUID is too long for 16 bits
      assert.equal(list[0].descriptors.length, 2);
      assert.equal(list[1].properties, 10);
      assert.equal(list[1].declarationHandle, 40);
      assert.equal(list[1].valueHandle, 41);
      assert.equal(list[1].endHandle, 42);
      assert.equal(list[1].uuid, null); // UUID is too long for 16 bits
      assert.equal(list[1].descriptors.length, 1);
      return tester.nextTest();
    }));
  });
}

exports.testFindCharacteristic = function(device) {
  var uuid = '0x1801';
  device.findService(uuid, function(err, list) {
    list[0].findCharacteristics('0x2A05', function(err, list) {
      assert.ifError(err);
      assert.equal(list.constructor.name, 'Array');
      assert.equal(list.length, 1);
      assert.equal(list[0].properties, 32);
      assert.equal(list[0].declarationHandle, 13);
      assert.equal(list[0].valueHandle, 14);
      assert.equal(list[0].endHandle, 15);
      assert.equal(list[0].uuid.shortString, '0x2A05');
      return tester.nextTest();
    });
  });
}
