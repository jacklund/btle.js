#include <errno.h>

#include "gatt.h"
#include "btio.h"
#include "gattException.h"
#include "util.h"

// Guard class for mutexes
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

// Struct for write callbacks
struct Gatt::writeData
{
  writeData() : data(NULL), callback(NULL) {}

  void* data;
  writeCallback callback;
};

// Struct for read callbacks
struct Gatt::readData {
  readData() : data(NULL), handle(0), callback(NULL) {}

  void* data;
  uint16_t handle;
  readCallback callback;
};

// Encode a Bluetooth LE packet
// Arguments:
//  opcode - the opcode for the operation
//  handle - the handle for the attribute to operate on
//  buffer - the buffer to put the packet data into
//  buflen - the length of buffer
//  value  - the optional value for the operation
//  vlen   - the length of the value
size_t
Gatt::encode(uint8_t opcode, uint16_t handle, uint8_t* buffer, size_t buflen,
    const uint8_t* value, size_t vlen)
{
  // The minimum packet length
	size_t ret = sizeof(buffer[0]) + sizeof(handle);

  // Validating the buffer
  if (buffer == NULL)
    return 0;

  if (buflen < ret)
    return 0;

  if (vlen > 0) {
    // If we have a value, but the buffer is too small
    // for it, we have to truncate the value, according to
    // the spec
    if (vlen > buflen - ret)
      vlen = buflen - ret;
  }

  // Write the opcode and handle
	buffer[0] = opcode;
	att_put_u16(handle, &buffer[1]);

  // Write the value
	if (vlen > 0) {
		memcpy(&buffer[3], value, vlen);
		ret += vlen;
	}

  // Return the actual length of the data
	return ret;
}

// Constructor
Gatt::Gatt()
: sock(0), tcp(NULL), poll_handle(NULL), imtu(0), cid(0),
  connectCb(NULL), connectData(NULL), closeCb(NULL), closeData(NULL),
  errorCb(NULL), errorData(NULL)
{
    pthread_mutex_init(&readMapLock, NULL);
}

// Destructor
Gatt::~Gatt()
{
  delete this->tcp;
  delete this->poll_handle;
  pthread_mutex_destroy(&readMapLock);
}

//
// Connect to the Bluetooth device
// Arguments:
//  opts    - Connection options
//  connect - Connect callback
//  data    - Optional callback data
//
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

//
// Close the connection
// Arguments:
//  cb   - Close callback
//  data - Optional callback data
//
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

//
// Read an attribute using the handle
// Arguments:
//  handle   - The handle
//  callback - The callback
//  data     - Optional callback data
//
void
Gatt::readAttribute(uint16_t handle, readCallback callback, void* data)
{
  // Set up the callback for the read
  struct readData* rd = new struct readData();
  rd->data = data;
  rd->callback = callback;

  // Write the callback into the map
  {
    LockGuard(this->readMapLock);
    readMap.insert(std::pair<uint8_t, struct readData*>(ATT_OP_READ_RESP, rd));
  }

  // Write to the device
  uv_write_t* req = new uv_write_t;
  uv_buf_t buf = getBuffer();
  size_t len = encode(ATT_OP_READ_REQ, handle, (uint8_t*) buf.base, buf.len);
  buf.len = len;
  uv_write(req, (uv_stream_t*) this->tcp, &buf, 1, NULL);
}

//
// Listen for notifications from the device for the given attribute (by handle)
// Arguments:
//  handle   - The handle for the attribute
//  callback - Callback for the notifications
//  data     - Optional callback data
//
void
Gatt::listenForNotifications(uint16_t handle, readCallback callback, void* data)
{
  // Set up the read callback
  struct readData* rd = new struct readData();
  rd->data = data;
  rd->handle = handle;
  rd->callback = callback;
  {
    LockGuard(this->readMapLock);
    readMap.insert(std::pair<uint8_t, struct readData*>(ATT_OP_HANDLE_NOTIFY, rd));
  }
}

//
// Write an attribute value to the device with no response expected
// Arguments:
//  handle   - The handle for the attribute
//  data     - The data to write into the attribute
//  length   - The size of the data
//  callback - The callback called when the write completes
//  cbData   - Optional callback data
//
void
Gatt::writeCommand(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback, void* cbData)
{
  // Create the write request
  uv_write_t* req = new uv_write_t;

  // Set up the callback
  if (callback != NULL) {
    struct writeData* wd = new struct writeData();
    wd->data = cbData;
    wd->callback = callback;
    req->data = wd;
  }

  // Do the write
  uv_buf_t buf = getBuffer();
  size_t len = encode(ATT_OP_WRITE_CMD, handle, (uint8_t*) buf.base, buf.len, data, length);
  buf.len = len;
  uv_write(req, (uv_stream_t*) this->tcp, &buf, 1, callback == NULL ? NULL : onWrite);
}

//
// Write an attribute value to the device with response
// Arguments:
//  handle   - The handle for the attribute
//  data     - The data to write into the attribute
//  length   - The size of the data
//  callback - The callback called when the write completes
//  cbData   - Optional callback data
//
void
Gatt::writeRequest(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback, void* cbData)
{
  // Set up the read callback for the response
  struct readData* rd = new struct readData();
  {
    LockGuard(this->readMapLock);
    readMap.insert(std::pair<uint8_t, struct readData*>(ATT_OP_WRITE_RESP, rd));
  }

  // Set up the write request and callback
  uv_write_t* req = new uv_write_t;
  if (callback != NULL) {
    struct writeData* wd = new struct writeData();
    wd->data = cbData;
    wd->callback = callback;
    req->data = wd;
  }

  // Perform the write
  uv_buf_t buf = getBuffer();
  size_t len = encode(ATT_OP_WRITE_REQ, handle, (uint8_t*) buf.base, buf.len, data, length);
  buf.len = len;
  uv_write(req, (uv_stream_t*) this->tcp, &buf, 1, callback == NULL ? NULL : onWrite);
}

//
// Internal Callbacks, mostly just call the passed-in callbacks
//

//
// Write callback
//
void
Gatt::onWrite(uv_write_t* req, int status)
{
  struct writeData* wd = (struct writeData*) req->data;
  if (wd->callback) wd->callback(wd->data, status);
}

//
// Close callback
//
void
Gatt::onClose(uv_handle_t* handle)
{
  Gatt* gatt = (Gatt*) handle->data;
  if (gatt->closeCb)
    gatt->closeCb(gatt->closeData);
}

//
// Read callback
//
void
Gatt::onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf)
{
  Gatt* gatt = (Gatt*) stream->data;

  // nread < 0 signals an error
  if (nread < 0) {
    if (gatt->errorCb) {
      // We call the callback and let it decide whether to close the
      // connection
      uv_err_t err = uv_last_error(uv_default_loop());
      gatt->errorCb(gatt->errorData, err);
    } else {
      // If no error callback, close the connection on error
      gatt->close(NULL, NULL);
      // TODO: Throw an exception?
    }
  } else {
    uint8_t opcode = buf.base[0];

    // Error opcode means there was an error on the device side
    if (opcode == ATT_OP_ERROR) {
      // TODO: Handle error
      printf("Got error on handle %x: %x\n", *(uint16_t*) &buf.base[2], *(uint8_t*) &buf.base[4]);
    } else {

      // Not an error - we look up the opcode in our read map
      // to see what to do with the data
      struct readData* rd = NULL;
      {
        LockGuard(gatt->readMapLock);

        ReadMap::iterator it = gatt->readMap.find(opcode);
        if (it != gatt->readMap.end()) {
          rd = it->second;
          // If this opcode only has a single response, remove
          // the entry from the map
          if (gatt->isSingleResponse(opcode)) gatt->readMap.erase(it);
        }
      }

      // If it's found, call the callback
      if (rd) {
        if (rd->callback) {
          // TODO: This should probably be a map of handle => callback
          // so we can handle multiple streams of reads
          if (rd->handle != 0) {
            uint16_t handle = *(uint16_t*)(&buf.base[1]);
            if (handle == rd->handle) {
              rd->callback(rd->data, (uint8_t*) buf.base, nread);
            } else {
              // Not the right handle? We just drop it.
            }
          } else {
            // No handle? Just call the callback then
            rd->callback(rd->data, (uint8_t*) buf.base, nread);
          }
        }
        // If this opcode only has a single response, delete the
        // callback data
        if (gatt->isSingleResponse(opcode)) delete rd;
      } else {
        // TODO: Figure out how to handle if we can't find it
      }
    }
  }

  delete buf.base;
}

//
// Connect callback
//
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

//
// Utilities
//

//
// Check if this opcode yields only a single response
//
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

//
// Get a buffer of the right size to send to device
//
uv_buf_t
Gatt::getBuffer()
{
  size_t bufSize = (this->cid == ATT_CID) ? ATT_DEFAULT_LE_MTU : this->imtu;
  return uv_buf_init(new char[bufSize], bufSize);
}

//
// libuv allocation callback
//
uv_buf_t
Gatt::onAlloc(uv_handle_t* handle, size_t suggested)
{
  return uv_buf_init(new char[suggested], suggested);
}
