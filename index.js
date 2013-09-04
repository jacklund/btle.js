/*!
 * btle.js: a node.js Bluetooth LE Client
 * Copyright(c) 2013 Jack Lund <jack.lund@geekheads.net>
 * MIT Licensed
 */

module.exports = require('./lib/btle');

/*
module.exports.connect = module.exports.createConnection = function (address, openListener) {
  var client = new module.exports(address);
  if (typeof openListener === 'function') {
    client.on('open', openListener);
  }
  return client;
};
*/