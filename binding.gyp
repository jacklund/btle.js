{
  "targets": [
  {
    "target_name": "btle",
      "sources": [
        "src/btio.c",
        "src/att.cc",
        "src/btleException.cc",
        "src/btleConnection.cc",
        "src/connection.cc",
        "src/hci.cc",
        "src/server.cc",
        "src/serverInterface.cc",
        "src/util.cc"
      ],
      "link_settings": {
        "libraries": [
          "-lbluetooth"
          ]
      }

  }
  ]
}
