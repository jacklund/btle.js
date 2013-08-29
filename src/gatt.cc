#include <errno.h>

#include "gatt.h"
#include "btio.h"
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

// Struct for read callbacks
struct Gatt::readData {
  readData() : data(NULL), handle(0), callback(NULL) {}

  void* data;
  uint16_t handle;
  Connection::readCallback callback;
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
Gatt::Gatt(Connection* conn)
  : connection(conn)
{
  conn->registerReadCallback(onRead, static_cast<void*>(this));
  pthread_mutex_init(&readMapLock, NULL);
}

// Destructor
Gatt::~Gatt()
{
  pthread_mutex_destroy(&readMapLock);
}

//
// Read an attribute using the handle
// Arguments:
//  handle   - The handle
//  callback - The callback
//  data     - Optional callback data
//
void
Gatt::readAttribute(uint16_t handle, Connection::readCallback callback, void* data)
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
  uv_buf_t buf = connection->getBuffer();
  size_t len = encode(ATT_OP_READ_REQ, handle, (uint8_t*) buf.base, buf.len);
  buf.len = len;
  connection->write(buf);
}

//
// Listen for notifications from the device for the given attribute (by handle)
// Arguments:
//  handle   - The handle for the attribute
//  callback - Callback for the notifications
//  data     - Optional callback data
//
void
Gatt::listenForNotifications(uint16_t handle, Connection::readCallback callback, void* data)
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
Gatt::writeCommand(uint16_t handle, const uint8_t* data, size_t length, Connection::writeCallback callback, void* cbData)
{
  // Do the write
  uv_buf_t buf = connection->getBuffer();
  size_t len = encode(ATT_OP_WRITE_CMD, handle, (uint8_t*) buf.base, buf.len, data, length);
  buf.len = len;
  connection->write(buf, callback, cbData);
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
Gatt::writeRequest(uint16_t handle, const uint8_t* data, size_t length, Connection::writeCallback callback, void* cbData)
{
  // Perform the write
  uv_buf_t buf = connection->getBuffer();
  size_t len = encode(ATT_OP_WRITE_REQ, handle, (uint8_t*) buf.base, buf.len, data, length);
  buf.len = len;
  connection->write(buf, callback, cbData);
}

//
// Internal Callbacks, mostly just call the passed-in callbacks
//

//
// Read callback
//
void
Gatt::onRead(void* data, uint8_t* buf, int nread)
{
  Gatt* gatt = (Gatt*) data;

  uint8_t opcode = buf[0];

  // Error opcode means there was an error on the device side
  if (opcode == ATT_OP_ERROR) {
    // TODO: Handle error
    printf("Got error on handle %x: %x\n", *(uint16_t*) &buf[2], *(uint8_t*) &buf[4]);
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
          uint16_t handle = *(uint16_t*)(&buf[1]);
          if (handle == rd->handle) {
            rd->callback(rd->data, (uint8_t*) buf, nread);
          } else {
            // Not the right handle? We just drop it.
          }
        } else {
          // No handle? Just call the callback then
          rd->callback(rd->data, (uint8_t*) buf, nread);
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
