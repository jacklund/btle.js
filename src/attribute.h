#ifndef ATTRIBUTE_H
#define ATTRIBUTE_H

#include <vector>
#include <stdint.h>
#include <bluetooth/uuid.h>

// Definition of handle_t
typedef uint16_t handle_t;

// Attribute class. Encapsulates a bluetooth attribute.
// It has a handle, a type, and a value. Note that one or
// more or those may not get set
class Attribute {
public:
  Attribute();
  virtual ~Attribute();

  handle_t getHandle() const { return handle; }
  void setHandle(handle_t handle) { this->handle = handle; }

  bool containsType() const { return hasType; }
  const bt_uuid_t& getType() const { return type; }
  void setType(const bt_uuid_t& type);

  void setValue(const uint8_t* value, size_t len);
  const uint8_t* getValue(size_t& len) const { len = vlen; return value; }

private:
  handle_t handle;
  bt_uuid_t type;
  bool hasType;
  uint8_t* value;
  size_t vlen;
};

typedef std::vector<Attribute*> AttributeList;

#endif
