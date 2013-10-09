var UUID = require('./uuid');
var Characteristic = require('./characteristic');

function Service(connection, handle, groupEndHandle, uuid) {
  this.handle = handle;
  this.groupEndHandle = groupEndHandle;
  this.uuid = uuid;

  // Find the characteristics for this service
  this.findCharacteristics = function(uuidIn, callback) {
    var CHAR_UUID = UUID.createUUID('0x2803');  // Characteristic UUID
    connection.readByType(this.handle, this.groupEndHandle, CHAR_UUID.toString(), function(err, list) {
      if (err) {
        callback(err, null);
      } else {
        var chars = [];
        // A null incoming uuid means find all chars
        if (uuidIn == null) {
          for (var i = 0; i < list.length; ++i) {
              chars.push(new Characteristic(connection, list[i].handle, list[i].data.readUInt8(0),
                list[i].data.readUInt16LE(1), list[i].data.readUInt16LE(3)));
          }
        } else {
          // Find the char that matches the UUID
          var uuid = UUID.createUUID(uuidIn);
          for (var i = 0; i < list.length; ++i) {
            var uuidVal = UUID.createUUID(list[i].data.readUInt16LE(3));
            if (uuidVal.isEqual(uuid)) {
              chars.push(new Characteristic(connection, list[i].handle, list[i].data.readUInt8(0),
                list[i].data.readUInt16LE(1), uuidVal));
            }
          }
        }
        callback(err, chars);
      }
    });
  }
}

module.exports = Service;
