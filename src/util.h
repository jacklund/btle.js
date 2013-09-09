#ifndef UTIL_H
#define UTIL_H

#include <unistd.h>
#include <node.h>

void printBuffer(const char* data, size_t len);

bool setOpts(struct set_opts& opts, v8::Local<v8::String> destination, v8::Local<v8::Object> options);

v8::Handle<v8::String> getKey(const char* value);

bool getSourceAddr(v8::Handle<v8::String> key, v8::Local<v8::Object> options, struct set_opts& opts);

const char* getStringValue(v8::Local<v8::String> string);

bool getIntValue(v8::Local<v8::Number> number, int& intValue);

bool getBooleanValue(v8::Local<v8::Boolean> boolean, int& value);


#endif
