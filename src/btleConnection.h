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
  static v8::Handle<v8::Value> FindInformation(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadHandle(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddNotificationListener(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteCommand(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteRequest(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

protected:
  // Emits an error based on the last error
  void emit_error();
  // Emits an error with the given error message
  void emit_error(const char* errorMessage);

  // Callbacks sent into gatt.cc
  static void onConnect(void* data, int status, int events);
  static void onClose(void* data);
  static bool onReadAttribute(uint8_t status, void* data, uint8_t* buf, int len, const char* error);
  static bool onReadNotification(uint8_t status, void* data, uint8_t* buf, int len, const char* error);
  static void onWrite(void* data, const char* error);
  static bool onFindInformation(uint8_t status, void* data, uint8_t* buf, int len, const char* error);
  static void onError(void* data, const char* error);

  void handleConnect(int status, int events);
  bool handleFindInformation(uint8_t status, uint16_t startHandle,
    uint16_t endHandle, uint8_t* buf, int len, struct callbackData* cd, const char* error);

  // Callback called when we tell v8 to make a reference weak
  static void weak_cb(v8::Persistent<v8::Value> object, void* parameter);

  // Send off the Find Information data to node.js
  void sendFindInformation(struct callbackData* cd);
  void sendFindInfoError(struct callbackData* cd, uint8_t err, const char* error);
  void parseFindInfo(uint8_t*& ptr, uint16_t& handle, uint8_t* buf, int len);
  const char* createErrorMessage(uint8_t err);

private:
  static v8::Persistent<v8::Function> constructor;

  v8::Handle<v8::Object> self;
  Gatt* gatt;
  Connection* connection;
  v8::Persistent<v8::Function> connectionCallback;
  v8::Persistent<v8::Function> closeCallback;
  v8::Persistent<v8::Object> findInfoResponse;
};

#endif
