#include <errno.h>
#include <node_buffer.h>

#include "btleException.h"
#include "central.h"
#include "debug.h"
#include "btio.h"
#include "util.h"

using namespace v8;
using namespace node;

// Constructor definition
Persistent<Function>
Central::constructor;

Central::Central()
: sock(-1), cid(0), mtu(0), poll_handle(NULL), tcp(NULL)
{
  memset(&this->src, 0, sizeof(this->src));
  memset(&this->dst, 0, sizeof(this->dst));
}

Central::~Central()
{
}

void
Central::Init(Handle<Object> exports)
{
  Local<FunctionTemplate> t = FunctionTemplate::New(Central::New);
  t->InstanceTemplate()->SetInternalFieldCount(2);
  t->SetClassName(String::New("CentralInterface"));
  NODE_SET_PROTOTYPE_METHOD(t, "listen", Central::Listen);
  NODE_SET_PROTOTYPE_METHOD(t, "write", Central::Write);
  NODE_SET_PROTOTYPE_METHOD(t, "getMTU", Central::GetMTU);
  NODE_SET_PROTOTYPE_METHOD(t, "setMTU", Central::SetMTU);
  NODE_SET_PROTOTYPE_METHOD(t, "close", Central::Close);

  exports->Set(String::NewSymbol("CentralInterface"), t->GetFunction());
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
Central::GetMTU(const Arguments& args)
{
  HandleScope scope;

  Central* central = ObjectWrap::Unwrap<Central>(args.This());
  Local<Number> mtu = Number::New(central->mtu);

  return scope.Close(mtu);
}

Handle<Value>
Central::SetMTU(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() == 0) {
    ThrowException(Exception::TypeError(String::New("No argument provided")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsNumber()) {
    ThrowException(Exception::TypeError(String::New("Argument must be a number")));
    return scope.Close(Undefined());
  }

  Central* central = ObjectWrap::Unwrap<Central>(args.This());
  central->mtu = args[0]->IntegerValue();

  return scope.Close(Undefined());
}

//
// Listen method. On successful connection, initiates reading from the
// socket.
// 
// Arguments:
//   options (Object, optional): listen options
//   callback (Function, optional): callback to call on connect
// Events:
//   'connect': emitted on connection if no callback is specified
//   'error': emitted on an error if no callback is specified
//   'data': emitted when data is read from the socket
//
Handle<Value>
Central::Listen(const Arguments& args)
{
  HandleScope scope;
  if (debug) printf("Central::Listen\n");

  // Grab the object
  Central* central = ObjectWrap::Unwrap<Central>(args.This());

  // Parse the arguments
  Local<Object> options;
  if (args.Length() > 0) {
    if (args[0]->IsFunction()) {
      central->connectionCallback = Persistent<Function>::New(Local<Function>::Cast(args[0]));
      options = Object::New();
    } else if (args[0]->IsObject()) {
      options = args[0]->ToObject();
      if (args.Length() > 1) {
        if (!args[1]->IsFunction()) {
          ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
          return scope.Close(Undefined());
        }
        central->connectionCallback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
      }
    } else {
      ThrowException(Exception::TypeError(String::New("First argument must be either listen options or a callback")));
      return scope.Close(Undefined());
    }
  }

  // Set the listen options
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

  // Start polling for incoming connection attempts
  central->poll_handle = new uv_poll_t;
  central->poll_handle->data = central;
  uv_poll_init_socket(uv_default_loop(), central->poll_handle, central->sock);
  uv_poll_start(central->poll_handle, UV_READABLE, onConnect);

  if (debug) printf("Central::Listen returning\n");
  return scope.Close(Undefined());
}

//
// Handler for connections
//
void
Central::onConnect(uv_poll_t* handle, int status, int events)
{
  if (debug) printf("Central::onConnect, status=%d, events=%.02x\n", status, events);

  // Stop polling for connections
  uv_poll_stop(handle);
  uv_close((uv_handle_t*) handle, onClose);

  // Grab the object
  Central* central = (Central*) handle->data;

  int cli_sock = -1;
  if (status == 0) {
    // Accept the connection
    cli_sock = accept(central->sock, NULL, NULL);
    if (debug) printf("cli_sock = %d\n", cli_sock);
    if (cli_sock >= 0) {
      // Close the listen socket
      ::close(central->sock);
      central->sock = -1;

      // Get some of the connect parameters
      bt_io_get(cli_sock,
        BT_IO_OPT_SOURCE_BDADDR, &central->src,
        BT_IO_OPT_DEST, &central->dst,
        BT_IO_OPT_CID, &central->cid,
        BT_IO_OPT_IMTU, &central->mtu,
        BT_IO_OPT_INVALID);
      
      // Wrap the socket in uv's tcp
      if (debug) printf("dst = %s, cid = %d, mtu = %d\n", central->dst, central->cid, central->mtu);
      central->sock = cli_sock;
      central->tcp = new uv_tcp_t();
      uv_tcp_init(uv_default_loop(), central->tcp);
      uv_tcp_open(central->tcp, central->sock);
      central->tcp->data = (void*) central;

      // Start reading from the socket (really, polling for read)
      uv_read_start((uv_stream_t*) central->tcp, onAlloc, onRead);

      if (central->connectionCallback.IsEmpty()) {
        // No callback - emit 'connect'
        const int argc = 2;
        Local<Value> argv[argc] = { String::New("connect"), Local<Value>::New(central->self) };
        MakeCallback(central->self, "emit", argc, argv);
      } else {
        // Call the provided callback
        const int argc = 2;
        Local<Value> argv[argc] = { Local<Value>::New(Null()), Local<Value>::New(central->self) };
        central->connectionCallback->Call(central->self, argc, argv);
      }
    }
  }
  if (status != 0 || cli_sock < 0) {
    // Close the listen socket
    ::close(central->sock);
    central->sock = -1;

    // Get the error
    uv_err_t err = uv_last_error(uv_default_loop());
    const char* errString = status != 0 ? "connect" : "accept";
    Local<Value> error = ErrnoException(errno, errString, uv_strerror(err));
    if (central->connectionCallback.IsEmpty()) {
      // No callback - emit 'error'
      const int argc = 2;
      Local<Value> argv[argc] = { String::New("error"), error };
      MakeCallback(central->self, "emit", argc, argv);
    } else {
      // Call the provided callback with the error
      const int argc = 2;
      Local<Value> argv[argc] = { error, Local<Value>::New(central->self) };
      central->connectionCallback->Call(central->self, argc, argv);
    }
  }
  if (debug) printf("Central::onConnect returning, tcp = %p\n", central->tcp);
}

Handle<Value>
Central::Close(const Arguments& args)
{
  HandleScope scope;
  if (debug) printf("Central::Close\n");

  // Grab the object
  Central* central = ObjectWrap::Unwrap<Central>(args.This());

  central->close();

  if (debug) printf("Central::Close returning\n");
  return scope.Close(Undefined());
}

void
Central::close()
{
  if (this->tcp) {
    if (debug) printf("Closing bluetooth connection\n");
    uv_close((uv_handle_t*) this->tcp, onClose);
  }
  if (this->poll_handle) {
    if (debug) printf("Closing poll handle\n");
    uv_close((uv_handle_t*) this->poll_handle, onClose);
  }
}

//
// libuv allocation callback
//
uv_buf_t
Central::onAlloc(uv_handle_t* handle, size_t suggested)
{
  if (debug) printf("Central::onAlloc\n");
  return uv_buf_init(new char[suggested], suggested);
}

void
Central::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
  if (debug) printf("Central::onRead, nread=%d\n", nread);
  // Emit 'data' event with Buffer containing data
  Central* central = static_cast<Central*>(stream->data);
  // nread < 0 signals an error
  if (nread < 0) {
    uv_read_stop(stream);
    uv_err_t err = uv_last_error(uv_default_loop());
    printf("err code = %d, sys_errno_ = %d\n", err.code, err.sys_errno_);
    if (err.code == UV_ECONNRESET) {
      const int argc = 1;
      Local<Value> argv[argc] = { String::New("remoteClose") };
      MakeCallback(central->self, "emit", argc, argv);
      central->close();
    } else {
      Local<Value> error = ErrnoException(errno, "read", uv_strerror(err));
      const int argc = 2;
      Local<Value> argv[argc] = { String::New("error"), error };
      MakeCallback(central->self, "emit", argc, argv);
    }
  } else if (nread > 0) {
    Buffer* buffer = Buffer::New(nread);
    memcpy(Buffer::Data(buffer), buf.base, nread);
    const int argc = 2;
    Local<Value> argv[argc] = { String::New("data"), Local<Value>::New(buffer->handle_) };
    MakeCallback(central->self, "emit", argc, argv);
  }
  if (debug) printf("Central::onRead returning\n");
}

void
Central::onClose(uv_handle_t* handle)
{
  if (debug) printf("Central::onClose\n");
  Central* central = static_cast<Central*>(handle->data);
  if (((uv_tcp_t*) handle) == central->tcp) {
    if (debug) printf("Bluetooth handle closed\n");
    delete central->tcp;
    central->tcp = NULL;
    const int argc = 1;
    Local<Value> argv[argc] = { String::New("close") };
    MakeCallback(central->self, "emit", argc, argv);
  } else if (((uv_poll_t*) handle) == central->poll_handle) {
    if (debug) printf("Poll handle closed\n");
    delete central->poll_handle;
    central->poll_handle = NULL;
  }
  if (debug) printf("Central::onClose returning\n");
}

struct WriteData {
  WriteData() : central(NULL) {}
  Central* central;
  Persistent<Function> callback;
};

Handle<Value>
Central::Write(const Arguments& args)
{
  HandleScope scope;

  if (debug) printf("Central::Write\n");
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

  if (args.Length() > 1) {
    if (!args[1]->IsFunction()) {
      ThrowException(Exception::TypeError(String::New("Second argument must be a callback")));
      return scope.Close(Undefined());
    }
    wd->callback = Persistent<Function>::New(Local<Function>::Cast(args[1]));
  }

  char* data = Buffer::Data(args[0]);
  size_t len = Buffer::Length(args[0]);
  if (len > central->mtu) {
    char buffer[256];
    sprintf(buffer, "Data length of %d is greater than MTU value of %d", len, central->mtu);
    ThrowException(Exception::TypeError(String::New(buffer)));
  }
  central->write(data, len, wd);

  if (debug) printf("Central::Write returning\n");
  return scope.Close(Undefined());
}

void
Central::write(char* data, size_t len, void* wdData)
{
  struct WriteData* wd = static_cast<struct WriteData*>(wdData);
  if (wd == NULL) {
    wd = new struct WriteData();
    wd->central = this;
  }
  uv_buf_t buf = uv_buf_init(data, len);
  uv_write_t* req = new uv_write_t();
  req->data = wd;

  if (debug) {
    printf("Central::write: ");
    printBuffer(data, len);
    printf("\n");
  }
  uv_write(req, getStream(), &buf, 1, onWrite);
}

void
Central::onWrite(uv_write_t* req, int status)
{
  if (debug) printf("Central::onWrite, status = %d\n", status);
  struct WriteData* wd = (struct WriteData*) req->data;
  if (wd->callback.IsEmpty()) {
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
  delete req;
  delete wd;
  if (debug) printf("Central::onWrite returning\n");
}
