#include "gatt.h"
#include "btio.h"

class LockGuard {
public:
  LockGuard(pthread_mutex_t m) : lock(m) {
    pthread_mutex_lock(&lock);
  }

  ~LockGuard() {
    pthread_mutex_unlock(&lock);
  }

private:
  pthread_mutex_t lock;
}

static uint16_t enc_read_req(uint16_t handle, uint8_t *pdu, size_t len)
{
	const uint16_t min_len = sizeof(pdu[0]) + sizeof(handle);

	if (pdu == NULL)
		return 0;

	if (len < min_len)
		return 0;

	pdu[0] = ATT_OP_READ_REQ;
	att_put_u16(handle, &pdu[1]);

	return min_len;
}

Gatt::Gatt()
: sock(0), tcp(NULL), poll_handle(NULL), connectCb(NULL),
  connectData(NULL), closeCb(NULL), closeData(NULL), errorCb(NULL),
  errorData(NULL), readMapLock(PTHREAD_MUTEX_INITIALIZER) {
}

Gatt::~Gatt() {
  delete this->tcp;
  delete this->poll_handle;
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
  uv_poll_start(this->poll_handle, UV_WRITABLE, onConnect);
}

void
Gatt::close(closeCallback cb, void* data) {
  if (this->tcp)
  {
    this->closeCb = cb;
    this->closeData = data;
    uv_close((uv_handle_t*) this->tcp, onClose);
    this->tcp = NULL;
  }
}

void
Gatt::onWrite(uv_write_t* req, int status) {
  // TODO: handle status < 0
  delete req;
}

void
Gatt::readAttribute(uint16_t handle, void* data, readCallback callback) {
  struct readData* rd = new struct readData();
  rd->data = data;
  rd->callback = callback;
  {
    LockGuard(this->readMapLock);
    readMap.insert(std::pair<uint8_t, struct readData*>(data[0], rd);
  }
  write_req_t* req = new write_req_t;
  req->buf = getBuffer();
  size_t len = enc_read_req(handle, req->buf->base, req->buf->len);
  uv_write(req, this->tcp, &req->buf, 1, onWrite);
}

void
Gatt::onClose(uv_handle_t* handle) {
  Gatt* gatt = (Gatt*) handle->data;
  if (gatt->closeCb)
    gatt->closeCb(gatt->closeData);
}

void
Gatt::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf) {
  Gatt* gatt = (Gatt*) stream->data;
  if (nread < 0) {
    if (gatt->errorCb) {
      // We call the callback and let it decide whether to close the
      // connection
      uv_err_t err = uv_last_error(uv_default_loop());
      gatt->errorCb(gatt->errorData, err);
    } else {
      // If no error callback, close the connection on error
      gatt->close(NULL, NULL);
    }
  } else {
    LockGuard(this->readMapLock);

    ReadMap::iterator it = this->readMap.find(buf.base[0]);
    if (it != ReadMap::end) {
      struct readData* rd = it->second;
      readMap.erase(it);
      rd->callback(rd->data, buf->base, nread);
      delete rd;
    }
  }

  delete buf->base;
}

void
Gatt::onConnect(uv_poll_t* handle, int status, int events) {
  printf("onConnect called, status = %d, events = %d\n", status, events);
  // Stop polling
  uv_poll_stop(handle);
  uv_close((uv_handle_t*) handle, onClose);
  Gatt* gatt = (Gatt*) handle->data;
  if (status == 0) {

    // Get our socket
    int fd = handle->io_watcher.fd;

    // Get the CID and MTU information
    bt_io_get(fd, BT_IO_OPT_IMTU, &this->imtu,
        BT_IO_OPT_CID, &this->cid, BT_IO_OPT_INVALID);

    // Convert the socket to a TCP handle, and start reading
    gatt->tcp = new uv_tcp_t();
    uv_tcp_init(uv_default_loop(), gatt->tcp);
    uv_tcp_open(gatt->tcp, fd);
    gatt->tcp->data = (void*) gatt;
    uv_read_start((uv_stream_t*) gatt->tcp, onAlloc, onRead);
  }
  gatt->connectCb(gatt->connectData, status, events);
}

uv_buf_t
Gatt::getBuffer() {
  size_t bufSize = (this->cid == ATT_CID) ? ATT_DEFAULT_LE_MTU : this->imtu;
  return uv_buf_init(new char[bufSize], bufSize);
}

uv_buf_t
Gatt::onAlloc(uv_handle_t* handle, size_t suggested) {
  printf("alloc_cb called\n");
  return uv_buf_init(new char[suggested], suggested);
}
