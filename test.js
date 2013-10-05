var btle = require('./index');

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

  /*
  conn.findInformation(0x0001, 0xffff, function(err, object) {
    if (err) {
      console.log("Got error: " + err);
    } else {
      console.log(object);
    }
  });
  */

  // Listen for notifications
  conn.addNotificationListener(0x25, function(err, attrib) {
    if (err) {
      console.log("Notification error: " + err);
    } else {
      console.log("Temperature, " + attrib.value.readUInt16LE(0) + ", " + attrib.value.readUInt16LE(2));
    }
  });

  conn.addNotificationListener(0x2D, function(err, attrib) {
    if (err) {
      console.log("Notification error: " + err);
    } else {
      console.log("Accelerometer, " + attrib.value.readUInt8(0) + ", " + attrib.value.readUInt8(1) + ", " + attrib.value.readUInt8(2));
    }
  });

  // Read from handle 0x25
  conn.readHandle(0x25, function(err, attrib) {
    if (err) {
      console.log("Error: " + err);
    } else {
      console.log(attrib);
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
});
