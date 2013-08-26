{
  "targets": [
  {
    "target_name": "btleConnection",
      "sources": [ "src/btio.c", "src/gatt.cc", "src/gattException.cc", "src/btleConnection.cc", "src/util.cc" ],
      "link_settings": {
        "libraries": [
          "-lbluetooth"
          ]
      }

  }
  ]
}
