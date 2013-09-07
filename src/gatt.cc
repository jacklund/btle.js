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
  readData() : request(0), expectedResponse(0), data(NULL), callback(NULL) {}

  opcode_t request;
  opcode_t expectedResponse;
  void* data;
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

size_t
Gatt::encode(uint8_t opcode, uint16_t startHandle, uint16_t endHandle, bt_uuid_t* uuid, uint8_t* buffer, size_t buflen)
{
  // The minimum packet length
	size_t ret = sizeof(buffer[0]) + sizeof(startHandle) + sizeof(endHandle);

  // Validating the buffer
  if (buffer == NULL)
    return 0;

  if (buflen < ret)
    return 0;

  // Write the opcode and handle
	buffer[0] = opcode;
	att_put_u16(startHandle, &buffer[1]);
  att_put_u16(endHandle, &buffer[3]);

  // Write the UUID
  if (uuid != NULL) {
    switch (uuid->type) {
      case bt_uuid_t::BT_UUID16:
        att_put_u16(uuid->value.u16, &buffer[5]);
        ret += sizeof(uuid->value.u16);
        break;

      case bt_uuid_t::BT_UUID32:
        att_put_u32(uuid->value.u32, &buffer[5]);
        ret += sizeof(uuid->value.u32);
        break;

      case bt_uuid_t::BT_UUID128:
        att_put_u128(uuid->value.u128, &buffer[5]);
        ret += sizeof(uuid->value.u128);
        break;

      default:
        return 0;
    }
  }

  // Return the actual length of the data
	return ret;
}

// Constructor
Gatt::Gatt(Connection* conn)
  : connection(conn), errorHandler(NULL), errorData(NULL), currentRequest(NULL),
    attrListData(NULL), endHandle(0)
{
  conn->registerReadCallback(onRead, static_cast<void*>(this));
  pthread_mutex_init(&notificationMapLock, NULL);
}

// Destructor
Gatt::~Gatt()
{
  pthread_mutex_destroy(&notificationMapLock);
}

bool
Gatt::setCurrentRequest(opcode_t request, opcode_t response, void* data, readCallback callback) {
  // Set up the callback for the read
  struct readData* rd = new struct readData();
  rd->request = ATT_OP_FIND_INFO_REQ;
  rd->expectedResponse = ATT_OP_FIND_INFO_RESP;
  rd->data = data;
  rd->callback = callback;

  bool ret =__sync_bool_compare_and_swap(&currentRequest, NULL, rd);
  if (!ret) delete rd;

  return ret;
}

//
// Issue a "Find Information" command
//
void
Gatt::findInformation(uint16_t startHandle, uint16_t endHandle, attributeListCallback callback, void* data)
{
  if (setCurrentRequest(ATT_OP_FIND_INFO_REQ, ATT_OP_FIND_INFO_RESP, this, onFindInfo)) {
    this->attributeList.clear();
    this->attrListCallback = callback;
    this->attrListData = data;
    this->endHandle = endHandle;

    doFindInformation(startHandle, endHandle);
  } else {
    // TODO: return error saying we already have an outstanding request
    fprintf(stderr, "Already have request pending\n");
  }
}

void
Gatt::doFindInformation(handle_t startHandle, handle_t endHandle)
{
  // Write to the device
  uv_buf_t buf = connection->getBuffer();
  size_t len = encode(ATT_OP_FIND_INFO_REQ, startHandle, endHandle, NULL, (uint8_t*) buf.base, buf.len);
  buf.len = len;
  connection->write(buf);
}

bool
Gatt::onFindInfo(uint8_t status, void* data, uint8_t* buf, int len, const char* error)
{
  Gatt* gatt = (Gatt*) data;
  return gatt->handleFindInfo(status, buf, len, error);
}

bool
Gatt::handleFindInfo(uint8_t status, uint8_t* buf, size_t len, const char* error)
{
  if (error) {
    this->attrListCallback(status, this->attrListData, this->attributeList, error);
    return true;
  } else if (status == 0) {
    parseAttributeList(this->attributeList, buf, len);
    if (this->attributeList.back().handle < endHandle) {
      doFindInformation(this->attributeList.back().handle+1, endHandle);
      return false;
    } else {
      this->attrListCallback(status, this->attrListData, this->attributeList, error);
      return true;
    }
  } else if (status == ATT_ECODE_ATTR_NOT_FOUND) {
    this->attrListCallback(0, this->attrListData, this->attributeList, error);
    return true;
  } else {
    this->attrListCallback(status, this->attrListData, this->attributeList, error);
    return true;
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
  if (setCurrentRequest(ATT_OP_READ_REQ, ATT_OP_READ_RESP, data, callback)) {
    // Write to the device
    uv_buf_t buf = connection->getBuffer();
    size_t len = encode(ATT_OP_READ_REQ, handle, (uint8_t*) buf.base, buf.len);
    buf.len = len;
    connection->write(buf);
  } else {
  }
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
Gatt::onRead(void* data, uint8_t* buf, int nread, const char* error)
{
  Gatt* gatt = (Gatt*) data;
  gatt->handleRead(data, buf, nread, error);
}

void
Gatt::handleRead(void* data, uint8_t* buf, int nread, const char* error)
{
  if (error) {
    callbackCurrentRequest(0, NULL, 0, error);
  } else {
    uint8_t opcode = buf[0];
    struct readData* rd = NULL;
    handle_t handle;

    switch (opcode) {
      case ATT_OP_ERROR:
        {
          uint8_t request = *(uint8_t*) &buf[1];
          uint8_t errorCode = *(uint8_t*) &buf[4];
          if (currentRequest != NULL && currentRequest->request == request) {
            callbackCurrentRequest(errorCode, NULL, 0, NULL);
          }
          else if (errorHandler != NULL) {
            const char* message = getErrorString(errorCode);
            char buffer[1024];
            if (message != NULL) {
              sprintf(buffer, "Error on %s for handle 0x%02X: %s",
                getOpcodeName(*(uint8_t*) &buf[1]), *(uint16_t*) &buf[2], message);
            } else {
              sprintf(buffer, "Error on %s for handle 0x%02X: 0x%02X",
                getOpcodeName(*(uint8_t*) &buf[1]), *(uint16_t*) &buf[2], errorCode);
            }
            errorHandler(errorData, buffer);
          }
        }
        break;

      case ATT_OP_HANDLE_NOTIFY:
        handle = *(handle_t*)(&buf[1]);
        {
          LockGuard(this->notificationMapLock);
          NotificationMap::iterator it = notificationMap.find(handle);
          if (it != notificationMap.end()) {
            rd = it->second;
          }
        }
        if (rd != NULL) {
          if (rd->callback != NULL) {
            // Note: Remove the opcode and handle before calling the callback
            rd->callback(0, rd->data, (uint8_t*) (&buf[3]), nread - 3, error);
          }
        } else {
          if (errorHandler != NULL) {
            char buffer[1024];
            sprintf(buffer, "Got unexpected notification for handle %x", handle);
            errorHandler(errorData, buffer);
          }
        }
        break;

      default:
        if (currentRequest != NULL) {
          // Note: Remove the opcode before calling the callback
          callbackCurrentRequest(0, (uint8_t*)(&buf[1]), nread-1, NULL);
        } else {
          if (errorHandler != NULL) {
            char buffer[1024];
            sprintf(buffer, "Got unexpected data with opcode %x\n", opcode);
            errorHandler(errorData, buffer);
          }
        }
    }
  }
}

void
Gatt::parseAttributeList(AttributeList& list, uint8_t* buf, int len)
{
  uint8_t format = buf[0];
  uint8_t* ptr = &buf[1];
  struct Attribute attribute;
  while (ptr - buf < len) {
    attribute.handle = att_get_u16(ptr);
    ptr += sizeof(handle_t);
    if (format == ATT_FIND_INFO_RESP_FMT_16BIT) {
      attribute.uuid = att_get_uuid16(ptr);
      ptr += sizeof(uint16_t);
    } else {
      attribute.uuid = att_get_uuid128(ptr);
      ptr += sizeof(uint128_t);
    }
    list.push_back(attribute);
  }
}

//
// Utilities
//

void
Gatt::callbackCurrentRequest(uint8_t status, uint8_t* buffer, size_t len, const char* error)
{
  if (currentRequest->callback != NULL) {
    bool remove = currentRequest->callback(status, currentRequest->data, buffer, len, error);
    if (remove) removeCurrentRequest();
  }
}

void
Gatt::removeCurrentRequest()
{
  struct readData* rd = __sync_lock_test_and_set(&currentRequest, NULL);
  delete rd;
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
