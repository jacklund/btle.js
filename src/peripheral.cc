#include <errno.h>
#include <node_buffer.h>

#include "peripheral.h"
#include "btio.h"
#include "btleException.h"
#include "central.h"
#include "hci.h"
#include "util.h"

using namespace v8;
using namespace node;

// Constructor definition
Persistent<Function>
Peripheral::constructor;

// Callback data structure
struct callbackData {
  callbackData() : peripheral(NULL), data(NULL), startHandle(0), endHandle(0) {}
  Peripheral* peripheral;
  void* data;
  uint16_t startHandle;
  uint16_t endHandle;
};

// Constructor
Peripheral::Peripheral() : att(NULL)
{
}

// Destructor
Peripheral::~Peripheral()
{
  delete att;
}

// Node.js initialization
void
Peripheral::Init(Handle<Object> exports)
{
  Local<FunctionTemplate> t = FunctionTemplate::New(Peripheral::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::New("PeripheralInterface"));
  NODE_SET_PROTOTYPE_METHOD(t, "connect", Peripheral::Connect);
  NODE_SET_PROTOTYPE_METHOD(t, "findInformation", Peripheral::FindInformation);
  NODE_SET_PROTOTYPE_METHOD(t, "findByTypeValue", Peripheral::FindByTypeValue);
  NODE_SET_PROTOTYPE_METHOD(t, "readByType", Peripheral::ReadByType);
  NODE_SET_PROTOTYPE_METHOD(t, "readByGroupType", Peripheral::ReadByGroupType);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Peripheral::Close);
  NODE_SET_PROTOTYPE_METHOD(t, "readHandle", Peripheral::ReadHandle);
  NODE_SET_PROTOTYPE_METHOD(t, "addNotificationListener", Peripheral::AddNotificationListener);
  NODE_SET_PROTOTYPE_METHOD(t, "writeCommand", Peripheral::WriteCommand);

  exports->Set(String::NewSymbol("PeripheralInterface"), t->GetFunction());
}

// Node.js new object construction
Handle<Value>
Peripheral::New(const Arguments& args)
{
  HandleScope scope;

  assert(args.IsConstructCall());

  Peripheral* peripheral = new Peripheral();
  peripheral->self = Persistent<Object>::New(args.This());
  peripheral->Wrap(args.This());

  return scope.Close(args.This());
}

// Connect node.js method
Handle<Value>
Peripheral::Connect(const Arguments& args)
{
  HandleScope scope;
  struct set_opts opts;

  if (args.Length() < 2) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsString()) {
    ThrowException(Exception::TypeError(String::New("Destination object must be a string")));
    return scope.Close(Undefined());
  }

  Local<String> destination = Local<String>::Cast(args[0]);

  Local<Object> options;
  Persistent<Function> callback;

  if (args[1]->IsObject() && !args[1]->IsFunction()) {
    if (args.Length() < 3) {
      ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
      return scope.Close(Undefined());
    }
    if (!args[2]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Third argument must be a callback")));
      return scope.Close(Undefined());
    }
    options = args[1]->ToObject();
    callback = Persistent<Function>::New(Local<Function>::Cast(args[2]));
  } else if (args[1]->IsFunction()) {
    options = Object::New();
    callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  } else {
      ThrowException(Exception::TypeError(String::New("Second argument must be either a callback or options")));
      return scope.Close(Undefined());
  }

  if (!setOpts(opts, destination, options)) {
    return scope.Close(Undefined());
  }

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  //callback.MakeWeak(*callback, weak_cb);
  peripheral->connectionCallback = callback;

  peripheral->att = new Att();
  peripheral->att->onError(onError, peripheral);
  try {
    peripheral->att->connect(opts, onConnect, (void*) peripheral);
  } catch (BTLEException& e) {
    peripheral->emit_error();
  }

  return scope.Close(Undefined());
}

// Send a Find Information request
Handle<Value>
Peripheral::FindInformation(const Arguments& args)
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

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[2]));
  //callback.MakeWeak(*callback, weak_cb);

  int startHandle, endHandle;
  getIntValue(args[0]->ToNumber(), startHandle);
  getIntValue(args[1]->ToNumber(), endHandle);
  
  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->peripheral = peripheral;
  cd->startHandle = startHandle;
  cd->endHandle = endHandle;

  peripheral->att->findInformation(startHandle, endHandle, onFindInformation, cd);
  return scope.Close(Undefined());
}

// Send a Find By Type Value request
Handle<Value>
Peripheral::FindByTypeValue(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 5) {
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

  if (!args[2]->IsString()) {
    ThrowException(Exception::TypeError(String::New("Third argument must be a string")));
    return scope.Close(Undefined());
  }

  if (!Buffer::HasInstance(args[3])) {
    ThrowException(Exception::TypeError(String::New("Fourth argument must be a buffer")));
    return scope.Close(Undefined());
  }

  if (!args[4]->IsFunction()) {
    ThrowException(Exception::TypeError(String::New("Fifth argument must be a callback")));
    return scope.Close(Undefined());
  }

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[4]));
  //callback.MakeWeak(*callback, weak_cb);

  int startHandle, endHandle;
  getIntValue(args[0]->ToNumber(), startHandle);
  getIntValue(args[1]->ToNumber(), endHandle);

  bt_uuid_t uuid;
  bt_string_to_uuid(&uuid, getStringValue(args[2]->ToString()));
  
  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->peripheral = peripheral;
  cd->startHandle = startHandle;
  cd->endHandle = endHandle;

  peripheral->att->findByTypeValue(startHandle, endHandle, uuid,
      (const uint8_t*) Buffer::Data(args[3]), Buffer::Length(args[3]), onFindByType, cd);
  return scope.Close(Undefined());
}

// Send a Read By Type request
Handle<Value>
Peripheral::ReadByType(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 4) {
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

  if (!args[2]->IsString()) {
    ThrowException(Exception::TypeError(String::New("Third argument must be a string")));
    return scope.Close(Undefined());
  }

  if (!args[3]->IsFunction()) {
    ThrowException(Exception::TypeError(String::New("Fourth argument must be a callback")));
    return scope.Close(Undefined());
  }

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[3]));
  //callback.MakeWeak(*callback, weak_cb);

  int startHandle, endHandle;
  getIntValue(args[0]->ToNumber(), startHandle);
  getIntValue(args[1]->ToNumber(), endHandle);

  bt_uuid_t uuid;
  bt_string_to_uuid(&uuid, getStringValue(args[2]->ToString()));
  
  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->peripheral = peripheral;
  cd->startHandle = startHandle;
  cd->endHandle = endHandle;

  peripheral->att->readByType(startHandle, endHandle, uuid, onReadByType, cd);
  return scope.Close(Undefined());
}

// Send a Read By Group Type request
Handle<Value>
Peripheral::ReadByGroupType(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 4) {
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

  if (!args[2]->IsString()) {
    ThrowException(Exception::TypeError(String::New("Third argument must be a string")));
    return scope.Close(Undefined());
  }

  if (!args[3]->IsFunction()) {
    ThrowException(Exception::TypeError(String::New("Fourth argument must be a callback")));
    return scope.Close(Undefined());
  }

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[3]));
  //callback.MakeWeak(*callback, weak_cb);

  int startHandle, endHandle;
  getIntValue(args[0]->ToNumber(), startHandle);
  getIntValue(args[1]->ToNumber(), endHandle);

  bt_uuid_t uuid;
  bt_string_to_uuid(&uuid, getStringValue(args[2]->ToString()));
  
  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->peripheral = peripheral;
  cd->startHandle = startHandle;
  cd->endHandle = endHandle;

  // Note: Can re-use onReadByType
  peripheral->att->readByGroupType(startHandle, endHandle, uuid, onReadByGroupType, cd);
  return scope.Close(Undefined());
}

// Read an attribute
Handle<Value>
Peripheral::ReadHandle(const Arguments& args)
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

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  //callback.MakeWeak(*callback, weak_cb);
  
  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->peripheral = peripheral;

  int handle;
  getIntValue(args[0]->ToNumber(), handle);
  peripheral->att->readAttribute(handle, onReadAttribute, cd);
  return scope.Close(Undefined());
}

// Write an attribute without a response
Handle<Value>
Peripheral::WriteCommand(const v8::Arguments& args)
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
    //callback.MakeWeak(*callback, weak_cb);
  }

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  int handle;
  getIntValue(args[0]->ToNumber(), handle);

  struct callbackData* cd = new struct callbackData();
  cd->peripheral = peripheral;
  if (args.Length() > 2) {
    cd->data = *callback;
  }

  peripheral->att->writeCommand(handle, (const uint8_t*) Buffer::Data(args[1]), Buffer::Length(args[1]),
      onWrite, cd);

  return scope.Close(Undefined());
}

// Write an attribute with a response
Handle<Value>
Peripheral::WriteRequest(const v8::Arguments& args)
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

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  int handle;
  getIntValue(args[0]->ToNumber(), handle);

  peripheral->att->writeRequest(handle, (const uint8_t*) Buffer::Data(args[1]), Buffer::Length(args[1]));

  return scope.Close(Undefined());
}

// Add a listener for notifications
Handle<Value>
Peripheral::AddNotificationListener(const Arguments& args)
{
  HandleScope scope;

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

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
  //callback.MakeWeak(*callback, weak_cb);

  struct callbackData* cd = new struct callbackData();
  cd->data = *callback;
  cd->peripheral = peripheral;

  int handle;
  getIntValue(args[0]->ToNumber(), handle);
  peripheral->att->listenForNotifications(handle, onReadNotification, cd);

  return scope.Close(Undefined());
}

// Close the connection
Handle<Value>
Peripheral::Close(const Arguments& args)
{
  HandleScope scope;

  Peripheral* peripheral = ObjectWrap::Unwrap<Peripheral>(args.This());

  if (args.Length() > 0) {
    if (!args[0]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Argument must be a callback")));
      return scope.Close(Undefined());
    } else {
      Persistent<Function> callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
      //callback.MakeWeak(*callback, weak_cb);
      peripheral->closeCallback = callback;
    }
  }

  if (peripheral->att) {
    peripheral->att->close(onClose, (void*) peripheral);
  }

  return scope.Close(Undefined());
}

Local<Object>
Peripheral::getAttributeInfo(Att::AttributeInfo* attribute)
{
  Local<Object> ret = Object::New();
  ret->Set(String::New("handle"), Integer::New(attribute->handle));
  char buffer[128];
  bt_uuid_to_string(&attribute->type, buffer, sizeof(buffer));
  ret->Set(String::New("type"), String::New(buffer));

  return ret;
}

Local<Object>
Peripheral::getHandlesInfo(Att::HandlesInfo* attribute)
{
  Local<Object> ret = Object::New();
  ret->Set(String::New("handle"), Integer::New(attribute->handle));
  ret->Set(String::New("groupEndHandle"), Integer::New(attribute->groupEndHandle));

  return ret;
}

Local<Object>
Peripheral::getAttributeData(Att::AttributeData* attribute)
{
  Local<Object> ret = Object::New();
  ret->Set(String::New("handle"), Integer::New(attribute->handle));
  Buffer* buffer = Buffer::New(attribute->length);
  memcpy(Buffer::Data(buffer), attribute->data, attribute->length);
  ret->Set(String::New("value"), buffer->handle_);

  return ret;
}

Local<Object>
Peripheral::getGroupAttributeData(Att::GroupAttributeData* attribute)
{
  Local<Object> ret = Object::New();
  ret->Set(String::New("handle"), Integer::New(attribute->handle));
  ret->Set(String::New("groupEndHandle"), Integer::New(attribute->groupEndHandle));
  Buffer* buffer = Buffer::New(attribute->length);
  memcpy(Buffer::Data(buffer), attribute->data, attribute->length);
  ret->Set(String::New("value"), buffer->handle_);

  return ret;
}

// Emit an 'error' event
void
Peripheral::emit_error()
{
    uv_err_t err = uv_last_error(uv_default_loop());
    const int argc = 3;
    Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
    Local<Value> argv[argc] = { String::New("error"),
                                error,
                                Local<Value>::New(this->self) };
    MakeCallback(this->self, "emit", argc, argv);
}

void
Peripheral::emit_error(const char* errorMessage)
{
    const int argc = 3;
    Local<Value> error = String::New(errorMessage);
    Local<Value> argv[argc] = { String::New("error"),
                                error,
                                Local<Value>::New(this->self) };
    MakeCallback(this->self, "emit", argc, argv);
}

// Callback executed when we get connected
void
Peripheral::onConnect(void* data, int status, int events)
{
  Peripheral* peripheral = (Peripheral *) data;
  peripheral->handleConnect(status, events);
}

void
Peripheral::handleConnect(int status, int events)
{
  if (status == 0) {
    if (connectionCallback.IsEmpty()) {
      // Emit a 'connect' event, with this as sole arg
      const int argc = 2;
      Local<Value> argv[argc] = { String::New("connect"), Local<Value>::New(this->self) };
      MakeCallback(self, "emit", argc, argv);
    } else {
      // Call the provided callback
      const int argc = 2;
      Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(this->self) };
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
      const int argc = 2;
      Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
      Local<Value> argv[argc] = { error, Local<Value>::New(this->self) };
      connectionCallback->Call(self, argc, argv);
    }
  }
}

const char*
Peripheral::createErrorMessage(uint8_t err)
{
  const char* msg = Att::getErrorString(err);
  char buffer[128];
  if (msg == NULL) {
    sprintf(buffer, "Error code %02X", err);
    msg = buffer;
  }
  return msg;
}

void
Peripheral::sendError(struct callbackData* cd, uint8_t err, const char* error)
{
    Persistent<Function> callback = static_cast<Function*>(cd->data);
    const int argc = 2;
    const char* msg = error == NULL ? createErrorMessage(err) : error;
    Local<Object> ret = Object::New();
    ret->Set(String::New("errorCode"), Integer::New(err));
    ret->Set(String::New("errorMessage"), String::New(msg));
    Local<Value> argv[argc] = { ret, Local<Value>::New(Null()) };
    callback->Call(self, argc, argv);
    delete cd;
}

// Find Information callback
void
Peripheral::onFindInformation(uint8_t status, void* data, void* list, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Att::AttributeInfoList* infoList = (Att::AttributeInfoList*) list;
  cd->peripheral->handleFindInformation(status, *infoList, cd, error);
  delete infoList;
}

void
Peripheral::handleFindInformation(uint8_t status, Att::AttributeInfoList& list, struct callbackData* cd, const char* error)
{
  if (error) {
    sendError(cd, status, error);
  } else if (status == 0) {
    // Create the response object
    Local<Array> response = Array::New(list.size());

    Att::AttributeInfoList::iterator iter = list.begin();
    size_t index = 0;
    while (iter != list.end()) {
      response->Set(index++, getAttributeInfo(*iter));
      delete *iter;
      ++iter;
    }
    Persistent<Function> callback = static_cast<Function*>(cd->data);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), response };
    callback->Call(self,  argc, argv);
    delete cd;
  } else {
    sendError(cd, status, error);
  }
}

// FindByTypeValue callback

void
Peripheral::onFindByType(uint8_t status, void* data, void* list, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Att::HandlesInfoList* infoList = (Att::HandlesInfoList*) list;
  cd->peripheral->handleFindByType(status, *infoList, cd, error);
  delete infoList;
}

void
Peripheral::handleFindByType(uint8_t status, Att::HandlesInfoList& list, struct callbackData* cd, const char* error)
{
  if (error) {
    sendError(cd, status, error);
  } else if (status == 0) {
    // Create the response object
    Local<Array> response = Array::New(list.size());

    size_t index = 0;
    Att::HandlesInfoList::iterator iter = list.begin();
    while (iter != list.end()) {
      response->Set(index++, getHandlesInfo(*iter));
      delete *iter;
      ++iter;
    }
    Persistent<Function> callback = static_cast<Function*>(cd->data);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), response };
    callback->Call(self,  argc, argv);
    delete cd;
  } else {
    sendError(cd, status, error);
  }
}

// ReadByType callback

void
Peripheral::onReadByType(uint8_t status, void* data, void* list, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Att::AttributeDataList* dataList = (Att::AttributeDataList*) list;
  cd->peripheral->handleReadByType(status, *dataList, cd, error);
  delete dataList;
}

void
Peripheral::handleReadByType(uint8_t status, Att::AttributeDataList& list, struct callbackData* cd, const char* error)
{
  if (error) {
    sendError(cd, status, error);
  } else if (status == 0) {
    // Create the response object
    Local<Array> response = Array::New(list.size());

    size_t index = 0;
    Att::AttributeDataList::iterator iter = list.begin();
    while (iter != list.end()) {
      response->Set(index++, getAttributeData(*iter));
      delete *iter;
      ++iter;
    }
    Persistent<Function> callback = static_cast<Function*>(cd->data);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), response };
    callback->Call(self,  argc, argv);
    delete cd;
  } else {
    sendError(cd, status, error);
  }
}

// ReadByGroupType callback

void
Peripheral::onReadByGroupType(uint8_t status, void* data, void* list, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Att::GroupAttributeDataList* dataList = (Att::GroupAttributeDataList*) list;
  cd->peripheral->handleReadByGroupType(status, *dataList, cd, error);
  delete dataList;
}

void
Peripheral::handleReadByGroupType(uint8_t status, Att::GroupAttributeDataList& list, struct callbackData* cd, const char* error)
{
  if (error) {
    sendError(cd, status, error);
  } else if (status == 0) {
    // Create the response object
    Local<Array> response = Array::New(list.size());

    size_t index = 0;
    Att::GroupAttributeDataList::iterator iter = list.begin();
    while (iter != list.end()) {
      response->Set(index++, getGroupAttributeData(*iter));
      delete *iter;
      ++iter;
    }
    Persistent<Function> callback = static_cast<Function*>(cd->data);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), response };
    callback->Call(self,  argc, argv);
    delete cd;
  } else {
    sendError(cd, status, error);
  }
}

// Read attribute callback
void
Peripheral::onReadAttribute(uint8_t status, void* data, uint8_t* buf, int len, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Persistent<Function> callback = static_cast<Function*>(cd->data);
  if (status == 0 && error == NULL) {
    Buffer* buffer = Buffer::New(len);
    memcpy(Buffer::Data(buffer), buf, len);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(buffer->handle_) };
    callback->Call(cd->peripheral->self, argc, argv);
    delete cd;
  } else {
    const int argc = 2;
    const char* msg = error == NULL ? cd->peripheral->createErrorMessage(status) : error;
    Local<Value> argv[argc] = { String::New(msg), Local<Value>::New(Null()) };
    callback->Call(cd->peripheral->self, argc, argv);
  }
}

// Read notification callback
void
Peripheral::onReadNotification(uint8_t status, void* data, uint8_t* buf, int len, const char* error)
{
  struct callbackData* cd = static_cast<struct callbackData*>(data);
  Persistent<Function> callback = static_cast<Function*>(cd->data);
  if (status == 0 && error == NULL) {
    Buffer* buffer = Buffer::New(len);
    memcpy(Buffer::Data(buffer), buf, len);
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(buffer->handle_) };
    callback->Call(cd->peripheral->self,  argc, argv);
    // NOTE: We don't delete cd here because we reuse it for the notifications
  } else {
    const int argc = 2;
    const char* msg = error == NULL ? cd->peripheral->createErrorMessage(status) : error;
    Local<Value> argv[argc] = { String::New(msg), Local<Value>::New(Null()) };
    callback->Call(cd->peripheral->self, argc, argv);
  }
}

// Write callback
void
Peripheral::onWrite(void* data, const char* error)
{
  struct callbackData* cd = (struct callbackData*) data;
  if (error) {
    // Error on write. Emit error if we don't have a callback
    if (cd->data == NULL) {
      cd->peripheral->emit_error(error);
    } else {
      // Call the provided callback with the error data
      Persistent<Function> callback = static_cast<Function*>(cd->data);
      const int argc = 1;
      Local<Value> err = ErrnoException(errno, "write", error);
      Local<Value> argv[argc] = { err };
      callback->Call(cd->peripheral->self, argc, argv);
    }
  } else {
    // Successful write. If they provided a callback, call it
    if (cd->data != NULL) {
      Persistent<Function> callback = static_cast<Function*>(cd->data);
      const int argc = 1;
      Local<Value> argv[argc] = { Local<Value>::New(Null()) };
      callback->Call(cd->peripheral->self, argc, argv);
    }
  }
  delete cd;
}

// Called when we request a value be made "weak"
void
Peripheral::weak_cb(Persistent<Value> object, void* parameter)
{
  Function* callback = static_cast<Function*>(parameter);

  delete callback;

  object.Dispose();
  object.Clear();
}

// Close callback
void
Peripheral::onClose(void* data)
{
  Peripheral* peripheral = (Peripheral *) data;

  if (peripheral->closeCallback.IsEmpty()) {
    // Emit a 'close' event, with no args
    const int argc = 1;
    Local<Value> argv[argc] = { String::New("close") };
    MakeCallback(peripheral->self, "emit", argc, argv);
  } else {
    const int argc = 1;
    Local<Value> argv[argc] = { Local<Value>::New(Null()) };
    peripheral->closeCallback->Call(Context::GetCurrent()->Global(), argc, argv);
  }
}

void
Peripheral::onError(void* data, const char* error)
{
  Peripheral* peripheral = (Peripheral*) data;

  peripheral->emit_error(error);
}

// Node.js initialization
extern "C" void init(Handle<Object> exports)
{
  Peripheral::Init(exports);
  Central::Init(exports);
  HCI::Init(exports);
}

NODE_MODULE(btle, init)
