#include <errno.h>
#include <node_buffer.h>

#include "central.h"
#include "btio.h"
#include "util.h"

using namespace v8;
using namespace node;

// Constructor definition
Persistent<Function>
Central::constructor;

Central::Central()
{
}

Central::~Central()
{
}

void
Central::Init(Handle<Object> exports)
{
  Local<FunctionTemplate> t = FunctionTemplate::New(Central::New);
  t->InstanceTemplate()->SetInternalFieldCount(2);
  t->SetClassName(String::New("Central"));
  NODE_SET_PROTOTYPE_METHOD(t, "listen", Central::Listen);
  NODE_SET_PROTOTYPE_METHOD(t, "write", Central::Write);

  exports->Set(String::NewSymbol("Central"), t->GetFunction());
}

Handle<Value>
Central::New(const Arguments& args)
{
  HandleScope scope;

  assert(args.IsConstructCall());

  Central* central = new Central();
  central->self = Persistent<Object>::New(args.This());
  central->Wrap(args.This());

  return scope.Close(args.This());
}

Handle<Value>
Central::Listen(const Arguments& args)
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
  Central* central = ObjectWrap::Unwrap<Central>(args.This());

  if (args[0]->IsFunction()) {
    central->connectionCallback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
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
    central->connectionCallback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  }

  struct set_opts opts;
  if (!setOpts(opts, options)) {
    return scope.Close(Undefined());
  }

  // Listen
  central->sock = bt_io_listen(&opts);
  if (central->sock == -1) {
    // Throw exception
    throw BTLEException("Error creating socket", errno);
  }

  central->poll_handle = new uv_poll_t;
  central->poll_handle->data = central;
  uv_poll_init_socket(uv_default_loop(), central->poll_handle, central->sock);
  uv_poll_start(central->poll_handle, UV_READABLE, onConnect);

  return scope.Close(Undefined());
}

void
Central::onConnect(uv_poll_t* handle, int status, int events)
{
  // Need to do this, otherwise onClose will crash
  Central* central = (Central*) handle->data;
  central->stopPolling();
  handle->data = NULL;
  uv_close((uv_handle_t*) handle, onClose);
  if (status == 0) {
    // Convert the socket to a TCP handle, and start reading
    central->tcp = new uv_tcp_t();
    uv_tcp_init(uv_default_loop(), central->tcp);
    uv_tcp_open(central->tcp, central->sock);
    central->tcp->data = (void*) central;
    uv_read_start((uv_stream_t*) central->tcp, onAlloc, onRead);

    // Call the provided callback
    const int argc = 2;
    Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(central->self) };
    central->connectionCallback->Call(central->self, argc, argv);
  } else {
    // Call the provided callback with the error
    uv_err_t err = uv_last_error(uv_default_loop());
    const int argc = 2;
    Local<Value> error = ErrnoException(errno, "connect", uv_strerror(err));
    Local<Value> argv[argc] = { error, Local<Value>::New(central->self) };
    central->connectionCallback->Call(central->self, argc, argv);
  }
}

//
// libuv allocation callback
//
uv_buf_t
Central::onAlloc(uv_handle_t* handle, size_t suggested)
{
  return uv_buf_init(new char[suggested], suggested);
}

void
Central::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
  // Emit 'data' event with Buffer containing data
  Central* central = static_cast<Central*>(stream->data);
  // nread < 0 signals an error
  if (nread < 0) {
    uv_err_t err = uv_last_error(uv_default_loop());
    const int argc = 2;
    Local<Value> error = ErrnoException(errno, "read", uv_strerror(err));
    Local<Value> argv[argc] = { String::New("error"), error };
    MakeCallback(central->self, "emit", argc, argv);
  } else {
    Buffer* buffer = Buffer::New(len);
    memcpy(Buffer::Data(buffer), buf, len);
    const int argc = 2;
    Local<Value> argv[argc] = { String::New("data"), Local<Value>::New(buffer->handle_) };
    MakeCallback(central->self, "emit", argc, argv);
  }
}

void
Central::onClose(void* data)
{
  Central* central = static_cast<Central*>(data);
  const int argc = 1;
  Local<Value> argv[argc] = { String::New("close") };
  MakeCallback(central->self, "emit", argc, argv);
}

struct WriteData {
  WriteData() : central(NULL) {}
  Central* central;
  Persistent<Function> callback;
  Persistent<Buffer> buffer;
};

Handle<Value>
Central::Write(const Arguments& args)
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

  Central* central = ObjectWrap::Unwrap<Central>(args.This());

  struct WriteData* wd = new struct WriteData();
  wd->central = central;
  wd->buffer = Persistent<Buffer>::New<Local<Buffer>::Cast(args[0]);

  if (args.Length() > 1) {
    if (!args[1]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
      return scope.Close(Undefined());
    }
    wd->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  }

  uv_buf_t buf = uv_buf_init(Buffer::Data(args[0]), Buffer::Length(args[0]));
  uv_write_t* req = new uv_write_t();
  req->data = wd;

  uv_write(req, getStream(), &buf, 1, onWrite);

  return scope.Close(Undefined());
}

void
Central::onWrite(uv_write_t* req, int status)
{
  struct writeData* wd = (struct writeData*) req->data;
  if (wd->callback) {
    if (status < 0) {
      uv_err_t err = uv_last_error(uv_default_loop());
      if (wd->callback.IsEmpty()) {
        // Emit an 'error' event
        const int argc = 2;
        Local<Value> argv[argc] = { String::New("error"), String::New(uv_strerror(err)) };
        MakeCallback(wd->central->self, "emit", argc, argv);
      } else {
        const int argc = 1;
        Local<Value> argv[argc] = { String::New(uv_strerror(err)) };
        wd->callback->Call(wd->central->self, argc, argv);
      }
    } else {
      if (!wd->callback.IsEmpty()) {
        const int argc = 1;
        Local<Value> argv[argc] = { Local<Value>::New(Null()) };
        wd->callback->Call(wd->central->self, argc, argv);
      }
    }
  }
  wd->buffer.Dispose();
  delete req;
  delete wd;
}
