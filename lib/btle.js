// javascript shim that lets our object inherit from EventEmitter
var Connection = require('../build/Debug/btle.node').Connection;
var events = require('events');

inherits(Connection, events.EventEmitter);
exports.Connection = Connection;

// extend prototype
function inherits(target, source) {
  for (var k in source.prototype)
    target.prototype[k] = source.prototype[k];
}

module.exports.BtIOType = {
	BT_IO_L2CAP: 0,
	BT_IO_RFCOMM: 1,
	BT_IO_SCO: 2,
	BT_IO_INVALID: 3,
}

module.exports.BtAddrType = {
	BDADDR_BREDR      :    0,
	BDADDR_LE_PUBLIC  :    1,
	BDADDR_LE_RANDOM  :    2
}

module.exports.BtSecurityLevel = {
	BT_SECURITY_SDP	   :    0,
	BT_SECURITY_LOW	   :    1,
	BT_SECURITY_MEDIUM :    2,
	BT_SECURITY_HIGH   :    3
}

module.exports.BtMode = {
	L2CAP_MODE_BASIC      : 0,
	L2CAP_MODE_RETRANS    : 1,
	L2CAP_MODE_FLOWCTL    : 2,
	L2CAP_MODE_ERTM	      : 3,
	L2CAP_MODE_STREAMING  : 4
}
