assert = require('assert');
uuid = require('../lib/uuid');

var uuid1 = uuid.getUUID('0x24A0');
var uuid2 = uuid.getUUID('000024A0-0000-1000-8000-00805f9b34fb');
var uuid3 = uuid.getUUID(0x24A0);
var uuid4 = uuid.getUUID('0x24a0');

assert.equal(uuid1, uuid2);
assert.equal(uuid1, uuid3);
assert.equal(uuid1, uuid4);

assert.equal(uuid1, uuid.getUUID(uuid1));

var bad = uuid.getUUID('foobar');

assert.equal(bad, null);

console.log('Success!');
