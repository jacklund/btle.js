#ifndef GATT_H
#define GATT_H

#include <node.h>
#include <pthread.h>
#include <map>

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

  // Constructor/Destructor
  Gatt(Connection* connection);
  virtual ~Gatt();

  // Read a bluetooth attribute
  void readAttribute(uint16_t handle, Connection::readCallback callback, void* data);

  // Write data to an attribute without expecting a response
  void writeCommand(uint16_t handle, const uint8_t* data, size_t length, Connection::writeCallback callback=NULL, void* cbData=NULL);

  // Write data to an attribute, expecting a response
  void writeRequest(uint16_t handle, const uint8_t* data, size_t length, Connection::writeCallback callback=NULL, void* cbData=NULL);

  // Listen for incoming notifications from the device
  void listenForNotifications(uint16_t handle, Connection::readCallback callback, void* data);

  // Handle errors
  void onError(errorCallback handler, void* data) {
    errorHandler = handler;
    errorData = data;
  }

private:
  struct readData;

  static void onRead(void* data, uint8_t* buf, int len);

  // Utilities
  // Encode a bluetooth packet
  size_t encode(uint8_t opcode, uint16_t handle, uint8_t* buffer, size_t buflen,
    const uint8_t* value = NULL, size_t vlen = 0);

  // Tells if this command has a single response, or an indefinite number
  bool isSingleResponse(uint8_t opcode);

  // Internal data
  Connection* connection;  // Bluetooth connection

  // Error handler
  errorCallback errorHandler;
  void* errorData;

  // Map of opcode => callback
  typedef uint8_t opcode_t;
  typedef std::map<opcode_t, readData*> ReadMap;
  ReadMap readMap;
  pthread_mutex_t readMapLock; // Associated lock

  // Map of handle => callback
  typedef uint16_t handle_t;
  typedef std::map<handle_t, readData*> NotificationMap;
  NotificationMap notificationMap;
  pthread_mutex_t notificationMapLock; // Associated lock
};

#endif
