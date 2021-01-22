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

#include "sntp.h"
#include <time.h>

#include "ESP_ATMod.h"
#include "command.h"
#include "settings.h"
#include "asnDecode.h"
#include "debug.h"

/*
 * Constants
 */

const char * suffix_CUR = "_CUR";
const char * suffix_DEF = "_DEF";

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
		{ "+CIPSTA_DEF", MODE_QUERY_SET, CMD_AT_CIPSTA_DEF },
		{ "+CIPSTA", MODE_QUERY_SET, CMD_AT_CIPSTA },
		{ "+CIPSSLSIZE", MODE_QUERY_SET, CMD_AT_CIPSSLSIZE },
		{ "+CIPSSLAUTH", MODE_QUERY_SET, CMD_AT_CIPSSLAUTH },
		{ "+CIPSSLFP", MODE_QUERY_SET, CMD_AT_CIPSSLFP },
		{ "+CIPSSLCERT", MODE_NO_CHECKING, CMD_AT_CIPSSLCERT },
		{ "+CIPSSLMFLN", MODE_QUERY_SET, CMD_AT_CIPSSLMFLN },
		{ "+CIPSSLSTA", MODE_NO_CHECKING, CMD_AT_CIPSSLSTA },
		{ "+CIPRECVMODE", MODE_QUERY_SET, CMD_AT_CIPRECVMODE },
		{ "+CIPRECVLEN", MODE_QUERY_SET, CMD_AT_CIPRECVLEN },
		{ "+CIPRECVDATA", MODE_QUERY_SET, CMD_AT_CIPRECVDATA },
		{ "+CIPDNS_CUR", MODE_QUERY_SET, CMD_AT_CIPDNS_CUR },
		{ "+CIPDNS_DEF", MODE_QUERY_SET, CMD_AT_CIPDNS_DEF },
		{ "+CIPDNS", MODE_QUERY_SET, CMD_AT_CIPDNS },
		{ "+SYSCPUFREQ", MODE_QUERY_SET, CMD_AT_SYSCPUFREQ },
		{ "+CIPSNTPCFG", MODE_QUERY_SET, CMD_AT_CIPSNTPCFG },
		{ "+SNTPTIME?", MODE_EXACT_MATCH, CMD_AT_SNTPTIME },
		{ "+CIPSNTPTIME?", MODE_EXACT_MATCH, CMD_AT_CIPSNTPTIME }
};

/*
 * Static functions
 */

commands_t findCommand(uint8_t* input, uint16_t inpLen);
String readStringFromBuffer(unsigned char *inpBuf, uint16_t &offset, bool escape);
bool readNumber(unsigned char *inpBuf, uint16_t &offset, uint32_t &output);
bool readIpAddress(unsigned char *inpBuf, uint16_t &offset, uint32_t &output);
uint8_t readHex(char c);

/*
 * Commands
 */

static void cmd_AT();
static void cmd_ATE();
static void cmd_AT_GMR();
static void cmd_AT_RST();
static void cmd_AT_CWAUTOCONN();
static void cmd_AT_CWMODE(commands_t cmd);
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
static void cmd_AT_CIPSSLAUTH();
static void cmd_AT_CIPSSLFP();
static void cmd_AT_CIPSSLCERT();
static void cmd_AT_CIPRECVMODE();
static void cmd_AT_CIPRECVLEN();
static void cmd_AT_CIPRECVDATA();
static void cmd_AT_CIPDNS(commands_t cmd);
static void cmd_AT_SYSCPUFREQ();
static void cmd_AT_CIPSSLMFLN();
static void cmd_AT_CIPSSLSTA();
static void cmd_AT_SNTPTIME();
static void cmd_AT_CIPSNTPCFG();
static void cmd_AT_CIPSNTPTIME();

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
		cmd_AT_CWMODE(cmd);

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
	else if (cmd == CMD_AT_CIPSTA || cmd == CMD_AT_CIPSTA_CUR || cmd == CMD_AT_CIPSTA_DEF)
		// AT+CIPSTA - Sets or prints the network configuration
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

	// ------------------------------------------------------------------------------------ AT+CIPSSLAUTH
	else if (cmd == CMD_AT_CIPSSLAUTH)  // AT+CIPSSLAUTH - Authentication type
		cmd_AT_CIPSSLAUTH();

	// ------------------------------------------------------------------------------------ AT+CIPSSLFP
	else if (cmd == CMD_AT_CIPSSLFP)  // AT+CIPSSLFP - Shows or stores certificate fingerprint
		cmd_AT_CIPSSLFP();

	// ------------------------------------------------------------------------------------ AT+CIPSSLCERT
	else if (cmd == CMD_AT_CIPSSLCERT)  // AT+CIPSSLCERT - Load CA certificate in PEM format
		cmd_AT_CIPSSLCERT();

	// ------------------------------------------------------------------------------------ AT+CIPRECVMODE
	else if (cmd == CMD_AT_CIPRECVMODE)  // AT+CIPRECVMODE - Set TCP Receive Mode
		cmd_AT_CIPRECVMODE();

	// ------------------------------------------------------------------------------------ AT+CIPRECVLEN
	else if (cmd == CMD_AT_CIPRECVLEN)  // AT+CIPRECVLEN - Get TCP Data Length in Passive Receive Mode
		cmd_AT_CIPRECVLEN();

	// ------------------------------------------------------------------------------------ AT+CIPRECVDATA
	else if (cmd == CMD_AT_CIPRECVDATA)  // AT+CIPRECVDATA - Get TCP Data in Passive Receive Mode
		cmd_AT_CIPRECVDATA();

	// ------------------------------------------------------------------------------------ AT+CIPDNS
	else if (cmd == CMD_AT_CIPDNS || cmd == CMD_AT_CIPDNS_CUR || cmd == CMD_AT_CIPDNS_DEF)
		// AT+CIPDNS - Sets User-defined DNS Servers
		cmd_AT_CIPDNS(cmd);

	// ------------------------------------------------------------------------------------ AT+SYSCPUFREQ
	else if (cmd == CMD_AT_SYSCPUFREQ)  // AT+SYSCPUFREQ - Set or Get the Current CPU Frequency
		cmd_AT_SYSCPUFREQ();

	// ------------------------------------------------------------------------------------ AT+CIPSSLMFLN
	else if (cmd == CMD_AT_CIPSSLMFLN)  // AT+CIPSSLMFLN - Check the capability of MFLN for a site
		cmd_AT_CIPSSLMFLN();

	// ------------------------------------------------------------------------------------ AT+CIPSSLSTA
	else if (cmd == CMD_AT_CIPSSLSTA)  // AT+CIPSSLSTA - Check the MFLN status for a connection
		cmd_AT_CIPSSLSTA();

	// ------------------------------------------------------------------------------------ AT+SNTPTIME?
	else if (cmd == CMD_AT_SNTPTIME)  // AT+SNTPTIME? - get time
		cmd_AT_SNTPTIME();

	// ------------------------------------------------------------------------------------ AT+CIPSNTPCFG
	else if (cmd == CMD_AT_CIPSNTPCFG)  // AT+CIPSNTPCFG - configure SNTP time
		cmd_AT_CIPSNTPCFG();

	// ------------------------------------------------------------------------------------ AT+CIPSNTPTIME?
	else if (cmd == CMD_AT_CIPSNTPTIME)  // AT+CIPSNTPTIME? - get time in asctime format
		cmd_AT_CIPSNTPTIME();

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
	Serial.println(F("AT version:1.7.0.0 (partial)"));
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
void cmd_AT_CWMODE(commands_t cmd)
{
	uint16_t offset = 9;  // offset to ? or =
	if (cmd != CMD_AT_CWMODE)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		const char *cmdSuffix = "";
		if (cmd == CMD_AT_CWMODE_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_CWMODE_DEF)
			cmdSuffix = suffix_DEF;

		Serial.printf_P(PSTR("+CWMODE%s:%d\r\n"), cmdSuffix, WiFi.getMode());
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
		const char *cmdSuffix = "";
		if (cmd == CMD_AT_CWDHCP_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_CWDHCP_DEF)
			cmdSuffix = suffix_DEF;

		uint8_t dhcp;

		if (cmd == CMD_AT_CWDHCP_DEF)
			dhcp = Settings::getDhcpMode();
		else
			dhcp = gsCwDhcp;

		Serial.printf_P(PSTR("+CWDHCP%s:%d\r\n"), cmdSuffix, dhcp);

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

					if (cmd != CMD_AT_CWDHCP_CUR)
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

			const char *cmdSuffix = "";
			if (cmd == CMD_AT_CWJAP_CUR)
				cmdSuffix = suffix_CUR;
			else if (cmd == CMD_AT_CWJAP_DEF)
				cmdSuffix = suffix_DEF;

			Serial.printf_P(PSTR("+CWJAP%s:"), cmdSuffix);

			// +CWJAP_CUR:<ssid>,<bssid>,<channel>,<rssi>
			Serial.printf_P(PSTR("\"%s\",\"%02x:%02x:%02x:%02x:%02x:%02x\",%d,%d\r\n"), ssid,
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

			if (cmd != CMD_AT_CWJAP_CUR)
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
 * AT+CIPSTA - Sets or prints the network configuration
 */
void cmd_AT_CIPSTA(commands_t cmd)
{
	uint16_t offset = 9;

	if (cmd != CMD_AT_CIPSTA)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		ipConfig_t cfg;

		if (cmd == CMD_AT_CIPSTA_DEF)
		{
			cfg = Settings::getNetConfig();
		}
		else
		{
			cfg = { WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask() };
		}

		const char *cmdSuffix = "";
		if (cmd == CMD_AT_CIPSTA_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_CIPSTA_DEF)
			cmdSuffix = suffix_DEF;

		if (WiFi.status() != WL_CONNECTED || cfg.ip == 0)
		{
			Serial.printf_P(PSTR("+CIPSTA%s:ip:\"0.0.0.0\"\r\n"), cmdSuffix);
			Serial.printf_P(PSTR("+CIPSTA%s:gateway:\"0.0.0.0\"\r\n"), cmdSuffix);
			Serial.printf_P(PSTR("+CIPSTA%s:netmask:\"0.0.0.0\"\r\n"), cmdSuffix);
		}
		else
		{
			Serial.printf_P(PSTR("+CIPSTA%s:ip:\"%s\"\r\n"), cmdSuffix, IPAddress(cfg.ip).toString().c_str());
			Serial.printf_P(PSTR("+CIPSTA%s:gateway:\"%s\"\r\n"), cmdSuffix, IPAddress(cfg.gw).toString().c_str());
			Serial.printf_P(PSTR("+CIPSTA%s:netmask:\"%s\"\r\n"), cmdSuffix, IPAddress(cfg.mask).toString().c_str());
		}
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		uint8_t error = 1;

		++offset;

		do
		{
			ipConfig_t cfg;

			if (!readIpAddress(inputBuffer, offset, cfg.ip))
				break;

			if (inputBuffer[offset] != ',')
			{
				if (inputBufferCnt != offset + 2)
					break;

				// Only IP address is given, derive gateway and subnet /24
				if (cfg.ip != 0)
				{
					cfg.gw = (cfg.ip & 0x00ffffff) | 0x01000000;
					cfg.mask = 0x00ffffff;
				}

				error = 0;
			}
			else  // read gateway and mask
			{
				++offset;

				if (!readIpAddress(inputBuffer, offset, cfg.gw) || inputBuffer[offset] != ',')
					break;

				++offset;

				if (!readIpAddress(inputBuffer, offset, cfg.mask) || inputBufferCnt != offset + 2)
					break;

				error = 0;
			}

			// We got the network configuration

			if (cmd != CMD_AT_CIPSTA_CUR)
			{
				// Save the network configuration
				Settings::setNetConfig(cfg);
				Settings::setDhcpMode(1);  // Stop DHCP
			}

			gsCipStaCfg = cfg;
			gsCwDhcp = 1;  // Stop DHCP

			// Reconfigure (stop DHCP and set the static addresses)
			setDhcpMode();

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

				if (gsCipSslSize != 16384)
					static_cast<BearSSL::WiFiClientSecure*>(cli)->setBufferSizes(gsCipSslSize, 512);

				if (gsCipSslAuth == 0)
				{
					static_cast<BearSSL::WiFiClientSecure*>(cli)->setInsecure();
				}
				else if (gsCipSslAuth == 1 && fingerprintValid)
				{
					static_cast<BearSSL::WiFiClientSecure*>(cli)->setFingerprint(fingerprint);
				}
				else if (gsCipSslAuth == 2 && CAcert != nullptr) // certificate chain verification
				{
					static_cast<BearSSL::WiFiClientSecure*>(cli)->setTrustAnchors(CAcert);
				}
				else
				{
					delete cli;
					break;  // error
				}
			}

			// Check OOM
			if (cli == nullptr)
				break;

			// Test if the remote host exists
		    IPAddress remoteIP;
		    uint16_t _timeout = 5000;
		    if (!WiFi.hostByName(remoteAddr, remoteIP, _timeout))
		    {
		    	delete cli;
		    	error = 100;

		    	Serial.println(F("DNS Fail"));
		    	break;
		    }

		    // Connect using remote host name, not ip address (necessary for TLS)
			if (!cli->connect(remoteAddr, remotePort))
			{
				Serial.println("connect fail");

				delete cli;
				error = 100;

				break;
			}

			if (gsCipMux == 0)
				Serial.println(F("CONNECT\r\n\r\nOK"));
			else
				Serial.printf_P(PSTR("%d,CONNECT\r\n\r\nOK\r\n"), linkID);

			clients[linkID].client = cli;
			clients[linkID].type = type;
			clients[linkID].lastAvailableBytes = 0;

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
				Serial.println("MUX=0");
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
			Serial.println(F("link is not valid"));
			break;
		}

		if (!readNumber(inputBuffer, offset, size) || offset + 2 != inputBufferCnt)
			break;

		if (size > 2048)
		{
			Serial.println(F("too long"));
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
		const char *cmdSuffix = "";
		if (cmd == CMD_AT_UART_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_UART_DEF)
			cmdSuffix = suffix_DEF;

		Serial.printf_P(PSTR("+UART%s:"), cmdSuffix);

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

		Serial.printf("%d,%d,%d,%d,0\r\nOK\r\n", baudRate, databits, stopbits, parity);
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

			if (cmd != CMD_AT_UART_CUR)
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
 * AT+CIPSSLSIZE - Sets the Size of SSL Buffer - only sizes 512, 1024, 2048 and 4096 are supported
 */
void cmd_AT_CIPSSLSIZE()
{
	uint16_t offset = 13;

	if (inputBuffer[offset] == '=')
	{
		unsigned int sslSize = 0;

		++offset;

		if (readNumber(inputBuffer, offset, sslSize) && inputBufferCnt == offset + 2
				&& (sslSize == 512 || sslSize == 1024 || sslSize == 2048 || sslSize == 4096 || sslSize == 16384))
		{
			if (sslSize == 16384)
				sslSize = 0;  // default value

			gsCipSslSize = sslSize;

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
 * AT+CIPSSLAUTH - Authentication type
 *                 0 = none
 *                 1 = fingerprint
 *                 2 = certificate chain
 */
void cmd_AT_CIPSSLAUTH()
{
	bool error = true;

	if (inputBuffer[13] == '?' && inputBufferCnt == 16)
	{
		Serial.printf_P(PSTR("+CIPSSLAUTH:%d\r\n"), gsCipSslAuth);
		error = false;
	}
	else if (inputBuffer[13] == '=')
	{
		uint16_t offset = 14;
		uint32_t sslAuth;

		if (readNumber(inputBuffer, offset, sslAuth) && sslAuth <= 2 && inputBufferCnt == offset + 2)
		{
			if (sslAuth == 1 && !fingerprintValid)
			{
				Serial.println(F("fp not valid"));
			}
			else if (sslAuth == 2 && CAcert == nullptr)
			{
				Serial.println(F("CA cert not loaded"));
			}
			else
			{
				gsCipSslAuth = sslAuth;

				error = false;
			}
		}
	}

	if (! error)
	{
		Serial.printf_P(MSG_OK);
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPSSLFP - Shows or stores certificate SHA-1 fingerprint
 *               Fingerprint format: "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
 *                                or "xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx:xx"
 */
void cmd_AT_CIPSSLFP()
{
	if (inputBuffer[11] == '?' && inputBufferCnt == 14)
	{
		if (fingerprintValid)
		{
			Serial.print(F("+CIPSSLFP:\""));

			for (int i = 0; i < 20; ++i)
			{
				if (i > 0)
					Serial.print(':');

				Serial.printf("%02x", fingerprint[i]);
			}

			Serial.println(F("\"\r\n\r\nOK"));
		}
		else
		{
			Serial.println("not valid");
			Serial.printf_P(MSG_ERROR);
		}
	}
	else if (inputBuffer[11] == '=' && inputBuffer[12] == '"' && (inputBufferCnt == 56 || inputBufferCnt == 75)) // count = 16 + 2*20 (+ 19)
	{
		uint8_t fp[20];
		uint16_t offset = 13;
		int i;

		for (i = 0; i < 20; ++i)
		{
			if (!isxdigit(inputBuffer[offset]) || !isxdigit(inputBuffer[offset + 1]))
				break;

			fp[i] = readHex(inputBuffer[offset]) << 4 | readHex(inputBuffer[offset + 1]);
			offset += 2;

			if (i < 19 && inputBufferCnt == 75 && inputBuffer[offset++] != ':')
				break;
		}

		if (i == 20 && inputBuffer[offset] == '"')
		{
			memcpy(fingerprint, fp, sizeof(fingerprint));
			fingerprintValid = true;

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
 * AT+CIPSSLCERT - Load CA certificate in PEM format
 */
void cmd_AT_CIPSSLCERT()
{
	if (inputBufferCnt == 15)
	{
		PemCertificate = new char[MAX_PEM_CERT_LENGTH];
		PemCertificatePos = 0;
		PemCertificateCount = 0;

		if (PemCertificate != nullptr)
		{
			gsCertLoading = true;

			Serial.printf_P(MSG_OK);
			Serial.print('>');
		}
		else  // OOM
		{
			Serial.printf_P(MSG_ERROR);
		}
	}
	else if (inputBuffer[13] == '?' && inputBufferCnt == 16)
	{
		if (CAcert == nullptr)
		{
			Serial.println(F("+CIPSSLCERT:no cert"));
		}
		else
		{
			Serial.print(F("+CIPSSLCERT:"));

			const br_x509_certificate *cert = &(CAcert->getX509Certs()[0]);

			uint8_t *cnBytes = getCnFromDer(cert->data, cert->data_len);

			if (cnBytes != nullptr)
			{
				char *cn = new char[cnBytes[0] + 1];

				if (cn != nullptr)
				{
					memcpy(cn, &(cnBytes[1]), cnBytes[0]);
					cn[cnBytes[0]] = '\0';

					Serial.println(cn);

					delete cn;
				}
				else
				{
					Serial.println(F("cert ok"));
				}
			}
			else
			{
				Serial.println(F("cert ok"));
			}
		}

		Serial.printf_P(MSG_OK);
	}
	else if (!memcmp_P(&(inputBuffer[13]), PSTR("=DELETE"), 7) && inputBufferCnt == 22)
	{
		if (CAcert == nullptr)
		{
			Serial.println(F("+CIPSSLCERT:no cert"));
		}
		else
		{
			delete CAcert;
			CAcert = nullptr;

			Serial.print(F("+CIPSSLCERT:deleted"));
		}

		Serial.printf_P(MSG_OK);
	}

}

/*
 * AT+CIPRECVMODE - Set TCP Receive Mode
 */
void cmd_AT_CIPRECVMODE()
{
	if (inputBuffer[14] == '?' && inputBufferCnt == 17)
	{
		Serial.printf_P(PSTR("+CIPRECVMODE:%d\r\n\r\nOK\r\n"), gsCipRecvMode);
	}
	else if (inputBuffer[14] == '=')
	{
		uint32_t recvMode;
		uint16_t offset = 15;

		if (readNumber(inputBuffer, offset, recvMode) && recvMode <= 1 && inputBufferCnt == offset + 2)
		{
			gsCipRecvMode = recvMode;
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
 * AT+CIPRECVLEN - Get TCP Data Length in Passive Receive Mode
 */
void cmd_AT_CIPRECVLEN()
{
	if (inputBuffer[13] == '?' && inputBufferCnt == 16)
	{
		Serial.print(F("+CIPRECVLEN:"));

		for (uint8_t i = 0; i <= 4; ++i)
		{
			int avail = 0;

			if (i > 0)
				Serial.print(',');

			if (clients[i].client != nullptr)
			{
				avail = clients[i].client->available();
			}

			Serial.print(avail);
		}

		Serial.println();
		Serial.printf_P(MSG_OK);
	}
}

/*
 * AT+CIPRECVDATA - Get TCP Data in Passive Receive Mode
 */
void cmd_AT_CIPRECVDATA()
{
	uint8_t error = 1;

	do
	{
		uint8_t linkId = 0;
		uint16_t offset;
		uint32_t size = 0;

		// Test the input

		if (inputBuffer[14] != '=')
			break;

		// Read linkId
		if (inputBuffer[15] >= '0' && inputBuffer[15] <= '5' && inputBuffer[16] == ',')
		{
			if (gsCipMux == 0)
			{
				Serial.println("MUX=0");
				break;
			}

			linkId = inputBuffer[15] - '0';

			offset = 17;
		}
		else
			offset = 15;

		client_t *cli = &(clients[linkId]);

		// Test the link
		if (cli->client == nullptr)
		{
			Serial.println(F("link is not valid"));
			break;
		}

		if (!readNumber(inputBuffer, offset, size) || offset + 2 != inputBufferCnt)
			break;

		if (size > 2048)
		{
			Serial.println(F("too long"));
			break;
		}

		AT_DEBUG_PRINTF("--- linkId: %d, size: %d\r\n", linkId, size);

		// Send the data

		int bytes = SendData(linkId, size);

		if (bytes > 0)
		{
			cli->lastAvailableBytes -= bytes;
			error = 0;

			// Check if the recv mode changed to 0. If so, then close the client.
			if (gsCipRecvMode == 0)
			{
				DeleteClient(linkId);
			}
		}

	} while (0);

	if (error > 0)
		Serial.printf_P(MSG_ERROR);
	else
		Serial.printf_P(MSG_OK);
}

/*
 * AT+CIPDNS - Sets User-defined DNS Servers
 */
void cmd_AT_CIPDNS(commands_t cmd)
{
	uint16_t offset = 9;

	if (cmd != CMD_AT_CIPDNS)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		dnsConfig_t cfg;

		if (cmd == CMD_AT_CIPDNS_DEF)
		{
			cfg = Settings::getDnsConfig();
		}
		else
		{
			cfg = { WiFi.dnsIP(0), WiFi.dnsIP(1) };
		}

		const char *cmdSuffix = "";
		if (cmd == CMD_AT_CIPDNS_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_CIPDNS_DEF)
			cmdSuffix = suffix_DEF;

		if (cfg.dns1 != 0)
		{
			Serial.printf_P(PSTR("+CIPDNS%s:%s\r\n"), cmdSuffix, IPAddress(cfg.dns1).toString().c_str());

			if (cfg.dns2 != 0 && cfg.dns1 != cfg.dns2)
			{
				Serial.printf_P(PSTR("+CIPDNS%s:%s\r\n"), cmdSuffix, IPAddress(cfg.dns2).toString().c_str());
			}
		}
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		uint8_t error = 1;

		dnsConfig_t cfg = { 0, 0 };
		uint32_t dnsEnable;

		++offset;

		do
		{
			if (!readNumber(inputBuffer, offset, dnsEnable) || dnsEnable > 1)
				break;

			// enable = 0 ... no dns data, enable = 1 ... one or two ip addresses
			if ((dnsEnable == 0 && inputBufferCnt != offset + 2) ||
					(dnsEnable == 1 && inputBuffer[offset] != ','))
				break;

			if (dnsEnable == 1)
			{
				++offset;

				if (!readIpAddress(inputBuffer, offset, cfg.dns1))
					break;

				if (cfg.dns1 == 0)
				{
					Serial.println(F("IP1 invalid"));
					break;
				}

				if (inputBufferCnt != offset + 2)
				{
					if (inputBuffer[offset] != ',')
						break;

					++offset;

					if (!readIpAddress(inputBuffer, offset, cfg.dns2) || inputBufferCnt != offset + 2)
						break;

					if (cfg.dns2 == 0)
					{
						Serial.println(F("IP2 invalid"));
						break;
					}
				}
			}

			// We got the dns configuration

			if (cmd != CMD_AT_CIPDNS_CUR)
			{
				Settings::setDnsConfig(cfg);
			}

			gsCipDnsCfg = cfg;

			setDns();

			error = 0;

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
 * AT+SYSCPUFREQ - Set or Get the Current CPU Frequency
 */
void cmd_AT_SYSCPUFREQ()
{
	uint8_t error = 1;

	if (inputBuffer[13] == '?' && inputBufferCnt == 16)
	{
		uint8_t freq = system_get_cpu_freq();

		Serial.printf("+SYSCPUFREQ:%d\r\n", freq);
		error = 0;
	}

	else if (inputBuffer[13] == '=')
	{
		uint16_t offset = 14;
		uint32_t freq;

		if (readNumber(inputBuffer, offset, freq) && (freq == 80 || freq == 160))
		{
			if (system_update_cpu_freq(freq) != 0)
				error = 0;  // Success
		}
	}

	if (error == 0)
	{
		Serial.printf_P(MSG_OK);
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPSSLMFLN - Check the capability of MFLN for a site
 * Format: AT+CIPSSLMFLN=site,port,length
 * Example: AT+CIPSSLMFLN="tls.mbed.org",443,512
 */
void cmd_AT_CIPSSLMFLN()
{
	uint8_t error = 1;
	char remoteSite[41];
	uint32_t remotePort;
	uint32_t maxLen;

	do
	{
		if (inputBuffer[13] != '=' || inputBuffer[14] != '"')
			break;

		uint16_t offset = 15;

		// Read remote address

		uint8_t pos = 0;
		error = 4;

		while (pos < sizeof(remoteSite)-1 && inputBuffer[offset] != '"' && inputBuffer[offset] >= ' ')
		{
			remoteSite[pos++] = inputBuffer[offset++];
		}
		remoteSite[pos] = 0;

		if (inputBuffer[offset] != '"' || inputBuffer[offset + 1] != ',')
			break;

		offset += 2;

		// Read remote port

		error = 100;  // Unspecified error

		if (!readNumber(inputBuffer, offset, remotePort) || remotePort > 65535)
			break;

		// Buffer length

		error = 7;

		if (inputBuffer[offset] != ',')
			break;

		++offset;

		if (!readNumber(inputBuffer, offset, maxLen) || (maxLen != 512 && maxLen != 1024 && maxLen != 2048 && maxLen != 4096))
			break;

		if (offset + 2 != inputBufferCnt)
			break;

		// Check if connected to an AP
		if (!WiFi.isConnected())
		{
			error = 6;
			break;
		}

		error = 0;

		// Read the MFLN status
		bool mfln = BearSSL::WiFiClientSecure::probeMaxFragmentLength(remoteSite, remotePort, maxLen);

		Serial.printf_P(PSTR("+CIPSSLMFLN:%s\r\n"), mfln ? "TRUE" : "FALSE");

	} while (0);

	if (error == 0)
		Serial.printf_P(MSG_OK);
	else
	{
		if (error == 4)
			Serial.println(F("HOSTNAME ERROR\r\n"));
		else if (error == 6)
			Serial.println(F("NO AP"));
		else if (error == 7)
			Serial.println(F("SIZE ERROR\r\n"));

		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPSSLSTA - Check the MFLN status for a connection
 */
void cmd_AT_CIPSSLSTA()
{
	uint8_t error = 1;

	do
	{
		wl_status_t status = WiFi.status();

		if (status != WL_CONNECTED)
		{
			error = 2;
			break;
		}

		uint16_t offset = 13;
		uint32_t linkId = 0;

		// Read the input

		if (inputBuffer[12] == '=')
		{
			if (!readNumber(inputBuffer, offset, linkId) || linkId > 4 || inputBufferCnt != offset + 2)
				break;

			if (gsCipMux == 0)
			{
				Serial.println(F("MUX=0"));
				break;
			}
		}
		else if (inputBufferCnt != 14)
			break;
		else if (gsCipMux != 0)
		{
			Serial.println(F("MUX=1"));
			break;
		}

		// Check the client
		WiFiClient *cli = clients[linkId].client;

		if (cli == nullptr || !cli->connected())
		{
			error = 3;
			break;
		}

		if (clients[linkId].type != 2)
		{
			error = 4;
			break;
		}

		error = 0;

		// Print the status

		bool mfln = static_cast<WiFiClientSecure *>(cli)->getMFLNStatus();

		Serial.printf_P(PSTR("+CIPSSLSTA:%d\r\n"), mfln);

	} while (0);

	if (error == 0)
		Serial.printf_P(MSG_OK);
	else
	{
		if (error == 2)
			Serial.println(F("NOT CONNECTED"));
		else if (error == 3)
			Serial.println(F("NOT OPENED"));
		else if (error == 4)
			Serial.println(F("NOT A SSL"));

		Serial.printf_P(MSG_ERROR);
	}

}

/*
 * AT+SNTPTIME? - get time
 */
void cmd_AT_SNTPTIME()
{
	time_t now = time(nullptr);
	if (gsSTNPEnabled && (now > 8 * 3600 * 2))
	{
		now += gsSTNPTimezone * 3600;

		struct tm *info = localtime((const time_t *)&now);

		Serial.printf_P(PSTR("+SNTPTIME:%ld,%04d-%02d-%02d %02d:%02d:%02d\r\n"),
		            now, info->tm_year+1900, info->tm_mon+1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);

		Serial.println(F("OK"));
	}
	else
	{
		Serial.println(F("+SNTPTIME:Enable SNTP first (AT+CIPSNTPCFG)"));
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPSNTPCFG - configure SNTP time
 */
void cmd_AT_CIPSNTPCFG()  // FIXME:
{
	uint8_t error = 1;

	if (inputBuffer[13] == '?' && inputBufferCnt == 16)
	{
		Serial.printf_P(PSTR("+CIPSNTPCFG:%d"), gsSTNPEnabled ? 1 : 0);

		if (gsSTNPEnabled)
		{
			Serial.printf_P(PSTR(",%d"), gsSTNPTimezone);

			for (uint8_t i = 0; i < 3; ++i)
			{
				const char *sn = sntp_getservername(i);
				if (sn != nullptr)
					Serial.printf_P(PSTR(",\"%s\""), sn);
			}
		}

		Serial.println();

		error = 0;
	}

	else if (inputBuffer[13] == '=')
	{
		uint16_t offset = 14;
		error = 1;

		do
		{
			uint32_t sntpEnabled;
			uint32_t sntpTimezone;
			String sntpServer[3];

			bool tzNegative = false;

			if (!readNumber(inputBuffer, offset, sntpEnabled) || sntpEnabled > 1)
				break;

			// If enabling, read additional parameters
			if (sntpEnabled)
				{
				  if (inputBuffer[offset] != ',')
					  break;
				if (inputBuffer[++offset] == '-')
				{
					tzNegative = true;
					++offset;
				}

				if (!readNumber(inputBuffer, offset, sntpTimezone) || sntpEnabled > 12)
					break;

				for (uint8_t i = 0; i < 3; ++i)
				{
					if (inputBuffer[offset] != ',')
						break;

					sntpServer[i] = readStringFromBuffer(inputBuffer, ++offset, true);
				}
			}

			if (inputBufferCnt != offset + 2)
				break;

			gsSTNPEnabled = (sntpEnabled == 1);

			if (gsSTNPEnabled)
			{
				for (uint8_t i = 0; i < 3; ++ i)
					gsSNTPServer[i] = sntpServer[i];

				gsSTNPTimezone = (tzNegative ? -1 : +1) * sntpTimezone;

				configTime(gsSTNPTimezone, 0, nullIfEmpty(gsSNTPServer[0]), nullIfEmpty(gsSNTPServer[1]), nullIfEmpty(gsSNTPServer[2]));
			}

			error = 0;

		} while (0);
	}

	if (error == 0)
	{
		Serial.printf_P(MSG_OK);
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CIPSNTPTIME? - get time in asctime format
 * In case the time is not set correctly, returns 1. 1. 1970
 */
void cmd_AT_CIPSNTPTIME()
{
	time_t now = time(nullptr);

	if (gsSTNPEnabled && (now > 8 * 3600 * 2))
		now += gsSTNPTimezone * 3600;
	else
		now = 0;

	struct tm *info = localtime((const time_t *)&now);

	Serial.printf_P(PSTR("+CIPSNTPTIME:%s"), asctime(info));
	Serial.println(F("OK"));
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

/*
 * Reads a IPv4 address from buffer, returns a 32 bit integer
 * The address is enclosed in double quotes
 */
bool readIpAddress(unsigned char *inpBuf, uint16_t &offset, uint32_t &output)
{
	bool ret = false;
	uint32_t out = 0;

	if (inpBuf[offset] != '"')
		return false;

	++offset;

	for (int i = 1; i <= 4; ++i)
	{
		uint32_t addrByte = 0;

		if (!readNumber(inpBuf, offset, addrByte) || addrByte > 255)
			break;

		out = (out >> 8) | (addrByte << 24);

		// Check the separator
		if (i == 4)
			ret = true;
		else if (i < 4 && inpBuf[offset] != '.')
			break;
		else
			++offset;
	}

	if (inpBuf[offset] != '"')
		return false;

	++offset;

	if (ret)
		output = out;

	return ret;
}

/*
 * Translates ASCII to 1 nibble hex
 */
uint8_t readHex(char c)
{
	if (c >= 'a')
		c -= 'a' - 10;
	else if (c >= 'A')
		c -= 'A' - 10;
	else
		c -= '0';

	return c;
}
