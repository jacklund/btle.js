#ifndef SERVER_INTERFACE_H
#define SERVER_INTERFACE_H

#include <node.h>

#include "server.h"

class ServerInterface: node::ObjectWrap {
public:
  ServerInterface();
  virtual ~ServerInterface();

  static void Init(v8::Handle<v8::Object> exports);

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Listen(const v8::Arguments& args);
  static v8::Handle<v8::Value> Write(const v8::Arguments& args);

private:
  static v8::Persistent<v8::Function> constructor;

  static void onConnect(void* data, int status, int events);
  static void onRead(void* data, uint8_t* buf, int len, const char* error);
  static void onWrite(void* data, const char* error);
  static void onClose(void* data);
  static void onError(void* data, const char* error);

  v8::Handle<v8::Object> self;
  Server* server;
  v8::Persistent<v8::Function> connectionCallback;
};

#endif
