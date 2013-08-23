{
  "targets": [
  {
    "target_name": "btleConnection",
      "sources": [ "src/btio.c", "src/gatt.cc", "src/gattException.cc", "src/btleConnection.cc" ],
      "link_settings": {
        "libraries": [
          "-lbluetooth"
          ]
      }

  }
  ]
}
