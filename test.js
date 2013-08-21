var btle = require('./src/btleConnection');

conn = new btle.BTLEConnection();

conn.on('error', function(err) {
  console.error("Connection error: %s", err);
  conn.close();
});

conn.on('close', function() {
  console.log("Got close");
});

connect_opts = {
  destination: 'BC:6A:29:C3:52:A9',
  type: btle.BtIOType.BT_IO_L2CAP,
  sourceType: btle.BtAddrType.BDADDR_LE_PUBLIC,
  destType: btle.BtAddrType.BDADDR_LE_PUBLIC,
  securityLevel: btle.BtSecurityLevel.BT_SECURITY_LOW,
  cid: 4
}

conn.connect(connect_opts, function(err) {
  console.log("Got connection, err = " + err);
});
