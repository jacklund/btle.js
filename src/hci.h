#ifndef HCI_H
#define HCI_H

#include <node.h>

class HCI
{
public:
  enum HCIState {
    HCI_STATE_DOWN,
    HCI_STATE_UNAUTHORIZED,
    HCI_STATE_UNSUPPORTED,
    HCI_STATE_UNKNOWN,
    HCI_STATE_UP
  };

  HCI();
  virtual ~HCI();

  // Node.js stuff
  static void Init(Handle<Object> exports);
  static void StartAdvertising(const Arguments& args);

private:
  int getAdvType(Local<Value>& arg);
  int getHCISocket();
  HCIState getAdapterState();
  void setAdvertisingParameters(le_set_advertising_parameters_cp& params);
  void setAdvertisingData(uint8_t* data, uint8_t length);
  void startAdvertising(uint8_t* data, uint8_t length);
  void stopAdvertising();

  int hciSocket;
  HCIState state;
  bool socketClosed;
  struct hci_dev_info hciDevInfo;
  bool isAdvertising;
};

#endif
