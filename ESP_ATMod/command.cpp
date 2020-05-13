/*
 * command.cpp
 *
 * Part of ESP_ATMod: modified AT command processor for ESP8266
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

#include "Arduino.h"
#include "ESP8266WiFi.h"
#include <PolledTimeout.h>

#include "ESP_ATMod.h"
#include "command.h"
#include "settings.h"
#include "debug.h"

/*
 * Command list
 */

enum cmdMode_t {
	MODE_NO_CHECKING,  // no checking
	MODE_EXACT_MATCH,  // exact match
	MODE_QUERY_SET     // '?' or '=' follows
};

typedef struct {
	const char *text;
	const cmdMode_t mode;
	const commands_t cmd;
} commandDef_t;

static const commandDef_t commandList[] = {
		{ "E", MODE_NO_CHECKING, CMD_ATE },
		{ "+GMR", MODE_EXACT_MATCH, CMD_AT_GMR },
		{ "+RST", MODE_EXACT_MATCH, CMD_AT_RST },
		{ "+SYSRAM?", MODE_EXACT_MATCH, CMD_AT_SYSRAM },
		{ "+RFMODE", MODE_QUERY_SET, CMD_AT_RFMODE },
		{ "+UART_CUR", MODE_QUERY_SET, CMD_AT_UART_CUR },
		{ "+UART_DEF", MODE_QUERY_SET, CMD_AT_UART_DEF },
		{ "+UART", MODE_QUERY_SET, CMD_AT_UART },
		{ "+RESTORE", MODE_EXACT_MATCH, CMD_AT_RESTORE },
		{ "+CWAUTOCONN", MODE_QUERY_SET, CMD_AT_CWAUTOCONN },
		{ "+CWDHCP_CUR", MODE_QUERY_SET, CMD_AT_CWDHCP_CUR },
		{ "+CWDHCP_DEF", MODE_QUERY_SET, CMD_AT_CWDHCP_DEF },
		{ "+CWDHCP", MODE_QUERY_SET, CMD_AT_CWDHCP },
		{ "+CWJAP_CUR", MODE_QUERY_SET, CMD_AT_CWJAP_CUR },
		{ "+CWJAP_DEF", MODE_QUERY_SET, CMD_AT_CWJAP_DEF },
		{ "+CWJAP", MODE_QUERY_SET, CMD_AT_CWJAP },
		{ "+CWMODE_CUR", MODE_QUERY_SET, CMD_AT_CWMODE_CUR },
		{ "+CWMODE_DEF", MODE_QUERY_SET, CMD_AT_CWMODE_DEF },
		{ "+CWMODE", MODE_QUERY_SET, CMD_AT_CWMODE },
		{ "+CWQAP", MODE_EXACT_MATCH, CMD_AT_CWQAP },
		{ "+CIFSR", MODE_EXACT_MATCH, CMD_AT_CIFSR },
		{ "+CIPCLOSE", MODE_NO_CHECKING, CMD_AT_CIPCLOSE },
		{ "+CIPDINFO", MODE_QUERY_SET, CMD_AT_CIPDINFO },
		{ "+CIPMUX", MODE_QUERY_SET, CMD_AT_CIPMUX },
		{ "+CIPSEND", MODE_NO_CHECKING, CMD_AT_CIPSEND },
		{ "+CIPSTATUS", MODE_EXACT_MATCH, CMD_AT_CIPSTATUS },
		{ "+CIPSTART", MODE_NO_CHECKING, CMD_AT_CIPSTART },
		{ "+CIPSTA_CUR", MODE_QUERY_SET, CMD_AT_CIPSTA_CUR },
		{ "+CIPSTA", MODE_QUERY_SET, CMD_AT_CIPSTA },
		{ "+CIPSSLSIZE", MODE_QUERY_SET, CMD_AT_CIPSSLSIZE }
};

/*
 * Static functions
 */

commands_t findCommand(uint8_t* input, uint16_t inpLen);
String readStringFromBuffer(unsigned char *inpBuf, uint16_t &offset, bool escape);
bool readNumber(unsigned char *inpBuf, uint16_t &offset, uint32_t &output);

/*
 * Commands
 */

static void cmd_AT();
static void cmd_ATE();
static void cmd_AT_GMR();
static void cmd_AT_RST();
static void cmd_AT_CWAUTOCONN();
static void cmd_AT_MODE(commands_t cmd);
static void cmd_AT_CIPMUX();
static void cmd_AT_CIPDINFO();
static void cmd_AT_CWDHCP(commands_t cmd);
static void cmd_AT_CWJAP(commands_t cmd);
static void cmd_AT_CWQAP();
static void cmd_AT_SYSRAM();
static void cmd_AT_RFMODE();
static void cmd_AT_CIPSTATUS();
static void cmd_AT_CIFSR();
static void cmd_AT_CIPSTA(commands_t cmd);
static void cmd_AT_CIPSTART();
static void cmd_AT_CIPSEND();
static void cmd_AT_CIPCLOSE();
static void cmd_AT_UART(commands_t cmd);
static void cmd_AT_RESTORE();
static void cmd_AT_CIPSSLSIZE();

/*
 * Processes the command buffer
 */
void processCommandBuffer(void)
{
	commands_t cmd = findCommand(inputBuffer, inputBufferCnt);

	// ------------------------------------------------------------------------------------ AT
	if (cmd == CMD_AT)
		cmd_AT();

	// ------------------------------------------------------------------------------------ ATE
	else if (cmd == CMD_ATE)  // ATE0, ATE1 - echo enabled / disabled
		cmd_ATE();

	// ------------------------------------------------------------------------------------ AT+GMR
	else if (cmd == CMD_AT_GMR)  // AT+GMR - firmware version
		cmd_AT_GMR();

	// ------------------------------------------------------------------------------------ AT+RST
	else if (cmd == CMD_AT_RST)  // AT+RST - soft reset
		cmd_AT_RST();

	// ------------------------------------------------------------------------------------ AT+CWAUTOCONN
	else if (cmd == CMD_AT_CWAUTOCONN)  // AT+CWAUTOCONN - auto connect to AP
		cmd_AT_CWAUTOCONN();

	// ------------------------------------------------------------------------------------ AT+CWMODE
	else if (cmd == CMD_AT_CWMODE || cmd == CMD_AT_CWMODE_CUR || cmd == CMD_AT_CWMODE_DEF)
		// AT+CWMODE - Sets the Current Wi-Fi mode (only mode 1 implemented)
		cmd_AT_MODE(cmd);

	// ------------------------------------------------------------------------------------ AT+CIPMUX
	else if (cmd == CMD_AT_CIPMUX)  // AT+CIPMUX - Enable or Disable Multiple Connections
		cmd_AT_CIPMUX();

	// ------------------------------------------------------------------------------------ AT+CIPDINFO
	else if (cmd == CMD_AT_CIPDINFO)  // AT+CIPDINFO - Shows the Remote IP and Port with +IPD
		cmd_AT_CIPDINFO();

	// ------------------------------------------------------------------------------------ AT+CWDHCP
	else if (cmd == CMD_AT_CWDHCP || cmd == CMD_AT_CWDHCP_CUR || cmd == CMD_AT_CWDHCP_DEF)
		// AT+CWDHCP=x,y - Enables/Disables DHCP
		cmd_AT_CWDHCP(cmd);

	// ------------------------------------------------------------------------------------ AT+CWJAP
	else if (cmd == CMD_AT_CWJAP || cmd == CMD_AT_CWJAP_CUR || cmd == CMD_AT_CWJAP_DEF)
		// AT+CWJAP="ssid","pwd" [,"bssid"] - Connects to an AP, only ssid, pwd and bssid supported
		cmd_AT_CWJAP(cmd);

	// ------------------------------------------------------------------------------------ AT+CWQAP
	else if (cmd == CMD_AT_CWQAP)  // AT+CWQAP - Disconnects from the AP
		cmd_AT_CWQAP();

	// ------------------------------------------------------------------------------------ AT+SYSRAM
	else if (cmd == CMD_AT_SYSRAM)  // AT+SYSRAM? - Checks the Remaining Space of RAM
		cmd_AT_SYSRAM();

	// ------------------------------------------------------------------------------------ AT+RFMODE
	else if (cmd == CMD_AT_RFMODE)  // AT+RFMODE - Sets or queries current RF mode (custom command)
		cmd_AT_RFMODE();

	// ------------------------------------------------------------------------------------ AT+CIPSTATUS
	else if (cmd == CMD_AT_CIPSTATUS)  // AT+CIPSTATUS - Gets the Connection Status
		cmd_AT_CIPSTATUS();

	// ------------------------------------------------------------------------------------ AT+CIFSR
	else if (cmd == CMD_AT_CIFSR)  // AT+CIFSR - Gets the Local IP Address
		cmd_AT_CIFSR();

	// ------------------------------------------------------------------------------------ AT+CIPSTA
	else if (cmd == CMD_AT_CIPSTA || cmd == CMD_AT_CIPSTA_CUR)
		// AT+CIPSTA?, AT+CIPSTA_CUR? -  Obtains the current IP address
		cmd_AT_CIPSTA(cmd);

	// ------------------------------------------------------------------------------------ AT+CIPSTART
	else if (cmd == CMD_AT_CIPSTART)
		// AT+CIPSTART - Establishes TCP Connection, UDP Transmission or SSL Connection
		cmd_AT_CIPSTART();

	// ------------------------------------------------------------------------------------ AT+CIPSEND
	else if (cmd == CMD_AT_CIPSEND)  // AT+CIPSEND - Sends Data
		cmd_AT_CIPSEND();

	// ------------------------------------------------------------------------------------ AT+CIPCLOSE
	else if (cmd == CMD_AT_CIPCLOSE)  // AT+CIPCLOSE - Closes the TCP/UDP/SSL Connection
		cmd_AT_CIPCLOSE();

	// ------------------------------------------------------------------------------------ AT+UART
	else if (cmd == CMD_AT_UART || cmd == CMD_AT_UART_CUR || cmd == CMD_AT_UART_DEF)
		// AT+UART=baudrate,databits,stopbits,parity,flow - UART Configuration
		cmd_AT_UART(cmd);

	// ------------------------------------------------------------------------------------ AT+RESTORE
	else if (cmd == CMD_AT_RESTORE)  // AT+RESTORE - Restores the Factory Default Settings
		cmd_AT_RESTORE();

	// ------------------------------------------------------------------------------------ AT+CIPSSLSIZE
	else if (cmd == CMD_AT_CIPSSLSIZE)
		// AT+CIPSSLSIZE - Sets the Size of SSL Buffer - the command is parsed but ignored
		cmd_AT_CIPSSLSIZE();

	else
	{
		Serial.printf_P(MSG_ERROR);
	}

	// Clear the buffer
	inputBufferCnt = 0;
}

/****************************************************************************************
 * Commands
 */

/*
 * AT
 */
void cmd_AT()
{
	Serial.printf_P(MSG_OK);
}

/*
 * ATE0, ATE1 - echo enabled / disabled
 */
void cmd_ATE()
{
	uint32_t echo;
	uint16_t offset = 3;

	if (!readNumber(inputBuffer, offset, echo) || echo > 1 || inputBufferCnt != offset + 2)
		Serial.printf_P(MSG_ERROR);
	else
	{
		gsEchoEnabled = echo;
		Serial.printf_P(MSG_OK);
	}
}

/*
 * AT+GMR - firmware version
 */
void cmd_AT_GMR()
{
	Serial.println(F("AT version:1.6.0.0 (partial)"));
	Serial.printf_P(PSTR("SDK version:%s\r\n"), system_get_sdk_version());
	Serial.printf_P(PSTR("Compile time:%s %s\r\n"), __DATE__, __TIME__);
	Serial.printf_P(PSTR("Version ESP_ATMod:%s\r\n"), APP_VERSION);
	Serial.println(F("OK"));
}

/*
 * AT+RST - soft reset
 */
void cmd_AT_RST()
{
	Serial.printf_P(MSG_OK);
	Serial.flush();

	ESP.reset();
}

/*
 * AT+CWAUTOCONN - auto connect to AP
 */
void cmd_AT_CWAUTOCONN()
{
	if (inputBuffer[13] == '?')
	{
		Serial.print(F("+CWAUTOCONN:"));
		Serial.println(WiFi.getAutoConnect() ? "1" : "0");
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[13] == '=')
	{
		uint32_t autoconn;
		uint16_t offset = 14;

		if (readNumber(inputBuffer, offset, autoconn) && autoconn <= 1 && inputBufferCnt == offset + 2)
		{
			WiFi.setAutoConnect(autoconn);
			Serial.printf_P(MSG_OK);
		}
		else
		{
			Serial.printf_P(MSG_ERROR);
		}
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CWMODE - Sets the Current Wi-Fi mode (only mode 1 implemented)
 */
void cmd_AT_MODE(commands_t cmd)
{
	uint16_t offset = 9;  // offset to ? or =
	if (cmd != CMD_AT_CWMODE)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		Serial.print(F("+CWMODE"));
		if (cmd == CMD_AT_CWMODE_CUR)
			Serial.print(F("_CUR"));
		else if (cmd == CMD_AT_CWMODE_DEF)
			Serial.print(F("_DEF"));
		Serial.print(':');

		Serial.println(WiFi.getMode());
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		uint32_t mode;

		++offset;

		if (readNumber(inputBuffer, offset, mode) && mode <= 3 && inputBufferCnt == offset + 2)
		{
			if (mode == 1)  // Only MODE 1 is supported
				Serial.printf_P(MSG_OK);
			else
				Serial.println(F("ERROR NOT SUPPORTED"));
		}
		else
			Serial.printf_P(MSG_ERROR);
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPMUX - Enable or Disable Multiple Connections
 */
void cmd_AT_CIPMUX()
{
	bool error = true;

	if (inputBuffer[9] == '?' && inputBufferCnt == 12)
	{
		Serial.printf_P(PSTR("+CIPMUX:%d\r\n\r\nOK\r\n"), gsCipMux);
		error = false;
	}
	else if (inputBuffer[9] == '=')
	{
		uint32_t mux;
		uint16_t offset = 10;

		if (readNumber(inputBuffer, offset, mux) && mux <= 1 && inputBufferCnt == offset + 2)
		{
			bool openedError = false;

			for (uint8_t i = 0; i <= 4; ++i)
			{
				if (clients[i].client != nullptr)
				{
					Serial.println(F("link is builded"));
					openedError = true;
					break;
				}
			}

			if (!openedError)
			{
				gsCipMux = mux;
				Serial.printf_P(MSG_OK);
				error = false;
			}
		}
	}

	if (error)
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPDINFO - Shows the Remote IP and Port with +IPD
 */
void cmd_AT_CIPDINFO()
{
	if (inputBuffer[11] == '?' && inputBufferCnt == 14)
	{
		Serial.printf_P(PSTR("+CIPDINFO:%s\r\n\r\nOK\r\n"), gsCipdInfo ? "TRUE" : "FALSE");
	}
	else if (inputBuffer[11] == '=')
	{
		uint32_t ipdInfo;
		uint16_t offset = 12;

		if (readNumber(inputBuffer, offset, ipdInfo) && ipdInfo <= 1 && inputBufferCnt == offset + 2)
		{
			gsCipdInfo = ipdInfo;
			Serial.printf_P(MSG_OK);
		}
		else
		{
			Serial.printf_P(MSG_ERROR);
		}
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CWDHCP=x,y - Enables/Disables DHCP
 */
void cmd_AT_CWDHCP(commands_t cmd)
{
	bool error = true;

	uint16_t offset = 9;  // offset to ? or =
	if (cmd != CMD_AT_CWDHCP)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		Serial.print(F("+CWDHCP"));

		if (cmd == CMD_AT_CWDHCP_DEF)
		{
			Serial.printf_P(PSTR("_DEF:%d"), Settings::getDhcpMode());
		}
		else
		{
			if (cmd == CMD_AT_CWDHCP_CUR)
				Serial.print(F("_CUR"));
			Serial.print(':');

			Serial.println(gsCwDhcp);
		}

		Serial.printf_P(MSG_OK);
		error = false;
	}
	else if (inputBuffer[offset] == '=')
	{
		uint32_t mode;
		uint32_t en;

		++offset;

		if (readNumber(inputBuffer, offset, mode) && mode <= 2 && inputBuffer[offset] == ',')
		{
			++offset;

			const WiFiMode_t dhcpToMode[3] = { WIFI_AP, WIFI_STA, WIFI_AP_STA };

			if (dhcpToMode[mode] == WiFi.getMode())  // The mode must match the current mode
			{
				if (readNumber(inputBuffer, offset, en) && en <= 1 && inputBufferCnt == offset + 2)
				{
					gsCwDhcp = 1 | en << 1;  // Only Station DHCP is supported

					setDhcpMode();

					if (cmd == CMD_AT_CWDHCP_DEF)
						Settings::setDhcpMode(gsCwDhcp);

					Serial.printf_P(MSG_OK);
					error = false;
				}
			}
		}
	}

	if (error)
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CWJAP="ssid","pwd" [,"bssid"] - Connects to an AP, only ssid, pwd and bssid supported
 */
void cmd_AT_CWJAP(commands_t cmd)
{
	uint16_t offset = 8;  // offset to ? or =
	if (cmd != CMD_AT_CWJAP)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		if (WiFi.status() != WL_CONNECTED)
		{
			Serial.println(F("No AP"));
		}
		else
		{
			struct station_config conf;

			if (cmd == CMD_AT_CWJAP_DEF)
		        wifi_station_get_config_default(&conf);
			else
			    wifi_station_get_config(&conf);

			char ssid[33];

			memcpy(ssid, conf.ssid, sizeof(conf.ssid));
			ssid[32] = 0;  // Nullterm in case of 32 char ssid

			Serial.print(F("+CWJAP"));
			if (cmd == CMD_AT_CWJAP_CUR)
				Serial.print(F("_CUR"));
			else if (cmd == CMD_AT_CWJAP_DEF)
				Serial.print(F("_DEF"));

			// +CWJAP_CUR:<ssid>,<bssid>,<channel>,<rssi>
			Serial.printf_P(PSTR(":\"%s\",\"%02x:%02x:%02x:%02x:%02x:%02x\",%d,%d\r\n"), ssid,
					conf.bssid[0], conf.bssid[1], conf.bssid[2], conf.bssid[3], conf.bssid[4], conf.bssid[5],
					WiFi.channel(), WiFi.RSSI());
		}
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		bool error = true;

		do {
			String ssid;
			String pwd;
			String bssid;
			uint8_t uBssid[6];

			++offset;

			ssid = readStringFromBuffer(inputBuffer, offset, true);
			if (ssid.isEmpty() || (inputBuffer[offset] != ','))
				break;

			++offset;

			pwd = readStringFromBuffer(inputBuffer, offset, true);

			if (pwd.isEmpty())
				break;

			if (inputBuffer[offset] == ',')
			{
				++offset;

				bssid = readStringFromBuffer(inputBuffer, offset, false);

				if (bssid.isEmpty() || bssid.length() != 17)
					break;

				char fmt[40];
				strcpy_P(fmt, PSTR("%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx"));
				if (sscanf(bssid.c_str(), fmt, &uBssid[0], &uBssid[1], &uBssid[2], &uBssid[3], &uBssid[4], &uBssid[5]) != 6)
					break;
			}

			if (inputBufferCnt != offset + 2)
				break;

			if (cmd == CMD_AT_CWJAP_DEF)
				WiFi.persistent(true);

			// If connected, disconnect first
			if (WiFi.status() == WL_CONNECTED)
			{
				WiFi.disconnect();

				esp8266::polledTimeout::oneShot disconnectTimeout(5000);
				while (WiFi.status() == WL_CONNECTED && !disconnectTimeout)
				{
					delay(50);
				}

				if (WiFi.status() == WL_CONNECTED)
					break;  // Still connected
			}

			uint8_t *pBssid = nullptr;
			if (!bssid.isEmpty())
				pBssid = uBssid;

			WiFi.begin(ssid, pwd, 0, pBssid);

			WiFi.persistent(false);

			gsFlag_Connecting = true;

			// Hack: while connecting we need the autoreconnect feature to be switched on
			//       otherwise the connection fails (?)
			WiFi.setAutoReconnect(true);

			error = false;

		} while (0);

		if (error)
		{
			Serial.printf_P(MSG_ERROR);
		}
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CWQAP - Disconnects from the AP
 */
void cmd_AT_CWQAP()
{
	if (WiFi.status() == WL_CONNECTED)
	{
		WiFi.disconnect();
	}
	Serial.printf_P(MSG_OK);
}

/*
 * AT+SYSRAM? - Checks the Remaining Space of RAM
 */
void cmd_AT_SYSRAM()
{
	Serial.printf_P(PSTR("+SYSRAM:%d\r\nOK\r\n"), ESP.getFreeHeap());
}

/*
 * AT+RFMODE - Sets or queries current RF mode (custom command)
 */
void cmd_AT_RFMODE()
{
	if (inputBuffer[9] == '?' && inputBufferCnt == 12)
	{
		Serial.printf_P(PSTR("+RFMODE:%d\r\nOK\r\n"), wifi_get_phy_mode());
	}
	else if (inputBuffer[9] == '=')
	{
		uint16_t offset = 10;
		uint32_t mode;

		if (readNumber(inputBuffer, offset, mode) && mode >= 1 && mode <= 3 && inputBufferCnt == offset + 2)
		{
			phy_mode_t phymode = phy_mode_t(mode);
			wifi_set_phy_mode(phymode);

			Serial.printf_P(MSG_OK);
		}
		else
		{
			Serial.printf_P(MSG_ERROR);
		}
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPSTATUS - Gets the Connection Status
 */
void cmd_AT_CIPSTATUS()
{
	wl_status_t status = WiFi.status();

	if (status != WL_CONNECTED)
	{
		Serial.println(F("STATUS:5\r\n\r\nOK"));
	}
	else
	{
		bool statusPrinted = false;
		uint8_t maxCli = 0;  // Maximum client number
		if (gsCipMux == 1)
			maxCli = 4;

		for (uint8_t i = 0; i <= maxCli; ++i)
		{
			WiFiClient *cli = clients[i].client;
			if (cli != nullptr && cli->connected())
			{
				if (!statusPrinted)
				{
					Serial.println(F("STATUS:3"));
					statusPrinted = true;
				}

				const char types_text[3][4] = { "TCP", "UDP", "SSL" };
				Serial.printf_P(PSTR("+CIPSTATUS:%d,\"%s\",\"%s\",%d,%d,0\r\n"), i, types_text[clients[i].type],
						cli->remoteIP().toString().c_str(), cli->remotePort(), cli->localPort());
			}
		}

		if (!statusPrinted)
		{
			char stat;

			if (gsWasConnected)
				stat = '4';
			else
				stat = '2';

			Serial.printf_P(PSTR("STATUS:%c\r\n"), stat);
		}

		Serial.printf_P(MSG_OK);
	}
}

/*
 * AT+CIFSR - Gets the Local IP Address
 */
void cmd_AT_CIFSR()
{
	IPAddress ip = WiFi.localIP();
	if (! ip.isSet())
		Serial.println(F("+CISFR:STAIP,\"0.0.0.0\""));
	else
	Serial.printf_P(PSTR("+CISFR:STAIP,\"%s\"\r\n"), ip.toString().c_str());

	Serial.printf_P(PSTR("+CIFSR:STAMAC,\"%s\"\r\n"), WiFi.macAddress().c_str());
	Serial.printf_P(MSG_OK);
}

/*
 * AT+CIPSTA?, AT+CIPSTA_CUR? -  Obtains the current IP address
 */
void cmd_AT_CIPSTA(commands_t cmd)
{
	int offset = 9;

	if (cmd == CMD_AT_CIPSTA_CUR)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		IPAddress ip = WiFi.localIP();
		if (! ip.isSet())
		{
			Serial.println(F("+CIPSTA:ip:\"0.0.0.0\""));
			Serial.println(F("+CIPSTA:gateway:\"0.0.0.0\""));
			Serial.println(F("+CIPSTA:netmask:\"0.0.0.0\""));
		}
		else
		{
			Serial.printf_P(PSTR("+CIPSTA:ip:\"%s\"\r\n"), ip.toString().c_str());
			Serial.printf_P(PSTR("+CIPSTA:gateway:\"%s\"\r\n"), WiFi.gatewayIP().toString().c_str());
			Serial.printf_P(PSTR("+CIPSTA:netmask:\"%s\"\r\n"), WiFi.subnetMask().toString().c_str());
		}
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		// TODO: parameter setting (ip, gateway, mask)
		Serial.println(F("ERROR NOT IMPLEMENTED"));
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}

}

/*
 * AT+CIPSTART - Establishes TCP Connection, UDP Transmission or SSL Connection
 */
void cmd_AT_CIPSTART()
{
	/*
	 * AT+CIPMUX=0:  AT+CIPSTART=<type>,<remote IP>,<remote port>[,<TCP keep alive>]
	 * AT+CIPMUX=1:  AT+CIPSTART=<link ID>,<type>,<remote IP>,<remote port>[,<TCP keep alive>]
	 */
	uint8_t error = 1;  // 1 = generic error, 0 = ok
	uint16_t offset = 11;

	// Parse the input

	uint8_t linkID = 0;
	clientTypes_t type = TYPE_NONE;  // 0 = TCP, 1 = UDP, 2 = SSL
	char remoteAddr[41];
	uint32_t remotePort = 0;

	do
	{
		if (inputBuffer[offset] != '=')
			break;

		++offset;  error = 2;

		// Read link ID
		if (gsCipMux == 1)
		{
			if (inputBuffer[offset] >= '0' && inputBuffer[offset] <= '4' && inputBuffer[offset + 1] == ',')
				linkID = inputBuffer[offset] - '0';
			else
				break;

			offset += 2;
		}

		error = 3;

		if (inputBuffer[offset] != '"')
			break;

		++offset;

		// Read type

		if (!memcmp(inputBuffer + offset, "TCP", 3))
			type = TYPE_TCP;
		else if (!memcmp(inputBuffer + offset, "UDP", 3))
			type = TYPE_UDP;
		else if (!memcmp(inputBuffer + offset, "SSL", 3))
			type = TYPE_SSL;
		else
			break;

		offset += 3;  error = 4;

		if (inputBuffer[offset] != '"' || inputBuffer[offset + 1] != ',' || inputBuffer[offset + 2] != '"')
			break;

		offset += 3;

		// Read remote address

		uint8_t pos = 0;

		while (pos < sizeof(remoteAddr)-1 && inputBuffer[offset] != '"' && inputBuffer[offset] >= ' ')
		{
			remoteAddr[pos++] = inputBuffer[offset++];
		}
		remoteAddr[pos] = 0;

		if (inputBuffer[offset] != '"' || inputBuffer[offset + 1] != ',')
			break;

		offset += 2;

		// Read remote port

		error = 100;  // Unspecified error

		if (!readNumber(inputBuffer, offset, remotePort) || remotePort > 65535)
			break;

		// TCP timeout is read but ignored

		if (offset + 2 < inputBufferCnt)
		{
			if (inputBuffer[offset] != ',')
				break;

			++offset;

			while (inputBuffer[offset] >= '0' && inputBuffer[offset] <= '9')
			{
				++offset;
			}
		}

		if (offset + 2 != inputBufferCnt)
			break;

		error = 0;

	} while (0);

	if (!error)
	{
		do
		{
			AT_DEBUG_PRINTF("--- linkId=%d, type=%d, addr=%s, port=%d\r\n", linkID, type, remoteAddr, (uint16_t)remotePort);

			// Check if connected to an AP
			if (!WiFi.isConnected())
			{
				error = 6;
				break;
			}

			// Check if the client is already connected

			if (clients[linkID].client != nullptr)
			{
				error = 5;
				break;
			}

			WiFiClient *cli = nullptr;

			error = 99;

			if (type == 0)  // TCP
			{
				cli = new WiFiClient();
			}
			else if (type == 2)  // SSL
			{
				cli = new BearSSL::WiFiClientSecure();
				((BearSSL::WiFiClientSecure*)cli)->setInsecure();  // TODO: implement TLS security
			}

			// Check OOM
			if (cli == nullptr)
				break;

		    IPAddress remoteIP;
		    uint16_t _timeout = 5000;
		    if (!WiFi.hostByName(remoteAddr, remoteIP, _timeout))
		    {
		    	error = 100;
		    	Serial.println(F("DNS Fail"));
		    	break;
		    }

			if (!cli->connect(remoteIP, remotePort))
			{
				free(cli);
				error = 100;
				break;
			}

			if (gsCipMux == 0)
				Serial.println(F("CONNECT\r\n\r\nOK"));
			else
				Serial.printf_P(PSTR("%d,CONNECT\r\n\r\nOK\r\n"), linkID);

			clients[linkID].client = cli;
			clients[linkID].type = type;

			gsWasConnected = true;  // Flag for CIPSTATUS command

			error = 0;

		} while (0);
	}

	if (error > 0)
	{
		if (error == 100)
		{
			Serial.printf_P(MSG_ERROR);
			Serial.println(F("CLOSED"));
		}
		else
		{
			if (error == 3)
				Serial.println(F("Link type ERROR\r\n"));
			else if (error == 4)
				Serial.println(F("IP ERROR\r\n"));
			else if (error == 5)
				Serial.println(F("ALREADY CONNECTED\r\n"));
			else if (error == 6)
				Serial.println(F("no ip"));

			Serial.printf_P(MSG_ERROR);
		}
	}
}

/*
 * AT+CIPSEND - Sends Data
 */
void cmd_AT_CIPSEND()
{
	uint8_t error = 1;

	do
	{
		uint8_t linkId = 0;
		uint16_t offset;
		uint32_t size = 0;

		// Test the input

		if (inputBuffer[10] != '=')
			break;

		// Read linkId
		if (inputBuffer[11] >= '0' && inputBuffer[11] <= '5' && inputBuffer[12] == ',')
		{
			if (gsCipMux == 0)
			{
				Serial.println("MUX=0\r\n\r\n");
				break;
			}

			linkId = inputBuffer[11] - '0';

			offset = 13;
		}
		else
			offset = 11;

		client_t *cli = &(clients[linkId]);

		// Test the link
		if (cli->client == nullptr || !cli->client->connected())
		{
			Serial.println(F("link is not valid\r\n"));
			break;
		}

		if (!readNumber(inputBuffer, offset, size) || offset + 2 != inputBufferCnt)
			break;

		if (size > 2048)
		{
			Serial.println(F("too long\r\n"));
			break;
		}

		AT_DEBUG_PRINTF("--- linkId: %d, size: %d\r\n", linkId, size);

		// Start reading data into the buffer

		cli->sendLength = size;

		gsLinkIdReading = linkId;
		dataRead = 0;

		error = 0;

	} while (0);

	if (error > 0)
		Serial.printf_P(MSG_ERROR);
	else
		Serial.print(F("OK\r\n>"));
}

/*
 * AT+CIPCLOSE - Closes the TCP/UDP/SSL Connection
 */
void cmd_AT_CIPCLOSE()
{
	uint8_t error = 1;

	do
	{
		uint16_t offset = 12;
		uint32_t linkId = 0;

		// Read the input

		if (inputBuffer[11] == '=')
		{
			if (!readNumber(inputBuffer, offset, linkId) || linkId > 5 || inputBufferCnt != offset + 2)
				break;

			if (gsCipMux == 0)
			{
				Serial.println(F("MUX=0"));
				break;
			}
		}
		else if (inputBufferCnt != 13)
			break;
		else if (gsCipMux != 0)
		{
			Serial.println(F("MUX=1"));
			break;
		}

		error = 0;

		for (uint8_t id = 0; id <= 4; ++id)
		{
			if (id == linkId || linkId == 5)
			{
				// Disconnect
				WiFiClient *cli = clients[id].client;

				if (cli == nullptr)
				{
					if (linkId != 5)
					{
						if (gsCipMux != 0)
							Serial.println(F("UNLINK"));

						error = 1;
						break;
					}
				}
				else
				{
					if (cli->connected())
						cli->stop();

					DeleteClient(id);

					if (gsCipMux == 0)
						Serial.println(F("CLOSED"));
					else
						Serial.printf_P(PSTR("%d,CLOSED\r\n"), id);
				}
			}

			if (error)
				break;
		}
	} while (0);

	if (error > 0)
		Serial.printf_P(MSG_ERROR);
	else
		Serial.printf_P(MSG_OK);
}

/*
 * AT+UART=baudrate,databits,stopbits,parity,flow - UART Configuration
 */
void cmd_AT_UART(commands_t cmd)
{
	uint16_t offset = 7;
	if (cmd == CMD_AT_UART_CUR || cmd == CMD_AT_UART_DEF)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset+3)
	{
		Serial.print(F("+UART"));
		if (cmd == CMD_AT_UART_CUR)
			Serial.print(F("_CUR"));
		else if (cmd == CMD_AT_UART_DEF)
			Serial.print(F("_DEF"));

		/*
		 * UART Register USC0:
		 * 		UCSBN   4  //StopBits Count (2bit) 0:disable, 1:1bit, 2:1.5bit, 3:2bit
		 * 		UCBN    2  //DataBits Count (2bin) 0:5bit, 1:6bit, 2:7bit, 3:8bit
		 * 		UCPAE   1  //Parity Enable
		 * 		UCPA    0  //Parity 0:even, 1:odd
		 */

		uint32_t uartConfig;
		uint32_t baudRate;

		if (cmd == CMD_AT_UART_DEF)
		{
			uartConfig = Settings::getUartConfig();
			baudRate = Settings::getUartBaudRate();
		}
		else
		{
			uartConfig = USC0(0);
			baudRate = Serial.baudRate();
		}

		uint8_t databits = 5 + ((uartConfig >> UCBN) & 3);
		uint8_t stopbits = (uartConfig >> UCSBN) & 3;
		uint8_t parity = uartConfig & 3;

		Serial.printf(":%d,%d,%d,%d,0\r\nOK\r\n", baudRate, databits, stopbits, parity);
	}
	else if (inputBuffer[offset] == '=')
	{
		uint8_t error = 1;

		++offset;

		do
		{
			uint32_t baudRate;
			uint32_t dataBits;
			uint32_t stopBits;
			uint32_t parity;
			uint32_t flow;
			SerialConfig uartConfig;

			if (!readNumber(inputBuffer, offset, baudRate) || baudRate < 110 || baudRate > 921600 || inputBuffer[offset] != ',')
				break;

			++offset;

			if (!readNumber(inputBuffer, offset, dataBits) || dataBits < 5 || dataBits > 8 || inputBuffer[offset] != ',')
				break;

			++offset;

			if (!readNumber(inputBuffer, offset, stopBits) || stopBits < 1 || stopBits > 3 || inputBuffer[offset] != ',')
				break;

			++offset;

			if (!readNumber(inputBuffer, offset, parity) || parity > 2 || inputBuffer[offset] != ',')
				break;

			++offset;

			if (!readNumber(inputBuffer, offset, flow) || flow > 3 || inputBufferCnt != offset + 2)
				break;

			if (flow != 0)
			{
				Serial.println(F("NOT IMPLEMENTED"));
				break;
			}

			uartConfig = (SerialConfig) (((dataBits - 5) << UCBN) | (stopBits << UCSBN) | parity);

			AT_DEBUG_PRINTF("--- %d,%02x\r\n", baudRate, uartConfig);

			error = 0;

			// Restart the serial interface

			Serial.flush();
			Serial.end();
			Serial.begin(baudRate, uartConfig);
			delay(250);  // To let the line settle

			if (cmd == CMD_AT_UART_DEF)
			{
				Settings::setUartBaudRate(baudRate);
				Settings::setUartConfig(uartConfig);
			}

		} while (0);

		if (error == 0)
			Serial.printf_P(MSG_OK);
		else if (error == 1)
			Serial.printf_P(MSG_ERROR);
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+RESTORE - Restores the Factory Default Settings
 */
void cmd_AT_RESTORE()
{
	Serial.printf_P(MSG_OK);

	// Reset the EEPROM configuration
	Settings::reset();

	ESP.reset();
}

/*
 * AT+CIPSSLSIZE - Sets the Size of SSL Buffer - the command is parsed but ignored
 */
void cmd_AT_CIPSSLSIZE()
{
	uint16_t offset = 13;

	if (inputBuffer[offset] == '=')
	{
		unsigned int sslSize = 0;

		++offset;

		if (readNumber(inputBuffer, offset, sslSize) && inputBufferCnt == offset + 2 && sslSize >= 2048)
		{
			Serial.printf_P(MSG_OK);
		}
		else
		{
			Serial.printf_P(MSG_ERROR);
		}
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*********************************************************************************************
 * Searches the input buffer for a command. Returns command code or CMD_ERROR
 */
commands_t findCommand(uint8_t *input, uint16_t inpLen)
{
	// Check the buffer invariants
	if (inpLen < 4 || input[0] != 'A' || input[1] != 'T' || input[inpLen - 2] != '\r' || input[inpLen - 1] != '\n')
		return CMD_ERROR;

	// AT command as a special case
	if (inpLen == 4)
		return CMD_AT;

	// Check the command list
	for (unsigned int i = 0; i < sizeof(commandList) / sizeof(commandDef_t); ++i)
	{
		const char *cmd = commandList[i].text;

		if (!memcmp(cmd, input + 2, strlen(cmd)))
		{
			// We have a command

			switch (commandList[i].mode)
			{
			case MODE_NO_CHECKING:
				break;

			case MODE_EXACT_MATCH:
				if (inpLen != strlen(cmd) + 4)  // Check exact length
					return CMD_ERROR;
				break;

			case MODE_QUERY_SET:
			{
				char c = input[strlen(cmd) + 2];
				if (c != '=' && c != '?')
					return CMD_ERROR;
				if (c == '?' && inpLen != strlen(cmd) + 5) // Check exact length
					return CMD_ERROR;
			}
				break;

			default:
				return CMD_ERROR;
			}

			return commandList[i].cmd;
		}
	}

	return CMD_ERROR;
}

/*
 * Read quote delimited string from the buffer, adjust offset according to read data
 * Maximum text length is 100 characters
 */
String readStringFromBuffer(unsigned char *inpBuf, uint16_t &offset, bool escape)
{
	String sRet;

	if (inpBuf[offset] == '"')
	{
		char s[100];
		char *ps = s;

		++offset;

		// Until the end of the string
		while (inpBuf[offset] != '"' && inpBuf[offset] >= ' ')
		{
			if (!escape || inpBuf[offset] != '\\')
				*ps++ = inpBuf[offset];
			else if (inpBuf[offset] < ' ')
				break;  // Incorrect escaped char
			else
				*ps++ = inpBuf[++offset];  // Write next char

			++offset;

			if (ps - s > (int)sizeof(s))
				break;  // Buffer overflow
		}

		if (inpBuf[offset] == '"' && ps != s)
		{
			// We got the string
			sRet.concat(s, ps - s);
			++offset;
		}
	}

	// Error return
	return sRet;
}

/*
 * Reads a number from the buffer
 */
bool readNumber(unsigned char *inpBuf, uint16_t &offset, uint32_t &output)
{
	bool ret = false;
	uint32_t out = 0;

	while (inpBuf[offset] >= '0' && inpBuf[offset] <= '9')
	{
		out = out * 10 + inpBuf[offset++] - '0';
		ret = true;
	}

	if (ret)
		output = out;

	return ret;
}
