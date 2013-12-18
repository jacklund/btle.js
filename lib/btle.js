var btle = require('../build/Release/btle.node');
var PeripheralInterface = btle.PeripheralInterface;
var CentralInterface = btle.CentralInterface;
var HCI = btle.HCI;
var events = require('events');

module.exports.HCI = btle.HCI;
module.exports.PeripheralInterface = btle.PeripheralInterface;
module.exports.CentralInterface = btle.CentralInterface;

var debug = false;

module.exports.setDebug = function(value) {
  if (typeof value != 'boolean') {
    throw new TypeError('setDebug takes a boolean value only');
  }
  debug = value;
  btle.setDebug(value);
}

module.exports.debug = function() {
  return debug;
}

// javascript shim that lets our object inherit from EventEmitter
inherits(PeripheralInterface, events.EventEmitter);
inherits(CentralInterface, events.EventEmitter);

// extend prototype
function inherits(target, source) {
  for (var k in source.prototype)
    target.prototype[k] = source.prototype[k];
}

// Connect function
module.exports.connect = function(destination, opts, callback) {
  var peripheral = new PeripheralInterface();
  peripheral.connect(destination, opts, callback);
}

// Listen function
module.exports.listen = function(opts, callback) {
  var central = new CentralInterface();
  central.listen(opts, callback);
}

// Bluetooth I/O types
module.exports.IOTypes = {
  L2CAP  : 0,
  RFCOMM : 1,
  SCO    : 2,
}

// Bluetooth address types
module.exports.AddrTypes = {
  BREDR      :    0,
  LE_PUBLIC  :    1,
  LE_RANDOM  :    2
}

// Bluetooth security levels
module.exports.SecurityLevel = {
  SDP     :    0,
  LOW     :    1,
  MEDIUM  :    2,
  HIGH    :    3
}

// L2CAP modes
module.exports.L2CAP_Mode = {
  BASIC      : 0,
  RETRANS    : 1,
  FLOWCTL    : 2,
  ERTM       : 3,
  STREAMING  : 4
}
