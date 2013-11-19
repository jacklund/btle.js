#include <errno.h>
#include <node_buffer.h>

#include "serverInterface.h"
#include "btio.h"
#include "util.h"

using namespace v8;
using namespace node;

// Constructor definition
Persistent<Function>
ServerInterface::constructor;

ServerInterface::ServerInterface()
: server(new Server())
{
}

ServerInterface::~ServerInterface()
{
  delete server;
}

void
ServerInterface::Init(Handle<Object> exports)
{
  Local<FunctionTemplate> t = FunctionTemplate::New(ServerInterface::New);
  t->InstanceTemplate()->SetInternalFieldCount(2);
  t->SetClassName(String::New("ServerInterface"));
  NODE_SET_PROTOTYPE_METHOD(t, "listen", ServerInterface::Listen);
  NODE_SET_PROTOTYPE_METHOD(t, "write", ServerInterface::Write);

  exports->Set(String::NewSymbol("Server"), t->GetFunction());
}

Handle<Value>
ServerInterface::New(const Arguments& args)
{
  HandleScope scope;

  assert(args.IsConstructCall());

  ServerInterface* server = new ServerInterface();
  server->self = Persistent<Object>::New(args.This());
  server->Wrap(args.This());

  return scope.Close(args.This());
}

Handle<Value>
ServerInterface::Listen(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsObject()) {
    ThrowException(Exception::TypeError(String::New("First argument must be either listen options or a callback")));
    return scope.Close(Undefined());
  }

  Local<Object> options;
  ServerInterface* serverInterface = ObjectWrap::Unwrap<ServerInterface>(args.This());

  if (args[0]->IsFunction()) {
    serverInterface->connectionCallback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
    options = Object::New();
  } else {
    if (args.Length() < 2) {
      ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
      return scope.Close(Undefined());
    }
    if (!args[1]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
      return scope.Close(Undefined());
    }
    options = args[0]->ToObject();
    serverInterface->connectionCallback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  }

  struct set_opts opts;
  if (!setOpts(opts, options)) {
    return scope.Close(Undefined());
  }

  serverInterface->server->listen(opts, onConnect, (void*)serverInterface);

  return scope.Close(Undefined());
}

void
ServerInterface::onConnect(void* data, int status, int events)
{
  ServerInterface* serverInterface = static_cast<ServerInterface*>(data);
  if (status == 0) {
    serverInterface->server->setReadCallback(onRead, (void*) serverInterface);
    serverInterface->server->setErrorCallback(onError, (void*) serverInterface);
    serverInterface->server->setCloseCallback(onClose, (void*) serverInterface);
    // Call the provided callback
    const int argc = 1;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(serverInterface->self) };
    serverInterface->connectionCallback->Call(serverInterface->self, argc, argv);
  } else {
    // Call the provided callback with the error
    uv_err_t err = uv_last_error(uv_default_loop());
    const int argc = 2;
    Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
    Local<Value> argv[argc] = { error, Local<Value>::New(serverInterface->self) };
    serverInterface->connectionCallback->Call(serverInterface->self, argc, argv);
  }
}

void
ServerInterface::onRead(void* data, uint8_t* buf, int len, const char* error)
{
  // Emit 'data' event with Buffer containing data
  ServerInterface* serverInterface = static_cast<ServerInterface*>(data);
  Buffer* buffer = Buffer::New(len);
  memcpy(Buffer::Data(buffer), buf, len);
  const int argc = 2;
  Local<Value> argv[argc] = { String::New("data"), Local<Value>::New(buffer->handle_) };
  MakeCallback(serverInterface->self, "emit", argc, argv);
}

void
ServerInterface::onClose(void* data)
{
  ServerInterface* serverInterface = static_cast<ServerInterface*>(data);
  const int argc = 1;
  Local<Value> argv[argc] = { String::New("close") };
  MakeCallback(serverInterface->self, "emit", argc, argv);
}

void
ServerInterface::onError(void* data, const char* error)
{
  ServerInterface* serverInterface = static_cast<ServerInterface*>(data);
  const int argc = 2;
  Local<Value> argv[argc] = { String::New("error"), String::New(error) };
  MakeCallback(serverInterface->self, "emit", argc, argv);
}

struct WriteData {
  WriteData() : serverInterface(NULL) {}
  ServerInterface* serverInterface;
  Persistent<Function> callback;
};

Handle<Value>
ServerInterface::Write(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!Buffer::HasInstance(args[0])) {
    ThrowException(Exception::TypeError(String::New("First argument must be a Buffer")));
    return scope.Close(Undefined());
  }

  ServerInterface* serverInterface = ObjectWrap::Unwrap<ServerInterface>(args.This());

  struct WriteData* wd = new struct WriteData();
  wd->serverInterface = serverInterface;

  if (args.Length() > 1) {
    if (!args[1]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
      return scope.Close(Undefined());
    }
    wd->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  }

  serverInterface->server->write((char*) Buffer::Data(args[0]), Buffer::Length(args[0]),
      onWrite, (void*) wd);

  return scope.Close(Undefined());
}

void
ServerInterface::onWrite(void* data, const char* error)
{
  struct WriteData* wd = static_cast<struct WriteData*>(data);
  if (error) {
    if (wd->callback.IsEmpty()) {
      // Emit an 'error' event
      const int argc = 2;
      Local<Value> argv[argc] = { String::New("error"), String::New(error) };
      MakeCallback(wd->serverInterface->self, "emit", argc, argv);
    } else {
      const int argc = 1;
      Local<Value> argv[argc] = { String::New(error) };
      wd->callback->Call(wd->serverInterface->self, argc, argv);
    }
  } else {
    if (!wd->callback.IsEmpty()) {
      const int argc = 1;
      Local<Value> argv[argc] = { Local<Value>::New(Null()) };
      wd->callback->Call(wd->serverInterface->self, argc, argv);
    }
  }
}
