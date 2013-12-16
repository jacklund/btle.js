var characteristic = require('./characteristic');
var service = require('./service');
var uuid = require('./uuid');

module.exports.Services = Services = {
  GAP         : uuid.getUUID(0x1800),
  GATT        : uuid.getUUID(0x1801),
  DEVICE_INFO : uuid.getUUID(0x180A)
}

module.exports.AttributeTypes = {
  PRIMARY_SERVICE   : uuid.getUUID(0x2800),
  SECONDARY_SERVICE : uuid.getUUID(0x2801),
  INCLUDE           : uuid.getUUID(0x2802),
  CHARACTERISTIC    : uuid.getUUID(0x2803)
}

module.exports.CharDescriptors = {
  CHAR_EXTENDED_PROPERTIES_UUID : uuid.getUUID(0x2900),
  CHAR_USER_DESCRIPTION_UUID    : uuid.getUUID(0x2901),
  CLIENT_CHAR_CONFIGURATION     : uuid.getUUID(0x2902),
  SERVER_CHAR_CONFIGURATION     : uuid.getUUID(0x2903),
  CHAR_FORMAT                   : uuid.getUUID(0x2904),
  CHAR_AGGREGATE_FORMAT         : uuid.getUUID(0x2905)
}

module.exports.CharTypes = CharTypes = {
  DEVICE_NAME                   : uuid.getUUID(0x2A00),
  APPEARANCE                    : uuid.getUUID(0x2A01),
  PERIPHERAL_PRIVACY_FLAG       : uuid.getUUID(0x2A02),
  RECONNECTION_ADDRESS          : uuid.getUUID(0x2A03),
  PERIPHERAL_PREFER_CONN_PARAMS : uuid.getUUID(0x2A04),
  SERVICE_CHANGED               : uuid.getUUID(0x2A05),
  SYSTEM_ID                     : uuid.getUUID(0x2A23),
  MODEL_NUMBER_STRING           : uuid.getUUID(0x2A24),
  SERIAL_NUMBER_STRING          : uuid.getUUID(0x2A25),
  FIRMWARE_REVISION_STRING      : uuid.getUUID(0x2A26),
  HARDWARE_REVISION_STRING      : uuid.getUUID(0x2A27),
  SOFTWARE_REVISION_STRING      : uuid.getUUID(0x2A28),
  MANUFACTURER_NAME_STRING      : uuid.getUUID(0x2A29)
}

module.exports.createGATTService = function(start, end) {
  if (!start) start = 0x0000;
  if (!end) end = 0x0000;
  var characteristics = [];
  var data = new Buffer(4);
  data.writeUInt16LE(start, 0);
  data.writeUInt16LE(end, 2);
  characteristics.push(characteristic.create(0x02, characteristic.Properties.INDICATE, CharTypes.SERVICE_CHANGED, data));
  return service.create(0x01, Services.GATT, null, characteristics);
}
