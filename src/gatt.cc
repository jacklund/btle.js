#include "gatt.h"
#include "btio.h"

Gatt::Gatt()
: sock(0), tcp(NULL), poll_handle(NULL), connectCb(NULL),
  connectData(NULL), closeCb(NULL), closeData(NULL), errorCb(NULL),
  errorData(NULL) {
}

Gatt::~Gatt() {
}

void
Gatt::onError(errorCallback cb, void* data) {
  this->errorCb = cb;
  this->errorData = data;
}

void
Gatt::connect(struct set_opts& opts, connectCallback connect, void* data) {
  this->sock = bt_io_connect(&opts);
  if (this->sock == -1)
  {
    // TODO: Throw exception
  }

  this->connectCb = connect;
  this->connectData = data;
  this->poll_handle = new uv_poll_t;
  this->poll_handle->data = this;
  uv_poll_init_socket(uv_default_loop(), this->poll_handle, this->sock);
  uv_poll_start(this->poll_handle, UV_WRITABLE, connect_cb);
}

void
Gatt::close(closeCallback cb, void* data) {
  if (this->tcp)
  {
    this->closeCb = cb;
    this->closeData = data;
    uv_close((uv_handle_t*) this->tcp, close_cb);
    this->tcp = NULL;
  }
}

void
Gatt::close_cb(uv_handle_t* handle) {
  Gatt* gatt = (Gatt*) handle->data;
  if (gatt->closeCb)
    gatt->closeCb(gatt->closeData);
}

void
Gatt::read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  Gatt* gatt = (Gatt*) stream->data;
  if (nread < 0) {
    if (gatt->errorCb) {
      uv_err_t err = uv_last_error(uv_default_loop());
      gatt->errorCb(gatt->errorData, err);
    } else {
      gatt->close(NULL, NULL);
    }
  }
}

void
Gatt::connect_cb(uv_poll_t* handle, int status, int events) {
  printf("connect_cb called, status = %d, events = %d\n", status, events);
  // Stop polling
  uv_poll_stop(handle);
  uv_close((uv_handle_t*) handle, close_cb);
  Gatt* gatt = (Gatt*) handle->data;
  if (status == 0) {

    // Convert to a TCP handle, and start reading
    int fd = handle->io_watcher.fd;
    gatt->tcp = new uv_tcp_t();
    uv_tcp_init(uv_default_loop(), gatt->tcp);
    uv_tcp_open(gatt->tcp, fd);
    gatt->tcp->data = (void*) gatt;
    uv_read_start((uv_stream_t*) gatt->tcp, alloc_cb, read_cb);
  }
  gatt->connectCb(gatt->connectData, status, events);
}

uv_buf_t Gatt::alloc_cb(uv_handle_t* handle, size_t suggested) {
  printf("alloc_cb called\n");
  return uv_buf_init(new char[suggested], suggested);
}
