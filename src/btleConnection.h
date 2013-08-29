#ifndef BTLE_CONNECTION_H
#define BTLE_CONNECTION_H

#include <node.h>

#include "gatt.h"

/**
 * Node.js interface class, does all the node.js object wrapping stuff,
 * and delegates all the protocol stuff to gatt.{h,cc}.
 */
class BTLEConnection: node::ObjectWrap {
public:
  // Constructor/Destructor
  BTLEConnection();
  virtual ~BTLEConnection();

  // Node.js initialization
  static void Init();

  // Methods which translate to methods on the node.js object
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Connect(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadHandle(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddNotificationListener(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteCommand(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteRequest(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

protected:
  // Emits an error based on the last error
  void emit_error();

  // Callbacks sent into gatt.cc
  static void onConnect(void* data, int status, int events);
  static void onClose(void* data);
  static void onReadAttribute(void* data, uint8_t* buf, int len);
  static void onReadNotification(void* data, uint8_t* buf, int len);
  static void onWrite(void* data, int status);

  // Callback called when we tell v8 to make a reference weak
  static void weak_cb(v8::Persistent<v8::Value> object, void* parameter);

private:
  static v8::Persistent<v8::Function> constructor;

  v8::Handle<v8::Object> self;
  Gatt* gatt;
  Connection* connection;
  v8::Persistent<v8::Function> connectionCallback;
  v8::Persistent<v8::Function> closeCallback;
};

#endif
