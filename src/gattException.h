#ifndef GATT_EXCEPTION_H
#define GATT_EXCEPTION_H

#include <exception>
#include <string>

class gattException : public std::exception {
public:
  gattException(const char* message, int err);
  gattException(const char* message);
  virtual ~gattException() throw();

  virtual const char* what();

private:
  std::string message;
};
#endif
