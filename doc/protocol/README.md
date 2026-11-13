# Anycubic ACE Pro Communication Protocol

The ESP32-S3 must be built with `USBMode=default` (USB-OTG/TinyUSB) and `CDCOnBoot=default` for USB host operation. The USB host and native USB Serial/JTAG share the USB PHY; use the DevKitC-1 USB-to-UART connector for `Serial` monitoring.

Sources:
- https://github.com/Jupsi/ACEPRO
- https://github.com/printers-for-people/ACEResearch/blob/main/PROTOCOL.md