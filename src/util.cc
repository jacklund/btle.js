#include <stdio.h>
#include <stdlib.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#include "util.h"
#include "btio.h"

using namespace v8;

static char stringBuffer[1024];
const char* getStringValue(Local<String> string)
{
  string->WriteUtf8(stringBuffer, sizeof(stringBuffer));
  return stringBuffer;
}

bool getIntValue(Local<Number> number, int& intValue)
{
  double dbl = number->Value();
  int tmp = (int) dbl;
  if (dbl != tmp) return false;
  intValue = tmp;
  return true;
}

bool getBooleanValue(Local<Boolean> boolean, int& value)
{
  value = boolean->Value();
  return true;
}

Handle<String> getKey(const char* value)
{
  return Handle<String>(String::New(value));
}

bool getSourceAddr(Handle<String> key, Local<Object> options, struct set_opts& opts)
{
  Local<Value> value = options->Get(key);
  if (!value->IsString()) {
    ThrowException(Exception::TypeError(String::New("Source option must be a string")));
    return false;
  }
  const char* src = getStringValue(value->ToString());
  if (!strncmp(src, "hci", 3))
    hci_devba(atoi(src + 3), &opts.src);
  else
    str2ba(src, &opts.src);

  return true;
}

bool setOpts(struct set_opts& opts, Local<String> destination, Local<Object> options)
{
  str2ba(getStringValue(destination), &opts.dst);

  return setOpts(opts, options);
}

bool setOpts(struct set_opts& opts, Local<Object> options)
{
  memset((void*) &opts, 0, sizeof(opts));

  /* Set defaults */
  opts.type = BT_IO_L2CAP;
  opts.defer = 30;
  opts.master = -1;
  opts.mode = L2CAP_MODE_BASIC;
  opts.flushable = -1;
  opts.priority = 0;
  opts.src_type = BDADDR_LE_PUBLIC;
  opts.dst_type = BDADDR_LE_PUBLIC;
  opts.cid = ATT_CID;
  opts.sec_level = BT_SECURITY_LOW;

  Handle<String> key = getKey("source");
  if (options->Has(key)) {
    if (!getSourceAddr(key, options, opts)) return false;
  } else {
		bacpy(&opts.src, BDADDR_ANY);
  }

  key = getKey("type");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Type option must be a number")));
      return false;
    }
    int type;
    if (!getIntValue(value->ToNumber(), type) || type < BT_IO_L2CAP || type > BT_IO_SCO) {
      ThrowException(Exception::TypeError(String::New("Type option must be one of the BtIOType values")));
      return false;
    }
    opts.type = (BtIOType) type;
  }

  key = getKey("sourceType");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("SourceType option must be a number")));
      return false;
    }
    int type;
    if (!getIntValue(value->ToNumber(), type) || type < BDADDR_BREDR || type > BDADDR_LE_RANDOM) {
      ThrowException(Exception::TypeError(String::New("SourceType option must be one of the BtAddrType values")));
      return false;
    }
    opts.src_type = type;
  }

  key = getKey("destType");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("DestType option must be a number")));
      return false;
    }
    int type;
    if (!getIntValue(value->ToNumber(), type) || type < BDADDR_BREDR || type > BDADDR_LE_RANDOM) {
      ThrowException(Exception::TypeError(String::New("DestType option must be one of the BtAddrType values")));
      return false;
    }
    opts.dst_type = type;
  }

  key = getKey("defer");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Defer option must be a number")));
      return false;
    }
    int defer;
    if (!getIntValue(value->ToNumber(), defer)) {
      ThrowException(Exception::TypeError(String::New("Defer option must be an integer")));
      return false;
    }
    opts.defer = defer;
  }

  key = getKey("securityLevel");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("SecurityLevel option must be a number")));
      return false;
    }
    int sec_level;
    if (!getIntValue(value->ToNumber(), sec_level) || sec_level < BT_SECURITY_SDP || sec_level > BT_SECURITY_HIGH) {
      ThrowException(Exception::TypeError(String::New("SecurityLevel option must be one of the BtSecurityLevel values")));
      return false;
    }
    opts.sec_level = sec_level;
  }

  key = getKey("channel");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Channel option must be a number")));
      return false;
    }
    int channel;
    if (!getIntValue(value->ToNumber(), channel)) {
      ThrowException(Exception::TypeError(String::New("Channel option must be an integer")));
      return false;
    }
    opts.channel = channel;
  }

  key = getKey("psm");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("PSM option must be a number")));
      return false;
    }
    int psm;
    if (!getIntValue(value->ToNumber(), psm)) {
      ThrowException(Exception::TypeError(String::New("PSM option must be an integer")));
      return false;
    }
    opts.psm = psm;
  }

  key = getKey("cid");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("CID option must be a number")));
      return false;
    }
    int cid;
    if (!getIntValue(value->ToNumber(), cid)) {
      ThrowException(Exception::TypeError(String::New("CID option must be an integer")));
      return false;
    }
    opts.cid = cid;
  }

  key = getKey("mtu");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("MTU option must be a number")));
      return false;
    }
    int mtu;
    if (!getIntValue(value->ToNumber(), mtu)) {
      ThrowException(Exception::TypeError(String::New("MTU option must be an integer")));
      return false;
    }
    opts.mtu = mtu;
  }

  key = getKey("imtu");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("IMTU option must be a number")));
      return false;
    }
    int imtu;
    if (!getIntValue(value->ToNumber(), imtu)) {
      ThrowException(Exception::TypeError(String::New("IMTU option must be an integer")));
      return false;
    }
    opts.imtu = imtu;
  }

  key = getKey("omtu");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("OMTU option must be a number")));
      return false;
    }
    int omtu;
    if (!getIntValue(value->ToNumber(), omtu)) {
      ThrowException(Exception::TypeError(String::New("OMTU option must be an integer")));
      return false;
    }
    opts.omtu = omtu;
  }

  key = getKey("master");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Master option must be a number")));
      return false;
    }
    int master;
    if (!getIntValue(value->ToNumber(), master)) {
      ThrowException(Exception::TypeError(String::New("Master option must be an integer")));
      return false;
    }
    opts.master = master;
  }

  key = getKey("mode");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Mode option must be a number")));
      return false;
    }
    int mode;
    if (!getIntValue(value->ToNumber(), mode) || mode < L2CAP_MODE_BASIC || mode > L2CAP_MODE_STREAMING) {
      ThrowException(Exception::TypeError(String::New("Mode option must be an integer and one of the BtMode values")));
      return false;
    }
    opts.mode = mode;
  }

  key = getKey("flushable");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsBoolean()) {
      ThrowException(Exception::TypeError(String::New("Flushable option must be true or false")));
      return false;
    }
    getBooleanValue(value->ToBoolean(), opts.flushable);
  }

  key = getKey("priority");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("Priority option must be a number")));
      return false;
    }
    int priority;
    if (!getIntValue(value->ToNumber(), priority)) {
      ThrowException(Exception::TypeError(String::New("Priority option must be an integer")));
      return false;
    }
    opts.priority = priority;
  }

  return true;
}

void printBuffer(const char* data, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    printf("%02X", data[i]);
  }
}
