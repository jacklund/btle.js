#include <errno.h>
#include <node_buffer.h>

#include "btleException.h"
#include "central.h"
#include "btio.h"
#include "util.h"

using namespace v8;
using namespace node;

// Constructor definition
Persistent<Function>
Central::constructor;

Central::Central()
: sock(-1), cid(0), imtu(0), poll_handle(NULL), tcp(NULL)
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
  printf("Central::Listen\n");

  if (args.Length() < 1) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  if (!args[0]->IsObject() && !args[0]->IsNull()) {
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

  printf("Central::Listen returning\n");
  return scope.Close(Undefined());
}

void
Central::onConnect(uv_poll_t* handle, int status, int events)
{
  printf("Central::onConnect, status=%d, events=%.02x\n", status, events);
  uv_poll_stop(handle);
  uv_close((uv_handle_t*) handle, onClose);
  Central* central = (Central*) handle->data;
  if (status == 0) {
    int cli_sock = accept(central->sock, NULL, NULL);
    if (cli_sock < 0) {
      printf("Error on accept, errno = %d\n", errno);
    } else {
      printf("cli_sock = %d\n", cli_sock);
    }

    bt_io_get(cli_sock,
			BT_IO_OPT_SOURCE_BDADDR, &central->src,
			BT_IO_OPT_DEST, &central->dst,
			BT_IO_OPT_CID, &central->cid,
			BT_IO_OPT_IMTU, &central->imtu,
			BT_IO_OPT_INVALID);
    
    printf("dst = %s, cid = %d, imtu = %d\n", central->dst, central->cid, central->imtu);
    central->poll_handle = new uv_poll_t;
    central->sock = cli_sock;
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
  printf("Central::onConnect returning, tcp = %p\n", central->tcp);
}

//
// libuv allocation callback
//
uv_buf_t
Central::onAlloc(uv_handle_t* handle, size_t suggested)
{
  printf("Central::onAlloc\n");
  return uv_buf_init(new char[suggested], suggested);
}

void
Central::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
  printf("Central::onRead, nread=%d\n", nread);
  // Emit 'data' event with Buffer containing data
  Central* central = static_cast<Central*>(stream->data);
  // nread < 0 signals an error
  if (nread < 0) {
    uv_read_stop(stream);
  } else if (nread > 0) {
    Buffer* buffer = Buffer::New(nread);
    memcpy(Buffer::Data(buffer), buf.base, nread);
    const int argc = 2;
    Local<Value> argv[argc] = { String::New("data"), Local<Value>::New(buffer->handle_) };
    MakeCallback(central->self, "emit", argc, argv);
  }
  printf("Central::onRead returning\n");
}

void
Central::onClose(uv_handle_t* handle)
{
  printf("Central::onClose\n");
  Central* central = static_cast<Central*>(handle->data);
  delete handle;
  if (((uv_poll_t*) handle) == central->poll_handle) central->poll_handle = NULL;
  printf("Central::onClose returning\n");
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

  printf("Central::Write\n");
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

  uv_buf_t buf = uv_buf_init(Buffer::Data(args[0]), Buffer::Length(args[0]));
  uv_write_t* req = new uv_write_t();
  req->data = wd;

  uv_write(req, central->getStream(), &buf, 1, onWrite);

  printf("Central::Write returning\n");
  return scope.Close(Undefined());
}

void
Central::onWrite(uv_write_t* req, int status)
{
  printf("Central::onWrite, status = %d\n", status);
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
  printf("Central::onWrite returning\n");
}
