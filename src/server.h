#ifndef SERVER_H
#define SERVER_H

#include <uv.h>

class Server {
public:
  // Callback types
  typedef void (*ConnectCallback)(void* data, int status, int events);
  typedef void (*CloseCallback)(void* data);
  typedef void (*ErrorCallback)(void* data, const char* error);
  typedef void (*ReadCallback)(void* data, uint8_t* buf, int len, const char* error);
  typedef void (*WriteCallback)(void* data, const char* error);

  Server();
  virtual ~Server();

  void setReadCallback(ReadCallback callback, void* data);
  void setCloseCallback(CloseCallback callback, void* data);
  void setErrorCallback(ErrorCallback callback, void* data);

  void listen(struct set_opts& opts, ConnectCallback callback, void* data);
  void write(char* buffer, unsigned int bufLen, WriteCallback callback = NULL, void* data = NULL);

private:
  static uv_buf_t onAlloc(uv_handle_t* handle, size_t suggested);
  static void onConnect(uv_poll_t* handle, int status, int events);
  static void onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static void onWrite(uv_write_t* req, int status);
  static void onClose(uv_handle_t* handle);

  void shutdown();
  void stopPolling();
  void stopReading();

  uv_stream_t* getStream() { return (uv_stream_t*) tcp; }

  int sock;
  uv_poll_t* poll_handle;
  uv_tcp_t* tcp;
  ConnectCallback connectCallback;
  void* connectData;
  ReadCallback readCallback;
  void* readData;
  CloseCallback closeCallback;
  void* closeData;
  ErrorCallback errorCallback;
  void* errorData;
};

#endif
