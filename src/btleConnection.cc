#include <stdlib.h>
#include <errno.h>
#include <node_buffer.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>

#include "btleConnection.h"
#include "btio.h"

using namespace v8;
using namespace node;

Persistent<Function> BTLEConnection::constructor;

void BTLEConnection::Init() {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("BTLEConnection"));
  tpl->InstanceTemplate()->SetInternalFieldCount(4);

  constructor = Persistent<Function>::New(tpl->GetFunction());
}

Handle<String> BTLEConnection::getKey(const char* value) {
  return Handle<String>(String::New(value));
}

static char stringBuffer[1024];
const char* BTLEConnection::getStringValue(Local<String> string) {
  string->WriteUtf8(stringBuffer, sizeof(stringBuffer));
  return stringBuffer;
}

bool BTLEConnection::getIntValue(Local<Number> number, int& intValue) {
  double dbl = number->Value();
  int tmp = (int) dbl;
  if (dbl != tmp) return false;
  intValue = tmp;
  return true;
}

bool BTLEConnection::getBooleanValue(Local<Boolean> boolean, int& value) {
  value = boolean->Value();
  return true;
}

bool BTLEConnection::getSourceAddr(Handle<String> key, Local<Object> options, struct set_opts& opts) {
  Local<Value> value = options->Get(key);
  if (!value->IsString()) {
    ThrowException(Exception::TypeError(String::New("Source option must be a string")));
    return false;
  }
  const char* src = getStringValue(value->ToString());
  if (!strncmp(src, "hci", 3))
    hci_devba(atoi(src + 3), &opts.src);
  else
    str2ba(src, &opts.src);

  return true;
}

bool BTLEConnection::setOpts(struct set_opts& opts, Local<Object> options) {
  memset((void*) &opts, 0, sizeof(opts));

  /* Set defaults */
  opts.type = BT_IO_SCO;
  opts.defer = 30;
  opts.master = -1;
  opts.mode = L2CAP_MODE_BASIC;
  opts.flushable = -1;
  opts.priority = 0;
  opts.src_type = BDADDR_BREDR;
  opts.dst_type = BDADDR_BREDR;

  Handle<String> key = getKey("source");
  if (options->Has(key)) {
    if (!getSourceAddr(key, options, opts)) return false;
  } else {
		bacpy(&opts.src, BDADDR_ANY);
  }

  key = getKey("destination");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsString()) {
      ThrowException(Exception::TypeError(String::New("Destination option must be a string")));
      return false;
    }
    str2ba(getStringValue(value->ToString()), &opts.dst);
  }

  key = getKey("type");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Type option must be a number")));
      return false;
    }
    int type;
    if (!getIntValue(value->ToNumber(), type) || type < BT_IO_L2CAP || type > BT_IO_SCO) {
      ThrowException(Exception::TypeError(String::New("Type option must be one of the BtIOType values")));
      return false;
    }
    opts.type = (BtIOType) type;
  }

  key = getKey("sourceType");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("SourceType option must be a number")));
      return false;
    }
    int type;
    if (!getIntValue(value->ToNumber(), type) || type < BDADDR_BREDR || type > BDADDR_LE_RANDOM) {
      ThrowException(Exception::TypeError(String::New("SourceType option must be one of the BtAddrType values")));
      return false;
    }
    opts.src_type = type;
  }

  key = getKey("destType");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("DestType option must be a number")));
      return false;
    }
    int type;
    if (!getIntValue(value->ToNumber(), type) || type < BDADDR_BREDR || type > BDADDR_LE_RANDOM) {
      ThrowException(Exception::TypeError(String::New("DestType option must be one of the BtAddrType values")));
      return false;
    }
    opts.dst_type = type;
  }

  key = getKey("defer");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Defer option must be a number")));
      return false;
    }
    int defer;
    if (!getIntValue(value->ToNumber(), defer)) {
      ThrowException(Exception::TypeError(String::New("Defer option must be an integer")));
      return false;
    }
    opts.defer = defer;
  }

  key = getKey("securityLevel");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("SecurityLevel option must be a number")));
      return false;
    }
    int sec_level;
    if (!getIntValue(value->ToNumber(), sec_level) || sec_level < BT_SECURITY_SDP || sec_level > BT_SECURITY_HIGH) {
      ThrowException(Exception::TypeError(String::New("SecurityLevel option must be one of the BtSecurityLevel values")));
      return false;
    }
    opts.sec_level = sec_level;
  }

  key = getKey("channel");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Channel option must be a number")));
      return false;
    }
    int channel;
    if (!getIntValue(value->ToNumber(), channel)) {
      ThrowException(Exception::TypeError(String::New("Channel option must be an integer")));
      return false;
    }
    opts.channel = channel;
  }

  key = getKey("psm");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("PSM option must be a number")));
      return false;
    }
    int psm;
    if (!getIntValue(value->ToNumber(), psm)) {
      ThrowException(Exception::TypeError(String::New("PSM option must be an integer")));
      return false;
    }
    opts.psm = psm;
  }

  key = getKey("cid");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("CID option must be a number")));
      return false;
    }
    int cid;
    if (!getIntValue(value->ToNumber(), cid)) {
      ThrowException(Exception::TypeError(String::New("CID option must be an integer")));
      return false;
    }
    opts.cid = cid;
  }

  key = getKey("mtu");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("MTU option must be a number")));
      return false;
    }
    int mtu;
    if (!getIntValue(value->ToNumber(), mtu)) {
      ThrowException(Exception::TypeError(String::New("MTU option must be an integer")));
      return false;
    }
    opts.mtu = mtu;
  }

  key = getKey("imtu");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("IMTU option must be a number")));
      return false;
    }
    int imtu;
    if (!getIntValue(value->ToNumber(), imtu)) {
      ThrowException(Exception::TypeError(String::New("IMTU option must be an integer")));
      return false;
    }
    opts.imtu = imtu;
  }

  key = getKey("omtu");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("OMTU option must be a number")));
      return false;
    }
    int omtu;
    if (!getIntValue(value->ToNumber(), omtu)) {
      ThrowException(Exception::TypeError(String::New("OMTU option must be an integer")));
      return false;
    }
    opts.omtu = omtu;
  }

  key = getKey("master");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Master option must be a number")));
      return false;
    }
    int master;
    if (!getIntValue(value->ToNumber(), master)) {
      ThrowException(Exception::TypeError(String::New("Master option must be an integer")));
      return false;
    }
    opts.master = master;
  }

  key = getKey("mode");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Mode option must be a number")));
      return false;
    }
    int mode;
    if (!getIntValue(value->ToNumber(), mode) || mode < L2CAP_MODE_BASIC || mode > L2CAP_MODE_STREAMING) {
      ThrowException(Exception::TypeError(String::New("Mode option must be an integer and one of the BtMode values")));
      return false;
    }
    opts.mode = mode;
  }

  key = getKey("flushable");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsBoolean()) {
      ThrowException(Exception::TypeError(String::New("Flushable option must be true or false")));
      return false;
    }
    getBooleanValue(value->ToBoolean(), opts.flushable);
  }

  key = getKey("priority");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Priority option must be a number")));
      return false;
    }
    int priority;
    if (!getIntValue(value->ToNumber(), priority)) {
      ThrowException(Exception::TypeError(String::New("Priority option must be an integer")));
      return false;
    }
    opts.priority = priority;
  }

  return true;
}

Handle<Value> BTLEConnection::New(const Arguments& args) {
  HandleScope scope;

  assert(args.IsConstructCall());

  BTLEConnection* self = new BTLEConnection();
  self->Wrap(args.This());

  return scope.Close(args.This());
}

Handle<Value> BTLEConnection::Connect(const Arguments& args) {
  HandleScope scope;
  struct set_opts opts;

  if (args.Length() < 1) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsObject()) {
    ThrowException(Exception::TypeError(String::New("Options argument must be an object")));
    return scope.Close(Undefined());
  }

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

  if (args.Length() > 1) {
    if (!args[1]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
      return scope.Close(Undefined());
    } else {
      Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
      callback.MakeWeak(*callback, weak_cb);
      conn->connectionCallback = callback;
    }
  }

  if (!setOpts(opts, args[0]->ToObject())) {
    return scope.Close(Undefined());
  }

  conn->gatt = new Gatt();
  //conn->gatt->onError(error_cb, (void*) conn);
  conn->gatt->connect(opts, connect_cb, (void*) conn);

  return scope.Close(Undefined());
}

Handle<Value> BTLEConnection::ReadHandle(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsUint32()) {
    ThrowException(Exception::TypeError(String::New("First argument must be a handle number")));
    return scope.Close(Undefined());
  }

  if (!args[1]->IsFunction()) {
    ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
    return scope.Close(Undefined());
  }

  return scope.Close(Undefined());
}

Handle<Value> BTLEConnection::Write(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!Buffer::HasInstance(args[0])) {
    ThrowException(Exception::TypeError(String::New("First argument must be a buffer")));
    return scope.Close(Undefined());
  }

  if (!args[1]->IsFunction()) {
    ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
    return scope.Close(Undefined());
  }

  size_t len = Buffer::Length(args[0]->ToObject());
  char* buffer = Buffer::Data(args[0]->ToObject());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  callback.MakeWeak(*callback, weak_cb);

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

  uv_write_t* req = new uv_write_t();
  req->data = *callback;
  /*
  uv_buf_t* buf = new uv_buf_t();
  buf->base = buffer;
  buf->len = len;
  */
  uv_buf_t buf;
  buf.base = buffer;
  buf.len = len;
  uv_buf_t bufs[] = { buf };
  uv_write(req, (uv_stream_t *) conn->tcp, bufs, 1, write_cb);

  return scope.Close(Undefined());
}

Handle<Value> BTLEConnection::Close(const Arguments& args) {
  HandleScope scope;

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

  if (args.Length() > 0) {
    if (!args[0]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Argument must be a callback")));
      return scope.Close(Undefined());
    } else {
      Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
      callback.MakeWeak(*callback, weak_cb);
      conn->closeCallback = callback;
    }
  }

  if (conn->gatt) {
    conn->gatt->close(close_cb, (void*) conn);
  }

  return scope.Close(Undefined());
}

// Emit an 'error' event
void BTLEConnection::emit_error() {
    uv_err_t err = uv_last_error(uv_default_loop());
    const int argc = 2;
    Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
    Local<Value> argv[argc] = { String::New("error"),
                                error };
    MakeCallback(this->getHandle(), "emit", argc, argv);
}

Persistent<Object> BTLEConnection::getHandle() {
  return handle_;
}

// Callback executed when we get connected
void BTLEConnection::connect_cb(void* data, int status, int events) {
  printf("BTLEConnection::connect_cb called, data = %x, status = %d, events = %d\n", data, status, events);
  BTLEConnection* conn = (BTLEConnection *) data;
  if (status == 0) {
    if (conn->connectionCallback.IsEmpty()) {
      // Emit a 'connect' event, with no args
      const int argc = 1;
      Local<Value> argv[argc] = { String::New("connect") };
      MakeCallback(Context::GetCurrent()->Global(), "emit", argc, argv);
    } else {
      const int argc = 1;
      Local<Value> argv[argc] = { Local<Value>::New(Null()) };
      conn->connectionCallback->Call(Context::GetCurrent()->Global(), argc, argv);
    }
  } else {
    if (conn->connectionCallback.IsEmpty()) {
      conn->emit_error();
    } else {
      uv_err_t err = uv_last_error(uv_default_loop());
      const int argc = 1;
      Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
      Local<Value> argv[argc] = { error };
      conn->connectionCallback->Call(Context::GetCurrent()->Global(), argc, argv);
    }
  }
}

void BTLEConnection::write_cb(uv_write_t* req, int status) {
  // Here we set up our callback for the read based on the expected op code
  Persistent<Function> callback = Persistent<Function>((Function*) req->data);
  Local<Value> argv[0] = { };
  callback->Call(Context::GetCurrent()->Global(), 0, argv);
}

void BTLEConnection::weak_cb(Persistent<Value> object, void* parameter) {
  Function* callback = static_cast<Function*>(parameter);

  delete callback;

  object.Dispose();
  object.Clear();
}

// Read callback
void BTLEConnection::read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  BTLEConnection* conn = (BTLEConnection *) stream->data;
  if (nread < 0) {
    conn->emit_error();
  } else {
    // Convert our data buffer to a Buffer
    Buffer* buffer = Buffer::New(buf.base, buf.len);

    // Emit a 'message' event with the Buffer as callback parameter
    const int argc = 2;
    Local<Value> argv[argc] = { String::New("message"),
                                Local<Value>::New(buffer->handle_) };
    MakeCallback(Context::GetCurrent()->Global(), "emit", argc, argv);
  }
}

void BTLEConnection::close_cb(void* data) {
  BTLEConnection* conn = (BTLEConnection *) data;

  if (conn->closeCallback.IsEmpty()) {
    // Emit a 'close' event, with no args
    const int argc = 1;
    Local<Value> argv[argc] = { String::New("close") };
    MakeCallback(Context::GetCurrent()->Global(), "emit", argc, argv);
  } else {
    const int argc = 1;
    Local<Value> argv[argc] = { Local<Value>::New(Null()) };
    conn->closeCallback->Call(Context::GetCurrent()->Global(), argc, argv);
  }
}

extern "C" void init(Handle<Object> exports) {
  Local<FunctionTemplate> t = FunctionTemplate::New(BTLEConnection::New);
  t->InstanceTemplate()->SetInternalFieldCount(2);
  t->SetClassName(String::New("BTLEConnection"));
  NODE_SET_PROTOTYPE_METHOD(t, "connect", BTLEConnection::Connect);
  NODE_SET_PROTOTYPE_METHOD(t, "close", BTLEConnection::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "write", BTLEConnection::Write);

  exports->Set(String::NewSymbol("BTLEConnection"), t->GetFunction());
}

NODE_MODULE(btleConnection, init)
