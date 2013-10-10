module.exports.Services = {
  GAP         : 0x1800,
  GATT        : 0x1801,
  DEVICE_INFO : 0x180A
}

module.exports.Attributes = {
  PRIMARY_SERVICE_UUID   : 0x2800,
  SECONDARY_SERVICE_UUID : 0x2801,
  INCLUDE_UUID           : 0x2802,
  CHARACTERISTIC_UUID    : 0x2803
}

module.exports.CharacteristicDescriptors = {
  CHAR_EXTENDED_PROPERTIES_UUID : 0x2900,
  CHAR_USER_DESCRIPTION_UUID    : 0x2901,
  CLIENT_CHAR_CONFIGURATION     : 0x2902,
  SERVER_CHAR_CONFIGURATION     : 0x2903,
  CHAR_FORMAT                   : 0x2904,
  CHAR_AGGREGATE_FORMAT         : 0x2905
}

module.exports.CharacteristicTypes = {
  DEVICE_NAME                   : 0x2A00,
  APPEARANCE                    : 0x2A01,
  PERIPHERAL_PRIVACY_FLAG       : 0x2A02,
  RECONNECTION_ADDRESS          : 0x2A03,
  PERIPHERAL_PREFER_CONN_PARAMS : 0x2A04,
  SERVICE_CHANGED               : 0x2A05,
  SYSTEM_ID                     : 0x2A23,
  MODEL_NUMBER_STRING           : 0x2A24,
  SERIAL_NUMBER_STRING          : 0x2A25,
  FIRMWARE_REVISION_STRING      : 0x2A26,
  HARDWARE_REVISION_STRING      : 0x2A27,
  SOFTWARE_REVISION_STRING      : 0x2A28,
  MANUFACTURER_NAME_STRING      : 0x2A29
}
