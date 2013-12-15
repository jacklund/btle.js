{
  "targets": [
  {
    "target_name": "btle",
      "sources": [
        "src/att.cc",
        "src/btio.c",
        "src/btleException.cc",
        "src/central.cc",
        "src/connection.cc",
        "src/debug.cc",
        "src/hci.cc",
        "src/peripheral.cc",
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
