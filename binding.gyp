{
  "targets": [
  {
    "target_name": "btleConnection",
      "sources": [ "src/btio.c", "src/btleConnection.cc" ],
      "link_settings": {
        "libraries": [
          "-lbluetooth"
          ]
      }

  }
  ]
}
