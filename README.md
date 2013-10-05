btle.js
=======
(Pronounced "BeetleJuice").

Node.js Bluetooth LE module for Linux. Uses a C/C++ addon to make direct calls to the Linux Bluetooth stack. Currently supported functionality includes:

* Read Attribute
* Read by Group Type
* Read by Type
* Write Command
* Write Request
* Find Information
* Listen for Notifications

Still to be implemented:

* Read Multiple
* Read Blob
* Listen for Indications

I've also modified the interface so the ATT functions now return an attribute object, which contains an integer handle,
a UUID type, and a value as a buffer. Note that, depending on what function you call, one of the values may be null
because it's not returned by the particular call. Next iteration I plan on introducing attribute caching, so as
you make the calls, it populates the attributes it returns with the information it already knows about.

## Installation
btle.js is available on [npm](https://npmjs.org/package/btle.js). To install it from there, just do:

    $ npm install btle.js

You can also just clone the project.

## Usage:

    var btle = require('btle.js');

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

      // Listen for notifications for handle 0x25
      // Function returns an error string and an attribute
      conn.addNotificationListener(0x25, function(err, attrib) {
        if (err) {
          console.log("Notification error: " + err);
        } else {
          console.log("Temperature, " + attrib.value.readUInt16LE(0) + ", " + attrib.value.readUInt16LE(2));
        }
      });

      // Write a 1 to handle 0x29 to turn on the thermometer
      buffer = new Buffer([1]);
      conn.writeCommand(0x29, buffer);

      // Write 0100 to handle 0x26 to turn on continuous readings
      buffer = new Buffer([1, 0]);
      conn.writeCommand(0x26, buffer);
    });

