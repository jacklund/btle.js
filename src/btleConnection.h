#ifndef BTLE_CONNECTION_H
#define BTLE_CONNECTION_H

#include <node.h>

#include "att.h"

/**
 * Node.js interface class, does all the node.js object wrapping stuff,
 * and delegates all the protocol stuff to att.{h,cc}.
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
  static v8::Handle<v8::Value> FindByTypeValue(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadByType(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadHandle(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadByGroupType(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddNotificationListener(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteCommand(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteRequest(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

protected:
  // Convert an attribute object to a Javascript object
  static v8::Local<v8::Object> getAttributeInfo(Att::AttributeInfo* attribute);
  static v8::Local<v8::Object> getHandlesInfo(Att::HandlesInfo* attribute);
  static v8::Local<v8::Object> getAttributeData(Att::AttributeData* attribute);
  static v8::Local<v8::Object> getGroupAttributeData(Att::GroupAttributeData* attribute);

  // Emits an error based on the last error
  void emit_error();
  // Emits an error with the given error message
  void emit_error(const char* errorMessage);

  // Callbacks sent into att.cc
  static void onConnect(void* data, int status, int events);
  static void onClose(void* data);
  static void onReadAttribute(uint8_t status, void* data, uint8_t* buf, int len, const char* error);
  static void onReadNotification(uint8_t status, void* data, uint8_t* buf, int len, const char* error);
  static void onWrite(void* data, const char* error);
  static void onFindInformation(uint8_t status, void* data, void* list, const char* error);
  static void onFindByType(uint8_t status, void* data, void* list, const char* error);
  static void onReadByType(uint8_t status, void* data, void* list, const char* error);
  static void onReadByGroupType(uint8_t status, void* data, void* list, const char* error);
  static void onError(void* data, const char* error);

  void handleConnect(int status, int events);
  void handleFindInformation(uint8_t status, Att::AttributeInfoList& list, struct callbackData* cd, const char* error);
  void handleFindByType(uint8_t status, Att::HandlesInfoList& list, struct callbackData* cd, const char* error);
  void handleReadByType(uint8_t status, Att::AttributeDataList& list, struct callbackData* cd, const char* error);
  void handleReadByGroupType(uint8_t status, Att::GroupAttributeDataList& list, struct callbackData* cd, const char* error);

  // Callback called when we tell v8 to make a reference weak
  static void weak_cb(v8::Persistent<v8::Value> object, void* parameter);

  // Translate the error code and return an error
  void sendError(struct callbackData* cd, uint8_t err, const char* error);

  const char* createErrorMessage(uint8_t err);

private:
  static v8::Persistent<v8::Function> constructor;

  v8::Handle<v8::Object> self;
  Att* att;
  Connection* connection;
  v8::Persistent<v8::Function> connectionCallback;
  v8::Persistent<v8::Function> closeCallback;
};

#endif
