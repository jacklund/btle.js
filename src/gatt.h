#ifndef GATT_H
#define GATT_H

#include <node.h>
#include <pthread.h>
#include <map>
#include <bluetooth/uuid.h>

#include "connection.h"

/*
 * Class which encapsulates all the GATT protocol requests. It also contains the
 * bluetooth connection to the device, although that should probably be extracted
 * into a separate class which is used by this class (so other BLE protocol classes
 * can use the same connection).
 */
class Gatt {
public:
  typedef void (*errorCallback)(void* data, const char* error);
  typedef bool (*readCallback)(uint8_t status, void* data, uint8_t* buf, int len);

  // Constructor/Destructor
  Gatt(Connection* connection);
  virtual ~Gatt();

  // Find information
  void findInformation(uint16_t startHandle, uint16_t endHandle, readCallback callback, void* data);

  // Read a bluetooth attribute
  void readAttribute(uint16_t handle, readCallback callback, void* data);

  // Write data to an attribute without expecting a response
  void writeCommand(uint16_t handle, const uint8_t* data, size_t length, Connection::writeCallback callback=NULL, void* cbData=NULL);

  // Write data to an attribute, expecting a response
  void writeRequest(uint16_t handle, const uint8_t* data, size_t length, Connection::writeCallback callback=NULL, void* cbData=NULL);

  // Listen for incoming notifications from the device
  void listenForNotifications(uint16_t handle, readCallback callback, void* data);

  // Handle errors
  void onError(errorCallback handler, void* data) {
    errorHandler = handler;
    errorData = data;
  }

private:
  struct readData;

  // Some useful typedefs
  typedef uint8_t opcode_t;
  typedef uint16_t handle_t;

  static void onRead(void* data, uint8_t* buf, int len);

  // Utilities
  // Set the current request atomically
  bool setCurrentRequest(opcode_t request, opcode_t response, void* data, readCallback callback);

  // Make the callback for the current request
  void callbackCurrentRequest(uint8_t status, uint8_t* buffer, size_t len);

  // Remove the current request atomically
  void removeCurrentRequest();

  // Encode a bluetooth packet
  size_t encode(uint8_t opcode, uint16_t handle, uint8_t* buffer, size_t buflen,
    const uint8_t* value = NULL, size_t vlen = 0);
  size_t encode(uint8_t opcode, uint16_t startHandle, uint16_t endHandle, bt_uuid_t* uuid,
    uint8_t* buffer, size_t buflen);

  // Convert a device error code to a human-readable message
  const char* getErrorString(uint8_t errorCode);

  // Convert an opcode to the name of the operation
  const char* getOpcodeName(uint8_t opcode);

  // Internal data
  Connection* connection;  // Bluetooth connection

  // Error handler
  errorCallback errorHandler;
  void* errorData;

  // Current outstanding request
  struct readData* currentRequest;

  // Map of handle => callback
  typedef std::map<handle_t, readData*> NotificationMap;
  NotificationMap notificationMap;
  pthread_mutex_t notificationMapLock; // Associated lock
};

#endif
