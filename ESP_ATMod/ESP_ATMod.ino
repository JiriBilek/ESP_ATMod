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
 *
 * TODO:
 * - Implement AT+CWLAP
 * - Implement AT+CIPSTA=, AT+CIPSTA_DEF, persistent data
 * - TLS Security - fingerprint, ad hoc certificate, list of certificates in FS
 * 		AT+CIPSSLAUTH
 * - Implement AT+CIPDNS_CUR, AT+CIPDNS_DEF
 * - Implement AT+CIPRECVMODE, AT+CIPRECVLEN, AT+CIPRECVDATA
 * - Implement AT+CIPSNTPCFG, AT+CIPSNTPTIME
 */

#include "Arduino.h"
#include "string.h"

#include <ESP8266WiFi.h>
#include <PolledTimeout.h>

extern "C" {
#include "user_interface.h"
}

#include "ESP_ATMod.h"
#include "WifiEvents.h"
#include "command.h"
#include "settings.h"
#include "debug.h"

/*
 * Defines
 */

const char APP_VERSION[] = "0.1.0";

/*
 * Constants
 */

const char MSG_OK[] PROGMEM = "\r\nOK\r\n";
const char MSG_ERROR[] PROGMEM = "\r\nERROR\r\n";

/*
 * Global variables
 */

uint8_t inputBuffer[INPUT_BUFFER_LEN];  // Input buffer
uint16_t inputBufferCnt;  // Number of bytes in inputBuffer

WiFiEventHandler onConnectedHandler;
WiFiEventHandler onGotIPHandler;
WiFiEventHandler onDisconnectedHandler;

client_t clients[5] = { { nullptr, TYPE_NONE, 0 },
						{ nullptr, TYPE_NONE, 0 },
						{ nullptr, TYPE_NONE, 0 },
						{ nullptr, TYPE_NONE, 0 },
						{ nullptr, TYPE_NONE, 0 } };

uint8_t sendBuffer[2048];
uint16_t dataRead = 0;  // Number of bytes read from the input to a send buffer

/*
 *  Global settings
 */
bool gsEchoEnabled = true;  // command ATE
uint8_t gsCipMux = 0;  // command AT+CIPMUX
uint8_t gsCipdInfo = 0;  // command AT+CIPDINFO
uint8_t gsCwDhcp = 3;  // command AT+CWDHCP
bool gsFlag_Connecting = false;  // Connecting in progress (CWJAP) - other commands ignored
int8_t gsLinkIdReading = -1;  // Link id where the data is read
bool gsWasConnected = false;  // Connection flag for AT+CIPSTATUS

/*
 *  The setup function is called once at startup of the sketch
 */
void setup()
{
	// Default DHCP configuration
	gsCwDhcp = Settings::getDhcpMode();
	setDhcpMode();

	// Default UART configuration
	uint32_t baudrate = Settings::getUartBaudRate();
	SerialConfig config = Settings::getUartConfig();
	Serial.begin(baudrate, config);

	inputBufferCnt = 0;

    // Register event handlers.
    // Call "onStationConnected" each time a station connects
	onConnectedHandler = WiFi.onStationModeConnected(&onStationConnected);
    // Call "onStationModeGotIP" each time a station fully connects
	onGotIPHandler = WiFi.onStationModeGotIP(&onStationGotIP);
    // Call "onStationDisconnected" each time a station disconnects
	onDisconnectedHandler = WiFi.onStationModeDisconnected(&onStationDisconnected);

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
				if (cli->available())
				{
					int avail = cli->available();

					uint8_t *buf = new uint8_t[avail];

					if (buf != nullptr)
					{
						Serial.print(F("\r\n+IPD"));

						if (gsCipMux == 1)
							Serial.printf_P(PSTR(",%d"), i);

						Serial.printf_P(PSTR(",%d"), avail);

						if (gsCipdInfo == 1)
						{
							IPAddress ip = cli->remoteIP();

							Serial.printf_P(PSTR(",%s,%d"), ip.toString().c_str(), cli->remotePort());
						}

						Serial.print(':');

						int bytes = cli->readBytes(buf, avail);
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
						if (gsCipMux == 0)
							Serial.printf_P(PSTR("\r\n+IPD,out of mem"));
						else
							Serial.printf_P(PSTR("\r\n+IPD,%d,out of mem"), i);
					}
				}

				if (!cli->available() && !cli->connected())
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
		else if (inputBufferCnt < INPUT_BUFFER_LEN)
		{
			inputBuffer[inputBufferCnt++] = c;

			if (gsEchoEnabled)
				Serial.write(c);

			if (c == '\n')  // LF (0x0a)
			{
				lineCompleted = true;
				break;
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
	else if (lineCompleted && gsFlag_Connecting)
	{
		Serial.println(F("\r\nbusy p..."));

		// Discard the input buffer
		inputBufferCnt = 0;
	}

	// Check for a new command
	else if (lineCompleted)
	{
		processCommandBuffer();
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
 */
void setDhcpMode()
{
	if (gsCwDhcp & 2)
	{
		WiFi.config(0, 0, 0);  // Enable Station DHCP
	}
	else
	{
		wifi_station_dhcpc_stop();
	}
}
