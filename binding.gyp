{
  "targets": [
  {
    "target_name": "btleConnection",
      "sources": [ "src/btio.c", "src/btleConnection.cc" ],
      "cflags": [
        "-fpermissive"
        ],
      "link_settings": {
        "libraries": [
          "-lbluetooth"
          ]
      }

  }
  ]
}
