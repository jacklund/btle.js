var btle = require('./index');
var device = require('./lib/device');
var util = require('util');

// Connect
device.connect('BC:6A:29:C3:52:A9', function(err, device) {
  process.on('SIGINT', function() {
    device.close();
  });

  if (err) {
    console.log(err);
    return;
  }

  // Register an error callback
  device.on('error', function(err) {
    console.error("Connection error: %s", err);
    device.close();
  });

  // Register a close callback
  device.on('close', function() {
    console.log("Got close");
  });

  console.log(device);

  /*
  //device.findService('0x1801', function(err, serviceList) {
  //device.findService('f000ffc0-0451-4000-b000-000000000000', function(err, serviceList) {
  device.findService(null, function(err, serviceList) {
    if (err) {
      console.log(err);
    } else {
      console.log(serviceList);
      //serviceList[0].findCharacteristics('0x2A00', function(err, list) {
      serviceList[3].findCharacteristics(null, function(err, list) {
        if (err) {
          console.log(err);
        } else {
          console.log(util.inspect(list, {depth: null}));
        }
      });
    }
  });

  device.findInformation(0x0001, 0xffff, function(err, object) {
    if (err) {
      console.log("Got error: " + err);
    } else {
      console.log(object);
    }
  });
  */

  // Listen for notifications
  device.addNotificationListener(0x25, function(err, value) {
    if (err) {
      console.log("Notification error: " + err);
    } else {
      console.log("Temperature, " + value.readUInt16LE(0) + ", " + value.readUInt16LE(2));
    }
  });

  device.addNotificationListener(0x2D, function(err, value) {
    if (err) {
      console.log("Notification error: " + err);
    } else {
      console.log("Accelerometer, " + value.readUInt8(0) + ", " + value.readUInt8(1) + ", " + value.readUInt8(2));
    }
  });

  // Read from handle 0x25
  device.readHandle(0x25, function(err, value) {
    if (err) {
      console.log("Error: " + err);
    } else {
      console.log(value);
    }

    // Write a 1 to handle 0x29 to turn on the thermometer
    var buffer = new Buffer([1]);
    device.writeCommand(0x29, buffer);

    // Wait for a second then read handle 0x25 again
    setTimeout(function() {
      device.readHandle(0x25, function(err, attrib) {
        console.log(attrib);

        // Write 0100 to handle 0x26
        buffer = new Buffer([1, 0]);
        device.writeCommand(0x26, buffer);

        device.writeCommand(0x31, new Buffer([1])); // Turn on accelerometer
        device.writeCommand(0x2E, buffer);
      });
    }, 1000);
  });
});
