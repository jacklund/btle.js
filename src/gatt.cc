#include <errno.h>

#include "gatt.h"
#include "btio.h"
#include "gattException.h"
#include "util.h"

class LockGuard
{
public:
  LockGuard(pthread_mutex_t m) : lock(m) {
    pthread_mutex_lock(&lock);
  }

  ~LockGuard() {
    pthread_mutex_unlock(&lock);
  }

private:
  pthread_mutex_t lock;
};

struct Gatt::writeData
{
  writeData() : data(NULL), callback(NULL) {}

  void* data;
  writeCallback callback;
};

struct Gatt::readData {
  readData() : data(NULL), handle(0), callback(NULL) {}

  void* data;
  uint16_t handle;
  readCallback callback;
};

size_t
Gatt::encode(uint8_t opcode, uint16_t handle, uint8_t* buffer, size_t buflen,
    const uint8_t* value, size_t vlen)
{
	size_t ret = sizeof(buffer[0]) + sizeof(handle);

  if (vlen > 0) {
    if (buffer == NULL)
      return 0;

    if (buflen < ret)
      return 0;

    if (vlen > buflen - ret)
      vlen = buflen - ret;
  }

	buffer[0] = opcode;
	att_put_u16(handle, &buffer[1]);

	if (vlen > 0) {
		memcpy(&buffer[3], value, vlen);
		ret += vlen;
	}

	return ret;
}

Gatt::Gatt()
: sock(0), tcp(NULL), poll_handle(NULL), imtu(0), cid(0),
  connectCb(NULL), connectData(NULL), closeCb(NULL), closeData(NULL),
  errorCb(NULL), errorData(NULL)
{
    pthread_mutex_init(&readMapLock, NULL);
}

Gatt::~Gatt()
{
  delete this->tcp;
  delete this->poll_handle;
  pthread_mutex_destroy(&readMapLock);
}

void
Gatt::onError(errorCallback cb, void* data)
{
  this->errorCb = cb;
  this->errorData = data;
}

void
Gatt::connect(struct set_opts& opts, connectCallback connect, void* data)
{
  this->sock = bt_io_connect(&opts);
  if (this->sock == -1)
  {
    // Throw exception
    throw gattException("Error connecting", errno);
  }

  this->connectCb = connect;
  this->connectData = data;
  this->poll_handle = new uv_poll_t;
  this->poll_handle->data = this;
  uv_poll_init_socket(uv_default_loop(), this->poll_handle, this->sock);
  uv_poll_start(this->poll_handle, UV_WRITABLE, onConnect);
}

void
Gatt::close(closeCallback cb, void* data)
{
  if (this->tcp)
  {
    this->closeCb = cb;
    this->closeData = data;
    uv_close((uv_handle_t*) this->tcp, onClose);
    this->tcp = NULL;
  }
}

void
Gatt::readAttribute(uint16_t handle, void* data, readCallback callback)
{
  struct readData* rd = new struct readData();
  rd->data = data;
  rd->callback = callback;
  {
    LockGuard(this->readMapLock);
    readMap.insert(std::pair<uint8_t, struct readData*>(ATT_OP_READ_RESP, rd));
  }
  uv_write_t* req = new uv_write_t;
  uv_buf_t buf = getBuffer();
  size_t len = encode(ATT_OP_READ_REQ, handle, (uint8_t*) buf.base, buf.len);
  buf.len = len;
  uv_write(req, (uv_stream_t*) this->tcp, &buf, 1, NULL);
}

void
Gatt::listenForNotifications(uint16_t handle, void* data, readCallback callback)
{
  struct readData* rd = new struct readData();
  rd->data = data;
  rd->handle = handle;
  rd->callback = callback;
  {
    LockGuard(this->readMapLock);
    readMap.insert(std::pair<uint8_t, struct readData*>(ATT_OP_HANDLE_NOTIFY, rd));
  }
}

void
Gatt::writeCommand(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback, void* cbData)
{
  uv_write_t* req = new uv_write_t;
  if (callback != NULL) {
    struct writeData* wd = new struct writeData();
    wd->data = cbData;
    wd->callback = callback;
    req->data = wd;
  }
  uv_buf_t buf = getBuffer();
  size_t len = encode(ATT_OP_WRITE_CMD, handle, (uint8_t*) buf.base, buf.len, data, length);
  buf.len = len;
  uv_write(req, (uv_stream_t*) this->tcp, &buf, 1, callback == NULL ? NULL : onWrite);
}

void
Gatt::onWrite(uv_write_t* req, int status)
{
  struct writeData* wd = (struct writeData*) req->data;
  if (wd->callback) wd->callback(wd->data, status);
}

void
Gatt::writeRequest(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback, void* cbData)
{
  struct readData* rd = new struct readData();
  {
    LockGuard(this->readMapLock);
    readMap.insert(std::pair<uint8_t, struct readData*>(ATT_OP_WRITE_RESP, rd));
  }
  uv_write_t* req = new uv_write_t;
  if (callback != NULL) {
    struct writeData* wd = new struct writeData();
    wd->data = cbData;
    wd->callback = callback;
    req->data = wd;
  }
  uv_buf_t buf = getBuffer();
  size_t len = encode(ATT_OP_WRITE_REQ, handle, (uint8_t*) buf.base, buf.len, data, length);
  buf.len = len;
  uv_write(req, (uv_stream_t*) this->tcp, &buf, 1, callback == NULL ? NULL : onWrite);
}

void
Gatt::onClose(uv_handle_t* handle)
{
  Gatt* gatt = (Gatt*) handle->data;
  if (gatt->closeCb)
    gatt->closeCb(gatt->closeData);
}

bool
Gatt::isSingleResponse(uint8_t opcode)
{
  switch (opcode)
  {
    case ATT_OP_HANDLE_NOTIFY:
    case ATT_OP_HANDLE_IND:
    case ATT_OP_HANDLE_CNF:
      return false;

    default:
      return true;
  }
}

void
Gatt::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
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
    uint8_t opcode = buf.base[0];
    if (opcode == ATT_OP_ERROR) {
      // TODO: Handle error
      printf("Got error on handle %x: %x\n", *(uint16_t*) &buf.base[2], *(uint8_t*) &buf.base[4]);
    } else {
      struct readData* rd = NULL;
      {
        LockGuard(gatt->readMapLock);

        ReadMap::iterator it = gatt->readMap.find(opcode);
        if (it != gatt->readMap.end()) {
          rd = it->second;
          if (gatt->isSingleResponse(opcode)) gatt->readMap.erase(it);
        }
      }
      if (rd) {
        if (rd->callback) {
          if (rd->handle != 0) {
            uint16_t handle = *(uint16_t*)(&buf.base[1]);
            if (handle == rd->handle) {
              rd->callback(rd->data, (uint8_t*) buf.base, nread);
            } else {
            }
          } else {
            rd->callback(rd->data, (uint8_t*) buf.base, nread);
          }
        }
        if (gatt->isSingleResponse(opcode)) delete rd;
      } else {
        // TODO: Figure out how to handle if we can't find it
      }
    }
  }

  delete buf.base;
}

void
Gatt::onConnect(uv_poll_t* handle, int status, int events)
{
  // Stop polling
  uv_poll_stop(handle);
  uv_close((uv_handle_t*) handle, onClose);
  Gatt* gatt = (Gatt*) handle->data;
  if (status == 0) {

    // Get our socket
    int fd = handle->io_watcher.fd;

    // Get the CID and MTU information
    bt_io_get(fd, BT_IO_OPT_IMTU, &gatt->imtu,
        BT_IO_OPT_CID, &gatt->cid, BT_IO_OPT_INVALID);

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
Gatt::getBuffer()
{
  size_t bufSize = (this->cid == ATT_CID) ? ATT_DEFAULT_LE_MTU : this->imtu;
  return uv_buf_init(new char[bufSize], bufSize);
}

uv_buf_t
Gatt::onAlloc(uv_handle_t* handle, size_t suggested)
{
  return uv_buf_init(new char[suggested], suggested);
}
