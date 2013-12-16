var characteristic = require('./characteristic');
var gatt = require('./gatt');
var service = require('./service');

module.exports.PeripheralPrivacyFlag = PeripheralPrivacyFlag = {
  DISABLED : 0x00,
  ENABLED  : 0x01
}

module.exports.Defaults = Defaults = {
  APPEARANCE : 0x00,
  PPF        : PeripheralPrivacyFlag.DISABLED,
  RECONN_ADDR : '00:00:00:00:00:00'
}

module.exports.createGAPService = function(deviceName, appearance, ppf, reconnAddr, ppcp) {
  if (!deviceName) {
    throw new TypeError('Device name is mandatory');
  }
  else if (typeof deviceName != 'string' && deviceName.constructor.name != 'Buffer') {
    throw new TypeError('Device name must be a string or a buffer');
  }

  if (!appearance) {
    appearance = Defaults.APPEARANCE;
  } else if (typeof appearance != 'number') {
    throw new TypeError('Appearance must be an integer');
  }

  if (!ppf) {
    ppf = Defaults.PPF;
  } else if (typeof ppf != 'number') {
    throw new TypeError('PPF must be an integer');
  }

  if (!reconnAddr) {
    reconnAddr = Defaults.RECONN_ADDR;
  } else if (typeof reconnAddr != 'string') {
    throw new TypeError('Reconnection Address must be a string');
  }

  var characteristics = [];
  // Device name characteristic
  characteristics.push(characteristic.create(0x02, characteristic.Properties.READ, gatt.CharTypes.DEVICE_NAME, deviceName));

  // Appearance characteristic
  characteristics.push(characteristic.create(0x03, characteristic.Properties.READ, gatt.CharTypes.APPEARANCE, appearance));

  // Peripheral Privacy Flag characteristic
  characteristics.push(characteristic.create(0x04,
        characteristic.Properties.READ, gatt.CharTypes.PERIPHERAL_PRIVACY_FLAG, ppf));

  // Reconnection Address characteristic
  characteristics.push(characteristic.create(0x05, characteristic.Properties.READ,
        gatt.CharTypes.RECONNECTION_ADDRESS, reconnAddr));

  // Peripheral Preferred Connection Parameters characteristic
  if (ppcp) characteristics.push(characteristic.create(0x06, characteristic.Properties.READ,
        gatt.CharTypes.PERIPHERAL_PREFER_CONN_PARAMS, appearance));

  return service.create(0x01, gatt.Services.GAP, null, characteristics);
}

