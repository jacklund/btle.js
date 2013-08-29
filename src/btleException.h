#ifndef BTLE_EXCEPTION_H
#define BTLE_EXCEPTION_H

#include <exception>
#include <string>

class BTLEException : public std::exception {
public:
  BTLEException(const char* message, int err);
  BTLEException(const char* message);
  virtual ~BTLEException() throw();

  virtual const char* what();

private:
  std::string message;
};
#endif
