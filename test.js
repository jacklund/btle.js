var btle = require('./index');

conn = new btle.Connection();

// Register an error callback
conn.on('error', function(err) {
  console.error("Connection error: %s", err);
  conn.close();
});

// Register a close callback
conn.on('close', function() {
  console.log("Got close");
});

// Our connection options
connect_opts = {
  destination: 'BC:6A:29:C3:52:A9',
  type: btle.BtIOType.BT_IO_L2CAP,
  sourceType: btle.BtAddrType.BDADDR_LE_PUBLIC,
  destType: btle.BtAddrType.BDADDR_LE_PUBLIC,
  securityLevel: btle.BtSecurityLevel.BT_SECURITY_LOW,
  cid: 4
}

// Connect
conn.connect(connect_opts, function(err) {
  process.on('SIGINT', function() {
    conn.close();
  });

  // Listen for notifications
  conn.addNotificationListener(0x25, function(buffer) {
    console.log("Temperature, " + buffer.readUInt16LE(0) + ", " + buffer.readUInt16LE(2));
  });

  conn.addNotificationListener(0x2D, function(buffer) {
    console.log("Accelerometer, " + buffer.readUInt8(0) + ", " + buffer.readUInt8(1) + ", " + buffer.readUInt8(2));
  });

  // Read from handle 0x25
  conn.readHandle(0x25, function(buffer) {
    console.log(buffer);

    // Write a 1 to handle 0x29 to turn on the thermometer
    buffer = new Buffer([1]);
    console.log(buffer);
    conn.writeCommand(0x29, buffer);

    // Wait for a second then read handle 0x25 again
    setTimeout(function() {
      conn.readHandle(0x25, function(buffer) {
	console.log(buffer);

	// Write 0100 to handle 0x26
	buffer = new Buffer([1, 0]);
	conn.writeCommand(0x26, buffer);

	conn.writeCommand(0x31, new Buffer([1])); // Turn on accelerometer
	conn.writeCommand(0x2E, buffer);
      });
    }, 1000);
  });
});
