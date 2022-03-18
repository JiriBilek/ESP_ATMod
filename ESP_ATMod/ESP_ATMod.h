/*
 * ESP_ATMod.h
 *
 * Copyright 2020, Jiri Bilek, https://github.com/JiriBilek
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef ESP_ATMOD_H_
#define ESP_ATMOD_H_

#include <ESP8266WiFi.h>

/*
 * Types
 */

enum clientTypes_t
{
	TYPE_TCP = 0,
	TYPE_UDP,
	TYPE_SSL,
	TYPE_NONE = 99
};

typedef struct
{
	WiFiClient *client;
	clientTypes_t type;
	uint16_t sendLength;
	uint16_t lastAvailableBytes;
	uint32_t lastActivityMillis;
} client_t;

typedef struct
{
	uint32_t ip;
	uint32_t gw;
	uint32_t mask;
} ipConfig_t;

typedef struct
{
	uint32_t dns1;
	uint32_t dns2;
} dnsConfig_t;

/*
 * Globals
 */

extern client_t clients[5];

extern const uint8_t SERVERS_COUNT;
extern WiFiServer servers[];

const uint16_t INPUT_BUFFER_LEN = 100;

extern uint8_t inputBuffer[INPUT_BUFFER_LEN]; // Input buffer
extern uint16_t inputBufferCnt;				  // Number of bytes in inputBuffer
extern uint8_t fingerprint[20];				  // SHA-1 certificate fingerprint for TLS connections
extern bool fingerprintValid;
extern BearSSL::X509List CAcert;   // CA certificate for TLS validation
extern size_t maximumCertificates; // Maximum amount of certificates to load

extern char *PemCertificate;		 // Buffer for loading a certificate
extern uint16_t PemCertificatePos;	 // Position in buffer while loading
extern uint16_t PemCertificateCount; // Number of chars read

extern uint16_t dataRead; // Number of bytes read from the input to a send buffer

/*
 * Global settings
 */

extern bool gsEchoEnabled;		// command ATE
extern uint8_t gsCipMux;		// command AT+CIPMUX
extern uint8_t gsCipdInfo;		// command AT+CIPDINFO
extern uint8_t gsCwDhcp;		// command AT+CWDHCP
extern bool gsFlag_Connecting;	// Connecting in progress
extern bool gsFlag_Busy;		// Command is busy other commands ignored
extern int8_t gsLinkIdReading;	// Link id for which are the data read
extern bool gsCertLoading;		// AT+CIPSSLCERT in progress
extern bool gsWasConnected;		// Connection flag for AT+CIPSTATUS
extern uint8_t gsCipSslAuth;	// command AT+CIPSSLAUTH: 0 = none, 1 = fingerprint, 2 = certificate chain
extern uint8_t gsCipRecvMode;	// command AT+CIPRECVMODE
extern ipConfig_t gsCipStaCfg;	// command AT+CIPSTA_CUR
extern dnsConfig_t gsCipDnsCfg; // command AT+CIPDNS
extern uint16_t gsCipSslSize;	// command AT+CIPSSLSIZE
extern bool gsSTNPEnabled;		// command AT+CIPSNTPCFG
extern int8_t gsSTNPTimezone;	// command AT+CIPSNTPCFG
extern String gsSNTPServer[3];	// command AT+CIPSNTPCFG
extern uint8_t gsServersMaxConn;	// command AT+CIPSERVERMAXCONN
extern uint32_t gsServerConnTimeout;	// command AT+CIPSSTO

extern const char APP_VERSION[];
extern const char MSG_OK[] PROGMEM;
extern const char MSG_ERROR[] PROGMEM;
extern const uint16_t MAX_PEM_CERT_LENGTH;

/*
 * Public functions
 */

void DeleteClient(uint8_t index);
void setDhcpMode();
void setDns();
int SendData(int clientIndex, int maxSize);

const char *nullIfEmpty(String &s);

#endif /* ESP_ATMOD_H_ */
