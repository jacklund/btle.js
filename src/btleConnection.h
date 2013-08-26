#ifndef BTLE_CONNECTION_H
#define BTLE_CONNECTION_H

#include <node.h>

#include "gatt.h"

class BTLEConnection: node::ObjectWrap {
public:
  static void Init();
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadHandle(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteCommand(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteRequest(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  v8::Persistent<v8::Object> getHandle();

protected:
  void emit_error();

  // Callbacks
  static void onConnect(void* data, int status, int events);
  static void onClose(void* data);
  static void onReadAttribute(void* data, uint8_t* buf, int len);
  static void onWrite(void* data, int status);

  static void weak_cb(v8::Persistent<v8::Value> object, void* parameter);

private:
  static v8::Persistent<v8::Function> constructor;

  v8::Handle<v8::Object> self;
  Gatt* gatt;
  uv_poll_t* handle;
  uv_tcp_t* tcp;
  v8::Persistent<v8::Function> connectionCallback;
  v8::Persistent<v8::Function> closeCallback;
};

#endif
