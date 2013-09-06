#ifndef CONNECTION_H
#define CONNECTION_H

#include <uv.h>

/**
 * Bluetooth LE connection class. Wraps all the low-level functionality of
 * btio.{c,h} in an object.
 */
class Connection {
public:
  // Callback types
  typedef void (*connectCallback)(void* data, int status, int events);
  typedef void (*closeCallback)(void* data);
  typedef void (*errorCallback)(void* data, const char* error);
  typedef void (*readCallback)(void* data, uint8_t* buf, int len, const char* error);
  typedef void (*writeCallback)(void* data, const char* error);

  // Constructor/Destructor
  Connection();
  virtual ~Connection();

  // Connect to a bluetooth device
  void connect(struct set_opts& opts, connectCallback connect, void* data);

  // Register callbacks
  void registerReadCallback(readCallback callback, void* cbData);

  // Construct a buffer of the correct size to talk to the device
  uv_buf_t getBuffer();

  // Write to the device
  void write(uv_buf_t& buffer, writeCallback callback = NULL, void* cbData = NULL);

  // Close the connection
  void close(closeCallback cb, void* data);

private:
  // Internal callbacks
  static void onConnect(uv_poll_t* handle, int status, int events);
  static void onClose(uv_handle_t* handle);
  static void onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static void onWrite(uv_write_t* req, int status);
  static uv_buf_t onAlloc(uv_handle_t* handle, size_t suggested);

  uv_stream_t* getStream() { return (uv_stream_t*) tcp; }

  // Internal data
  int sock;                // Socket
  uv_tcp_t* tcp;           // libuv TCP structure
  uv_poll_t* poll_handle;  // libuv poll handle
  uint16_t imtu;           // Incoming MTU size
  uint16_t cid;            // CID value for this connection

  readCallback readCb;
  void* readData;
};

#endif
