#include <string.h>
#include "gattException.h"

gattException::gattException(const char* msg, int err)
{
  message = msg;
  message += ": ";
  message += strerror(err);
}

gattException::gattException(const char* msg)
  : message(msg)
{
}

gattException::~gattException() throw()
{
}

const char* gattException::what()
{
  return message.c_str();
}
