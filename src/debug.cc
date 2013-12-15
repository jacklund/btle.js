#include <node.h>
#include <v8.h>

#include "debug.h"

bool debug = false;

using namespace v8;

Handle<Value> SetDebug(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1) {
    ThrowException(Exception::TypeError(String::New("setDebug takes one boolean argument")));
    return scope.Close(Undefined());
  }
  if (!args[0]->IsBoolean()) {
    ThrowException(Exception::TypeError(String::New("setDebug takes one boolean argument")));
    return scope.Close(Undefined());
  }

  debug = args[0]->BooleanValue();

  return scope.Close(Undefined());
}

void initDebug(Handle<Object> exports) {
    exports->Set(String::NewSymbol("setDebug"),
              FunctionTemplate::New(SetDebug)->GetFunction());
}

NODE_MODULE(setDebug, initDebug)
