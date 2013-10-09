var UUID = require('./uuid');
var characteristic = require('./characteristic');

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

        // Recursive function to iterate through list and populate
        // characteristic, finally calling the callback when done
        function createChars(item, uuid) {
          if (item) {
            // Filter by UUID if specified
            if (uuid) {
              var uuidVal = UUID.createUUID(item.data.readUInt16LE(3));
              if (!uuidVal.isEqual(uuid)) return createChars(list.shift(), uuid);
            }
            characteristic.createCharacteristic(connection, item.handle, item.data.readUInt8(0),
                item.data.readUInt16LE(1), item.data.readUInt16LE(3), function(err, ch) {
              if (err) callback(err, null);
              chars.push(ch);
              return createChars(list.shift());
            });
          } else {
            callback(err, chars);
          }
        }

        var uuid = null;
        if (uuidIn) uuid = UUID.createUUID(uuidIn);
        createChars(list.shift(), uuid);
      }
    });
  }
}

module.exports = Service;
