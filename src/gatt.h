#ifndef GATT_H
#define GATT_H

#include <node.h>
#include <pthread.h>
#include <map>

class Gatt {
public:
  typedef void (*connectCallback)(void* data, int status, int events);
  typedef void (*closeCallback)(void* data);
  typedef void (*errorCallback)(void* data, uv_err_t err);
  typedef void (*readCallback)(void* data, uint8_t* buf, int len);
  typedef void (*writeCallback)(void* data, int status);

  Gatt();
  virtual ~Gatt();

  void onError(errorCallback cb, void* data);
  void connect(struct set_opts& opts, connectCallback connect, void* data);
  void readAttribute(uint16_t handle, void* data, readCallback callback);
  void writeCommand(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback=NULL, void* cbData=NULL);
  void writeRequest(uint16_t handle, const uint8_t* data, size_t length, writeCallback callback=NULL, void* cbData=NULL);
  void listenForNotifications(uint16_t handle, void* data, readCallback callback);
  void close(closeCallback cb, void* data);

private:
  static void onConnect(uv_poll_t* handle, int status, int events);
  static void onClose(uv_handle_t* handle);
  static void onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static void onWrite(uv_write_t* req, int status);
  static uv_buf_t onAlloc(uv_handle_t* handle, size_t suggested);
  uv_buf_t getBuffer();
  size_t encode(uint8_t opcode, uint16_t handle, uint8_t* buffer, size_t buflen,
    const uint8_t* value = NULL, size_t vlen = 0);
  bool isSingleResponse(uint8_t opcode);

  struct readData;
  struct writeData;

  int sock;
  uv_tcp_t* tcp;
  uv_poll_t* poll_handle;
  uint16_t imtu;
  uint16_t cid;
  typedef std::map<uint8_t, readData*> ReadMap;
  ReadMap readMap;
  pthread_mutex_t readMapLock;

  // Callbacks
  connectCallback connectCb;
  void* connectData;
  closeCallback closeCb;
  void* closeData;
  errorCallback errorCb;
  void* errorData;
};

#endif
