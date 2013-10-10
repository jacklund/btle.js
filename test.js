var btle = require('./index');
var util = require('util');

// Connect
btle.connect('BC:6A:29:C3:52:A9', function(err, conn) {
  process.on('SIGINT', function() {
    conn.close();
  });

  // Register an error callback
  conn.on('error', function(err) {
    console.error("Connection error: %s", err);
    conn.close();
  });

  // Register a close callback
  conn.on('close', function() {
    console.log("Got close");
  });

  //conn.findService('0x1801', function(err, serviceList) {
  //conn.findService('f000ffc0-0451-4000-b000-000000000000', function(err, serviceList) {
  conn.findService(null, function(err, serviceList) {
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

/*
  conn.findInformation(0x0001, 0xffff, function(err, object) {
    if (err) {
      console.log("Got error: " + err);
    } else {
      console.log(object);
    }
  });

  // Listen for notifications
  conn.addNotificationListener(0x25, function(err, value) {
    if (err) {
      console.log("Notification error: " + err);
    } else {
      console.log("Temperature, " + value.readUInt16LE(0) + ", " + value.readUInt16LE(2));
    }
  });

  conn.addNotificationListener(0x2D, function(err, value) {
    if (err) {
      console.log("Notification error: " + err);
    } else {
      console.log("Accelerometer, " + value.readUInt8(0) + ", " + value.readUInt8(1) + ", " + value.readUInt8(2));
    }
  });

  // Read from handle 0x25
  conn.readHandle(0x25, function(err, value) {
    if (err) {
      console.log("Error: " + err);
    } else {
      console.log(value);
    }

    // Write a 1 to handle 0x29 to turn on the thermometer
    var buffer = new Buffer([1]);
    conn.writeCommand(0x29, buffer);

    // Wait for a second then read handle 0x25 again
    setTimeout(function() {
      conn.readHandle(0x25, function(err, attrib) {
        console.log(attrib);

        // Write 0100 to handle 0x26
        buffer = new Buffer([1, 0]);
        conn.writeCommand(0x26, buffer);

        conn.writeCommand(0x31, new Buffer([1])); // Turn on accelerometer
        conn.writeCommand(0x2E, buffer);
      });
    }, 1000);
  });
*/
});
