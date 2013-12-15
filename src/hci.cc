#include <sys/ioctl.h>
#include <sys/errno.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <node_buffer.h>

#include "debug.h"
#include "hci.h"
#include "util.h"

#define HCI_DEVICE_ID 0
#define DEFAULT_TIMEOUT 1000

using namespace v8;
using namespace node;

HCI::HCI()
: hciSocket(-1), state(HCI_STATE_DOWN), socketClosed(true), isAdvertising(false)
{
  memset(&this->hciDevInfo, 0, sizeof(this->hciDevInfo));
}

HCI::~HCI()
{
  closeHCISocket();
}

int
HCI::getAdvType(Local<Value> arg)
{
  Local<Object> options = arg->ToObject();
  Handle<String> key = getKey("advType");
  if (options->Has(key)) {
    Local<Value> value = options->Get(key);
    if (!value->IsNumber()) {
      ThrowException(Exception::TypeError(String::New("advType option must be integer")));
    }
    int val;
    getIntValue(value->ToNumber(), val);
    return val;
  }

  return 0;
}

// Node.js new object construction
Handle<Value>
HCI::New(const Arguments& args)
{
  HandleScope scope;

  assert(args.IsConstructCall());

  HCI* hci = new HCI();
  hci->self = Persistent<Object>::New(args.This());
  hci->Wrap(args.This());

  return scope.Close(args.This());
}

Handle<Value>
HCI::StartAdvertising(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  Local<Object> options;
  uint8_t* data = NULL;
  size_t len = 0;
  uint8_t* scanData = NULL;
  size_t scanLen = 0;
  if (args[0]->IsObject() || args[0]->IsNull()) {
    if (args.Length() < 2) {
      ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
      return scope.Close(Undefined());
    }
    if (!args[0]->IsNull()) options = args[0]->ToObject();
    if (args.Length() > 1) {
      if (Buffer::HasInstance(args[1])) {
        data = (uint8_t*) Buffer::Data(args[1]);
        len = Buffer::Length(args[1]);
      } else {
        ThrowException(Exception::TypeError(String::New("Second parameter must be a Buffer")));
        return scope.Close(Undefined());
      }
      if (args.Length() > 2) {
        if (Buffer::HasInstance(args[2])) {
          scanData = (uint8_t*) Buffer::Data(args[2]);
          scanLen = Buffer::Length(args[2]);
        } else if (!args[2]->IsNull()) {
          ThrowException(Exception::TypeError(String::New("Third parameter must be a Buffer")));
          return scope.Close(Undefined());
        }
      }
    }
  } else if (Buffer::HasInstance(args[0])) {
    data = (uint8_t*) Buffer::Data(args[0]);
    len = Buffer::Length(args[0]);
    if (args.Length() > 1) {
      if (Buffer::HasInstance(args[1])) {
        scanData = (uint8_t*) Buffer::Data(args[1]);
        scanLen = Buffer::Length(args[1]);
      } else {
        ThrowException(Exception::TypeError(String::New("Second parameter must be a Buffer")));
        return scope.Close(Undefined());
      }
    }
  } else {
    ThrowException(Exception::TypeError(String::New("First parameter must be an object, or a buffer")));
    return scope.Close(Undefined());
  }

  HCI* hci = ObjectWrap::Unwrap<HCI>(args.This());

  if (!options.IsEmpty()) {
    le_set_advertising_parameters_cp params;
    memset(&params, 0, sizeof(params));
    Handle<String> key = getKey("advType");
    if (options->Has(key)) {
      Local<Value> value = options->Get(key);
      if (!value->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("advType option must be integer")));
      }
      int val;
      getIntValue(value->ToNumber(), val);
      if (val < 0 || val > 3) {
        ThrowException(Exception::TypeError(String::New("advType option must be between 0 and 3")));
      }
      params.advtype = val;
    } else {
      params.advtype = 0;
    }
    key = getKey("minInterval");
    if (options->Has(key)) {
      Local<Value> value = options->Get(key);
      if (!value->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("minInterval option must be integer")));
      }
      int val;
      getIntValue(value->ToNumber(), val);
      params.min_interval = htobs(val);
    } else {
      params.min_interval = htobs(0x0800);
    }
    key = getKey("maxInterval");
    if (options->Has(key)) {
      Local<Value> value = options->Get(key);
      if (!value->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("maxInterval option must be integer")));
      }
      int val;
      getIntValue(value->ToNumber(), val);
      params.max_interval = htobs(val);
    } else {
      params.max_interval = htobs(0x0800);
    }
    key = getKey("chanMap");
    if (options->Has(key)) {
      Local<Value> value = options->Get(key);
      if (!value->IsNumber()) {
        ThrowException(Exception::TypeError(String::New("chanMap option must be integer")));
      }
      int val;
      getIntValue(value->ToNumber(), val);
      params.chan_map = val;
    }
    hci->setAdvertisingParameters(params);
  }

  if (scanData) {
    hci->setScanResponseData(scanData, scanLen);
  }
  hci->startAdvertising(data, (uint8_t) len);

  return scope.Close(Undefined());
}

Handle<Value>
HCI::StopAdvertising(const Arguments& args)
{
  HandleScope scope;

  HCI* hci = ObjectWrap::Unwrap<HCI>(args.This());

  hci->stopAdvertising();

  return scope.Close(Undefined());
}

Local<String>
HCI::errnoMessage(const char* msg)
{
  return String::Concat(String::New(msg),
      String::Concat(String::New(": "), String::New(strerror(errno))));
}

int
HCI::getHCISocket()
{
  if (this->socketClosed) {
    this->hciSocket = hci_open_dev(HCI_DEVICE_ID);
    if (this->hciSocket < 0) {
      ThrowException(Exception::Error(errnoMessage("Error opening HCI socket")));
    }
    this->socketClosed = false;
    this->hciDevInfo.dev_id = HCI_DEVICE_ID;
    if (ioctl(this->hciSocket, HCIGETDEVINFO, (void *)&this->hciDevInfo) < 0) {
      closeHCISocket();
      ThrowException(Exception::Error(errnoMessage("Error getting HCI socket state")));
    }
    getAdapterState();
    if (this->state != HCI_STATE_UP) {
      if (this->hciSocket >= 0) {
        closeHCISocket();
        ThrowException(Exception::Error(String::New("Adapter never came up")));
      }
    }
  }

  return this->hciSocket;
}

void
HCI::closeHCISocket()
{
  if (!this->socketClosed && this->hciSocket >= 0) {
    hci_close_dev(this->hciSocket);
    this->hciSocket = -1;
    this->socketClosed = true;
  }
}

HCI::HCIState
HCI::getAdapterState()
{
  if (hci_test_bit(HCI_UP, &this->hciDevInfo.flags)) {
    this->state = HCI_STATE_UP;
  } else {
    this->state = HCI_STATE_DOWN;
  }

  return this->state;
}

void
HCI::setAdvertisingParameters(le_set_advertising_parameters_cp& params)
{
  struct hci_request req;
  uint8_t status;
  memset(&req, 0, sizeof(req));
  req.ogf = OGF_LE_CTL;
  req.ocf = OCF_LE_SET_ADVERTISING_PARAMETERS;
  req.cparam = &params;
  req.clen = LE_SET_ADVERTISING_PARAMETERS_CP_SIZE;
  req.rparam = &status;
  req.rlen = 1;

  if (hci_send_req(getHCISocket(), &req, DEFAULT_TIMEOUT) < 0) {
    ThrowException(Exception::Error(errnoMessage("Error setting advertising parameters")));
  }
}

void
HCI::setAdvertisingData(uint8_t* data, uint8_t length)
{
  if (debug) {
    printf("Advertising data:\n");
    for (uint8_t i = 0; i < length; ++i) {
      printf("%02x ", data[i]);
    }
    printf("\n");
  }
  int timeout = DEFAULT_TIMEOUT;

  le_set_advertising_data_cp cp;
  memset(&cp, 0, sizeof(cp));
  cp.length = length;
  memcpy(&cp.data, data, length);

  struct hci_request rq;
  uint8_t status;
  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
  rq.cparam = &cp;
  rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if (hci_send_req(getHCISocket(), &rq, timeout) < 0) {
    ThrowException(Exception::Error(errnoMessage("Error setting advertising data")));
  }

  if (status) {
    ThrowException(Exception::Error(errnoMessage("Error setting advertising data")));
  }
}

void
HCI::setScanResponseData(uint8_t* data, uint8_t length)
{
  if (debug) {
    printf("Scan response data:\n");
    for (uint8_t i = 0; i < length; ++i) {
      printf("%02x ", data[i]);
    }
    printf("\n");
  }
  int timeout = DEFAULT_TIMEOUT;

  le_set_scan_response_data_cp cp;
  memset(&cp, 0, sizeof(cp));
  cp.length = length;
  memcpy(&cp.data, data, length);

  struct hci_request rq;
  uint8_t status;
  memset(&rq, 0, sizeof(rq));
  rq.ogf = OGF_LE_CTL;
  rq.ocf = OCF_LE_SET_SCAN_RESPONSE_DATA;
  rq.cparam = &cp;
  rq.clen = LE_SET_SCAN_RESPONSE_DATA_CP_SIZE;
  rq.rparam = &status;
  rq.rlen = 1;

  if (hci_send_req(getHCISocket(), &rq, timeout) < 0) {
    ThrowException(Exception::Error(errnoMessage("Error setting advertising data")));
  }

  if (status) {
    ThrowException(Exception::Error(errnoMessage("Error setting advertising data")));
  }
}

void
HCI::startAdvertising(uint8_t* data, uint8_t length)
{
  stopAdvertising();

  if (hci_le_set_advertise_enable(getHCISocket(), 1, DEFAULT_TIMEOUT) < 0) {
    ThrowException(Exception::Error(errnoMessage("Error enabling advertising")));
  }

  setAdvertisingData(data, length);

  isAdvertising = true;
}

void
HCI::stopAdvertising()
{
  hci_le_set_advertise_enable(getHCISocket(), 0, DEFAULT_TIMEOUT);
  isAdvertising = false;
}

// Node.js initialization
void
HCI::Init(Handle<Object> exports)
{
  Local<FunctionTemplate> t = FunctionTemplate::New(HCI::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::New("HCI"));
  NODE_SET_PROTOTYPE_METHOD(t, "startAdvertising", HCI::StartAdvertising);
  NODE_SET_PROTOTYPE_METHOD(t, "stopAdvertising", HCI::StopAdvertising);

  exports->Set(String::NewSymbol("HCI"), t->GetFunction());
}
