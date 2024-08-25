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
 * 0.2.5: revert AT+SYSTIME, implement AT+CIPSNTPCFG, AT+CIPSNTPTIME, AT+SNTPTIME
 * 0.2.6: add space to the response to command cmd_AT_CIPSEND (for compatibility with AT 1.x original firmware)
 * 0.2.7: fix 'busy p...' text sending while connecting - send on every received char
 * 0.2.8: add AT+CIPCLOSEMODE
 * 0.3.0: Stored certificates in filesystem with LittleFS
 * 0.3.1: AT+CWLAP, AT+CWLAPOPT and 'busy p...' fix
 * 0.3.2: AT+CWHOSTNAME
 * 0.3.3: AT+CIPSERVER, AT+CIPSTO, AT+CIPSERVERMAXCONN [J.A]
 * 0.3.4: SoftAP mode AT+CWMODE, AT+CWSAP, AT+CIPAP [J.A]
 * 0.3.6: AT+CIPSTAMAC and AT+CIPAPMAC query only [J.A]
 * 0.3.6a: Fixed long hostname in AT+CIPSTART, input buffer and string search increased to 200 characters
 * 0.3.6b: Checking the appropriate mode for some commands [J.A]
 * 0.4.0: Arduino ESP8266 Core 3.1.1 
 * 0.4.0a: AT+CIPSTATUS lists SoftAP TCP connections [J.A]
 * 0.4.0b: AT+CIPDOMAIN implementation [J.A]
 * 0.5.0: support Ethernet AT+CIPETH, AT+CIPETHMAC, AT+CEHOSTNAME and modified AT+CWDHCP [J.A]
 *
 * TODO:
 * - Implement AP mode DHCP settings and AT+CWLIF
 * - Implement UDP
 * - TLS Security - persistent fingerprint and single certificate, AT+CIPSSLAUTH_DEF
 */

#include "Arduino.h"
#include "LittleFS.h"
#include "string.h"

#include <ESP8266WiFi.h>
#include <PolledTimeout.h>

extern "C"
{
#include "user_interface.h"

#include "lwip/dns.h"
}

#include "ESP_ATMod.h"
#include "WifiEvents.h"
#include "command.h"
#include "settings.h"
#include "debug.h"
#include "asnDecode.h"

#ifdef ETHERNET_CLASS
ETHERNET_CLASS Ethernet(ETHERNET_CS);
#endif

/*
 * Defines
 */

const char APP_VERSION[] = "0.5.0";

/*
 * Constants
 */

const char MSG_OK[] PROGMEM = "\r\nOK\r\n";
const char MSG_ERROR[] PROGMEM = "\r\nERROR\r\n";

const uint16_t MAX_PEM_CERT_LENGTH = 4096; // Maximum size of a certificate loaded by AT+CIPSSLCERT

/*
 * Global variables
 */

uint8_t inputBuffer[INPUT_BUFFER_LEN]; // Input buffer
uint16_t inputBufferCnt;			   // Number of bytes in inputBuffer

WiFiEventHandler onConnectedHandler;
WiFiEventHandler onGotIPHandler;
WiFiEventHandler onDisconnectedHandler;

client_t clients[5] = {{nullptr, TYPE_NONE, 0, 0, 0},
					   {nullptr, TYPE_NONE, 0, 0, 0},
					   {nullptr, TYPE_NONE, 0, 0, 0},
					   {nullptr, TYPE_NONE, 0, 0, 0},
					   {nullptr, TYPE_NONE, 0, 0, 0}};

WiFiServer servers[] = {WiFiServer(0), WiFiServer(0), WiFiServer(0), WiFiServer(0), WiFiServer(0)};
const uint8_t SERVERS_COUNT = sizeof(servers) / sizeof(WiFiServer);

uint8_t sendBuffer[2048];
uint16_t dataRead = 0; // Number of bytes read from the input to a send buffer

// TLS specific variables

uint8_t fingerprint[20]; // SHA-1 certificate fingerprint for TLS connections
bool fingerprintValid;
BearSSL::X509List *CAcert; // CA certificates for TLS validation
size_t maximumCertificates;

char *PemCertificate = nullptr; // Buffer for loading a certificate
uint16_t PemCertificatePos;		// Position in buffer while loading
uint16_t PemCertificateCount;	// Number of chars read

/*
 *  Global settings
 */
bool gsEchoEnabled = true;			// command ATE
uint8_t gsCipMux = 0;				// command AT+CIPMUX
uint8_t gsCipdInfo = 0;				// command AT+CIPDINFO
uint8_t gsCwDhcp = CWDHCP_AP | CWDHCP_STA;	// command AT+CWDHCP
bool gsFlag_Connecting = false;		// Connecting in progress
bool gsFlag_Busy = false;			// Command is busy other commands will be ignored
int8_t gsLinkIdReading = -1;		// Link id where the data is read
bool gsCertLoading = false;			// AT+CIPSSLCERT in progress
bool gsWasConnected = false;		// Connection flag for AT+CIPSTATUS
bool gsEthConnected = false;		// track eth state for +ETH_ messages
IPAddress gsEthLastIP;				// for +ETH_GOT_IP message
uint8_t gsCipSslAuth = 0;			// command AT+CIPSSLAUTH: 0 = none, 1 = fingerprint, 2 = certificate chain
uint8_t gsCipRecvMode = 0;			// command AT+CIPRECVMODE
ipConfig_t gsCipStaCfg = {0, 0, 0}; // command AT+CIPSTA
dnsConfig_t gsCipDnsCfg = {0, 0};	// command AT+CIPDNS
ipConfig_t gsCipApCfg = {0, 0, 0}; // command AT+CIPAP
ipConfig_t gsCipEthCfg = {0, 0, 0};	// command AT+CIPETH
uint8_t gsCipEthMAC[6] = {0, 0, 0, 0, 0, 0};	// command AT+CIPETHMAC
uint16_t gsCipSslSize = 16384;		// command AT+CIPSSLSIZE
bool gsSTNPEnabled = true;			// command AT+CIPSNTPCFG
int8_t gsSTNPTimezone = 0;			// command AT+CIPSNTPCFG
String gsSNTPServer[3];				// command AT+CIPSNTPCFG
uint8_t gsServersMaxConn = 5;			// command AT+CIPSERVERMAXCONN
uint32_t gsServerConnTimeout = 180000;	// command AT+CIPSSTO

/*
 * Local prototypes
 */
static bool checkCertificateDuplicatesAndLoad(BearSSL::X509List &importCertList);

/*
 *  The setup function is called once at startup of the sketch
 */
void setup()
{
	// Fixes the problem with the core 3.x and autoconnect
	// For core pre 3.x please comment the next line out otherwise the compilation will fail
	enableWiFiAtBootTime();

	// Default static net configuration
	gsCipStaCfg = Settings::getNetConfig();

	// Default DNS configuration
	gsCipDnsCfg = Settings::getDnsConfig();

	// Default DHCP configuration
	gsCwDhcp = Settings::getDhcpMode();

	// Default SoftAP configuration
	gsCipApCfg = Settings::getApIpConfig();

	// apply CIPSTA_DEF values if STA started automatically
	if (WiFi.getMode() != WIFI_AP)
	{
		setDns();
		setDhcpMode();
	}

	// apply CIPAP_DEF values if SoftAP started automatically
	if (WiFi.getMode() != WIFI_STA)
	{
		applyCipAp();
	}

	// Default UART configuration
	uint32_t baudrate = Settings::getUartBaudRate();
	SerialConfig config = Settings::getUartConfig();
	Serial.begin(baudrate, config);

#ifdef ETHERNET_CLASS
	SPI.begin();
	SPI.setClockDivider(SPI_CLOCK_DIV4);
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);

	gsCipEthCfg = Settings::getEthIpConfig();
	configureEthernet();
#endif

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
	WiFi.persistent(false);
	WiFi.setAutoReconnect(true);

	// Set the SNTP defaults
	gsSNTPServer[0] = "pool.ntp.org";
	gsSNTPServer[1] = "time.nist.gov";
	gsSNTPServer[2] = "";

	// Default maximum certificates
	maximumCertificates = Settings::getMaximumCertificates();

	// Initialize certificate store
	CAcert = new BearSSL::X509List();

	// Load certificates from LittleFS
	if (LittleFS.begin())
	{
		// Open dir folder
		Dir dir = LittleFS.openDir("/");

		// Cycle all the content
		while (dir.next())
		{
			// Get filename
			String filename = dir.fileName();

			size_t originalCertCount = CAcert->getCount();

			// Check if maximum certificates has not been reached yet
			if (originalCertCount >= maximumCertificates)
			{
				Serial.printf_P(PSTR("\nCould not load %s. Reached the maximum of %d certificates"), filename.c_str(), maximumCertificates);
				Serial.printf_P(MSG_ERROR);
			}
			else
			{
				// Check if the file is in PEM format
				if (filename.endsWith(".pem"))
				{
					// Check if file has content
					if (dir.fileSize())
					{
						File file = LittleFS.open(filename, "r");

						if (!file)
						{
							Serial.printf_P("\nFailed to open file for reading");
							Serial.printf_P(MSG_ERROR);
							return;
						}

						// Read file content
						String fileContent = "";
						while (file.available())
						{
							fileContent += (char)file.read();
						}

						file.close();

						// Append certificate to seperate X509List
						BearSSL::X509List importCertList;
						importCertList.append(fileContent.c_str());

						if (importCertList.getCount() != 1)
						{
							Serial.printf_P(PSTR("\nFailed to add %s to the certificates list"), filename.c_str());
							Serial.printf_P(MSG_ERROR);
							return;
						}

						if (checkCertificateDuplicatesAndLoad(importCertList))
						{
							Serial.println(F("\nTried to load already existing certificate"));
							Serial.printf_P(MSG_ERROR);
						}
						else if (CAcert->getCount() == originalCertCount)
						{
							Serial.printf_P(PSTR("\nFailed to add %s to the certificates list"), filename.c_str());
							Serial.printf_P(MSG_ERROR);
						}
					}
					else
					{
						Serial.printf_P(PSTR("\n%s is empty"), filename.c_str());
						Serial.printf_P(MSG_ERROR);
					}
				}
				else
				{
					if (strcmp(filename.c_str(), ".gitkeep"))
					{
						Serial.printf_P(PSTR("\n%s is not a .pem file"), filename.c_str());
						Serial.printf_P(MSG_ERROR);
					}
				}
			}
		}
	}
	else
	{
		Serial.printf_P("\nInizializing FS failed.");
		Serial.printf_P(MSG_ERROR);
	}

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
		uint8_t maxCli = 0; // Maximum client number
		if (gsCipMux == 1)
			maxCli = 4;

		uint8_t freeLinkId = 255;
		uint8_t serversConnCount = 0;

		for (uint8_t i = 0; i <= maxCli; ++i)
		{
			WiFiClient *cli = clients[i].client;

			if (cli != nullptr)
			{
				int avail = cli->available();

				if (avail > clients[i].lastAvailableBytes) // For RECVMODE it is every non zero avail
				{
					clients[i].lastActivityMillis = millis();

					if (gsCipRecvMode == 0)
					{
						SendData(i, 0);
					}
					else // CIPRECVMODE = 1
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
					cli = nullptr;
				}
			}

			if (cli != nullptr)
			{
				boolean isServer = false;
				for (uint8_t j = 0; j < SERVERS_COUNT; ++j)
				{
					if (servers[j].status() == CLOSED)
						continue;
					if (cli->localPort() == servers[j].port())
					{
						isServer = true;
						break;
					}
				}
				if (isServer)
				{
					if (gsServerConnTimeout != 0 && cli->available() == 0
							&& millis() - clients[i].lastActivityMillis > gsServerConnTimeout)
					{
						if (gsCipMux == 1)
							Serial.printf_P(PSTR("%d,"), i);
						Serial.println(F("CLOSED"));
						DeleteClient(i);
					}
					else
					{
						serversConnCount++;
					}
				}
			}
			else if (freeLinkId == 255)
			{
				freeLinkId = i;
			}
		}

		// handle server clients. check for a new connection only if we can add it
		for (uint8_t i = 0; i < SERVERS_COUNT; ++i)
		{
			if (freeLinkId == 255 || serversConnCount >= gsServersMaxConn)
				break;
			if (servers[i].status() == CLOSED)
				continue;
			// WiFiClient cli = servers[i].available();  // Use for older cores where the function accept() doesn't exist
			WiFiClient cli = servers[i].accept();
			if (!cli)
				continue;
			clients[freeLinkId].client = new WiFiClient(cli);
			clients[freeLinkId].type = TYPE_TCP;
			clients[freeLinkId].lastAvailableBytes = 0;
			clients[freeLinkId].lastActivityMillis = millis();
			Serial.printf_P(PSTR("%d,CONNECT\r\n"), freeLinkId);
			gsWasConnected = true; // Flag for CIPSTATUS command

			serversConnCount++;
			freeLinkId = 255;
			for (uint8_t j = 0; j <= maxCli; ++j)
			{
				if (clients[j].client == nullptr)
				{
					freeLinkId = j;
					break;
				}
			}
		}
#ifdef ETHERNET_CLASS
		if (gsEthConnected != netif_is_up(Ethernet.getNetIf()))
		{
			gsEthConnected = netif_is_up(Ethernet.getNetIf());
			if (gsEthConnected)
			{
				Serial.println(F("+ETH_CONNECTED"));
			}
			else
			{
				Serial.println(F("+ETH_DISCONNECTED"));
			}
		}
		if (Ethernet.localIP().isSet() && gsEthLastIP != Ethernet.localIP())
		{
			gsEthLastIP = Ethernet.localIP();
			if (gsEthLastIP.isSet())
			{
				Serial.print(F("+ETH_GOT_IP=\""));
				Serial.print(gsEthLastIP);
				Serial.println('"');
			}
		}
#endif
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
				{
					Serial.println(F("\r\nSEND OK"));
					clients[gsLinkIdReading].lastActivityMillis = millis();
				}
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

			if (isAlphaNumeric(c) || strchr("/+= -\\\r\n", c) != nullptr)
			{
				// Check newlines - the header and footer must be separated by at least one '\n' from the base64 certificate data
				if (c == '\r')
					c = '\n';
				else if (c == 'n' && PemCertificatePos > 0 && PemCertificate[PemCertificatePos - 1] == '\\')
				{
					// Create real backslash from separate characters '\' and 'n'
					c = '\n';
					--PemCertificatePos;
				}

				// Ignore multiple newlines, save space in the buffer
				if (c != '\n' || (PemCertificatePos > 0 && PemCertificate[PemCertificatePos - 1] != '\n'))
				{
					PemCertificate[PemCertificatePos++] = c;

					// Check the end
					if (c == '-' && PemCertificatePos > 100)
					{
						if (!memcmp_P(PemCertificate + PemCertificatePos - 25, PSTR("-----END CERTIFICATE-----"), 25))
						{
							Serial.printf_P(PSTR("\r\nRead %d bytes\r\n"), PemCertificateCount);

							// Process the certificate
							PemCertificate[PemCertificatePos] = '\0';

							size_t originalCertCount = CAcert->getCount();

							// Append certificate to seperate X509List
							BearSSL::X509List importCertList;
							importCertList.append(PemCertificate);

							if (importCertList.getCount() != 1)
							{
								Serial.println(F("Loading certificate failed"));
								Serial.printf_P(MSG_ERROR);
								return;
							}

							delete PemCertificate;
							PemCertificate = nullptr;
							PemCertificatePos = 0;

							gsCertLoading = false;

							if (checkCertificateDuplicatesAndLoad(importCertList))
							{
								Serial.println(F("Tried to load already existing certificate"));
								Serial.printf_P(MSG_ERROR);
							}
							else if (CAcert->getCount() == (originalCertCount + 1))
							{
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
						Serial.printf_P(MSG_ERROR); // Invalid data
					}
				}
			}
			else if (c < ' ')
			{
			}
			else // illegal character in certificate
			{
				gsCertLoading = false;
				Serial.printf_P(MSG_ERROR); // Invalid data
			}

			if (!gsCertLoading)
			{
				// Read everything left before continuing
				while (Serial.available() > 0)
				{
					// Read the incoming byte
					Serial.read();
				}
			}
		}
		else if (inputBufferCnt < INPUT_BUFFER_LEN)
		{
			if (gsEchoEnabled)
				Serial.write(c);

			/*			if (inputBufferCnt == 0 && c != 'A')  // Wait for 'A' as the start of the command
			{}
			else*/
			// FIXME: problematic, some libraries send garbage to check if the module is alive
			{
				inputBuffer[inputBufferCnt++] = c;

				if (c == '\n') // LF (0x0a)
				{
					lineCompleted = true;
					break;
				}
			}
		}
		else
		{
			inputBufferCnt = 0;
			Serial.printf_P(MSG_ERROR); // Buffer overflow
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
			gsFlag_Busy = false;

			// Set up time to allow for certificate validation
			if (gsSTNPEnabled && time(nullptr) < 8 * 3600 * 2)
			{
				// FIXME: Note: the time zone is not used here, it didn't work for me
				configTime(0, 0, nullIfEmpty(gsSNTPServer[0]), nullIfEmpty(gsSNTPServer[1]), nullIfEmpty(gsSNTPServer[2]));
			}

			// Redefine dns to the static ones
			if (gsCipDnsCfg.dns1 != 0)
				setDns();

			break;

		case STATION_NO_AP_FOUND:
			Serial.println(F("\r\n+CWJAP:3\r\nFAIL"));
			gsFlag_Connecting = false;
			gsFlag_Busy = false;
			break;

		case STATION_CONNECT_FAIL:
			Serial.println(F("\r\n+CWJAP:4\r\nFAIL"));
			gsFlag_Connecting = false;
			gsFlag_Busy = false;
			break;

		case STATION_WRONG_PASSWORD:
			Serial.println(F("\r\n+CWJAP:2\r\nFAIL"));
			gsFlag_Connecting = false;
			gsFlag_Busy = false;
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
	if (gsFlag_Busy)
	{
		// Check for busy condition
		if (inputBufferCnt != 0)
		{
			Serial.println(F("\r\nbusy p..."));

			// Discard the input buffer
			inputBufferCnt = 0;
		}
	}
	else if (lineCompleted) // Check for a new command
	{
		processCommandBuffer();

		// Discard the garbage that may have come during the processing of the command
		while (Serial.available())
		{
			int c = Serial.peek();
			if (c < 0 || c == 'A') // we are waiting for empty serial or 'A' in AT command
				break;

			/* Note: There is a potential risk of discarding input data when the data
			 *       correctly starts with a character other than 'A'. This is the case
			 *       of AT+CIPSEND, e.g.
			 *       The risky time window opens after sending the last character of the
			 *       processed command response and lasts until the USART is empty
			 */
			Serial.read();
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
	if (gsCwDhcp & CWDHCP_STA)
	{
		WiFi.config(0, 0, 0); // Enable Station DHCP
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

void configureEthernet()
{
#ifdef ETHERNET_CLASS
	bool begin = false;

	if (gsCwDhcp & CWDHCP_ETH)
	{
		Ethernet.end();
		Ethernet.config(0, 0, 0); // Enable Ethernet DHCP
		begin = true;
	}
	else
	{
		if (gsCipEthCfg.ip != 0 && gsCipEthCfg.gw != 0 && gsCipEthCfg.mask != 0)
		{
			// Configure the network
			Ethernet.end();
			Ethernet.config(gsCipEthCfg.ip, gsCipEthCfg.gw, gsCipEthCfg.mask);
			setDns();
			begin = true;
		}
		else
		{
			dhcp_stop((netif*) Ethernet.getNetIf());
		}
	}
	if (begin) {
		uint8_t* mac = nullptr;
		if (gsCipEthMAC[0] != 0 || memcmp(gsCipEthMAC, gsCipEthMAC + 1, 5)) { // all zeros test
			mac = gsCipEthMAC;
		}
		if (Ethernet.begin(mac)) {
			if (mac == nullptr) {
				Ethernet.macAddress(gsCipEthMAC);
			}
		}
	}
#endif
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
		dns_setserver(0, IPAddress(64, 6, 64, 6));
		dns_setserver(1, nullptr);
	}
}

/*
 * wraps WiFi.softAPConfig, because it does a restart of the DHCP server
 * even if the settings didn't change.
 */
bool applyCipAp()
{
	if (!gsCipApCfg.ip)
		return true;
	ip_info apInfo;
	wifi_get_ip_info(SOFTAP_IF, &apInfo);
	if (gsCipApCfg.ip == apInfo.ip.addr && gsCipApCfg.gw == apInfo.gw.addr && gsCipApCfg.mask == apInfo.netmask.addr)
		return true;
  return WiFi.softAPConfig(gsCipApCfg.ip, gsCipApCfg.gw, gsCipApCfg.mask);
}

/*
 * Send data in a +IPD message (for AT+CIPRECVMODE=0) or +CIPRECVDATA (for AT+CIPRECVMODE=1)
 * Returns number of bytes sent or 0 (error)
 */
int SendData(int clientIndex, int maxSize)
{
	const char *respText[2] = {"+IPD", "+CIPRECVDATA"};

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

		if (gsCipdInfo == 1 && gsCipRecvMode == 0) // No CIPDINFO for CIPRECVDATA
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

/*
 * Returns the internal char* of the input string
 * In case of empty string returns nullptr
 */
const char *nullIfEmpty(String &s)
{
	if (s.isEmpty())
		return nullptr;

	return s.c_str();
}

/*
 * Checks if the newly added certificate is a duplicate
 */
bool checkCertificateDuplicatesAndLoad(BearSSL::X509List &importCertList)
{
	const br_x509_certificate *importedCert = &(importCertList.getX509Certs()[0]);

	for (size_t i = 0; i < CAcert->getCount(); i++)
	{
		const br_x509_certificate *cert = &(CAcert->getX509Certs()[i]);
		if (!memcmp(importedCert->data, cert->data, importedCert->data_len))
		{
			return true;
		}
	}

	// Certificate is not a duplicate
	CAcert->append(importedCert->data, importedCert->data_len);

	return false;
}
