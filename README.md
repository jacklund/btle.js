btle.js
=======
(Pronounced "BeetleJuice").

Node.js Bluetooth LE module for Linux. Uses a C/C++ addon to make direct calls to the Linux Bluetooth stack.

## Motivation
I wrote this originally because I wanted a way to access my [TI SensorTag](http://www.ti.com/ww/en/wireless_connectivity/sensortag/)
from a [Raspberry Pi](http://www.raspberrypi.org/), using [Node.js](http://nodejs.org/). There are several projects out there to do this, but all of the projects for Linux make external calls to scripts to do the Bluetooth functionality, which I didn't want to do - I really
wanted this to be pure Node.js.

What I did was take the Bluetooth client code from the [BlueZ project](http://www.bluez.org/), removed the dependency on Glib2,
and replaced it with [libuv](https://github.com/joyent/libuv). So, this is pure Node.js.

## Prerequisites
You'll need BlueZ 4.100 or greater to build btle.js. Most Linux systems should have this covered, but, unfortunately (as pointed out
by Peter Horrack - thanks Peter!), Raspberry Pi's currently do not come with anything later than 4.99 installed. You'll need to install
a more recent version of the BlueZ client library/development files to build btle.js. You can find those [here](http://www.bluez.org/download/).

## Functionality
Currently supported functionality includes:

#### ATT Protocol
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

#### GATT Protocol
* Find All Services
* Find Service by UUID
* Find All Characteristics for a Service
* Find Characteristic by UUID

## Installation
btle.js is available on [npm](https://npmjs.org/package/btle.js). To install it from there, just do:

    $ npm install btle.js

You can also just clone the project.

## API
The API follows the divisions that Bluetooth LE imposes. When you connect to a device, you are returned a `Device`
object, which has all the ATT (attribute) methods on it, so you can use those to read, write, find, and register
for notifications for attributes directly.

Additionally, you can access the GATT (Generic Attribute) interface by querying the `Device` object for services using `findService`,
which returns a `Service` object. Once you have a `Service` object, you can query that for characteristics using the
`findCharacteristics` method, which you can then query for the values, any descriptors, or even register for notifications from.

The API docs are [here](https://github.com/jacklund/btle.js/wiki/API-Docs).

## Usage:

    var btle = require('btle.js');

    // Connect
    btle.connect('BC:6A:29:C3:52:A9', function(err, device) {
      process.on('SIGINT', function() {
        device.close();
      });

      // Register an error callback
      device.on('error', function(err) {
        console.error("Connection error: %s", err);
        device.close();
      });

      // Register a close callback
      device.on('close', function() {
        console.log("Got close");
      });

      // Listen for notifications for handle 0x25
      // Function returns an error string and an attribute
      device.addNotificationListener(0x25, function(err, attrib) {
        if (err) {
          console.log("Notification error: " + err);
        } else {
          console.log("Temperature, " + attrib.value.readUInt16LE(0) + ", " + attrib.value.readUInt16LE(2));
        }
      });

      // Write a 1 to handle 0x29 to turn on the thermometer
      buffer = new Buffer([1]);
      device.writeCommand(0x29, buffer);

      // Write 0100 to handle 0x26 to turn on continuous readings
      buffer = new Buffer([1, 0]);
      device.writeCommand(0x26, buffer);
    });
