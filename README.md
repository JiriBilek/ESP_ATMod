# ESP 8266 AT Firmware - modified for TLS 1.2

This firmware comes as an [Arduino esp8266](https://github.com/esp8266/Arduino) sketch.

This file refers to version 0.2.2 of the firmware.

## Purpose

The AT firmware provided by Espressif comes with basic TLS ciphersuites only. Especially, the lack of GCM-based ciphersuites makes the SSL part of the Espressif's firmware unusable on more and more web sites. This firmware addresses this issue.

**The goal was to enable all modern ciphersuites implemented in BearSSL library included in esp8266/Arduino project including with some server authentication (server certificate checking).**

The firmware fits into 1024 KB flash and can be run even on ESP-01 module with 8 Mbit flash.

## Status

The firmware is still in work-in-progress state. It has been tested and is running on my devices but there might be deviations from the expected behaviour.
My testing environment uses the built-in [Arduino WifiEsp library](https://github.com/bportaluri/WiFiEsp).

## Installation

First you have to install Arduino IDE and the core for the ESP8266 chip (see [https://github.com/esp8266/Arduino](https://github.com/esp8266/Arduino)).
Next get all source files from this repository, place them in a folder named **ESP_ATMod** and compile and upload to your ESP module.

After flashing, the module will open serial connection on RX and TX pins with 115200 Bd, 8 bits, no parity. You can talk with the module using a serial terminal of your choice.

## Description

The firmware does not (and likely will not) implement the whole set of AT commands defined in Espressif's documentation.

The major differences are:

1. Only the station mode (AT+CWMODE=1) is supported, no AP or mixed mode.

2. Only TCP mode (with or without TLS) is supported, no UDP.

3. In multiplex mode (AT+CWMODE=1), 5 simultaneous connections are available. Due to memory constraints, there can be **only one TLS (SSL) connection** at a time.

New features:

1. Implemented TLS security with state-of-the-art ciphersuites: certificate fingerprint checking or certificate chain verification (only one CA certificate can be used).

## The Future

Next development will be focused on

1. Enabling LittleFS and storing certificates there, the idea is to validate TLS connection against the list of common CA certificates stored locally. No CRL though.

2. More complete AT command implementation.


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
| AT+UART_DEF | Default UART configuration, stored in flash |
| AT+SYSRAM | Print remaining RAM space |
| AT+RFMODE | Set the physical wifi mode - see below |
|  |  |
| AT+CWMODE | Only AT+CWMODE=1 (Station mode) implemented |
| AT+CWJAP, AT+CWJAP_CUR | Connect to AP, parameter &lt;pci_en&gt; not implemented |
| AT+CWJAP_DEF | Connect to AP, saved to flash. Parameter &lt;pci_en&gt; not implemented |
| AT+CWQAP | Disconnect from the AP |
| AT+CWDHCP, AT+CWDHCP_CUR | Enable/disable DHCP - only station mode enabling works |
| AT+CWDHCP_DEF | Enable/disable DHCP saved to flash - only station mode enabling works |
| AT+CWAUTOCONN | Enable/disable auto connecting to AP on start |
| AT+CIPSTA, AT+CIPSTA_CUR | Set and/or print current IP address, gateway and network mask |
| AT+CIPSTA_DEF | Set and/or print current IP address, gateway and network mask, stored in flash |
| AT+CIPDNS, AT+CIPDNS_CUR | Enable and disable static DNS, set and/or print the DNS server addresses |
| AT+CIPDNS_DEF | Default DNS setting, stored in flash |
| | |
| AT+CIPSTATUS | Get the connection status |
| AT+CIPSTART | Establish TCP or SSL (TLS) connection. Only one TLS connection at a time |
| AT+CIPSSLSIZE | Implemented, but does nothing |
| AT+CIPSEND | Send data |
| AT+CIPCLOSE | Close the TCP or SSL (TLS) connection |
| AT+CIFSR | Get the local IP address |
| AT+CIPMUX | Enable/disable multiple connections. Max. 5 conections, only one of them can be TLS |
| AT+CIPRECVMODE | Set TCP or SSL Receive Mode |
| AT+CIPRECVLEN | Get TCP or SSL Data Length in Passive Receive Mode |
| AT+CIPRECVDATA | Get TCP or SSL Data in Passive Receive Mode |
| +IPD | Receive network data |
| | |
| AT+CIPSSLAUTH | Set and query the TLS authentication mode - see below |
| AT+CIPSSLFP | Load or print the TLS server certificate fingerprint - see below |
| AT+CIPSSLCERT | Load, query or delete TLS CA certificate - see below |

## New and Changed Commands

### **AT+RFMODE - Get and Change the Physical Wifi Mode**

Sets and queries the physical wifi mode.

**Syntax:**

Query:

```
AT+RFMODE?
+RFMODE=1

OK
```

Set:

```
AT+RFMODE=<mode>

OK
```

The allowed values of &lt;mode&gt; are:

| Mode | Description |
| - | - |
| 1 | IEEE 802.11b |
| 2 | IEEE 802.11g |
| 3 | IEEE 802.11n |

### **AT+CIPSSLAUTH - Set and Query the TLS Authentication Mode**

Set or queries the selected TLS authentication mode. The default is no authentication. Try to avoid this because it is insecure and prone to MITM attack.

**Syntax:**

Query:

```
AT+CIPSSLAUTH?
+CIPSSLAUTH=0

OK
```

Set:

```
AT+CIPSSLAUTH=<mode>

OK
```

The allowed values of &lt;mode&gt; are:

| Mode | Description |
| - | - |
| 0 | No authentication. Default. Insecure |
| 1 | Server certificate fingerprint checking |
| 2 | Certificate chain checking |

Switching to mode 1 succeeds only when the certificate SHA-1 fingerprint is preloaded (see AT+CIPSSLFP).

Switching to mode 2 succeeds only when the CA certificate preloaded (see AT+CIPSSLCERT).


### **AT+CIPSSLFP - Load or Print TLS Server Certificate SHA-1 Fingerprint**

Load or print the saved server certificate fingerprint. The fingerprint is based on SHA-1 hash and is exactly 20 bytes long. When connecting, the TLS engine checks the fingerprint of the received certificate against the saved value. It ensures the device is connecting to the expected server. 

The SHA-1 certificate fingerprint for a site can be obtained e.g. in browser while examining the server certificate.

**Syntax:**

Query:

```
AT+CIPSSLFP?
+CIPSSLFP:"4F:D5:B1:C9:B2:8C:CF:D2:D5:9C:84:5D:76:F6:F7:A1:D0:A2:FA:3D"

OK
```

Set:

```
AT+CIPSSLFP="4F:D5:B1:C9:B2:8C:CF:D2:D5:9C:84:5D:76:F6:F7:A1:D0:A2:FA:3D"

OK
```
```
AT+CIPSSLFP="4FD5B1C9B28CCFD2D59C845D76F6F7A1D0A2FA3D"

OK
```

The fingerprint consists of exactly 20 bytes. They are set as hex values and may be divided with ':'.

### **AT+CIPSSLCERT - Load, Query or Delete TLS CA Certificate**

Load, query or delete CA certificate for TLS certificate chain verification. ONly one certificate at a time can be loaded. The certificate must be in PEM structure.

**Syntax:**

Query:

```
AT+CIPSSLCERT?
+CIPSSLCERT:no cert

OK
```

```
AT+CIPSSLCERT?
+CIPSSLCERT:DST Root CA X3

OK
```

Set:

```
AT+CIPSSLCERT

OK
>
```

You can now send the certificate (PEM encoding), no echo is given. After the last line (`-----END CERTIFICATE-----`), the certificate is parsed and loaded. The application responds with

```
Read 1216 bytes

OK
```
or with an error message. In case of a successful loading, the certificate is ready to use and you can turn the certificate checking on (`AT+CIPSSLAUTH=2`). 

The limit for the PEM certificate is 4096 characters total.

Delete:

```
AT+CIPSSLCERT=DELETE
+CIPSSLCERT:deleted

OK
```
The certificate is deleted from the memory. 

### **AT+CIPRECVMODE, AT+CIPRECVLEN, AT+CIPRECVDATA in SSL mode**

Commands 

- AT+CIPRECVMODE (Set TCP or SSL Receive Mode), 
- AT+CIPRECVLEN (Get TCP or SSL Data Length in Passive Receive Mode)
- AT+CIPRECVDATA (Get TCP or SSL Data in Passive Receive Mode) 

work in SSL mode in the same way as in TCP mode.
