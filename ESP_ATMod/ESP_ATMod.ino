/*
 * ESP_ATMod
 *
 * Modified AT command processor for ESP8266
 * Implements only a subset of AT commands
 * Main goal is to enable safe TLS 1.2 - uses all the state-of-the-art ciphersuits from BearSSL
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

/*
 * Version history:
 *
 * 0.1.0: First version for testing
 * 0.1.1: TLS fingerprint authentication (AT+CIPSSLAUTH, AT+CIPSSLFP)
 * 0.1.2: TLS CA certificate checking (AT+CIPSSLCERT)
 * 0.2.0: AT Version 1.7 - AT+CIPRECVMODE, AT+CIPRECVLEN, AT+CIPRECVDATA
 * 0.2.1: Fix the _CUR and _DEF command suffixes: empty suffix is equivalent to _DEF
 * 0.2.2: Full commands AT+CWDHCP (for station mode), AT+CIPSTA and AT+CIPDNS
 * 0.2.3: AT+SYSCPUFREQ (80 or 160) - inspired by LoBo AT, useful for TLS connections
 *        Resize the input buffer (AT+CIPSSLSIZE) - supported values: 512, 1024, 2048, 4096, 16384 (RFC 3546)
 *        Check the site MFLN capability (AT+CIPSSLMFLN), check the connection MFLN status (AT+CIPSSLSTA)
 * 0.2.4: AT+SYSTIME? for returning unixtime
 *
 * TODO:
 * - Implement AT+CWLAP
 * - TLS Security - list of certificates in FS, persistent fingerprint and single certificate, AT+CIPSSLAUTH_DEF
 * - Implement AT+CIPSNTPCFG, AT+CIPSNTPTIME
 */

#include "Arduino.h"
#include "string.h"

#include <ESP8266WiFi.h>
#include <PolledTimeout.h>

extern "C" {
#include "user_interface.h"

#include "lwip/dns.h"
}

#include "ESP_ATMod.h"
#include "WifiEvents.h"
#include "command.h"
#include "settings.h"
#include "debug.h"

/*
 * Defines
 */

const char APP_VERSION[] = "0.2.4";

/*
 * Constants
 */

const char MSG_OK[] PROGMEM = "\r\nOK\r\n";
const char MSG_ERROR[] PROGMEM = "\r\nERROR\r\n";

const uint16_t MAX_PEM_CERT_LENGTH = 4096;  // Maximum size of a certificate loaded by AT+CIPSSLCERT

/*
 * Global variables
 */

uint8_t inputBuffer[INPUT_BUFFER_LEN];  // Input buffer
uint16_t inputBufferCnt;  // Number of bytes in inputBuffer

WiFiEventHandler onConnectedHandler;
WiFiEventHandler onGotIPHandler;
WiFiEventHandler onDisconnectedHandler;

client_t clients[5] = { { nullptr, TYPE_NONE, 0, 0 },
						{ nullptr, TYPE_NONE, 0, 0 },
						{ nullptr, TYPE_NONE, 0, 0 },
						{ nullptr, TYPE_NONE, 0, 0 },
						{ nullptr, TYPE_NONE, 0, 0 } };

uint8_t sendBuffer[2048];
uint16_t dataRead = 0;  // Number of bytes read from the input to a send buffer

// TLS specific variables

uint8_t fingerprint[20];  // SHA-1 certificate fingerprint for TLS connections
bool fingerprintValid;
BearSSL::X509List *CAcert = nullptr;  // CA certificate for TLS validation

char *PemCertificate = nullptr;  // Buffer for loading a certificate
uint16_t PemCertificatePos;  // Position in buffer while loading
uint16_t PemCertificateCount;  // Number of chars read

/*
 *  Global settings
 */
bool gsEchoEnabled = true;  // command ATE
uint8_t gsCipMux = 0;  // command AT+CIPMUX
uint8_t gsCipdInfo = 0;  // command AT+CIPDINFO
uint8_t gsCwDhcp = 3;  // command AT+CWDHCP_CUR
bool gsFlag_Connecting = false;  // Connecting in progress (CWJAP) - other commands ignored
int8_t gsLinkIdReading = -1;  // Link id where the data is read
bool gsCertLoading = false;  // AT+CIPSSLCERT in progress
bool gsWasConnected = false;  // Connection flag for AT+CIPSTATUS
uint8_t gsCipSslAuth = 0;  // command AT+CIPSSLAUTH: 0 = none, 1 = fingerprint, 2 = certificate chain
uint8_t gsCipRecvMode = 0;  // command AT+CIPRECVMODE
ipConfig_t gsCipStaCfg = { 0, 0, 0 };  // command AT+CIPSTA
dnsConfig_t gsCipDnsCfg = { 0, 0 };  // command AT+CIPDNS
uint16_t gsCipSslSize = 16384;  // command AT+CIPSSLSIZE

/*
 *  The setup function is called once at startup of the sketch
 */
void setup()
{
	// Default static net configuration
	gsCipStaCfg = Settings::getNetConfig();

	// Default DNS configuration
	gsCipDnsCfg = Settings::getDnsConfig();
	setDns();

	// Default DHCP configuration
	gsCwDhcp = Settings::getDhcpMode();
	setDhcpMode();

	// Default UART configuration
	uint32_t baudrate = Settings::getUartBaudRate();
	SerialConfig config = Settings::getUartConfig();
	Serial.begin(baudrate, config);

	// Initialization of variables
	inputBufferCnt = 0;
	memset(fingerprint, 0, sizeof(fingerprint));
	fingerprintValid = false;

    // Register event handlers.
    // Call "onStationConnected" each time a station connects
	onConnectedHandler = WiFi.onStationModeConnected(&onStationConnected);
    // Call "onStationModeGotIP" each time a station fully connects
	onGotIPHandler = WiFi.onStationModeGotIP(&onStationGotIP);
    // Call "onStationDisconnected" each time a station disconnects
	onDisconnectedHandler = WiFi.onStationModeDisconnected(&onStationDisconnected);

	// Set the WiFi defaults
	WiFi.mode(WIFI_STA);
	WiFi.persistent(false);
	WiFi.setAutoReconnect(false);

	Serial.println(F("\r\nready"));
}

/*
 *  The loop function is called in an endless loop
 */
void loop()
{
	bool lineCompleted = false;

	// Check for data and closed connections - only when we can transmit data

	if (Serial.availableForWrite())
	{
		uint8_t maxCli = 0;  // Maximum client number
		if (gsCipMux == 1)
			maxCli = 4;

		for (uint8_t i = 0; i <= maxCli; ++i)
		{
			WiFiClient *cli = clients[i].client;

			if (cli != nullptr)
			{
				int avail = cli->available();

				if (avail > clients[i].lastAvailableBytes)  // For RECVMODE it is every non zero avail
				{
					if (gsCipRecvMode == 0)
					{
						SendData(i, 0);
					}
					else  // CIPRECVMODE = 1
					{
						clients[i].lastAvailableBytes = avail;

						Serial.print(F("\r\n+IPD,"));

						if (gsCipMux == 1)
						{
							Serial.print(i);
							Serial.print(',');
						}

						Serial.println(avail);
					}
				}

				if (cli->available() == 0 && !cli->connected())
				{
					if (gsCipMux == 1)
						Serial.printf_P(PSTR("%d,"), i);

					Serial.println(F("CLOSED"));

					DeleteClient(i);
				}
			}
		}
	}

	// Read the serial port into the input or send buffer
	int avail = Serial.available();
	while (avail > 0)
	{
		// Check for EOF and errors
		int c = Serial.peek();
		if (c < 0)
			break;

		c = Serial.read() & 0xff;

		if (gsLinkIdReading >= 0)
		{
			sendBuffer[dataRead] = c;

			if (++dataRead >= clients[gsLinkIdReading].sendLength)
			{
				Serial.printf_P(PSTR("\r\nRecv %d bytes\r\n"), clients[gsLinkIdReading].sendLength);

				// Send the data to the client
				size_t s = clients[gsLinkIdReading].client->write(sendBuffer, clients[gsLinkIdReading].sendLength);

				if (s == clients[gsLinkIdReading].sendLength)
					Serial.println(F("\r\nSEND OK"));
				else
				{
					Serial.println(F("\r\nSEND FAIL"));
					if (clients[gsLinkIdReading].client->connected())
						clients[gsLinkIdReading].client->stop();
				}

				// Stop data reading
				gsLinkIdReading = -1;
				dataRead = 0;
			}
		}
		else if (gsCertLoading)
		{
			++PemCertificateCount;

			// base 64 characters and hyphen for header and footer
			if (isAlphaNumeric(c) || strchr("/+= -\r\n", c) != nullptr)
			{
				// Check newlines - the header and footer must be separated by at least one '\n' from the base64 certificate data
				if (c == '\r')
					c = '\n';

				// Ignore multiple newlines, save space in the buffer
				if (c != '\n' || (PemCertificatePos > 0 && PemCertificate[PemCertificatePos - 1] != '\n'))
				{
					PemCertificate[PemCertificatePos++] = c;

					// Check the end
					if (c == '-' && PemCertificatePos > 100)
					{
						if (! memcmp_P(PemCertificate + PemCertificatePos - 25, PSTR("-----END CERTIFICATE-----"), 25))
						{
							Serial.printf_P(PSTR("\r\nRead %d bytes\r\n"), PemCertificateCount);

							// Process the certificate

							PemCertificate[PemCertificatePos] = '\0';
							// Serial.print(PemCertificate);

							BearSSL::X509List *certList = new BearSSL::X509List(PemCertificate);

							delete PemCertificate;
							PemCertificate = nullptr;
							PemCertificatePos = 0;

							gsCertLoading = false;

							if (certList->getCount() == 1)
							{
								if (CAcert != nullptr)
									delete CAcert;

								CAcert = certList;

								Serial.printf_P(MSG_OK);
							}
							else
							{
								Serial.println(F("no certificate"));
								Serial.printf_P(MSG_ERROR);
							}
						}
					}
					if (PemCertificatePos > MAX_PEM_CERT_LENGTH - 1)
					{
						gsCertLoading = false;
						Serial.printf_P(MSG_ERROR);  // Invalid data
					}
				}
			}
			else if (c < ' ')
			{}
			else  // illegal character in certificate
			{
				gsCertLoading = false;
				Serial.printf_P(MSG_ERROR);  // Invalid data
			}
		}
		else if (inputBufferCnt < INPUT_BUFFER_LEN)
		{
			if (gsEchoEnabled)
				Serial.write(c);

/*			if (inputBufferCnt == 0 && c != 'A')  // Wait for 'A' as the start of the command
			{}
			else*/  // FIXME: problematic, some libraries send garbage to check if the module is alive
			{
				inputBuffer[inputBufferCnt++] = c;

				if (c == '\n')  // LF (0x0a)
				{
					lineCompleted = true;
					break;
				}
			}
		}
		else
		{
			inputBufferCnt = 0;
			Serial.printf_P(MSG_ERROR);  // Buffer overflow
		}

		yield();
	}

	// Are we connecting now?
	if (gsFlag_Connecting)
	{
		station_status_t status = wifi_station_get_connect_status();

#if defined(AT_DEBUG)
		static station_status_t s = (station_status_t)100;

		if (s != status)
		{
			AT_DEBUG_PRINTF("--- status: %d\r\n", status);
			s = status;
		}
#endif

		switch (status)
		{
		case STATION_GOT_IP:
			Serial.println(F("\r\nOK"));
			gsFlag_Connecting = false;

			// Set up time to allow for certificate validation
			if (time(nullptr) < 8 * 3600 * 2)
				configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

			// Redefine dns to the static ones
			if (gsCipDnsCfg.dns1 != 0)
				setDns();

			break;

		case STATION_NO_AP_FOUND:
			Serial.println(F("\r\n+CWJAP:3\r\nFAIL"));
			gsFlag_Connecting = false;
			break;

		case STATION_CONNECT_FAIL:
			Serial.println(F("\r\n+CWJAP:4\r\nFAIL"));
			gsFlag_Connecting = false;
			break;

		case STATION_WRONG_PASSWORD:
			Serial.println(F("\r\n+CWJAP:2\r\nFAIL"));
			gsFlag_Connecting = false;
			break;

//		case STATION_IDLE:
			//return WL_IDLE_STATUS;
		default:
			break; // return WL_DISCONNECTED;
		}

		// Hack: while connecting we need the autoconnect feature to be switched on
		if (!gsFlag_Connecting)
			WiFi.setAutoReconnect(false);
	}

	// Check for a new command while connecting
	if (lineCompleted)
	{
		// Check for busy condition
		if (gsFlag_Connecting)
		{
			Serial.println(F("\r\nbusy p..."));

			// Discard the input buffer
			inputBufferCnt = 0;
		}

		// Check for a new command
		else
		{
			processCommandBuffer();
		}
	}
}

/*
 * Deleted the client objects and resets the client structure
 */
void DeleteClient(uint8_t index)
{
	// Check the input
	if (index > 4)
		return;

	client_t *cli = &(clients[index]);

	if (cli->client != nullptr)
	{
		delete cli->client;
		cli->client = nullptr;
		AT_DEBUG_PRINTF("--- client deleted: %d\r\n", index);
	}

	if (index == gsLinkIdReading)
		gsLinkIdReading = -1;

	cli->sendLength = 0;
	cli->type = TYPE_NONE;
}

/*
 * Set DHCP Mode
 * On stopping client DHCP, sets the network configuration
 */
void setDhcpMode()
{
	if (gsCwDhcp & 2)
	{
		WiFi.config(0, 0, 0);  // Enable Station DHCP
	}
	else
	{
		if (gsCipStaCfg.ip != 0 && gsCipStaCfg.gw != 0 && gsCipStaCfg.mask != 0)
		{
			// Configure the network
			WiFi.config(gsCipStaCfg.ip, gsCipStaCfg.gw, gsCipStaCfg.mask);
			setDns();
		}
		else
		{
			// Manually stop as WiFi.config performs tests on ip addresses
			wifi_station_dhcpc_stop();
		}
	}
}

/*
 * Set DNS servers
 */
void setDns()
{
	if (gsCipDnsCfg.dns1 != 0)
	{
		dns_setserver(0, IPAddress(gsCipDnsCfg.dns1));

		if (gsCipDnsCfg.dns2 != 0)
			dns_setserver(1, IPAddress(gsCipDnsCfg.dns2));
		else
			dns_setserver(1, nullptr);
	}
	else
	{
		// Default DNS server 64.6.64.6 (Verisign Free DNS)
		dns_setserver(0, IPAddress(64,6,64,6));
		dns_setserver(1, nullptr);
	}
}

/*
 * Send data in a +IPD message (for AT+CIPRECVMODE=0) or +CIPRECVDATA (for AT+CIPRECVMODE=1)
 * Returns number of bytes sent or 0 (error)
 */
int SendData(int clientIndex, int maxSize)
{
	const char *respText[2] = { "+IPD", "+CIPRECVDATA" };

	WiFiClient *cli = clients[clientIndex].client;
	int bytes;

	if (cli == nullptr)
		return 0;

	int avail = cli->available();

	if (avail == 0)
		return 0;

	if (maxSize > 0 && maxSize < avail)
		avail = maxSize;

	uint8_t *buf = new uint8_t[avail];

	if (buf != nullptr)
	{
		Serial.println();
		Serial.print(respText[gsCipRecvMode]);

		/* FIXME: Weird behaviour of the original firmware when CIPRECVMODE=1:
		 * It responds +CIPRECVDATA,<size> regardless of CIPMUX setting. It doesn't
		 * return the link id.
		 */
		if (gsCipMux == 1 && gsCipRecvMode == 0)
		{
			Serial.printf_P(PSTR(",%d"), clientIndex);
		}

		Serial.printf_P(PSTR(",%d"), avail);

		if (gsCipdInfo == 1 && gsCipRecvMode == 0)  // No CIPDINFO for CIPRECVDATA
		{
			IPAddress ip = cli->remoteIP();

			Serial.printf_P(PSTR(",%s,%d"), ip.toString().c_str(), cli->remotePort());
		}

		Serial.print(':');

		bytes = cli->readBytes(buf, avail);
		int bytesToSend = bytes;
		uint8_t *bufPtr = buf;

		while (bytesToSend > 0)
		{
			// Send data in chunks of 100 bytes to avoid wdt reset
			int txBytes = bytesToSend;
			if (bytesToSend > 100)
				txBytes = 100;

			// Wait for tx empty
			esp8266::polledTimeout::oneShot waitForTxReadyTimeout(500);

			while (!Serial.availableForWrite() && !waitForTxReadyTimeout)
			{
			}

			// In case of a timeout stop the transmission with an error
			if (waitForTxReadyTimeout)
				break;

			Serial.write(bufPtr, txBytes);

			bufPtr += txBytes;
			bytesToSend -= txBytes;

			yield();
		}

		delete buf;

		if (bytes < avail)
			Serial.printf_P(MSG_ERROR);
	}
	else
	{
		// Out of memory
		if (gsCipMux == 0 || gsCipRecvMode == 1)
			Serial.printf_P(PSTR("\r\n%s,out of mem"), respText[gsCipRecvMode]);
		else
			Serial.printf_P(PSTR("\r\n%s,%d,out of mem"), respText[gsCipRecvMode], clientIndex);

		return 0;
	}

	return bytes;
}
