#ifndef ATT_H
#define ATT_H

#include <node.h>
#include <pthread.h>
#include <map>
#include <vector>
#include <bluetooth/uuid.h>

#include "attribute.h"
#include "connection.h"

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

  typedef void (*ErrorCallback)(void* data, const char* error);
  typedef bool (*ReadAttributeCallback)(uint8_t status, void* data, Attribute* attribute, const char* error);
  typedef void (*AttributeListCallback)(uint8_t status, void* data, AttributeList* list, const char* error);

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

  typedef bool (*ReadCallback)(int status, struct readData* rd, uint8_t* buf, int len, const char* error);

  static void onRead(void* data, uint8_t* buf, int len, const char* error);
  void handleRead(void* data, uint8_t* buf, int read, const char* error);

  // Utilities
  // Set the current request atomically
  bool setCurrentRequest(opcode_t request, opcode_t response, void* data, handle_t handle, ReadCallback callback, ReadAttributeCallback attrCallback);
  bool setCurrentRequest(opcode_t request, opcode_t response, void* data, handle_t handle, const bt_uuid_t* type,
    ReadCallback callback, AttributeListCallback attrCallback);

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
  static bool onFindInfo(int status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleFindInfo(int status, struct readData* rd, uint8_t* buf, size_t len, const char* error);

  void doFindByType(handle_t startHandle, handle_t endHandle, const bt_uuid_t& type,
    const uint8_t* value, size_t vlen);
  static bool onFindByType(int status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleFindByType(int status, struct readData* rd, uint8_t* buf, int len, const char* error);

  void doReadByType(handle_t startHandle, handle_t endHandle, const bt_uuid_t& uuid);
  static bool onReadByType(int status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleReadByType(int status, struct readData* rd, uint8_t* buf, int len, const char* error);

  void doReadByGroupType(handle_t startHandle, handle_t endHandle, const bt_uuid_t& uuid);
  static bool onReadByGroupType(int status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleReadByGroupType(int status, struct readData* rd, uint8_t* buf, int len, const char* error);

  static bool onReadAttribute(int status, struct readData* rd, uint8_t* buf, int len, const char* error);
  bool handleReadAttribute(int status, struct readData* rd, uint8_t* buf, int len, const char* error);

  static bool onNotification(int status, struct readData* rd, uint8_t* buf, int len, const char* error);

  static void parseAttributeList(AttributeList& list, uint8_t* buf, int len);
  static void parseHandlesInformationList(AttributeList& list, const bt_uuid_t& type, uint8_t* buf, int len);
  static void parseAttributeDataList(AttributeList& list, const bt_uuid_t& type, uint8_t* buf, int len);
  static void parseGroupAttributeDataList(AttributeList& list, const bt_uuid_t& type, uint8_t* buf, int len);

  // Internal data
  Connection* connection;  // Bluetooth connection

  // Error handler
  ErrorCallback errorHandler;
  void* errorData;

  // Current outstanding request
  struct readData* currentRequest;

  ReadAttributeCallback readCallback;
  void* callbackData;

  AttributeList* attributeList;

  // Map of handle => callback
  typedef std::map<handle_t, readData*> NotificationMap;
  NotificationMap notificationMap;
  pthread_mutex_t notificationMapLock; // Associated lock
};

#endif
