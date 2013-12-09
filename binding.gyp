{
  "targets": [
  {
    "target_name": "btle",
      "sources": [
        "src/btio.c",
        "src/att.cc",
        "src/btleException.cc",
        "src/peripheral.cc",
        "src/connection.cc",
        "src/hci.cc",
        "src/central.cc",
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
