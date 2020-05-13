# ESP 8266 AT Firmware - modified for TLS 1.2

This firmware comes as an [Arduino esp8266](https://github.com/esp8266/Arduino) sketch.

## Purpose

The AT firmware provided by Espressif comes with basic TLS ciphersuites only. Especially, the lack of GCM-based ciphersuites makes the SSL part of the Espressif's firmware unusable on more and more web sites. This firmware addresses this issue.

**The goal was to enable all modern ciphersuites implemented in BearSSL library included in esp8266/Arduino project.**

The firmware fits into 1024 KB flash and can be run even on ESP-01 module with 8 Mbit flash.

## Status

The firmware is still in work-in-progress state. It has been tested and is running on my devices but there might be deviations from the expected behaviour.
My testing environment uses the built-in [Arduino WifiEsp library](https://github.com/bportaluri/WiFiEsp).

## Installation

First you have to install Arduino IDE and the core for the ESP8266 chip (see [https://github.com/esp8266/Arduino](https://github.com/esp8266/Arduino)).
Next get all source files from this repository, place them in a folder named **ESP_ATMod** and compile and upload to your ESP module.

After flashing, the application will open serial connection on RX and TX pins with 115200 Bd, 8 bits, no parity. You can talk with the module using a serial terminal of your choice.

## Description

The firmware does not implement the whole set of AT commands defined in Espressif's documentation.

The major differences are:

1. Only the station mode (AT+CWMODE=1) is supported, no AP or mixed mode.

2. Only DHCP mode is implemented for now.

3. Only active transmission of received data is implemented (AT+CIPRECVMODE=0)

4. Only TCP mode (with or without TLS) is supported, no UDP.

5. In multiplex mode (AT+CWMODE=1), 5 simultaneous connections are available. Due to memory constraints, there can be only one TLS (SSL) connection at a time.

6. For now, there is no TLS authentication security (neither fingerprint nor certificate chain verification). It will be added soon. 

## AT Command List

In the following table, the list of supported AT commands is given. In the comment, only a difference between this implementation and the original Espressif's AT command firmware is given. Please refer to the Espressif's documentation for further information.

| Command | Description |
| - | - |
| AT | Test |
| AT+RST | Restart the module |
| AT+GMR | Print version information |
| ATE | AT commands echoing |
| AT+RESTORE | Restore initial settings |
| AT+UART, AT+UART_CUR | Current UART configuration |
| AT+UART_DEF | Default UART configuration |
| AT+SYSRAM | Print remaining RAM space |
| AT+RFMODE | Set the physical wifi mode:<br>AT+RFMODE=&lt;n&gt;<br>1 - IEEE 802.11b<br>2 - IEEE 802.11g<br>3 - IEEE 802.11n<br>Query the physical wifi mode:<br>AT+RFMODE=?
|  |  |
| AT+CWMODE | Only AT+CWMODE=1 (Station mode) implemented |
| AT+CWJAP, AT+CWJAP_CUR | Connect to AP, parameter &lt;pci_en&gt; not implemented |
| AT+CWJAP_DEF | Connect to AP, saved to flash. Parameter &lt;pci_en&gt; not implemented |
| AT+CWQAP | Disconnect from the AP |
| AT+CWDHCP, AT+CWDHCP_CUR | Enable/disable DHCP - only station mode enabling works |
| AT+CWDHCP_DEF | Enable/disable DHCP saved to flash - only station mode enabling works |
| AT+CWAUTOCONN | Enable/disable auto connecting to AP on start |
| AT+CIPSTA_CUR | Print current IP address, gateway and network mask. Setting is not implemented |
| | |
| AT+CIPSTATUS | Get the connection status |
| AT+CIPSTART | Establish TCP or SSL (TLS) connection. Only one TLS connection at a time |
| AT+CIPSSLSIZE | Implemented, but does nothing |
| AT+CIPSEND | Send data |
| AT+CIPCLOSE | Close the TCP or SSL (TLS) connection |
| AT+CIFSR | Get the local IP address |
| AT+CIPMUX | Enable/disable multiple connections. Max. 5 conections, only one of them can be TLS |
| +IPD | Receive network data |

