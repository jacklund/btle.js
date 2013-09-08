btle.js
=======
(Pronounced "BeetleJuice").

Node.js Bluetooth LE module for Linux. Uses a C/C++ addon to make direct calls to the Linux Bluetooth stack. Currently supported functionality includes:

* Read Attribute
* Write Command
* Write Request
* Find Information
* Listen for Notifications

Still to be implemented:

* Read Multiple
* Read by Group Type
* Read by Type
* Read Blob
* Listen for Indications

## Usage:

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

        // Listen for notifications for handle 0x25
        conn.addNotificationListener(0x25, function(err, buffer) {
            if (err) {
              console.log("Notification error: " + err);
            } else {
              console.log("Temperature, " + buffer.readUInt16LE(0) + ", " + buffer.readUInt16LE(2));
            }
        });

        // Write a 1 to handle 0x29 to turn on the thermometer
        buffer = new Buffer([1]);
        conn.writeCommand(0x29, buffer);

        // Write 0100 to handle 0x26 to turn on continuous readings
        buffer = new Buffer([1, 0]);
        conn.writeCommand(0x26, buffer);
    });

