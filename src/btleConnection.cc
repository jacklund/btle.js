#include <errno.h>
#include <node_buffer.h>

#include "btleConnection.h"
#include "connection.h"
#include "btio.h"
#include "btleException.h"
#include "util.h"

using namespace v8;
using namespace node;

// Constructor definition
Persistent<Function> BTLEConnection::constructor;

// Callback data structure
struct callbackData {
  callbackData() : conn(NULL), data(NULL) {}
  BTLEConnection* conn;
  void* data;
};

// Constructor
BTLEConnection::BTLEConnection() : gatt(NULL), connection(NULL)
{
}

// Destructor
BTLEConnection::~BTLEConnection()
{
  delete gatt;
  delete connection;
}

// Node.js initialization
void BTLEConnection::Init()
{
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("Connection"));
  tpl->InstanceTemplate()->SetInternalFieldCount(4);

  constructor = Persistent<Function>::New(tpl->GetFunction());
}

// Node.js new object construction
Handle<Value> BTLEConnection::New(const Arguments& args)
{
  HandleScope scope;

  assert(args.IsConstructCall());

  BTLEConnection* conn = new BTLEConnection();
  conn->self = Persistent<Object>::New(args.This());
  conn->Wrap(args.This());

  return scope.Close(args.This());
}

// Connect node.js method
Handle<Value> BTLEConnection::Connect(const Arguments& args)
{
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

  conn->connection = new Connection();
  conn->gatt = new Gatt(conn->connection);
  try {
    conn->connection->connect(opts, onConnect, (void*) conn);
  } catch (BTLEException& e) {
    conn->emit_error();
  }

  return scope.Close(Undefined());
}

// Read an attribute
Handle<Value> BTLEConnection::ReadHandle(const Arguments& args)
{
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

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  callback.MakeWeak(*callback, weak_cb);
  
  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->conn = conn;

  int handle;
  getIntValue(args[0]->ToNumber(), handle);
  conn->gatt->readAttribute(handle, onReadAttribute, cd);
  return scope.Close(Undefined());
}

// Write an attribute without a response
Handle<Value> BTLEConnection::WriteCommand(const v8::Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 2) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsUint32()) {
    ThrowException(Exception::TypeError(String::New("First argument must be a handle number")));
    return scope.Close(Undefined());
  }

  if (!Buffer::HasInstance(args[1])) {
    ThrowException(Exception::TypeError(String::New("Second argument must be a buffer")));
    return scope.Close(Undefined());
  }

  Persistent<Function> callback;
  if (args.Length() > 2) {
    if (!args[2]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Third argument must be a callback")));
      return scope.Close(Undefined());
    }

    callback = Persistent<Function>::New(Local<Function>::Cast(args[2]));
    callback.MakeWeak(*callback, weak_cb);
  }

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

  int handle;
  getIntValue(args[0]->ToNumber(), handle);

  struct callbackData* cd = new struct callbackData();
  cd->conn = conn;
  if (args.Length() > 2) {
    cd->data = *callback;
  }

  conn->gatt->writeCommand(handle, (const uint8_t*) Buffer::Data(args[1]), Buffer::Length(args[1]),
      onWrite, cd);

  return scope.Close(Undefined());
}

// Write an attribute with a response
Handle<Value> BTLEConnection::WriteRequest(const v8::Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 2) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsUint32()) {
    ThrowException(Exception::TypeError(String::New("First argument must be a handle number")));
    return scope.Close(Undefined());
  }

  if (!Buffer::HasInstance(args[1])) {
    ThrowException(Exception::TypeError(String::New("Second argument must be a buffer")));
    return scope.Close(Undefined());
  }

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

  int handle;
  getIntValue(args[0]->ToNumber(), handle);

  conn->gatt->writeRequest(handle, (const uint8_t*) Buffer::Data(args[1]), Buffer::Length(args[1]));

  return scope.Close(Undefined());
}

// Add a listener for notifications
Handle<Value> BTLEConnection::AddNotificationListener(const Arguments& args)
{
  HandleScope scope;

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

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

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  callback.MakeWeak(*callback, weak_cb);

  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->conn = conn;

  int handle;
  getIntValue(args[0]->ToNumber(), handle);
  conn->gatt->listenForNotifications(handle, onReadNotification, cd);

  return scope.Close(Undefined());
}

// Close the connection
Handle<Value> BTLEConnection::Close(const Arguments& args)
{
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
    conn->connection->close(onClose, (void*) conn);
  }

  return scope.Close(Undefined());
}

// Emit an 'error' event
void BTLEConnection::emit_error()
{
    uv_err_t err = uv_last_error(uv_default_loop());
    const int argc = 2;
    Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
    Local<Value> argv[argc] = { String::New("error"),
                                error };
    MakeCallback(this->self, "emit", argc, argv);
}

// Callback executed when we get connected
void BTLEConnection::onConnect(void* data, int status, int events)
{
  BTLEConnection* conn = (BTLEConnection *) data;
  if (status == 0) {
    if (conn->connectionCallback.IsEmpty()) {
      // Emit a 'connect' event, with no args
      const int argc = 1;
      Local<Value> argv[argc] = { String::New("connect") };
      MakeCallback(conn->self, "emit", argc, argv);
    } else {
      // Call the provided callback
      const int argc = 1;
      Local<Value> argv[argc] = { Local<Value>::New(Null()) };
      conn->connectionCallback->Call(conn->self, argc, argv);
    }
  } else {
    // Got an error
    if (conn->connectionCallback.IsEmpty()) {
      // Emit an error event if no callback provided
      conn->emit_error();
    } else {
      // Call the provided callback with the error
      uv_err_t err = uv_last_error(uv_default_loop());
      const int argc = 1;
      Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
      Local<Value> argv[argc] = { error };
      conn->connectionCallback->Call(conn->self, argc, argv);
    }
  }
}

// Internal callback
static void onFree(char* data, void* hint)
{
  delete data;
}

// Read attribute callback
void BTLEConnection::onReadAttribute(void* data, uint8_t* buf, int len)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Persistent<Function> callback = static_cast<Function*>(cd->data);
  // Buffer minus the opcode
  Buffer* buffer = Buffer::New((char*) &buf[1], len-1, onFree, NULL);
  const int argc = 1;
  Local<Value> argv[argc] = { Local<Value>::New(buffer->handle_) };
  callback->Call(cd->conn->self,  argc, argv);
  delete cd;
}

// Read notification callback
void BTLEConnection::onReadNotification(void* data, uint8_t* buf, int len)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Persistent<Function> callback = static_cast<Function*>(cd->data);
  // Buffer minus the opcode and handle
  Buffer* buffer = Buffer::New((char*) &buf[3], len-3, onFree, NULL);
  const int argc = 1;
  Local<Value> argv[argc] = { Local<Value>::New(buffer->handle_) };
  callback->Call(cd->conn->self,  argc, argv);
  // NOTE: We don't delete cd here because we reuse it for the notifications
}

// Write callback
void BTLEConnection::onWrite(void* data, int status)
{
  struct callbackData* cd = (struct callbackData*) data;
  if (status < 0) {
    // Error on write. Emit error if we don't have a callback
    if (cd->data == NULL) {
      cd->conn->emit_error();
    } else {
      // Call the provided callback with the error data
      Persistent<Function> callback = static_cast<Function*>(cd->data);
      uv_err_t err = uv_last_error(uv_default_loop());
      const int argc = 1;
      Local<Value> error = ErrnoException(errno, "write", uv_strerror(err));
      Local<Value> argv[argc] = { error };
      callback->Call(cd->conn->self, argc, argv);
    }
  } else {
    // Successful write. If they provided a callback, call it
    if (cd->data != NULL) {
      Persistent<Function> callback = static_cast<Function*>(cd->data);
      const int argc = 1;
      Local<Value> argv[argc] = { Local<Value>::New(Null()) };
      callback->Call(cd->conn->self, argc, argv);
    }
  }
}

// Called when we request a value be made "weak"
void BTLEConnection::weak_cb(Persistent<Value> object, void* parameter)
{
  Function* callback = static_cast<Function*>(parameter);

  delete callback;

  object.Dispose();
  object.Clear();
}

// Close callback
void BTLEConnection::onClose(void* data)
{
  BTLEConnection* conn = (BTLEConnection *) data;

  if (conn->closeCallback.IsEmpty()) {
    // Emit a 'close' event, with no args
    const int argc = 1;
    Local<Value> argv[argc] = { String::New("close") };
    MakeCallback(conn->self, "emit", argc, argv);
  } else {
    const int argc = 1;
    Local<Value> argv[argc] = { Local<Value>::New(Null()) };
    conn->closeCallback->Call(Context::GetCurrent()->Global(), argc, argv);
  }
}

// Node.js initialization
extern "C" void init(Handle<Object> exports)
{
  Local<FunctionTemplate> t = FunctionTemplate::New(BTLEConnection::New);
  t->InstanceTemplate()->SetInternalFieldCount(2);
  t->SetClassName(String::New("BTLEConnection"));
  NODE_SET_PROTOTYPE_METHOD(t, "connect", BTLEConnection::Connect);
  NODE_SET_PROTOTYPE_METHOD(t, "close", BTLEConnection::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "readHandle", BTLEConnection::ReadHandle);
  NODE_SET_PROTOTYPE_METHOD(t, "addNotificationListener", BTLEConnection::AddNotificationListener);
  NODE_SET_PROTOTYPE_METHOD(t, "writeCommand", BTLEConnection::WriteCommand);

  exports->Set(String::NewSymbol("Connection"), t->GetFunction());
}

NODE_MODULE(btleConnection, init)
