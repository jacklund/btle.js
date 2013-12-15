module.exports.create = function(handle, type, value, endHandle) {
  if (value && value.constructor.name == 'UUID') {
    value = value.toBuffer();
  }
  return { handle: handle, type: type, value: value, endHandle: endHandle };
}
