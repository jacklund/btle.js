#ifndef ATT_H
#define ATT_H

#include <node.h>
#include <pthread.h>
#include <map>
#include <vector>
#include <bluetooth/uuid.h>

#include "connection.h"

typedef uint16_t handle_t;

/*
 * Class which encapsulates all the ATT protocol requests. It also contains the
 * bluetooth connection to the device, although that should probably be extracted
 * into a separate class which is used by this class (so other BLE protocol classes
 * can use the same connection).
 */
class Att {
public:
  static const size_t MAX_ATTR_VALUE_LENGTH = 253;

  // Some useful typedefs
  typedef uint8_t opcode_t;

  struct AttributeInfo {
    handle_t handle;
    bt_uuid_t type;
  };

  typedef std::vector<struct AttributeInfo*> AttributeInfoList;

  struct HandlesInfo {
    handle_t handle;
    handle_t groupEndHandle;
  };

  typedef std::vector<struct HandlesInfo*> HandlesInfoList;

  struct AttributeData {
    handle_t handle;
    uint8_t data[255];
    size_t length;
  };

  typedef std::vector<struct AttributeData*> AttributeDataList;

  struct GroupAttributeData {
    handle_t handle;
    handle_t groupEndHandle;
    uint8_t data[255];
    size_t length;
  };

  typedef std::vector<struct GroupAttributeData*> GroupAttributeDataList;

  typedef void (*ErrorCallback)(void* data, const char* error);
  typedef bool (*ReadAttributeCallback)(uint8_t status, void* data, uint8_t* buf, int len, const char* error);
  typedef void (*AttributeListCallback)(uint8_t status, void* data, void* list, const char* error);

  // Convert a device error code to a human-readable message
  static const char* getErrorString(uint8_t errorCode);

  // Convert an opcode to the name of the operation
  static const char* getOpcodeName(uint8_t opcode);

  // Constructor/Destructor
  Att(Connection* connection);
  virtual ~Att();

  // Find information
  void findInformation(uint16_t startHandle, uint16_t endHandle, AttributeListCallback callback, void* data);

  // Find by Type Value
  void findByTypeValue(uint16_t startHandle, uint16_t endHandle, const bt_uuid_t& type,
  const uint8_t* value, size_t vlen, AttributeListCallback callback, void* data);

  // Read by Type
  void readByType(uint16_t startHandle, uint16_t endHandle, const bt_uuid_t& uuid,
    AttributeListCallback callback, void* data);

  // Read a bluetooth attribute
  void readAttribute(uint16_t handle, ReadAttributeCallback callback, void* data);

  // Read by Group Type
  void readByGroupType(uint16_t startHandle, uint16_t endHandle, const bt_uuid_t& uuid,
    AttributeListCallback callback, void* data);

  // Write data to an attribute without expecting a response
  void writeCommand(uint16_t handle, const uint8_t* data, size_t length, Connection::WriteCallback callback=NULL, void* cbData=NULL);

  // Write data to an attribute, expecting a response
  void writeRequest(uint16_t handle, const uint8_t* data, size_t length, Connection::WriteCallback callback=NULL, void* cbData=NULL);

  // Listen for incoming notifications from the device
  void listenForNotifications(uint16_t handle, ReadAttributeCallback callback, void* data);

  // Handle errors
  void onError(ErrorCallback handler, void* data) {
    errorHandler = handler;
    errorData = data;
  }

private:
  struct readData;

  typedef bool (*ReadCallback)(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);

  static void onRead(void* data, uint8_t* buf, int len, const char* error);
  void handleRead(void* data, uint8_t* buf, int read, const char* error);

  // Utilities
  // Set the current request atomically
  bool setCurrentRequest(opcode_t request, opcode_t response, void* data, handle_t handle, ReadCallback callback, ReadAttributeCallback readAttrCb);
  bool setCurrentRequest(opcode_t request, opcode_t response, void* data, handle_t handle, const bt_uuid_t* type,
    ReadCallback callback, AttributeListCallback attrCallback, const uint8_t* value=NULL, size_t vlen=0);

  // Make the callback for the current request
  void callbackCurrentRequest(uint8_t status, uint8_t* buffer, size_t len, const char* error);

  // Remove the current request atomically
  void removeCurrentRequest();

  // Encode a bluetooth packet
  size_t encode(uint8_t opcode, uint16_t handle, uint8_t* buffer, size_t buflen,
    const uint8_t* value = NULL, size_t vlen = 0);
  size_t encode(uint8_t opcode, uint16_t startHandle, uint16_t endHandle, const bt_uuid_t* uuid,
    uint8_t* buffer, size_t buflen, const uint8_t* value = NULL, size_t vlen = 0);

  void doFindInformation(handle_t startHandle, handle_t endHandle);
  static bool onFindInfo(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleFindInfo(uint8_t status, struct readData* rd, uint8_t* buf, size_t len, const char* error);

  void doFindByType(handle_t startHandle, handle_t endHandle, const bt_uuid_t& type,
    const uint8_t* value, size_t vlen);
  static bool onFindByType(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleFindByType(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);

  void doReadByType(handle_t startHandle, handle_t endHandle, const bt_uuid_t& uuid);
  static bool onReadByType(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleReadByType(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);

  void doReadByGroupType(handle_t startHandle, handle_t endHandle, const bt_uuid_t& uuid);
  static bool onReadByGroupType(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleReadByGroupType(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);

  static bool onReadAttribute(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleReadAttribute(uint8_t status, struct readData* rd, uint8_t* buf, int len, const char* error);

  void parseAttributeList(AttributeInfoList& list, uint8_t* buf, int len);
  void parseHandlesInformationList(HandlesInfoList& list, const bt_uuid_t& type, uint8_t* buf, int len);
  void parseAttributeDataList(AttributeDataList& list, const bt_uuid_t& type, uint8_t* buf, int len);
  void parseGroupAttributeDataList(GroupAttributeDataList& list, const bt_uuid_t& type, uint8_t* buf, int len);

  // Internal data
  Connection* connection;  // Bluetooth connection

  // Error handler
  ErrorCallback errorHandler;
  void* errorData;

  // Current outstanding request
  struct readData* currentRequest;

  // Cached attribute list, used for repeated findInformation(),
  // since it may have to make multiple calls to the device
  AttributeInfoList* attributeList;
  GroupAttributeDataList* groupAttributeList;
  HandlesInfoList* handlesInfoList;

  // Map of handle => callback
  typedef std::map<handle_t, readData*> NotificationMap;
  NotificationMap notificationMap;
  pthread_mutex_t notificationMapLock; // Associated lock
};

#endif
