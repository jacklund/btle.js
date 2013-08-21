#ifndef BTLE_CONNECTION_H
#define BTLE_CONNECTION_H

#include <node.h>

class BTLEConnection: node::ObjectWrap {
public:
  static void Init();
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadHandle(const v8::Arguments& args);
  static v8::Handle<v8::Value> Write(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  v8::Persistent<v8::Object> getHandle();

protected:
  static v8::Handle<v8::String> getKey(const char* value);

  static const char* getStringValue(v8::Local<v8::String> string);

  static bool getIntValue(v8::Local<v8::Number> number, int& intValue);

  static bool getBooleanValue(v8::Local<v8::Boolean> boolean, int& value);

  static bool  getSourceAddr(v8::Handle<v8::String> key, v8::Local<v8::Object> options, struct set_opts& opts);

  static bool setOpts(struct set_opts& opts, v8::Local<v8::Object> options);

  void emit_error();

  // Callbacks
  static void connect_cb(uv_poll_t* handle, int status, int events);
  static void read_cb(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
  static void write_cb(uv_write_t* req, int status);
  static void weak_cb(v8::Persistent<v8::Value> object, void* parameter);
  static void close_cb(uv_handle_t* handle);

private:
  static v8::Persistent<v8::Function> constructor;

  uv_poll_t* handle;
  uv_tcp_t* tcp;
  v8::Persistent<v8::Function> connectionCallback;
  v8::Persistent<v8::Function> closeCallback;
};

#endif
