#include "attribute.h"

Attribute::Attribute() : handle(0), hasType(false), value(NULL), vlen(0)
{
}

Attribute::~Attribute()
{
  delete value;
}

void
Attribute::setType(const bt_uuid_t& type)
{
  this->type = type;
  this->hasType = true;
}

void
Attribute::setValue(const uint8_t* value, size_t len)
{
  this->value = new uint8_t[len];
  memcpy(this->value, value, len);
  this->vlen = len;
}
