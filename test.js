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
  conn.addNotificationListener(0x25, function(buffer) {
    console.log(buffer);
  });
  conn.readHandle(0x25, function(buffer) {
    console.log(buffer);
    buffer = new Buffer([1]);
    console.log(buffer);
    conn.writeCommand(0x29, buffer, function(err) {
      setTimeout(function() {
        conn.readHandle(0x25, function(buffer) {
          console.log(buffer);
          buffer = new Buffer([1, 0]);
          conn.writeCommand(0x26, buffer);
        });
      }, 1000);
    });
  });
});
