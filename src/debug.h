#ifndef DEBUG_H
#define DEBUG_H

#include <v8.h>

extern bool debug;

void initDebug(v8::Handle<v8::Object> exports);

#endif
