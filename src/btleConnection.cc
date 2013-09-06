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
Persistent<Function>
BTLEConnection::constructor;

// Callback data structure
struct callbackData {
  callbackData() : conn(NULL), data(NULL), startHandle(0), endHandle(0) {}
  BTLEConnection* conn;
  void* data;
  uint16_t startHandle;
  uint16_t endHandle;
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
void
BTLEConnection::Init()
{
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("Connection"));
  tpl->InstanceTemplate()->SetInternalFieldCount(4);

  constructor = Persistent<Function>::New(tpl->GetFunction());
}

// Node.js new object construction
Handle<Value>
BTLEConnection::New(const Arguments& args)
{
  HandleScope scope;

  assert(args.IsConstructCall());

  BTLEConnection* conn = new BTLEConnection();
  conn->self = Persistent<Object>::New(args.This());
  conn->Wrap(args.This());

  return scope.Close(args.This());
}

// Connect node.js method
Handle<Value>
BTLEConnection::Connect(const Arguments& args)
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
  conn->gatt->onError(onError, conn);
  try {
    conn->connection->connect(opts, onConnect, (void*) conn);
  } catch (BTLEException& e) {
    conn->emit_error();
  }

  return scope.Close(Undefined());
}

// Send a Find Information request
Handle<Value>
BTLEConnection::FindInformation(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 3) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsUint32()) {
    ThrowException(Exception::TypeError(String::New("First argument must be a handle number")));
    return scope.Close(Undefined());
  }

  if (!args[1]->IsUint32()) {
    ThrowException(Exception::TypeError(String::New("Second argument must be a handle number")));
    return scope.Close(Undefined());
  }

  if (!args[2]->IsFunction()) {
    ThrowException(Exception::TypeError(String::New("Third argument must be a callback")));
    return scope.Close(Undefined());
  }

  BTLEConnection* conn = ObjectWrap::Unwrap<BTLEConnection>(args.This());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[2]));
  callback.MakeWeak(*callback, weak_cb);

  int startHandle, endHandle;
  getIntValue(args[0]->ToNumber(), startHandle);
  getIntValue(args[1]->ToNumber(), endHandle);
  
  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->conn = conn;
  cd->startHandle = startHandle;
  cd->endHandle = endHandle;

  conn->gatt->findInformation(startHandle, endHandle, onFindInformation, cd);
  return scope.Close(Undefined());
}

// Read an attribute
Handle<Value>
BTLEConnection::ReadHandle(const Arguments& args)
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
Handle<Value>
BTLEConnection::WriteCommand(const v8::Arguments& args)
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
Handle<Value>
BTLEConnection::WriteRequest(const v8::Arguments& args)
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
Handle<Value>
BTLEConnection::AddNotificationListener(const Arguments& args)
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
Handle<Value>
BTLEConnection::Close(const Arguments& args)
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
void
BTLEConnection::emit_error()
{
    uv_err_t err = uv_last_error(uv_default_loop());
    const int argc = 2;
    Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
    Local<Value> argv[argc] = { String::New("error"),
                                error };
    MakeCallback(this->self, "emit", argc, argv);
}

void
BTLEConnection::emit_error(const char* errorMessage)
{
    const int argc = 2;
    Local<Value> error = String::New(errorMessage);
    Local<Value> argv[argc] = { String::New("error"),
                                error };
    MakeCallback(this->self, "emit", argc, argv);
}

// Callback executed when we get connected
void
BTLEConnection::onConnect(void* data, int status, int events)
{
  BTLEConnection* conn = (BTLEConnection *) data;
  conn->handleConnect(status, events);
}

void
BTLEConnection::handleConnect(int status, int events)
{
  if (status == 0) {
    if (connectionCallback.IsEmpty()) {
      // Emit a 'connect' event, with no args
      const int argc = 1;
      Local<Value> argv[argc] = { String::New("connect") };
      MakeCallback(self, "emit", argc, argv);
    } else {
      // Call the provided callback
      const int argc = 1;
      Local<Value> argv[argc] = { Local<Value>::New(Null()) };
      connectionCallback->Call(self, argc, argv);
    }
  } else {
    // Got an error
    if (connectionCallback.IsEmpty()) {
      // Emit an error event if no callback provided
      emit_error();
    } else {
      // Call the provided callback with the error
      uv_err_t err = uv_last_error(uv_default_loop());
      const int argc = 1;
      Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
      Local<Value> argv[argc] = { error };
      connectionCallback->Call(self, argc, argv);
    }
  }
}

// Internal callback
static void onFree(char* data, void* hint)
{
  delete data;
}

void
BTLEConnection::sendFindInformation(struct callbackData* cd)
{
    Persistent<Function> callback = static_cast<Function*>(cd->data);
    const int argc = 2;
    Local<Object> response = Local<Object>::New(findInfoResponse);
    findInfoResponse.Clear();
    Local<Value> argv[argc] = { Local<Value>::New(Null()), response };
    callback->Call(self,  argc, argv);
    delete cd;
}

const char*
BTLEConnection::createErrorMessage(uint8_t err)
{
  const char* msg = Gatt::getErrorString(err);
  char buffer[128];
  if (msg == NULL) {
    sprintf(buffer, "Error code %02X", err);
    msg = buffer;
  }
  return msg;
}

void
BTLEConnection::sendFindInfoError(struct callbackData* cd, uint8_t err, const char* error)
{
    Persistent<Function> callback = static_cast<Function*>(cd->data);
    const int argc = 2;
    findInfoResponse.Dispose();
    findInfoResponse.Clear();
    const char* msg = error == NULL ? createErrorMessage(err) : error;
    Local<Value> argv[argc] = { String::New(msg), Local<Value>::New(Null()) };
    callback->Call(self, argc, argv);
    delete cd;
}

void
BTLEConnection::parseFindInfo(uint8_t*& ptr, uint16_t& handle, uint8_t* buf, int len)
{
  uint8_t format = buf[0];
  while (ptr - buf < len) {
    handle = att_get_u16(ptr);
    Local<Integer> handleLocal = Integer::New(handle);
    ptr += sizeof(uint16_t);
    char buffer[128];
    bt_uuid_t uuid;
    if (format == ATT_FIND_INFO_RESP_FMT_16BIT) {
      uuid = att_get_uuid16(ptr);
      ptr += sizeof(uint16_t);
    } else {
      uuid = att_get_uuid128(ptr);
      ptr += sizeof(uint128_t);
    }
    bt_uuid_to_string(&uuid, buffer, sizeof(buffer));
    Local<String> uuidString = String::New(buffer);
    findInfoResponse->Set(handleLocal, uuidString);
  }
}

// Find Information callback
bool
BTLEConnection::onFindInformation(uint8_t status, void* data, uint8_t* buf, int len, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  return cd->conn->handleFindInformation(status, cd->startHandle, cd->endHandle, buf, len, cd, error);
}

bool
BTLEConnection::handleFindInformation(uint8_t status, uint16_t startHandle,
  uint16_t endHandle, uint8_t* buf, int len, struct callbackData* cd, const char* error)
{
  if (error) {
  } else if (status == 0) {
    // Create the response object
    if (findInfoResponse.IsEmpty())
      findInfoResponse = Persistent<Object>::New(Object::New());

    // Parse the data
    uint8_t* ptr = &buf[1];
    uint16_t handle = 0;
    parseFindInfo(ptr, handle, buf, len);

    // If we've got more data to retrieve, make another request
    if (handle < endHandle) {
      startHandle = handle+1;
      gatt->findInformation(startHandle, endHandle, onFindInformation, cd);
      // Signal we're not done
      return false;
    } else {
      // We're done
      sendFindInformation(cd);
    }
  } else {
    // Attribute Not Found means we're done getting data
    if (status == ATT_ECODE_ATTR_NOT_FOUND && !findInfoResponse.IsEmpty()) {
      sendFindInformation(cd);
    } else {
      sendFindInfoError(cd, status, error);
    }
  }

  // We're done
  return true;
}

// Read attribute callback
bool
BTLEConnection::onReadAttribute(uint8_t status, void* data, uint8_t* buf, int len, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Persistent<Function> callback = static_cast<Function*>(cd->data);
  if (status == 0 && error == NULL) {
    Buffer* buffer = Buffer::New((char*) buf, len, onFree, NULL);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(buffer->handle_) };
    callback->Call(cd->conn->self, argc, argv);
    delete cd;
  } else {
    const int argc = 2;
    const char* msg = error == NULL ? cd->conn->createErrorMessage(status) : error;
    Local<Value> argv[argc] = { String::New(msg), Local<Value>::New(Null()) };
    callback->Call(cd->conn->self, argc, argv);
  }

  return true;
}

// Read notification callback
bool
BTLEConnection::onReadNotification(uint8_t status, void* data, uint8_t* buf, int len, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Persistent<Function> callback = static_cast<Function*>(cd->data);
  if (status == 0 && error == NULL) {
    Buffer* buffer = Buffer::New((char*) buf, len, onFree, NULL);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(buffer->handle_) };
    callback->Call(cd->conn->self,  argc, argv);
    // NOTE: We don't delete cd here because we reuse it for the notifications
  } else {
    const int argc = 2;
    const char* msg = error == NULL ? cd->conn->createErrorMessage(status) : error;
    Local<Value> argv[argc] = { String::New(msg), Local<Value>::New(Null()) };
    callback->Call(cd->conn->self, argc, argv);
  }

  return false;
}

// Write callback
void
BTLEConnection::onWrite(void* data, const char* error)
{
  struct callbackData* cd = (struct callbackData*) data;
  if (error) {
    // Error on write. Emit error if we don't have a callback
    if (cd->data == NULL) {
      cd->conn->emit_error(error);
    } else {
      // Call the provided callback with the error data
      Persistent<Function> callback = static_cast<Function*>(cd->data);
      const int argc = 1;
      Local<Value> err = ErrnoException(errno, "write", error);
      Local<Value> argv[argc] = { err };
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
void
BTLEConnection::weak_cb(Persistent<Value> object, void* parameter)
{
  Function* callback = static_cast<Function*>(parameter);

  delete callback;

  object.Dispose();
  object.Clear();
}

// Close callback
void
BTLEConnection::onClose(void* data)
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

void
BTLEConnection::onError(void* data, const char* error)
{
  BTLEConnection* conn = (BTLEConnection*) data;

  conn->emit_error(error);
}

// Node.js initialization
extern "C" void init(Handle<Object> exports)
{
  Local<FunctionTemplate> t = FunctionTemplate::New(BTLEConnection::New);
  t->InstanceTemplate()->SetInternalFieldCount(2);
  t->SetClassName(String::New("BTLEConnection"));
  NODE_SET_PROTOTYPE_METHOD(t, "connect", BTLEConnection::Connect);
  NODE_SET_PROTOTYPE_METHOD(t, "findInformation", BTLEConnection::FindInformation);
  NODE_SET_PROTOTYPE_METHOD(t, "close", BTLEConnection::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "readHandle", BTLEConnection::ReadHandle);
  NODE_SET_PROTOTYPE_METHOD(t, "addNotificationListener", BTLEConnection::AddNotificationListener);
  NODE_SET_PROTOTYPE_METHOD(t, "writeCommand", BTLEConnection::WriteCommand);

  exports->Set(String::NewSymbol("Connection"), t->GetFunction());
}

NODE_MODULE(btle, init)
