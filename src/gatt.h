#ifndef GATT_H
#define GATT_H

#include <node.h>
#include <pthread.h>
#include <map>

/*
 * Class which encapsulates all the GATT protocol requests. It also contains the
 * bluetooth connection to the device, although that should probably be extracted
 * into a separate class which is used by this class (so other BLE protocol classes
 * can use the same connection).
 */
class Gatt {
public:
  // Callback types
  typedef void (*connectCallback)(void* data, int status, int events);
  typedef void (*closeCallback)(void* data);
  typedef void (*errorCallback)(void* data, uv_err_t err);
  typedef void (*readCallback)(void* data, uint8_t* buf, int len);
  typedef void (*writeCallback)(void* data, int status);

  // Constructor/Destructor
  Gatt();
  virtual ~Gatt();

  // Connect to a bluetooth device
  void connect(struct set_opts& opts, connectCallback connect, void* data);

  // Read a bluetooth attribute
  void readAttribute(uint16_t handle, readCallback callback, void* data);

  // Write data to an attribute without expecting a response
  void writeCommand(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback=NULL, void* cbData=NULL);

  // Write data to an attribute, expecting a response
  void writeRequest(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback=NULL, void* cbData=NULL);

  // Listen for incoming notifications from the device
  void listenForNotifications(uint16_t handle, readCallback callback, void* data);

  // Close the connection
  void close(closeCallback cb, void* data);

private:
  // Internal callbacks
  static void onConnect(uv_poll_t* handle, int status, int events);
  static void onClose(uv_handle_t* handle);
  static void onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static void onWrite(uv_write_t* req, int status);
  static uv_buf_t onAlloc(uv_handle_t* handle, size_t suggested);

  // Utilities
  // Construct a buffer of the correct size to talk to the device
  uv_buf_t getBuffer();

  // Encode a bluetooth packet
  size_t encode(uint8_t opcode, uint16_t handle, uint8_t* buffer, size_t buflen,
    const uint8_t* value = NULL, size_t vlen = 0);

  // Tells if this command has a single response, or an indefinite number
  bool isSingleResponse(uint8_t opcode);

  // Internal callback structures
  struct readData;
  struct writeData;

  // Internal data
  int sock;                // Socket
  uv_tcp_t* tcp;           // libuv TCP structure
  uv_poll_t* poll_handle;  // libuv poll handle
  uint16_t imtu;           // Incoming MTU size
  uint16_t cid;            // CID value for this connection

  typedef std::map<uint8_t, readData*> ReadMap;
  ReadMap readMap;             // Map of opcode => callback data
  pthread_mutex_t readMapLock; // Associated lock

  // Callbacks defined by caller
  connectCallback connectCb;
  void* connectData;
  closeCallback closeCb;
  void* closeData;
  errorCallback errorCb;
  void* errorData;
};

#endif
