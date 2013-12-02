#include <sys/ioctl.h>
#include <sys/errno.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <node_buffer.h>

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

Handle<Value>
HCI::StartAdvertising(const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1) {
    ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
    return scope.Close(Undefined());
  }

  uint8_t* data;
  size_t len;
  if (args[0]->IsString()) {
    const char* charData = getStringValue(args[0]->ToString());
    len = strlen(charData);
    data = new uint8_t[len];
    memcpy(data, charData, len);
  } else if (Buffer::HasInstance(args[0])) {
    len = Buffer::Length(args[0]);
    data = (uint8_t*) Buffer::Data(args[0]);
  } else {
    ThrowException(Exception::TypeError(String::New("Data must be a string or a Buffer")));
    return scope.Close(Undefined());
  }

  size_t scanRespLen = 0;
  uint8_t* scanRespData = NULL;
  int advType = 0;
  if (args.Length() > 1) {
    if (args[1]->IsString() || Buffer::HasInstance(args[1])) {
      if (args[1]->IsString()) {
        const char* charData = getStringValue(args[1]->ToString());
        scanRespLen = strlen(charData);
        scanRespData = new uint8_t[scanRespLen];
        memcpy(scanRespData, charData, scanRespLen);
      } else if (Buffer::HasInstance(args[1])) {
        scanRespLen = Buffer::Length(args[1]);
        scanRespData = (uint8_t*) Buffer::Data(args[1]);
      }
      if (args.Length() > 2) {
        if (args[2]->IsObject()) {
          advType = getAdvType(args[2]);
        }
      }
    } else if (args[1]->IsObject()) {
      if (args.Length() > 2) {
        ThrowException(Exception::TypeError(String::New("Second parameter must be a string or a Buffer")));
        return scope.Close(Undefined());
      }
      advType = getAdvType(args[1]);
    } else {
      ThrowException(Exception::TypeError(String::New("Second parameter must be a string or a Buffer, or an Object")));
      return scope.Close(Undefined());
    }
  }

  HCI* hci = ObjectWrap::Unwrap<HCI>(args.This());

  if (advType != 0) {
    le_set_advertising_parameters_cp params;
    memset(&params, 0, sizeof(params));
    params.min_interval = htobs(0x0800);
    params.max_interval = htobs(0x0800);
    params.chan_map = 7;
    params.advtype = advType;
    hci->setAdvertisingParameters(params);
  }

  hci->startAdvertising(data, (uint8_t) len);

  return scope.Close(Undefined());
}

int
HCI::getHCISocket()
{
  if (this->socketClosed) {
    this->hciSocket = hci_open_dev(HCI_DEVICE_ID);
    if (this->hciSocket < 0) {
      // TODO: Throw exception
    }
    this->hciDevInfo.dev_id = HCI_DEVICE_ID;
    ioctl(this->hciSocket, HCIGETDEVINFO, (void *)&this->hciDevInfo);
    if (getAdapterState() != HCI_STATE_UP) {
      if (this->hciSocket >= 0) {
        hci_close_dev(this->hciSocket);
        this->hciSocket = -1;
        this->socketClosed = true;
        // TODO: Throw exception
      }
    }
  }

  return this->hciSocket;
}

HCI::HCIState
HCI::getAdapterState()
{
  if (!hci_test_bit(HCI_STATE_UP, &this->hciDevInfo.flags)) {
    this->state = HCI_STATE_DOWN;
  } else {
    hci_le_set_advertise_enable(this->hciSocket, 1, 1000);
    if (hci_le_set_advertise_enable(this->hciSocket, 0, 1000) == -1) {
      switch (errno) {
        case EPERM:
          this->state = HCI_STATE_UNAUTHORIZED;
          break;

        case EIO:
          this->state = HCI_STATE_UNSUPPORTED;
          break;

        default:
          this->state = HCI_STATE_UNKNOWN;
          break;
      }
    } else {
      this->state = HCI_STATE_UP;
      this->socketClosed = false;
    }
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
    // TODO: Throw exception
  }
}

void
HCI::setAdvertisingData(uint8_t* data, uint8_t length)
{
  int timeout = DEFAULT_TIMEOUT;

  le_set_advertising_data_cp cp;
  memset(&cp, 0, sizeof(cp));
  cp.length = length;
  memcpy(&cp.data, data, sizeof(cp.data));

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
    // TODO: Throw exception
  }

  if (status) {
    errno = EIO;
    // TODO: Throw exception
  }
}

void
HCI::startAdvertising(uint8_t* data, uint8_t length)
{
  if (isAdvertising) {
    stopAdvertising();
  }

  setAdvertisingData(data, length);

  hci_le_set_advertise_enable(getHCISocket(), 1, DEFAULT_TIMEOUT);
}

void
HCI::stopAdvertising()
{
  if (isAdvertising) {
    hci_le_set_advertise_enable(getHCISocket(), 0, DEFAULT_TIMEOUT);
  }
}

// Node.js initialization
void
HCI::Init(Handle<Object> exports)
{
  exports->Set(String::NewSymbol("startAdvertising"),
      FunctionTemplate::New(StartAdvertising)->GetFunction());
}
