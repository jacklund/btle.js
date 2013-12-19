module.exports.create = function(handle, type, value, endHandle) {
  if (value != null && value != undefined) {
    switch (value.constructor.name) {
      case 'UUID':
        value = value.toBuffer();
        break;

      case 'Buffer':
        break;

      case 'String':
        value = new Buffer(value);
        break;

      case 'Number':
        value = new Buffer([value]);
        break;

      case 'Array':
        value = new Buffer(value);
        break;

      default:
        throw new TypeError('Unknown value type "' + value.constructor.name + '"');
    }
  }
  return { handle: handle, type: type, value: value, endHandle: endHandle || handle };
}
