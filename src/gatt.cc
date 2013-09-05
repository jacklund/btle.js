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
  readData() : data(NULL), callback(NULL) {}

  void* data;
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
  : connection(conn), errorHandler(NULL), errorData(NULL)
{
  conn->registerReadCallback(onRead, static_cast<void*>(this));
  pthread_mutex_init(&readMapLock, NULL);
  pthread_mutex_init(&notificationMapLock, NULL);
}

// Destructor
Gatt::~Gatt()
{
  pthread_mutex_destroy(&readMapLock);
  pthread_mutex_destroy(&notificationMapLock);
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
  rd->callback = callback;
  {
    LockGuard(this->notificationMapLock);
    notificationMap.insert(std::pair<handle_t, struct readData*>(handle, rd));
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
  struct readData* rd = NULL;
  handle_t handle;

  switch (opcode) {
    case ATT_OP_ERROR:
      if (gatt->errorHandler != NULL) {
        uint8_t errorCode = *(uint8_t*) &buf[4];
        const char* message = gatt->getErrorString(errorCode);
        char buffer[1024];
        if (message != NULL) {
          sprintf(buffer, "Error on %s for handle 0x%02X: %s",
            gatt->getOpcodeName(*(uint8_t*) &buf[1]), *(uint16_t*) &buf[2], message);
        } else {
          sprintf(buffer, "Error on %s for handle 0x%02X: 0x%02X",
            gatt->getOpcodeName(*(uint8_t*) &buf[1]), *(uint16_t*) &buf[2], errorCode);
        }
        gatt->errorHandler(gatt->errorData, buffer);
      }
      break;

    case ATT_OP_HANDLE_NOTIFY:
      handle = *(handle_t*)(&buf[1]);
      {
        LockGuard(gatt->notificationMapLock);
        NotificationMap::iterator it = gatt->notificationMap.find(handle);
        if (it != gatt->notificationMap.end()) {
          rd = it->second;
        }
      }
      if (rd != NULL) {
        if (rd->callback != NULL) {
          // Note: Remove the opcode and handle before calling the callback
          rd->callback(rd->data, (uint8_t*) (&buf[3]), nread - 3);
        }
      } else {
        if (gatt->errorHandler != NULL) {
          char buffer[1024];
          sprintf(buffer, "Got unexpected notification for handle %x", handle);
          gatt->errorHandler(gatt->errorData, buffer);
        }
      }
      break;

    default:
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
      if (rd != NULL) {
        if (rd->callback != NULL) {
          // Note: Remove the opcode beofre calling the callback
          rd->callback(rd->data, (uint8_t*) (&buf[1]), nread - 1);
        }
      } else {
        if (gatt->errorHandler != NULL) {
          char buffer[1024];
          sprintf(buffer, "Got unexpected data with opcode %x\n", opcode);
          gatt->errorHandler(gatt->errorData, buffer);
        }
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

const char*
Gatt::getErrorString(uint8_t errorCode)
{
  switch (errorCode) {
    case ATT_ECODE_INVALID_HANDLE:
      return "Invalid handle";
    case ATT_ECODE_READ_NOT_PERM:
      return "Attribute cannot be read";
    case ATT_ECODE_WRITE_NOT_PERM:
      return "Attribute cannot be written";
    case ATT_ECODE_INVALID_PDU:
      return "Attribute PDU is invalid";
    case ATT_ECODE_AUTHENTICATION:
      return "Authentication required";
    case ATT_ECODE_REQ_NOT_SUPP:
      return "Client request not supported";
    case ATT_ECODE_INVALID_OFFSET:
      return "Offset specified was past end of attribute";
    case ATT_ECODE_AUTHORIZATION:
      return "Authorization required";
    case ATT_ECODE_PREP_QUEUE_FULL:
      return "Too many prepare writes have been queued";
    case ATT_ECODE_ATTR_NOT_FOUND:
      return "No attribute found corresponding to the given handle";
    case ATT_ECODE_ATTR_NOT_LONG:
      return "Attribute cannot be read using the Read Blob request";
    case ATT_ECODE_INSUFF_ENCR_KEY_SIZE:
      return "Encryption key size is insufficient";
    case ATT_ECODE_INVAL_ATTR_VALUE_LEN:
      return "Attribute value length was invalid for this operation";
    case ATT_ECODE_UNLIKELY:
      return "Attribute request encountered unlikely error";
    case ATT_ECODE_INSUFF_ENC:
      return "Attribute requires encryption before it can be read or written";
    case ATT_ECODE_UNSUPP_GRP_TYPE:
      return "Attribute type is not a supported grouping attribute";
    case ATT_ECODE_INSUFF_RESOURCES:
      return "Insufficient Resources to complete the request";
    default:
      return NULL;
  }
}

const char*
Gatt::getOpcodeName(uint8_t opcode)
{
  switch (opcode) {
    case ATT_OP_ERROR:
      return "error response";

    case ATT_OP_MTU_REQ:
      return "exchange MTU request";

    case ATT_OP_MTU_RESP:
      return "exchange MTU response";

    case ATT_OP_FIND_INFO_REQ:
      return "find info request";

    case ATT_OP_FIND_INFO_RESP:
      return "find info response";

    case ATT_OP_FIND_BY_TYPE_REQ:
      return "find by type request";

    case ATT_OP_FIND_BY_TYPE_RESP:
      return "find by type response";

    case ATT_OP_READ_BY_TYPE_REQ:
      return "read by type request";

    case ATT_OP_READ_BY_TYPE_RESP:
      return "read by type response";

    case ATT_OP_READ_REQ:
      return "read request";

    case ATT_OP_READ_RESP:
      return "read response";

    case ATT_OP_READ_BLOB_REQ:
      return "read blob request";

    case ATT_OP_READ_BLOB_RESP:
      return "read blob response";

    case ATT_OP_READ_MULTI_REQ:
      return "read multiple request";

    case ATT_OP_READ_MULTI_RESP:
      return "read multiple response";

    case ATT_OP_READ_BY_GROUP_REQ:
      return "read by group type request";

    case ATT_OP_READ_BY_GROUP_RESP:
      return "read by group type response";

    case ATT_OP_WRITE_REQ:
      return "write request";

    case ATT_OP_WRITE_RESP:
      return "write response";

    case ATT_OP_WRITE_CMD:
      return "write command";

    case ATT_OP_PREP_WRITE_REQ:
      return "prepare write request";

    case ATT_OP_PREP_WRITE_RESP:
      return "prepare write response";

    case ATT_OP_EXEC_WRITE_REQ:
      return "execute write request";

    case ATT_OP_EXEC_WRITE_RESP:
      return "execute write response";

    case ATT_OP_HANDLE_NOTIFY:
      return "handle value notification";

    case ATT_OP_HANDLE_IND:
      return "handle value indication";

    case ATT_OP_HANDLE_CNF:
      return "handle value confirmation";

    case ATT_OP_SIGNED_WRITE_CMD:
      return "signed write command";

    default:
        return NULL;
  }
}
