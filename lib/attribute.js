var EventEmitter = require('events').EventEmitter;

function Attribute(handle, type, value, endHandle) {
  EventEmitter.call(this);

  this.handle = handle;
  this.type = type;
  this.value = value;
  this.endHandle = endHandle;
}

util.inherits(Attribute, EventEmitter);

Object.defineProperty(Attribute.prototype, 'value', {
  get: function() {
    return this._value;
  },
  set: function(value) {
    // Make sure the value is always a Buffer
    if (value != null && value != undefined) {
      switch (value.constructor.name) {
        case 'UUID':
          this._value = value.toBuffer();
          break;

        case 'Buffer':
          this._value = value;
          break;

        case 'String':
          this._value = new Buffer(value);
          break;

        case 'Number':
          this._value = new Buffer([value]);
          break;

        case 'Array':
          this._value = new Buffer(value);
          break;

        default:
          throw new TypeError('Unknown value type "' + value.constructor.name + '"');
      }
      this.emit('valueChanged', this);
    }
  }
});


module.exports.create = function(handle, type, value, endHandle) {
  return new Attribute(handle, type, value, endHandle || handle);
}
