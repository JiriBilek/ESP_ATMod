# ESP 8266 AT Firmware - modified for TLS 1.2

This firmware comes as an [Arduino esp8266](https://github.com/esp8266/Arduino#arduino-on-esp8266) sketch.

This file refers to version 0.3.0 of the firmware.

## Purpose

The AT firmware provided by Espressif comes with basic TLS ciphersuites only. Especially, the lack of GCM-based ciphersuites makes the SSL part of the Espressif's firmware unusable on more and more web sites. This firmware addresses this issue.

**The goal was to enable all modern ciphersuites implemented in BearSSL library included in esp8266/Arduino project including with some server authentication (server certificate checking).**

The firmware fits into 1024 KB flash and can be run even on ESP-01 module with 8 Mbit flash.

## Description

The firmware does not (and likely will not) implement the whole set of AT commands defined in Espressif's documentation.

The major differences are:

1. Only the station mode (AT+CWMODE=1) is supported, no AP or mixed mode.

2. Only TCP mode (with or without TLS) is supported, no UDP.

3. In multiplex mode (AT+CWMODE=1), 5 simultaneous connections are available. Due to memory constraints, there can be only one TLS (SSL) connection at a time with standard buffer size, more concurrent TLS connections can be made with a reduced buffer size (AT+CIPSSLSIZE). When the buffer size is 512 bytes, all 5 concurrent connections can be TLS.

New features:

1. Implemented TLS security with state-of-the-art ciphersuites: certificate fingerprint checking or certificate chain verification.

2. Implemented TLS MFLN check (RFC 3546), setting TLS receive buffer size, checking MFLN status of a connection.

## Status

The firmware is still in work-in-progress state. It has been tested and is running on my devices but there might be deviations from the expected behaviour.
My testing environment uses the built-in [Arduino WifiEsp library](https://github.com/bportaluri/WiFiEsp) and also the newer [WiFiEspAT library](https://github.com/jandrassy/WiFiEspAT).

## The Future

Next development will be focused on

1. More complete AT command implementation.

## Installation

There are two options for compiling and flashing this library.

### Arduino IDE

First you have to install [Arduino IDE](https://www.arduino.cc/en/software) and the [core](https://github.com/esp8266/Arduino#installing-with-boards-manager) for the ESP8266 chip. Next get all source files from this repository, place them in a folder named **ESP_ATMod** and compile and upload to your ESP module.

After flashing, the module will open serial connection on RX and TX pins with 115200 Bd, 8 bits, no parity. You can talk with the module using a serial terminal of your choice.

**DISCLAIMER**

Use the released Arduino IDE 1 and not the Arduino IDE 2.0 beta version. The beta version is not stable yet and contains a bug that messes up persistent wifi due the certificate uploads to the ESP's filesystem. Futhermore the Arduino IDE 2.0 beta version does not have the LittleFS Filesystem Uploader tool in place.

### PlatformIO

An alternative to using the Arduino IDE is to use PlatformIO.

1. Install [PlatformIO](https://platformio.org/)
2. Make sure that your device is in flashing mode
3. In your favourite terminal and from the root of this repository, run
   the following command to build and upload the sketch to the device:
   ```
   platformio run --target upload
   ```

This has been configured and tested for the ESP-01 Black.

## Add certificates

Certificates are stored in the ESP's filesystem with LittleFS. To add a certificate follow the following steps. 

**IMPORTANT: the certifcate must be in .pem format.**

1. Copy the certificate you want to the data directory in ESP_ATMod
2. Install the [LittleFS Filesystem Uploader](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin#installation) 
3. Select Tools > ESP8266 LittleFS Data Upload menu item. This should start uploading the files into ESP8266 flash file system. When done, IDE status bar will display LittleFS Image Uploaded message. Might take a few minutes for large file system sizes.
4. Now upload the ESP_ATMod sketch to the ESP.
5. The certificate(s) you uploaded are now loaded and ready to use (you can check them with AT+CIPSSLCERT?).
6. (Optional) You may delete the .gitkeep file in the data directory. It is only there to push and pull the data directory in git. Not deleting the .gitkeep file won't do any harm.

## AT Command List

In the following table, the list of supported AT commands is given. In the comment, only a difference between this implementation and the original Espressif's AT command firmware is given. Please refer to the Espressif's documentation for further information.

| Command | Description |
| - | - |
| AT | Test AT startup |
| AT+RST | Restart the module |
| AT+GMR | Print version information |
| ATE | AT commands echoing |
| AT+RESTORE | Restore initial settings |
| AT+UART, AT+UART_CUR | Current UART configuration |
| AT+UART_DEF | Default UART configuration, stored in flash |
| AT+SYSRAM | Print remaining RAM space |
| AT+RFMODE | Set the physical wifi mode - see below |
| AT+SYSCPUFREQ | Set or query the current CPU frequency |
| AT+SYSTIME | Print current time (UTC) |
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
| AT+CIPSSLSIZE | Change the size of the recevier buffer (512, 1024, 2048 or 4096 bytes) |
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
| AT+CIPSSLMFLN | Check if the site supports Maximum Fragment Length Negotiation (MFLN) |
| AT+CIPSSLSTA | Prints the MFLN status of a connection |

## New and Changed Commands

### **AT+RFMODE - Get and Change the Physical Wifi Mode**

Sets and queries the physical wifi mode.

**Query:**

*Syntax:*
```
AT+RFMODE?
```

*Answer:*
```
+RFMODE=1

OK
```

**Set:**

*Syntax:*
```
AT+RFMODE=<mode>
```

*Answer:*
```

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

**Query:**

*Syntax:*
```
AT+CIPSSLAUTH?
```

*Answer:*
```
+CIPSSLAUTH=0

OK
```

**Set:**

*Syntax:*
```
AT+CIPSSLAUTH=<mode>
```

*Answer:*
```

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

Load or print the saved server certificate fingerprint. The fingerprint is based on SHA-1 hash and is exactly 20 bytes long. When connecting, the TLS engine checks the fingerprint of the received certificate against the saved value. It ensures the device is connecting to the expected server. After a successful connection, the fingerprint is checked and is no longer needed for this connection.

The SHA-1 certificate fingerprint for a site can be obtained e.g. in browser while examining the server certificate.

**Query:**

*Syntax:*
```
AT+CIPSSLFP?
```

*Answer:*
```
+CIPSSLFP:"4F:D5:B1:C9:B2:8C:CF:D2:D5:9C:84:5D:76:F6:F7:A1:D0:A2:FA:3D"

OK
```

**Set:**

*Syntax:*
```
AT+CIPSSLFP="4F:D5:B1:C9:B2:8C:CF:D2:D5:9C:84:5D:76:F6:F7:A1:D0:A2:FA:3D"
```

or

```
AT+CIPSSLFP="4FD5B1C9B28CCFD2D59C845D76F6F7A1D0A2FA3D"
```

*Answer:*
```

OK
```

The fingerprint consists of exactly 20 bytes. They are set as hex values and may be divided with ':'.

### **AT+CIPSSLCERT - Load, Query or Delete TLS CA Certificate**

Load, query or delete CA certificate for TLS certificate chain verification. Currently maximum 5 certificates at a time can be loaded. The certificates must be in PEM structure. After a successful connection, the certificate is checked and is no longer needed for this connection.

**Query the first certificate:**

*Syntax:*
```
AT+CIPSSLCERT?
```

*Answer:*
```
+CIPSSLCERT:no cert

OK
```

or

```
+CIPSSLCERT:DST Root CA X3

OK
```

**Query specific certificate:**

*Syntax:*
```
AT+CIPSSLCERT?2
```

*Answer:*
```
+CIPSSLCERT:DST Root CA X3

OK
```

**Set:**

*Syntax:*
```
AT+CIPSSLCERT
```

*Answer:*
```

OK
>
```

You can now send the certificate (PEM encoding), no echo is given. After the last line (`-----END CERTIFICATE-----`), the certificate is parsed and loaded. The certificate should be sent with \n notation. For example [isrg-root-x1-cross-signed.pem](https://letsencrypt.org/certs/isrg-root-x1-cross-signed.pem): 

```
-----BEGIN CERTIFICATE-----
MIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/
MSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT
DkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1ow
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEB
AQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XC
ov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpL
wYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+D
LtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK
4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5
bHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5y
sR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZ
Xmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4
FQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBc
SLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2ql
PRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TND
TwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYw
SwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1
c3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx
+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEB
ATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQu
b3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9E
U1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26Ztu
MA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC
5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW
9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuG
WCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9O
he8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFC
Dfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5
-----END CERTIFICATE-----
```

Should be:
```
-----BEGIN CERTIFICATE-----\nMIIFYDCCBEigAwIBAgIQQAF3ITfU6UK47naqPGQKtzANBgkqhkiG9w0BAQsFADA/\nMSQwIgYDVQQKExtEaWdpdGFsIFNpZ25hdHVyZSBUcnVzdCBDby4xFzAVBgNVBAMT\nDkRTVCBSb290IENBIFgzMB4XDTIxMDEyMDE5MTQwM1oXDTI0MDkzMDE4MTQwM1ow\nTzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\ncmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwggIiMA0GCSqGSIb3DQEB\nAQUAA4ICDwAwggIKAoICAQCt6CRz9BQ385ueK1coHIe+3LffOJCMbjzmV6B493XC\nov71am72AE8o295ohmxEk7axY/0UEmu/H9LqMZshftEzPLpI9d1537O4/xLxIZpL\nwYqGcWlKZmZsj348cL+tKSIG8+TA5oCu4kuPt5l+lAOf00eXfJlII1PoOK5PCm+D\nLtFJV4yAdLbaL9A4jXsDcCEbdfIwPPqPrt3aY6vrFk/CjhFLfs8L6P+1dy70sntK\n4EwSJQxwjQMpoOFTJOwT2e4ZvxCzSow/iaNhUd6shweU9GNx7C7ib1uYgeGJXDR5\nbHbvO5BieebbpJovJsXQEOEO3tkQjhb7t/eo98flAgeYjzYIlefiN5YNNnWe+w5y\nsR2bvAP5SQXYgd0FtCrWQemsAXaVCg/Y39W9Eh81LygXbNKYwagJZHduRze6zqxZ\nXmidf3LWicUGQSk+WT7dJvUkyRGnWqNMQB9GoZm1pzpRboY7nn1ypxIFeFntPlF4\nFQsDj43QLwWyPntKHEtzBRL8xurgUBN8Q5N0s8p0544fAQjQMNRbcTa0B7rBMDBc\nSLeCO5imfWCKoqMpgsy6vYMEG6KDA0Gh1gXxG8K28Kh8hjtGqEgqiNx2mna/H2ql\nPRmP6zjzZN7IKw0KKP/32+IVQtQi0Cdd4Xn+GOdwiK1O5tmLOsbdJ1Fu/7xk9TND\nTwIDAQABo4IBRjCCAUIwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAQYw\nSwYIKwYBBQUHAQEEPzA9MDsGCCsGAQUFBzAChi9odHRwOi8vYXBwcy5pZGVudHJ1\nc3QuY29tL3Jvb3RzL2RzdHJvb3RjYXgzLnA3YzAfBgNVHSMEGDAWgBTEp7Gkeyxx\n+tvhS5B1/8QVYIWJEDBUBgNVHSAETTBLMAgGBmeBDAECATA/BgsrBgEEAYLfEwEB\nATAwMC4GCCsGAQUFBwIBFiJodHRwOi8vY3BzLnJvb3QteDEubGV0c2VuY3J5cHQu\nb3JnMDwGA1UdHwQ1MDMwMaAvoC2GK2h0dHA6Ly9jcmwuaWRlbnRydXN0LmNvbS9E\nU1RST09UQ0FYM0NSTC5jcmwwHQYDVR0OBBYEFHm0WeZ7tuXkAXOACIjIGlj26Ztu\nMA0GCSqGSIb3DQEBCwUAA4IBAQAKcwBslm7/DlLQrt2M51oGrS+o44+/yQoDFVDC\n5WxCu2+b9LRPwkSICHXM6webFGJueN7sJ7o5XPWioW5WlHAQU7G75K/QosMrAdSW\n9MUgNTP52GE24HGNtLi1qoJFlcDyqSMo59ahy2cI2qBDLKobkx/J3vWraV0T9VuG\nWCLKTVXkcGdtwlfFRjlBz4pYg1htmf5X6DYO8A4jqv2Il9DjXA6USbW1FzXSLr9O\nhe8Y4IWS6wY7bCkjCWDcRQJMEhg76fsO3txE+FiYruq9RUWhiF1myv4Q6W+CyBFC\nDfvp7OOGAN6dEOM4+qR9sdjoSYKEBpsr6GtPAQw4dy753ec5\n-----END CERTIFICATE-----
```

The application responds with

```
Read 1952 bytes

OK
```
or with an error message. In case of a successful loading, the certificate is ready to use and you can turn the certificate checking on (`AT+CIPSSLAUTH=2`). 

The limit for the PEM certificate is 4096 characters total. 

**Delete the first certificate:**

*Syntax:*
```
AT+CIPSSLCERT=DELETE
```

*Answer:*
```
+CIPSSLCERT:deleted

OK
```

**Query specific certificate:**

*Syntax:*
```
AT+CIPSSLCERT=DELETE:2
```

*Answer:*
```
+CIPSSLCERT:deleted certificate 2

OK
```

The certificate is deleted from the memory. 

### **AT+CIPRECVMODE, AT+CIPRECVLEN, AT+CIPRECVDATA in SSL mode**

Commands 

- AT+CIPRECVMODE (Set TCP or SSL Receive Mode), 
- AT+CIPRECVLEN (Get TCP or SSL Data Length in Passive Receive Mode)
- AT+CIPRECVDATA (Get TCP or SSL Data in Passive Receive Mode) 

work in SSL mode in the same way as in TCP mode.

### **AT+SYSCPUFREQ - Set or query the Current CPU Frequency**

Sets and queries the COU freqency. The only valid values are 80 and 160 Mhz.

**Query:**

*Syntax:*
```
AT+SYSCPUFREQ?
```

*Answer:*
```
+SYSCPUFREQ=80

OK
```

**Set:**

*Syntax:*
```
AT+SYSCPUFREQ=<freq>
```

*Answer:*
```

OK
```

The value freq may be 80 or 160.

### **AT+CIPSSLSIZE - Set the TLS Receiver Buffer Size**

Sets the TLS receiver buffer size. The size can be 512, 1024, 2048, 4096 or 16384 (default) bytes according to [RFC3546](https://tools.ietf.org/html/rfc3546). The value is used for all subsequent TLS connections, the opened connections are not affected.

*Syntax:*

```
AT+CIPSSLSIZE=512
```

*Answer:*
```

OK
```

### **AT+CIPSSLMFLN - Checks if the given site supports the MFLN TLS Extension**

The Maximum Fragment Length Negotiation extension is useful for lowering the RAM usage by reducing receiver buffer size on TLS connections. Newer TLS implementations support this extension but it would be wise to check the capability before changing a TLS buffer size and making a connection. As the server won't change this feature on the fly, you should test the MFLN capability only once.

*Syntax:*

AT+CIPSSLMFLN="*site*",*port*,*size*

The valid sizes are 512, 1024, 2048 and 4096.

```
AT+CIPSSLMFLN="www.github.com",443,512
```

*Answer:*
```
+CIPSSLMFLN:TRUE

OK
```

### **AT+CIPSSLSTA - Checks the status of the MFLN negotiation**

This command checks the MFLN status on an opened TLS connection.

*Syntax:*

AT+CIPSSLSTA[=linkID]

The *linkID* value is mandatory when the multiplexing is on (AT+CIPMUX=1). It should be not entered when the multiplexing is turned off.

```
AT+CIPSSLSTA=0
```

*Answer:*
```
+CIPSSLSTA:1

OK
```

The returned value of 1 means there was a MFLN negotiation. It holds even with the default receiver buffer size set.

### **AT+SYSTIME - Returns the current time UTC**

This command returns the current time as unix time (number of seconds since January 1st, 1970). The time zone is fixed to GMT (UTC). The time is obtained by querying NTP servers automatically, after connecting to the internet. Before connecting to the internet or in case of an error in communication with NTP servers, the time is unknown. This situation should be temporary.

*Syntax:*
```
AT+SYSTIME?
```

*Answer:*
```
+SYSTIME:1607438042
OK
```

If the current time is unknown, an error message is returned.
