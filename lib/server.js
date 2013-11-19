var Server = require('../build/Release/btle.node').Server;

// javascript shim that lets our object inherit from EventEmitter
inherits(Server, events.EventEmitter);

// extend prototype
function inherits(target, source) {
  for (var k in source.prototype)
    target.prototype[k] = source.prototype[k];
}

module.exports.Server = Server;
