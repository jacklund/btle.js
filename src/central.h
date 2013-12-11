#ifndef CENTRAL_H
#define CENTRAL_H

#include <node.h>
#include <bluetooth/bluetooth.h>

class Central: node::ObjectWrap {
public:
  Central();
  virtual ~Central();

  static void Init(v8::Handle<v8::Object> exports);

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Listen(const v8::Arguments& args);
  static v8::Handle<v8::Value> Write(const v8::Arguments& args);

private:
  static v8::Persistent<v8::Function> constructor;

  static void onConnect(uv_poll_t* handle, int status, int events);
  static uv_buf_t onAlloc(uv_handle_t* handle, size_t suggested);
  static void onRead(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static void onWrite(uv_write_t* req, int status);
  static void onClose(uv_handle_t* handle);

  uv_stream_t* getStream() { return (uv_stream_t*) tcp; }

  void mtuExchange(uint16_t mtu);
  void write(char* data, size_t len, void* wd);

  int sock;
	bdaddr_t src;
	char dst[256];
  uint16_t cid;
  unsigned int mtu;
  uv_poll_t* poll_handle;
  uv_tcp_t* tcp;
  v8::Handle<v8::Object> self;
  v8::Persistent<v8::Function> connectionCallback;
};

#endif
