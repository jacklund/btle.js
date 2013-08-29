#include <errno.h>

#include "connection.h"
#include "btio.h"
#include "btleException.h"

// Struct for write callbacks
struct writeData
{
  writeData() : data(NULL), callback(NULL) {}

  void* data;
  Connection::writeCallback callback;
};

// Constructor
Connection::Connection()
: sock(0), tcp(NULL), poll_handle(NULL), imtu(0), cid(0),
  readCb(NULL), readData(NULL), errorCb(NULL), errorData(NULL)
{
}

// Destructor
Connection::~Connection()
{
  delete this->tcp;
  delete this->poll_handle;
}

// Struct for connection callbacks
struct connectData
{
  connectData() : conn(NULL), callback(NULL), data(NULL) {}
  Connection* conn;
  Connection::connectCallback callback;
  void* data;
};

//
// Connect to the Bluetooth device
// Arguments:
//  opts    - Connection options
//  connect - Connect callback
//  data    - Optional callback data
//
void
Connection::connect(struct set_opts& opts, connectCallback connect, void* data)
{
  this->sock = bt_io_connect(&opts);
  if (this->sock == -1)
  {
    // Throw exception
    throw BTLEException("Error connecting", errno);
  }

  struct connectData* cd = new struct connectData();
  cd->callback = connect;
  cd->data = data;
  cd->conn = this;
  this->poll_handle = new uv_poll_t;
  this->poll_handle->data = cd;
  uv_poll_init_socket(uv_default_loop(), this->poll_handle, this->sock);
  uv_poll_start(this->poll_handle, UV_WRITABLE, onConnect);
}

// Register a read callback
void
Connection::registerReadCallback(readCallback callback, void* cbData)
{
  this->readCb = callback;
  this->readData = cbData;
}

// Register an error callback
void
Connection::registerErrorCallback(errorCallback callback, void* cbData)
{
  this->errorCb = callback;
  this->errorData = cbData;
}

// Write to the device
void
Connection::write(uv_buf_t& buffer, writeCallback callback, void* cbData)
{
  uv_write_t* req = new uv_write_t();
  if (callback != NULL) {
    struct writeData* wd = new struct writeData();
    wd->data = cbData;
    wd->callback = callback;
    req->data = wd;
  }

  uv_write(req, getStream(), &buffer, 1, callback == NULL ? NULL : onWrite);
}

// Struct for close callbacks
struct closeData
{
  closeData() : callback(NULL), data(NULL) {}
  Connection::closeCallback callback;
  void* data;
};

//
// Close the connection
// Arguments:
//  cb   - Close callback
//  data - Optional callback data
//
void
Connection::close(closeCallback cb, void* data)
{
  if (this->tcp)
  {
    struct closeData* cd = new struct closeData();
    cd->callback = cb;
    cd->data = data;
    uv_close((uv_handle_t*) this->tcp, onClose);
    delete this->tcp;
    this->tcp = NULL;
  }
}

//
// Internal Callbacks, mostly just call the passed-in callbacks
//

//
// Write callback
//
void
Connection::onWrite(uv_write_t* req, int status)
{
  struct writeData* wd = (struct writeData*) req->data;
  if (wd->callback) wd->callback(wd->data, status);
}

//
// Close callback
//
void
Connection::onClose(uv_handle_t* handle)
{
  struct closeData* cd = static_cast<struct closeData*>(handle->data);
  if (cd && cd->callback)
    cd->callback(cd->data);
}

//
// Read callback
//
void
Connection::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
  Connection* conn = static_cast<Connection*>(stream->data);

  // nread < 0 signals an error
  if (nread < 0) {
    if (conn->errorCb) {
      // We call the callback and let it decide whether to close the
      // connection
      uv_err_t err = uv_last_error(uv_default_loop());
      conn->errorCb(conn->errorData, err);
    } else {
      // If no error callback, close the connection on error
      conn->close(NULL, NULL);
      // TODO: Throw an exception?
    }
  } else {
    if (conn->readCb != NULL) {
      conn->readCb(conn->readData, (uint8_t*) buf.base, nread);
    }
  }

  delete buf.base;
}

//
// Connect callback
//
void
Connection::onConnect(uv_poll_t* handle, int status, int events)
{
  // Stop polling
  uv_poll_stop(handle);
  struct connectData* cd = static_cast<struct connectData*>(handle->data);
  // Need to do this, otherwise onClose will crash
  handle->data = NULL;
  uv_close((uv_handle_t*) handle, onClose);
  Connection* conn = cd->conn;
  if (status == 0) {

    // Get our socket
    int fd = handle->io_watcher.fd;

    // Get the CID and MTU information
    bt_io_get(fd, BT_IO_OPT_IMTU, &conn->imtu,
        BT_IO_OPT_CID, &conn->cid, BT_IO_OPT_INVALID);

    // Convert the socket to a TCP handle, and start reading
    conn->tcp = new uv_tcp_t();
    uv_tcp_init(uv_default_loop(), conn->tcp);
    uv_tcp_open(conn->tcp, fd);
    conn->tcp->data = (void*) conn;
    uv_read_start((uv_stream_t*) conn->tcp, onAlloc, onRead);
  }
  cd->callback(cd->data, status, events);
}

//
// Get a buffer of the right size to send to device
//
uv_buf_t
Connection::getBuffer()
{
  size_t bufSize = (this->cid == ATT_CID) ? ATT_DEFAULT_LE_MTU : this->imtu;
  return uv_buf_init(new char[bufSize], bufSize);
}

//
// libuv allocation callback
//
uv_buf_t
Connection::onAlloc(uv_handle_t* handle, size_t suggested)
{
  return uv_buf_init(new char[suggested], suggested);
}
