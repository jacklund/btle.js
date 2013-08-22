#ifndef GATT_H
#define GATT_H

#include <node.h>

class Gatt {
public:
  typedef void (*connectCallback)(void* data, int status, int events);
  typedef void (*closeCallback)(void* data);
  typedef void (*errorCallback)(void* data, uv_err_t err);

  Gatt();
  virtual ~Gatt();

  void onError(errorCallback cb, void* data);
  void connect(struct set_opts& opts, connectCallback connect, void* data);
  void close(closeCallback cb, void* data);

private:
  static void connect_cb(uv_poll_t* handle, int status, int events);
  static void close_cb(uv_handle_t* handle);
  static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static uv_buf_t alloc_cb(uv_handle_t* handle, size_t suggested);

  int sock;
  uv_tcp_t* tcp;
  uv_poll_t* poll_handle;
  connectCallback connectCb;
  void* connectData;
  closeCallback closeCb;
  void* closeData;
  errorCallback errorCb;
  void* errorData;
};

#endif
