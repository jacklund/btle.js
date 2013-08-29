#include <string.h>
#include "btleException.h"

BTLEException::BTLEException(const char* msg, int err)
{
  message = msg;
  message += ": ";
  message += strerror(err);
}

BTLEException::BTLEException(const char* msg)
  : message(msg)
{
}

BTLEException::~BTLEException() throw()
{
}

const char* BTLEException::what()
{
  return message.c_str();
}
