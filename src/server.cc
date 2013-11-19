#include <errno.h>

#include "btio.h"
#include "btleException.h"
#include "server.h"

// Struct for write callbacks
struct writeData
{
  writeData() : server(NULL), data(NULL), callback(NULL), buffer(NULL) {}

  Server* server;
  void* data;
  Server::WriteCallback callback;
  char* buffer;
};

Server::Server()
: sock(-1), poll_handle(NULL), tcp(NULL), connectCallback(NULL), connectData(NULL),
  readCallback(NULL), readData(NULL), closeCallback(NULL), closeData(NULL),
  errorCallback(NULL), errorData(NULL)
{}

Server::~Server()
{
  shutdown();
}

void
Server::setReadCallback(ReadCallback callback, void* data)
{
  this->readCallback = callback;
  this->readData = data;
}

void
Server::setCloseCallback(CloseCallback callback, void* data)
{
  this->closeCallback = callback;
  this->closeData = data;
}

void
Server::setErrorCallback(ErrorCallback callback, void* data)
{
  this->errorCallback = callback;
  this->errorData = data;
}

void
Server::listen(struct set_opts& opts, ConnectCallback callback, void* data)
{
  // Listen
  this->sock = bt_io_listen(&opts);
  if (this->sock == -1) {
    // Throw exception
    throw BTLEException("Error creating socket", errno);
  }

  this->connectCallback = callback;
  this->connectData = data;
  this->poll_handle = new uv_poll_t;
  this->poll_handle->data = this;
  uv_poll_init_socket(uv_default_loop(), this->poll_handle, this->sock);
  uv_poll_start(this->poll_handle, UV_READABLE, onConnect);
}

void
Server::onConnect(uv_poll_t* handle, int status, int events)
{
  // Need to do this, otherwise onClose will crash
  Server* server = (Server*) handle->data;
  server->stopPolling();
  handle->data = NULL;
  uv_close((uv_handle_t*) handle, onClose);
  if (status == 0) {
    // Convert the socket to a TCP handle, and start reading
    server->tcp = new uv_tcp_t();
    uv_tcp_init(uv_default_loop(), server->tcp);
    uv_tcp_open(server->tcp, server->sock);
    server->tcp->data = (void*) server;
    uv_read_start((uv_stream_t*) server->tcp, onAlloc, onRead);
  }
  server->connectCallback(server->connectData, status, events);
}

void
Server::shutdown()
{
  stopReading();
  stopPolling();
}

void
Server::stopPolling()
{
  if (this->poll_handle) {
    uv_poll_stop(this->poll_handle);
    delete this->poll_handle;
    this->poll_handle = NULL;
  }
}

void
Server::stopReading()
{
  if (this->tcp) {
    uv_read_stop(getStream());
    delete this->tcp;
    this->tcp = NULL;
  }
}

//
// libuv allocation callback
//
uv_buf_t
Server::onAlloc(uv_handle_t* handle, size_t suggested)
{
  return uv_buf_init(new char[suggested], suggested);
}

//
// Read callback
//
void
Server::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
  Server* server = static_cast<Server*>(stream->data);

  if (server->readCallback != NULL) {
    // nread < 0 signals an error
    if (nread < 0) {
      uv_err_t err = uv_last_error(uv_default_loop());
      server->readCallback(server->readData, NULL, nread, uv_strerror(err));
    } else {
      server->readCallback(server->readData, (uint8_t*) buf.base, nread, NULL);
    }
  }

  delete [] buf.base;
  buf.base = NULL;
}

void
Server::write(char* buffer, unsigned int bufLen, WriteCallback callback, void* data)
{
  uv_buf_t buf = uv_buf_init(buffer, bufLen);
  uv_write_t* req = new uv_write_t();
  struct writeData* wd = new struct writeData();
  wd->server = this;
  wd->data = data;
  wd->callback = callback;
  wd->buffer = buf.base;
  req->data = wd;

  uv_write(req, getStream(), &buf, 1, onWrite);
}

void
Server::onWrite(uv_write_t* req, int status)
{
  struct writeData* wd = (struct writeData*) req->data;
  if (wd->callback) {
    if (status < 0) {
      uv_err_t err = uv_last_error(uv_default_loop());
      wd->callback(wd->data, uv_strerror(err));
    } else {
      wd->callback(wd->data, NULL);
    }
  }
  delete [] wd->buffer;
  delete req;
  delete wd;
}

void
Server::onClose(uv_handle_t* handle)
{
  Server* server = static_cast<Server*>(handle->data);
  if (server->closeCallback) {
    server->closeCallback(server->closeData);
  }
}
