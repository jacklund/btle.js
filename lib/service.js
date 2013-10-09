var btle = require('./btle');
var UUID = require('./uuid');
var characteristic = require('./characteristic');

function Service(connection, handle, groupEndHandle, uuid) {
  this.handle = handle;
  this.groupEndHandle = groupEndHandle;
  this.uuid = uuid;
  var self = this;

  // Find the characteristics for this service
  this.findCharacteristics = function(uuidIn, callback) {
    var CHAR_UUID = UUID.createUUID('0x2803');  // Characteristic UUID
    var chars = [];
    var lastHandle = null;

    function getChars(startHandle) {
      connection.readByType(startHandle, groupEndHandle, CHAR_UUID.toString(), function(err, list) {

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
              // If there's an error, attach it
              if (err) ch.error = err;
              chars.push(ch);
              lastHandle = item.handle;
              return createChars(list.shift());
            });
          } else {
            // If we haven't gotten all our handles, go get more
            if (lastHandle < groupEndHandle) {
              getChars(lastHandle+1);
            } else {
              callback(err, chars);
            }
          }
        }

        if (err) {
          // BT returns this when we've gotten all the characteristics
          if (err.errorCode == btle.BtError.ATT_ECODE_ATTR_NOT_FOUND) {
            callback(null, chars);
          } else {
            callback(err, null);
          }
        } else {
          var uuid = null;
          if (uuidIn) uuid = UUID.createUUID(uuidIn);
          createChars(list.shift(), uuid);
        }
      });
    }

    getChars(handle);
  }
}

module.exports = Service;
