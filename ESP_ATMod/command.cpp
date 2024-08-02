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

const char *suffix_CUR = "_CUR";
const char *suffix_DEF = "_DEF";

/*
 * Command list
 */

enum cmdMode_t
{
	MODE_NO_CHECKING, // no checking
	MODE_EXACT_MATCH, // exact match
	MODE_QUERY_SET	  // '?' or '=' follows
};

typedef struct
{
	const char *text;
	const cmdMode_t mode;
	const commands_t cmd;
} commandDef_t;

static const commandDef_t commandList[] = {
	{"+RST", MODE_EXACT_MATCH, CMD_AT_RST},
	{"+GMR", MODE_EXACT_MATCH, CMD_AT_GMR},
	{"E", MODE_NO_CHECKING, CMD_ATE},
	{"+RESTORE", MODE_EXACT_MATCH, CMD_AT_RESTORE},
	{"+UART", MODE_QUERY_SET, CMD_AT_UART},
	{"+UART_CUR", MODE_QUERY_SET, CMD_AT_UART_CUR},
	{"+UART_DEF", MODE_QUERY_SET, CMD_AT_UART_DEF},
	{"+SYSRAM?", MODE_EXACT_MATCH, CMD_AT_SYSRAM},

	{"+CWMODE", MODE_QUERY_SET, CMD_AT_CWMODE},
	{"+CWMODE_CUR", MODE_QUERY_SET, CMD_AT_CWMODE_CUR},
	{"+CWMODE_DEF", MODE_QUERY_SET, CMD_AT_CWMODE_DEF},
	{"+CWJAP", MODE_QUERY_SET, CMD_AT_CWJAP},
	{"+CWJAP_CUR", MODE_QUERY_SET, CMD_AT_CWJAP_CUR},
	{"+CWJAP_DEF", MODE_QUERY_SET, CMD_AT_CWJAP_DEF},
	{"+CWLAPOPT", MODE_QUERY_SET, CMD_AT_CWLAPOPT},
	{"+CWLAP", MODE_EXACT_MATCH, CMD_AT_CWLAP},
	{"+CWQAP", MODE_EXACT_MATCH, CMD_AT_CWQAP},
	{"+CWSAP", MODE_QUERY_SET, CMD_AT_CWSAP},
	{"+CWSAP_CUR", MODE_QUERY_SET, CMD_AT_CWSAP_CUR},
	{"+CWSAP_DEF", MODE_QUERY_SET, CMD_AT_CWSAP_DEF},
	{"+CWDHCP", MODE_QUERY_SET, CMD_AT_CWDHCP},
	{"+CWDHCP_CUR", MODE_QUERY_SET, CMD_AT_CWDHCP_CUR},
	{"+CWDHCP_DEF", MODE_QUERY_SET, CMD_AT_CWDHCP_DEF},
	{"+CWAUTOCONN", MODE_QUERY_SET, CMD_AT_CWAUTOCONN},
	{"+CIPSTAMAC", MODE_QUERY_SET, CMD_AT_CIPSTAMAC},
	{"+CIPSTAMAC_CUR", MODE_QUERY_SET, CMD_AT_CIPSTAMAC_CUR},
	{"+CIPSTAMAC_DEF", MODE_QUERY_SET, CMD_AT_CIPSTAMAC_DEF},
	{"+CIPAPMAC", MODE_QUERY_SET, CMD_AT_CIPAPMAC},
	{"+CIPAPMAC_CUR", MODE_QUERY_SET, CMD_AT_CIPAPMAC_CUR},
	{"+CIPAPMAC_DEF", MODE_QUERY_SET, CMD_AT_CIPAPMAC_DEF},
	{"+CIPSTA", MODE_QUERY_SET, CMD_AT_CIPSTA},
	{"+CIPSTA_CUR", MODE_QUERY_SET, CMD_AT_CIPSTA_CUR},
	{"+CIPSTA_DEF", MODE_QUERY_SET, CMD_AT_CIPSTA_DEF},
	{"+CIPAP", MODE_QUERY_SET, CMD_AT_CIPAP},
	{"+CIPAP_CUR", MODE_QUERY_SET, CMD_AT_CIPAP_CUR},
	{"+CIPAP_DEF", MODE_QUERY_SET, CMD_AT_CIPAP_DEF},
	{"+CWHOSTNAME", MODE_QUERY_SET, CMD_AT_CWHOSTNAME},

	{"+CIPSTATUS", MODE_EXACT_MATCH, CMD_AT_CIPSTATUS},
	{"+CIPSTART", MODE_NO_CHECKING, CMD_AT_CIPSTART},
	{"+CIPSSLSIZE", MODE_QUERY_SET, CMD_AT_CIPSSLSIZE},
	{"+CIPSEND", MODE_NO_CHECKING, CMD_AT_CIPSEND},
	{"+CIPCLOSEMODE", MODE_NO_CHECKING, CMD_AT_CIPCLOSEMODE},
	{"+CIPCLOSE", MODE_NO_CHECKING, CMD_AT_CIPCLOSE},
	{"+CIFSR", MODE_EXACT_MATCH, CMD_AT_CIFSR},
	{"+CIPMUX", MODE_QUERY_SET, CMD_AT_CIPMUX},
	{"+CIPDINFO", MODE_QUERY_SET, CMD_AT_CIPDINFO},
	{"+CIPSERVER", MODE_NO_CHECKING, CMD_AT_CIPSERVER},
	{"+CIPSERVERMAXCONN", MODE_QUERY_SET, CMD_AT_CIPSERVERMAXCONN},
	{"+CIPSTO", MODE_QUERY_SET, CMD_AT_CIPSTO},
	{"+CIPRECVMODE", MODE_QUERY_SET, CMD_AT_CIPRECVMODE},
	{"+CIPRECVDATA", MODE_QUERY_SET, CMD_AT_CIPRECVDATA},
	{"+CIPRECVLEN", MODE_QUERY_SET, CMD_AT_CIPRECVLEN},
	{"+CIPSNTPCFG", MODE_QUERY_SET, CMD_AT_CIPSNTPCFG},
	{"+CIPSNTPTIME?", MODE_EXACT_MATCH, CMD_AT_CIPSNTPTIME},
	{"+CIPDNS", MODE_QUERY_SET, CMD_AT_CIPDNS},
	{"+CIPDNS_CUR", MODE_QUERY_SET, CMD_AT_CIPDNS_CUR},
	{"+CIPDNS_DEF", MODE_QUERY_SET, CMD_AT_CIPDNS_DEF},

	{"+SYSCPUFREQ", MODE_QUERY_SET, CMD_AT_SYSCPUFREQ},
	{"+RFMODE", MODE_QUERY_SET, CMD_AT_RFMODE},
	{"+CIPSSLAUTH", MODE_QUERY_SET, CMD_AT_CIPSSLAUTH},
	{"+CIPSSLFP", MODE_QUERY_SET, CMD_AT_CIPSSLFP},
	{"+CIPSSLCERTMAX", MODE_QUERY_SET, CMD_AT_CIPSSLCERTMAX},
	{"+CIPSSLCERT", MODE_NO_CHECKING, CMD_AT_CIPSSLCERT},
	{"+CIPSSLMFLN", MODE_QUERY_SET, CMD_AT_CIPSSLMFLN},
	{"+CIPSSLSTA", MODE_NO_CHECKING, CMD_AT_CIPSSLSTA},
	{"+SNTPTIME?", MODE_EXACT_MATCH, CMD_AT_SNTPTIME}};

/*
 * Static functions
 */

commands_t findCommand(uint8_t *input, uint16_t inpLen);
String readStringFromBuffer(unsigned char *inpBuf, uint16_t &offset, bool escape, bool allowEmpty = false);
bool readNumber(unsigned char *inpBuf, uint16_t &offset, uint32_t &output);
bool readIpAddress(unsigned char *inpBuf, uint16_t &offset, uint32_t &output);
uint8_t readHex(char c);
void printCertificateName(uint8_t certNumber);
int compWifiRssi(const void *elem1, const void *elem2);
void printCWLAP(int networksFound);
void printScanResult(int networksFound);

/*
 * Variables
 */
uint32_t sort_enable = 0;
uint32_t printMask = 0x7FF;
int rssiFilter = -100;
uint32_t authmodeMask = 0xFFFF;

/*
 * Commands
 */

static void cmd_AT();
static void cmd_AT_RST();
static void cmd_AT_GMR();
static void cmd_ATE();
static void cmd_AT_RESTORE();
static void cmd_AT_UART(commands_t cmd);
static void cmd_AT_SYSRAM();

static void cmd_AT_CWMODE(commands_t cmd);
static void cmd_AT_CWJAP(commands_t cmd);
static void cmd_AT_CWLAPOPT();
static void cmd_AT_CWLAP();
static void cmd_AT_CWQAP();
static void cmd_AT_CWSAP(commands_t cmd);
static void cmd_AT_CWDHCP(commands_t cmd);
static void cmd_AT_CWAUTOCONN();
static void cmd_AT_CIPXXMAC(commands_t cmd);
static void cmd_AT_CIPSTA(commands_t cmd);
static void cmd_AT_CIPAP(commands_t cmd);
static void cmd_AT_CWHOSTNAME();

static void cmd_AT_CIPSTATUS();
static void cmd_AT_CIPSTART();
static void cmd_AT_CIPSSLSIZE();
static void cmd_AT_CIPSEND();
static void cmd_AT_CIPCLOSEMODE();
static void cmd_AT_CIPCLOSE();
static void cmd_AT_CIFSR();
static void cmd_AT_CIPMUX();
static void cmd_AT_CIPSERVER();
static void cmd_AT_CIPSERVERMAXCONN();
static void cmd_AT_CIPSTO();
static void cmd_AT_CIPDINFO();
static void cmd_AT_CIPRECVMODE();
static void cmd_AT_CIPRECVDATA();
static void cmd_AT_CIPRECVLEN();
static void cmd_AT_CIPSNTPCFG();
static void cmd_AT_CIPSNTPTIME();
static void cmd_AT_CIPDNS(commands_t cmd);

static void cmd_AT_SYSCPUFREQ();
static void cmd_AT_RFMODE();
static void cmd_AT_CIPSSLAUTH();
static void cmd_AT_CIPSSLFP();
static void cmd_AT_CIPSSLCERTMAX();
static void cmd_AT_CIPSSLCERT();
static void cmd_AT_CIPSSLMFLN();
static void cmd_AT_CIPSSLSTA();
static void cmd_AT_SNTPTIME();

/*
 * Processes the command buffer
 */
void processCommandBuffer(void)
{
	commands_t cmd = findCommand(inputBuffer, inputBufferCnt);

	// ------------------------------------------------------------------------------------ AT
	if (cmd == CMD_AT)
		cmd_AT();

	// ------------------------------------------------------------------------------------ AT+RST
	else if (cmd == CMD_AT_RST) // AT+RST - soft reset
		cmd_AT_RST();

	// ------------------------------------------------------------------------------------ AT+GMR
	else if (cmd == CMD_AT_GMR) // AT+GMR - firmware version
		cmd_AT_GMR();

	// ------------------------------------------------------------------------------------ ATE
	else if (cmd == CMD_ATE) // ATE0, ATE1 - echo enabled / disabled
		cmd_ATE();

	// ------------------------------------------------------------------------------------ AT+RESTORE
	else if (cmd == CMD_AT_RESTORE) // AT+RESTORE - Restores the Factory Default Settings
		cmd_AT_RESTORE();

	// ------------------------------------------------------------------------------------ AT+UART
	else if (cmd == CMD_AT_UART || cmd == CMD_AT_UART_CUR || cmd == CMD_AT_UART_DEF)
		// AT+UART=baudrate,databits,stopbits,parity,flow - UART Configuration
		cmd_AT_UART(cmd);

	// ------------------------------------------------------------------------------------ AT+SYSRAM
	else if (cmd == CMD_AT_SYSRAM) // AT+SYSRAM? - Checks the Remaining Space of RAM
		cmd_AT_SYSRAM();

	// ------------------------------------------------------------------------------------ AT+CWMODE
	else if (cmd == CMD_AT_CWMODE || cmd == CMD_AT_CWMODE_CUR || cmd == CMD_AT_CWMODE_DEF)
		// AT+CWMODE - Sets the Current Wi-Fi mode
		cmd_AT_CWMODE(cmd);

	// ------------------------------------------------------------------------------------ AT+CWJAP
	else if (cmd == CMD_AT_CWJAP || cmd == CMD_AT_CWJAP_CUR || cmd == CMD_AT_CWJAP_DEF)
		// AT+CWJAP="ssid","pwd" [,"bssid"] - Connects to an AP, only ssid, pwd and bssid supported
		cmd_AT_CWJAP(cmd);

	// ------------------------------------------------------------------------------------ AT+CWLAPOPT
	else if (cmd == CMD_AT_CWLAPOPT) // AT+CWLAPOPT - Set the configuration for the command AT+CWLAP.
		cmd_AT_CWLAPOPT();

	// ------------------------------------------------------------------------------------ AT+CWLAP
	else if (cmd == CMD_AT_CWLAP) // AT+CWLAP - List available APs.
		cmd_AT_CWLAP();

	// ------------------------------------------------------------------------------------ AT+CWQAP
	else if (cmd == CMD_AT_CWQAP) // AT+CWQAP - Disconnects from the AP
		cmd_AT_CWQAP();

	// ------------------------------------------------------------------------------------ AT+CWSAP
	else if (cmd == CMD_AT_CWSAP || cmd == CMD_AT_CWSAP_CUR || cmd == CMD_AT_CWSAP_DEF)
		// AT+CWSAP="ssid","pwd",chl,ecn [,max conm, ssid hidden] - Function: to configure the SoftA
		cmd_AT_CWSAP(cmd);

	// ------------------------------------------------------------------------------------ AT+CWDHCP
	else if (cmd == CMD_AT_CWDHCP || cmd == CMD_AT_CWDHCP_CUR || cmd == CMD_AT_CWDHCP_DEF)
		// AT+CWDHCP=x,y - Enables/Disables DHCP
		cmd_AT_CWDHCP(cmd);

	// ------------------------------------------------------------------------------------ AT+CWAUTOCONN
	else if (cmd == CMD_AT_CWAUTOCONN) // AT+CWAUTOCONN - auto connect to AP
		cmd_AT_CWAUTOCONN();

	// ------------------------------------------------------------------------------------ AT+CIPSTAMAC
	else if (cmd == CMD_AT_CIPSTAMAC || cmd == CMD_AT_CIPSTAMAC_CUR || cmd == CMD_AT_CIPSTAMAC_DEF)
		// AT+CIPSTAMAC - Sets or prints the MAC Address of the ESP8266 Station
		cmd_AT_CIPXXMAC(cmd);

	// ------------------------------------------------------------------------------------ AT+CIPAPMAC
	else if (cmd == CMD_AT_CIPAPMAC || cmd == CMD_AT_CIPAPMAC_CUR || cmd == CMD_AT_CIPAPMAC_DEF)
		// AT+CIPAPMAC - Sets or prints the MAC Address of the ESP8266 SoftAP
		cmd_AT_CIPXXMAC(cmd);

	// ------------------------------------------------------------------------------------ AT+CIPSTA
	else if (cmd == CMD_AT_CIPSTA || cmd == CMD_AT_CIPSTA_CUR || cmd == CMD_AT_CIPSTA_DEF)
		// AT+CIPSTA - Sets or prints the network configuration
		cmd_AT_CIPSTA(cmd);

	// ------------------------------------------------------------------------------------ AT+CIPAP
	else if (cmd == CMD_AT_CIPAP || cmd == CMD_AT_CIPAP_CUR || cmd == CMD_AT_CIPAP_DEF)
		// AT+CIPAP - Sets or prints the SoftAP configuration
		cmd_AT_CIPAP(cmd);

	// ------------------------------------------------------------------------------------ AT+CWHOSTNAME
	else if (cmd == CMD_AT_CWHOSTNAME)
		// AT+CWHOSTNAME - Query/Set the host name of an ESP station
		cmd_AT_CWHOSTNAME();

	// ------------------------------------------------------------------------------------ AT+CIPSTATUS
	else if (cmd == CMD_AT_CIPSTATUS) // AT+CIPSTATUS - Gets the Connection Status
		cmd_AT_CIPSTATUS();

	// ------------------------------------------------------------------------------------ AT+CIPSTART
	else if (cmd == CMD_AT_CIPSTART)
		// AT+CIPSTART - Establishes TCP Connection, UDP Transmission or SSL Connection
		cmd_AT_CIPSTART();

	// ------------------------------------------------------------------------------------ AT+CIPSSLSIZE
	else if (cmd == CMD_AT_CIPSSLSIZE)
		// AT+CIPSSLSIZE - Sets the Size of SSL Buffer - the command is parsed but ignored
		cmd_AT_CIPSSLSIZE();

	// ------------------------------------------------------------------------------------ AT+CIPSEND
	else if (cmd == CMD_AT_CIPSEND) // AT+CIPSEND - Sends Data
		cmd_AT_CIPSEND();

	// ------------------------------------------------------------------------------------ AT+CIPCLOSE
	else if (cmd == CMD_AT_CIPCLOSEMODE) // AT+CIPCLOSEMODE - Defines the closing mode - parsed but ignored for now
		cmd_AT_CIPCLOSEMODE();

	// ------------------------------------------------------------------------------------ AT+CIPCLOSE
	else if (cmd == CMD_AT_CIPCLOSE) // AT+CIPCLOSE - Closes the TCP/UDP/SSL Connection
		cmd_AT_CIPCLOSE();

	// ------------------------------------------------------------------------------------ AT+CIFSR
	else if (cmd == CMD_AT_CIFSR) // AT+CIFSR - Gets the Local IP Address
		cmd_AT_CIFSR();

	// ------------------------------------------------------------------------------------ AT+CIPMUX
	else if (cmd == CMD_AT_CIPMUX) // AT+CIPMUX - Enable or Disable Multiple Connections
		cmd_AT_CIPMUX();

	// ------------------------------------------------------------------------------------ AT+CIPDINFO
	else if (cmd == CMD_AT_CIPDINFO) // AT+CIPDINFO - Shows the Remote IP and Port with +IPD
		cmd_AT_CIPDINFO();

	// ------------------------------------------------------------------------------------ AT+CIPSERVER
	else if (cmd == CMD_AT_CIPSERVER) // AT+CIPCIPSERVER - Deletes/Creates TCP Server
		cmd_AT_CIPSERVER();

	// ------------------------------------------------------------------------------------ AT+CIPSERVERMAXCONN
	else if (cmd == CMD_AT_CIPSERVERMAXCONN) // AT+CIPSERVERMAXCONN - Set the Maximum Connections Allowed by Server
		cmd_AT_CIPSERVERMAXCONN();

	// ------------------------------------------------------------------------------------ AT+CIPSTO
	else if (cmd == CMD_AT_CIPSTO) // AT+CIPSTO - Sets the TCP Server Timeout
		cmd_AT_CIPSTO();

	// ------------------------------------------------------------------------------------ AT+CIPRECVMODE
	else if (cmd == CMD_AT_CIPRECVMODE) // AT+CIPRECVMODE - Set TCP Receive Mode
		cmd_AT_CIPRECVMODE();

	// ------------------------------------------------------------------------------------ AT+CIPRECVDATA
	else if (cmd == CMD_AT_CIPRECVDATA) // AT+CIPRECVDATA - Get TCP Data in Passive Receive Mode
		cmd_AT_CIPRECVDATA();

	// ------------------------------------------------------------------------------------ AT+CIPRECVLEN
	else if (cmd == CMD_AT_CIPRECVLEN) // AT+CIPRECVLEN - Get TCP Data Length in Passive Receive Mode
		cmd_AT_CIPRECVLEN();

	// ------------------------------------------------------------------------------------ AT+CIPSNTPCFG
	else if (cmd == CMD_AT_CIPSNTPCFG) // AT+CIPSNTPCFG - configure SNTP time
		cmd_AT_CIPSNTPCFG();

	// ------------------------------------------------------------------------------------ AT+CIPSNTPTIME?
	else if (cmd == CMD_AT_CIPSNTPTIME) // AT+CIPSNTPTIME? - get time in asctime format
		cmd_AT_CIPSNTPTIME();

	// ------------------------------------------------------------------------------------ AT+CIPDNS
	else if (cmd == CMD_AT_CIPDNS || cmd == CMD_AT_CIPDNS_CUR || cmd == CMD_AT_CIPDNS_DEF)
		// AT+CIPDNS - Sets User-defined DNS Servers
		cmd_AT_CIPDNS(cmd);

	// ------------------------------------------------------------------------------------ AT+SYSCPUFREQ
	else if (cmd == CMD_AT_SYSCPUFREQ) // AT+SYSCPUFREQ - Set or Get the Current CPU Frequency
		cmd_AT_SYSCPUFREQ();

	// ------------------------------------------------------------------------------------ AT+RFMODE
	else if (cmd == CMD_AT_RFMODE) // AT+RFMODE - Sets or queries current RF mode (custom command)
		cmd_AT_RFMODE();

	// ------------------------------------------------------------------------------------ AT+CIPSSLAUTH
	else if (cmd == CMD_AT_CIPSSLAUTH) // AT+CIPSSLAUTH - Authentication type
		cmd_AT_CIPSSLAUTH();

	// ------------------------------------------------------------------------------------ AT+CIPSSLFP
	else if (cmd == CMD_AT_CIPSSLFP) // AT+CIPSSLFP - Shows or stores certificate fingerprint
		cmd_AT_CIPSSLFP();

	// ------------------------------------------------------------------------------------ AT+CIPSSLCERTMAX
	else if (cmd == CMD_AT_CIPSSLCERTMAX) // AT+CIPSSLCERTMAX - Get or set the maximum certificate amount
		cmd_AT_CIPSSLCERTMAX();

	// ------------------------------------------------------------------------------------ AT+CIPSSLCERT
	else if (cmd == CMD_AT_CIPSSLCERT) // AT+CIPSSLCERT - Load CA certificate in PEM format
		cmd_AT_CIPSSLCERT();

	// ------------------------------------------------------------------------------------ AT+CIPSSLMFLN
	else if (cmd == CMD_AT_CIPSSLMFLN) // AT+CIPSSLMFLN - Check the capability of MFLN for a site
		cmd_AT_CIPSSLMFLN();

	// ------------------------------------------------------------------------------------ AT+CIPSSLSTA
	else if (cmd == CMD_AT_CIPSSLSTA) // AT+CIPSSLSTA - Check the MFLN status for a connection
		cmd_AT_CIPSSLSTA();

	// ------------------------------------------------------------------------------------ AT+SNTPTIME?
	else if (cmd == CMD_AT_SNTPTIME) // AT+SNTPTIME? - get time
		cmd_AT_SNTPTIME();

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
 * AT+RST - soft reset
 */
void cmd_AT_RST()
{
	Serial.printf_P(MSG_OK);
	Serial.flush();

	ESP.reset();
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

	/* The Arduino code version is based on file core_version.h
	 * This file is by default unusable (version number 0) but can be (and maybe is) populated
	 * while installing the git version (file build_boards_manager_package.sh)
	 *
	 * If the file has the default contents, you can recreate it by git command. The idea is as follows,
	 * for windows you will have to create the file in a text editor and insert the git results manually:
	 *
	 * echo #define ARDUINO_ESP8266_GIT_VER 0x`git rev-parse --short=8 HEAD` > core_version.h
	 * echo #define ARDUINO_ESP8266_GIT_VER `git describe --tags` >> core_version.h
	 */
#if (ARDUINO_ESP8266_GIT_VER != 0)
	{
		Serial.printf_P(PSTR("Arduino core version:%s\r\n"), __STR(ARDUINO_ESP8266_GIT_DESC));
	}
#endif

	Serial.println(F("OK"));
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
 * AT+UART=baudrate,databits,stopbits,parity,flow - UART Configuration
 */
void cmd_AT_UART(commands_t cmd)
{
	uint16_t offset = 7;
	if (cmd == CMD_AT_UART_CUR || cmd == CMD_AT_UART_DEF)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
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

			uartConfig = (SerialConfig)(((dataBits - 5) << UCBN) | (stopBits << UCSBN) | parity);

			AT_DEBUG_PRINTF("--- %d,%02x\r\n", baudRate, uartConfig);

			error = 0;

			// Last message at the original speed
			Serial.printf_P(MSG_OK);

			// Restart the serial interface

			Serial.flush();
			Serial.end();
			Serial.begin(baudRate, uartConfig);
			delay(250); // To let the line settle

			if (cmd != CMD_AT_UART_CUR)
			{
				Settings::setUartBaudRate(baudRate);
				Settings::setUartConfig(uartConfig);
			}

		} while (0);

		if (error == 1)
			Serial.printf_P(MSG_ERROR);
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+SYSRAM? - Checks the Remaining Space of RAM
 */
void cmd_AT_SYSRAM()
{
	Serial.printf_P(PSTR("+SYSRAM:%d\r\nOK\r\n"), ESP.getFreeHeap());
}

/*
 * AT+CWMODE - Sets the Current Wi-Fi mode (only mode 1 implemented)
 */
void cmd_AT_CWMODE(commands_t cmd)
{
	uint16_t offset = 9; // offset to ? or =
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
			if (cmd != CMD_AT_CWMODE_CUR)
				WiFi.persistent(true);

			if (WiFi.mode((WiFiMode) mode))
				Serial.printf_P(MSG_OK);
			else
				Serial.printf_P(MSG_ERROR);

			WiFi.persistent(false);

			if (mode != WIFI_AP)
			{
				setDns();
				setDhcpMode();
			}
			if (mode != WIFI_STA)
			{
				applyCipAp();
			}
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
 * AT+CWJAP="ssid","pwd" [,"bssid"] - Connects to an AP, only ssid, pwd and bssid supported
 */
void cmd_AT_CWJAP(commands_t cmd)
{
	if (WiFi.getMode() == WIFI_AP)
	{
		Serial.printf_P(MSG_ERROR);
		return;
	}

	uint16_t offset = 8; // offset to ? or =
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
			ssid[32] = 0; // Nullterm in case of 32 char ssid

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

		do
		{
			String ssid;
			String pwd;
			String bssid;
			uint8_t uBssid[6];

			++offset;

			ssid = readStringFromBuffer(inputBuffer, offset, true);
			if (ssid.isEmpty() || (inputBuffer[offset] != ','))
				break;

			++offset;

			pwd = readStringFromBuffer(inputBuffer, offset, true, true);

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
					break; // Still connected
			}

			uint8_t *pBssid = nullptr;
			if (!bssid.isEmpty())
				pBssid = uBssid;

			WiFi.begin(ssid, pwd, 0, pBssid);

			WiFi.persistent(false);

			gsFlag_Connecting = true;
			gsFlag_Busy = true;

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
 * AT+CWLAPOPT - Set the configuration for the command AT+CWLAP.
 */
void cmd_AT_CWLAPOPT()
{
	bool error = false;

	if (inputBuffer[11] == '=')
	{
		uint16_t offset = 12;

		if (!readNumber(inputBuffer, offset, sort_enable))
		{
			error = true;
		}

		offset++;

		if (!readNumber(inputBuffer, offset, printMask))
		{
			error = true;
		}

		offset++;

		int signNumber = 1;
		if (inputBuffer[offset] == '-')
		{
			signNumber = -1;
			offset++;
		}

		uint32_t readRssiFilter;
		if (readNumber(inputBuffer, offset, readRssiFilter))
		{
			rssiFilter = readRssiFilter * signNumber;
		}

		offset++;

		readNumber(inputBuffer, offset, authmodeMask);

		if (error)
		{
			Serial.printf_P(MSG_ERROR);
		}
		else
		{
			Serial.printf_P(MSG_OK);
		}
	}
	else
	{
		Serial.printf_P(MSG_ERROR);
	}
}

/*
 * AT+CWLAP - List available APs.
 */
void cmd_AT_CWLAP()
{
	if (WiFi.getMode() == WIFI_AP)
	{
		Serial.printf_P(MSG_ERROR);
		return;
	}

	// Print found networks with printScanResult once the scan has finished
	WiFi.scanNetworksAsync(printScanResult);
	gsFlag_Busy = true;
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
 * AT+CWSAP="ssid","pwd",chl,ecn [,max conm, ssid hidden] - Function: to configure the SoftAP
 */
void cmd_AT_CWSAP(commands_t cmd)
{
	if (WiFi.getMode() == WIFI_STA)
	{
		Serial.printf_P(MSG_ERROR);
		return;
	}

	uint16_t offset = 8; // offset to ? or =
	if (cmd != CMD_AT_CWSAP)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{

		struct softap_config conf;

		if (cmd != CMD_AT_CWSAP_CUR)
			wifi_softap_get_config_default(&conf);
		else
			wifi_softap_get_config(&conf);

		char ssid[33];

		memcpy(ssid, conf.ssid, conf.ssid_len);
		ssid[conf.ssid_len] = 0; // Nullterm in case of 32 char ssid

		const char *cmdSuffix = "";
		if (cmd == CMD_AT_CWSAP_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_CWSAP_DEF)
			cmdSuffix = suffix_DEF;

		Serial.printf_P(PSTR("+CWSAP%s:"), cmdSuffix);

		// +CWSAP_CUR:<ssid>,<pwd>,<chl>,<ecn>,<max conn>,<ssid hidden>
		Serial.printf_P(PSTR("\"%s\",\"%s\",%d,%d,%d,%d\r\n"), ssid, conf.password,
				conf.channel, conf.authmode, conf.max_connection, conf.ssid_hidden);

		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		bool error = true;

		do
		{
			String ssid;
			String pwd;
			uint32_t channel;
			uint32_t enc;
			uint32_t max_conn = 4;
			uint32_t ssid_hidden = 0;

			++offset;

			ssid = readStringFromBuffer(inputBuffer, offset, true);
			if (ssid.isEmpty() || (inputBuffer[offset] != ','))
				break;

			++offset;

			pwd = readStringFromBuffer(inputBuffer, offset, true, true);

			++offset;

			if (!(readNumber(inputBuffer, offset, channel) && channel <= 14 && inputBuffer[offset] == ','))
				break;

			++offset;

			if (!(readNumber(inputBuffer, offset, enc) && enc < AUTH_MAX && enc != AUTH_WEP)) // WEP is not supported
				break;

			if (inputBuffer[offset] == ',')
			{
				++offset;

				if (!(readNumber(inputBuffer, offset, max_conn) && max_conn <= 4))
					break;

				if (inputBuffer[offset] == ',')
				{
					++offset;

					if (!(readNumber(inputBuffer, offset, ssid_hidden) && ssid_hidden <= 1))
						break;
				}
			}

			if (inputBufferCnt != offset + 2)
				break;

			if (cmd != CMD_AT_CWSAP_CUR)
				WiFi.persistent(true);

			error = !WiFi.softAP(ssid.c_str(), nullIfEmpty(pwd), channel, ssid_hidden, max_conn);

			// enc is not used. the ESP8266WiFi library sets WPA_WPA2_PSK
			// if password is entered and OPEN if password is empty

			WiFi.persistent(false);

		} while (0);

		if (error == 0)
		{
			Serial.printf_P(MSG_OK);
		}
		else if (error == 1)
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

	uint16_t offset = 9; // offset to ? or =
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

			const WiFiMode_t dhcpToMode[3] = {WIFI_AP, WIFI_STA, WIFI_AP_STA};

			if (dhcpToMode[mode] == WiFi.getMode()) // The mode must match the current mode
			{
				if (readNumber(inputBuffer, offset, en) && en <= 1 && inputBufferCnt == offset + 2)
				{
					gsCwDhcp = 1 | en << 1; // Only Station DHCP is supported

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
 * AT+CIPSTAMAC & AT+CIPSAPMAC - Sets or prints a MAC Address
 */
void cmd_AT_CIPXXMAC(commands_t cmd)
{
	uint8_t iface = STATION_IF;
	if (cmd == CMD_AT_CIPAPMAC || cmd == CMD_AT_CIPAPMAC_CUR
			|| cmd == CMD_AT_CIPAPMAC_DEF)
		iface = SOFTAP_IF;
	uint16_t offset = (iface == STATION_IF) ? 12 : 11;
	if (cmd != CMD_AT_CIPSTAMAC && cmd != CMD_AT_CIPAPMAC)
		offset += 4;
	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		String mac = (iface == STATION_IF) ? WiFi.macAddress() : WiFi.softAPmacAddress();
		const char *cmdSuffix = "";
		if (cmd == CMD_AT_CIPSTAMAC_CUR || cmd == CMD_AT_CIPAPMAC_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_CIPSTAMAC_DEF || cmd == CMD_AT_CIPAPMAC_DEF)
			cmdSuffix = suffix_DEF;
		Serial.printf_P(PSTR("+CIP%sMAC%s:\"%s\"\r\n"),
				(iface == STATION_IF) ? "STA" : "AP", cmdSuffix, mac.c_str());
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		uint8_t error = true;

		++offset;

		do
		{
			String mac;
			uint8_t uMac[6];

			mac = readStringFromBuffer(inputBuffer, offset, false);

			if (mac.isEmpty() || mac.length() != 17)
				break;

			char fmt[40];
			strcpy_P(fmt, PSTR("%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx"));
			if (sscanf(mac.c_str(), fmt, &uMac[0], &uMac[1], &uMac[2], &uMac[3],
					&uMac[4], &uMac[5]) != 6)
				break;

			if (inputBufferCnt != offset + 2)
				break;

			Serial.println(F("NOT IMPLEMENTED"));

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
 * AT+CIPSTA - Sets or prints the network configuration
 */
void cmd_AT_CIPSTA(commands_t cmd)
{
	if (WiFi.getMode() == WIFI_AP)
	{
		Serial.printf_P(MSG_ERROR);
		return;
	}

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
			cfg = {WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask()};
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
			else // read gateway and mask
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
				Settings::setDhcpMode(1); // Stop DHCP
			}

			gsCipStaCfg = cfg;
			gsCwDhcp = 1; // Stop DHCP

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
 * AT+CIPAP - Sets or prints the SoftAP configuration
 */
void cmd_AT_CIPAP(commands_t cmd)
{
	if (WiFi.getMode() == WIFI_STA)
	{
		Serial.printf_P(MSG_ERROR);
		return;
	}

	uint16_t offset = 8;

	if (cmd != CMD_AT_CIPAP)
		offset += 4;

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		ipConfig_t cfg;

		if (cmd == CMD_AT_CIPAP_DEF)
		{
			cfg = Settings::getApIpConfig();
		}
		else
		{
			ip_info info;
			wifi_get_ip_info(SOFTAP_IF, &info);
			cfg = {info.ip.addr, info.gw.addr, info.netmask.addr};
		}

		const char *cmdSuffix = "";
		if (cmd == CMD_AT_CIPAP_CUR)
			cmdSuffix = suffix_CUR;
		else if (cmd == CMD_AT_CIPAP_DEF)
			cmdSuffix = suffix_DEF;

		if (WiFi.getMode() == WIFI_STA || cfg.ip == 0)
		{
			Serial.printf_P(PSTR("+CIPSTA%s:ip:\"0.0.0.0\"\r\n"), cmdSuffix);
			Serial.printf_P(PSTR("+CIPSTA%s:gateway:\"0.0.0.0\"\r\n"), cmdSuffix);
			Serial.printf_P(PSTR("+CIPSTA%s:netmask:\"0.0.0.0\"\r\n"), cmdSuffix);
		}
		else
		{
			Serial.printf_P(PSTR("+CIPAP%s:ip:\"%s\"\r\n"), cmdSuffix, IPAddress(cfg.ip).toString().c_str());
			Serial.printf_P(PSTR("+CIPAP%s:gateway:\"%s\"\r\n"), cmdSuffix, IPAddress(cfg.gw).toString().c_str());
			Serial.printf_P(PSTR("+CIPAP%s:netmask:\"%s\"\r\n"), cmdSuffix, IPAddress(cfg.mask).toString().c_str());
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
			else // read gateway and mask
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

			gsCipApCfg = cfg;

			if (cmd != CMD_AT_CIPAP_CUR)
			{
				// Save the network configuration
				Settings::setApIpConfig(cfg);
			}

			applyCipAp();

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
 * AT+CWHOSTNAME - Query/Set the host name of an ESP station
 */
void cmd_AT_CWHOSTNAME()
{
	uint16_t offset = 13; // offset to ? or =

	if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{

		// query is enabled in AP mode in standard AT firmware

		Serial.printf_P(PSTR("+CWHOSTNAME:%s\r\n"), WiFi.hostname().c_str());
		Serial.printf_P(MSG_OK);
	}
	else if (inputBuffer[offset] == '=')
	{
		if (WiFi.getMode() == WIFI_AP)
		{
			Serial.printf_P(MSG_ERROR);
			return;
		}

		String hostname = readStringFromBuffer(inputBuffer, ++offset, false);

		if (hostname.isEmpty())
		{
			Serial.printf_P(MSG_ERROR);
			return;
		}

		WiFi.hostname(hostname);
		if (WiFi.hostname() == hostname)
			Serial.printf_P(MSG_OK);
		else
			Serial.printf_P(MSG_ERROR);
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
	/* Early AT firmware versions could do only STA and one TCP connection, but now with SoftAP
	 * and multiple connections support the STA status and the list of TCP connections are two
	 * independent informations. It is not possible to know which of the TCP connections use STA
	 * so the STA statuses 3 and 4 can't be evaluated for STA only.
	 */

	wl_status_t status = WiFi.status();
	bool statusPrinted = false;

	if (status != WL_CONNECTED)
	{
		Serial.println(F("STATUS:5"));
		statusPrinted = true;
	}
//	else  <-- no else. we have to list SoftAP TCP connections too
	{
		uint8_t maxCli = 0; // Maximum client number
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

				const char types_text[3][4] = {"TCP", "UDP", "SSL"};
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
	}
	Serial.printf_P(MSG_OK);
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
	uint8_t error = 1; // 1 = generic error, 0 = ok
	uint16_t offset = 11;

	// Parse the input

	uint8_t linkID = 0;
	clientTypes_t type = TYPE_NONE; // 0 = TCP, 1 = UDP, 2 = SSL
	char *remoteAddr;
	uint32_t remotePort = 0;

	do
	{
		if (inputBuffer[offset] != '=')
			break;

		++offset;
		error = 2;

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

		offset += 3;
		error = 4;

		if (inputBuffer[offset] != '"' || inputBuffer[offset + 1] != ',' || inputBuffer[offset + 2] != '"')
			break;

		offset += 3;

		// Read remote address

		remoteAddr = (char*)&inputBuffer[offset];  // the address is a pointer to the input string

		while (inputBuffer[offset] != '"' && inputBuffer[offset] > ' ')
		{
			offset++;
		}

		if (inputBuffer[offset] != '"' || inputBuffer[offset + 1] != ',')
			break;

		inputBuffer[offset] = 0;  // End of remoteAddress
		offset += 2;

		// Read remote port

		error = 100; // Unspecified error

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

			// Check if connected to an AP or SoftAP is started
			if (!(WiFi.isConnected() || (WiFi.getMode() & WIFI_AP)))
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

			if (type == 0) // TCP
			{
				cli = new WiFiClient();
			}
			else if (type == 2) // SSL
			{
				cli = new BearSSL::WiFiClientSecure();

				if (gsCipSslSize != 16384)
					static_cast<BearSSL::WiFiClientSecure *>(cli)->setBufferSizes(gsCipSslSize, 512);

				if (gsCipSslAuth == 0)
				{
					static_cast<BearSSL::WiFiClientSecure *>(cli)->setInsecure();
				}
				else if (gsCipSslAuth == 1 && fingerprintValid)
				{
					static_cast<BearSSL::WiFiClientSecure *>(cli)->setFingerprint(fingerprint);
				}
				else if (gsCipSslAuth == 2 && CAcert->getCount() > 0) // certificate chain verification
				{
					static_cast<BearSSL::WiFiClientSecure *>(cli)->setTrustAnchors(CAcert);
				}
				else
				{
					delete cli;
					break; // error
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

			gsWasConnected = true; // Flag for CIPSTATUS command

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
 * AT+CIPSSLSIZE - Sets the Size of SSL Buffer - only sizes 512, 1024, 2048 and 4096 are supported
 */
void cmd_AT_CIPSSLSIZE()
{
	uint16_t offset = 13;

	if (inputBuffer[offset] == '=')
	{
		unsigned int sslSize = 0;

		++offset;

		if (readNumber(inputBuffer, offset, sslSize) && inputBufferCnt == offset + 2 && (sslSize == 512 || sslSize == 1024 || sslSize == 2048 || sslSize == 4096 || sslSize == 16384))
		{
			if (sslSize == 16384)
				sslSize = 0; // default value

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
		Serial.print(F("OK\r\n> "));
}

/*
 * AT+CIPCLOSEMODE - Defines the closing mode of the connection.
 * Parsed but ignored for now.
 */
void cmd_AT_CIPCLOSEMODE()
{
	uint8_t error = 1;

	do
	{
		uint16_t offset = 16;
		uint32_t inputVal = 0;
		//		uint32_t linkId = 0;

		// Read the input

		if (inputBuffer[15] != '=')
			break;

		if (!readNumber(inputBuffer, offset, inputVal) || inputVal > 5)
			break;

		if (gsCipMux == 0)
		{
			// Check the <enable_abort> value and end of line
			if (inputVal > 1 || inputBufferCnt != offset + 2)
				break;
		}
		else
		{
			// Read the second number - <enable_abort> value
			if (inputBuffer[offset] != ',')
				break;

			++offset;
			//			linkId = inputVal;
			if (!readNumber(inputBuffer, offset, inputVal) || inputVal > 1 || inputBufferCnt != offset + 2)
				break;
		}

		// Check the client
		//		WiFiClient *cli = clients[linkId].client;

		//		if (cli != nullptr)  // Success only for an existing client
		error = 0;

	} while (0);

	if (error > 0)
		Serial.printf_P(MSG_ERROR);
	else
		Serial.printf_P(MSG_OK);
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
 * AT+CIFSR - Gets the Local IP Address
 */
void cmd_AT_CIFSR()
{
	IPAddress ip = WiFi.localIP();
	if (!ip.isSet())
		Serial.println(F("+CISFR:STAIP,\"0.0.0.0\""));
	else
		Serial.printf_P(PSTR("+CISFR:STAIP,\"%s\"\r\n"), ip.toString().c_str());

	Serial.printf_P(PSTR("+CIFSR:STAMAC,\"%s\"\r\n"), WiFi.macAddress().c_str());
	Serial.printf_P(MSG_OK);
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
			for (uint8_t i = 0; i < SERVERS_COUNT; ++i)
			{
				if (servers[i].status() != CLOSED)
				{
					Serial.println(F("CIPSERVER must be 0"));
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
 * AT+CIPCIPSERVER - Deletes/Creates TCP Server
 */
void cmd_AT_CIPSERVER()
{

	if (!gsCipMux)
	{
		Serial.printf_P(MSG_ERROR);
		return;
	}
	uint8_t error = 1; // 1 = generic error, 0 = ok
	uint16_t offset = strlen("AT+CIPSERVER");

	bool stop = false;
	uint32_t port = 0;

	do
	{
		if (inputBuffer[offset] != '=')
			break;

		++offset;
		if (inputBuffer[offset] != '0' && inputBuffer[offset] != '1')
			break;
		stop = inputBuffer[offset] == '0';
		++offset;

		if (inputBufferCnt > offset + 2)
		{
			if (inputBuffer[offset] != ',')
				break;

			++offset;
			error = 2;

			if (!readNumber(inputBuffer, offset, port) || port > 65535 || inputBufferCnt != offset + 2)
				break;

		}
		else if (!stop)
		{
			port = 333; // default AT fw server port
		}

		error = 0;
	} while (0);

	if (!error)
	{
		if (stop)
		{
			error = 3; // not found running
			for (int i = 0; i < SERVERS_COUNT; i++)
			{
				if (servers[i].status() == CLOSED)
					continue;
				if (servers[i].port() == port || port == 0)
				{
					servers[i].close();
					error = 0;
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < SERVERS_COUNT; i++)
			{
				if (servers[i].status() == CLOSED)
					continue;
				if (servers[i].port() == port)
				{
					error = 4; // already running
					break;
				}
			}
			if (!error)
			{
				for (int i = 0; i < SERVERS_COUNT; i++)
				{
					if (servers[i].status() == CLOSED)
					{
						servers[i].begin(port);
						if (servers[i].status() == CLOSED)
						{
							error = 5;
						}
						break;
					}
				}
			}
		}
	}
	if (error == 3 || error == 4)
	{
		Serial.println("no change");
	}
	Serial.printf_P(error ? MSG_ERROR : MSG_OK);
}

/*
 * AT+CIPSERVERMAXCONN - Set the Maximum Connections Allowed by Server
 */
void cmd_AT_CIPSERVERMAXCONN()
{
	uint16_t offset = strlen("AT+CIPSERVERMAXCONN");

	if (inputBuffer[offset] == '?')
	{
		Serial.printf_P(PSTR("+CIPSERVERMAXCONN:%d\r\n"), gsServersMaxConn);
		Serial.printf_P(MSG_OK);
		return;
	}

	uint8_t error = 1; // 1 = generic error, 0 = ok
	uint32_t max = 0;
	do
	{
		if (inputBuffer[offset] != '=')
			break;
		++offset;
		if (!readNumber(inputBuffer, offset, max) || max < 1 || max > 5 || inputBufferCnt != offset + 2)
			break;
		gsServersMaxConn = max;
		error = 0;
	} while (0);

	Serial.printf_P(error ? MSG_ERROR : MSG_OK);
}

/*
 * AT+CIPSTO - Sets the TCP Server Timeout
 */
void cmd_AT_CIPSTO()
{
	uint16_t offset = strlen("AT+CIPSTO");

	if (inputBuffer[offset] == '?')
	{
		Serial.printf_P(PSTR("+CIPSTO:%d\r\n"), gsServerConnTimeout / 1000);
		Serial.printf_P(MSG_OK);
		return;
	}

	uint8_t error = 1; // 1 = generic error, 0 = ok
	uint32_t to = 0;
	do
	{
		if (inputBuffer[offset] != '=')
			break;
		++offset;
		if (!readNumber(inputBuffer, offset, to) || to > 7200 || inputBufferCnt != offset + 2)
			break;
		gsServerConnTimeout = to * 1000;
		error = 0;
	} while (0);

	Serial.printf_P(error ? MSG_ERROR : MSG_OK);
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
 * AT+CIPSNTPCFG - configure SNTP time
 */
void cmd_AT_CIPSNTPCFG() // FIXME:
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
				for (uint8_t i = 0; i < 3; ++i)
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
			cfg = {WiFi.dnsIP(0), WiFi.dnsIP(1)};
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

		dnsConfig_t cfg = {0, 0};
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
				error = 0; // Success
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
			else if (sslAuth == 2 && CAcert->getCount() == 0)
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

	if (!error)
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
 * AT+CIPSSLCERTMAX - Get or set the maximum certificates amount
 */
void cmd_AT_CIPSSLCERTMAX()
{
	if (inputBuffer[16] == '?' && inputBufferCnt == 19)
	{
		Serial.printf_P(PSTR("+CIPSSLCERTMAX:%d\r\nOK\r\n"), maximumCertificates);
	}
	else if (inputBuffer[16] == '=')
	{
		uint16_t offset = 17;
		uint32_t max;

		if (readNumber(inputBuffer, offset, max))
		{
			maximumCertificates = max;
			Settings::setMaximumCertificates(maximumCertificates);

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
	uint16_t offset = 13; // offset to ? or =

	// Load certificate
	if (inputBufferCnt == offset + 2)
	{
		if (CAcert->getCount() >= maximumCertificates)
		{
			Serial.printf_P(PSTR("Reached the maximum of %d certificates\r\n"), maximumCertificates);
			Serial.printf_P(MSG_ERROR);
			return;
		}

		PemCertificate = new char[MAX_PEM_CERT_LENGTH];
		PemCertificatePos = 0;
		PemCertificateCount = 0;

		if (PemCertificate != nullptr)
		{
			gsCertLoading = true;

			Serial.printf_P(MSG_OK);
			Serial.print('>');
		}
		else // OOM
		{
			Serial.printf_P(MSG_ERROR);
		}
	}
	// Print all certificates
	else if (inputBuffer[offset] == '?' && inputBufferCnt == offset + 3)
	{
		if (CAcert->getCount() == 0)
		{
			Serial.println(F("+CIPSSLCERT:no certs loaded"));
		}
		else
		{
			for (size_t i = 0; i < CAcert->getCount(); i++)
			{
				Serial.printf_P(PSTR("+CIPSSLCERT,%d:"), i + 1);
				printCertificateName(i);
			}
		}

		Serial.printf_P(MSG_OK);
	}
	// Print specific certificate
	else if (inputBuffer[offset] == '?' && inputBufferCnt >= 16 && inputBufferCnt <= 18)
	{
		uint32_t certNumber;

		++offset;
		if (!readNumber(inputBuffer, offset, certNumber) || certNumber == 0)
		{
			Serial.printf_P(MSG_ERROR);
			return;
		}

		if (certNumber > CAcert->getCount())
		{
			Serial.printf_P(PSTR("+CIPSSLCERT,%d:no certificate\r\n"), certNumber);
			Serial.printf_P(MSG_ERROR);
			return;
		}
		else
		{
			Serial.printf_P(PSTR("+CIPSSLCERT,%d:"), certNumber);
			printCertificateName(certNumber - 1);
		}

		Serial.printf_P(MSG_OK);
	}
	// Delete specific certificate
	else if (!memcmp_P(&(inputBuffer[offset]), PSTR("=DELETE,"), 8) && (inputBufferCnt >= 22 && inputBufferCnt <= 25))
	{
		if (CAcert->getCount() == 0)
		{
			Serial.println(F("+CIPSSLCERT:no certificates"));
		}
		else
		{
			offset = 21;
			uint32_t certNumberToDelete;

			if (readNumber(inputBuffer, offset, certNumberToDelete) && certNumberToDelete <= CAcert->getCount() && certNumberToDelete != 0)
			{
				BearSSL::X509List *certList = new BearSSL::X509List();

				// Delete certificate
				for (size_t i = 0; i < CAcert->getCount(); i++)
				{
					if (certNumberToDelete != (i + 1))
					{
						const br_x509_certificate *cert = &(CAcert->getX509Certs()[i]);
						certList->append(cert->data, cert->data_len);
					}
				}

				delete CAcert;
				CAcert = certList;

/*				CAcert = BearSSL::X509List();

				for (size_t i = 0; i < certList.getCount(); i++)
				{
					const br_x509_certificate *cert = &(certList.getX509Certs()[i]);
					CAcert.append(cert->data, cert->data_len);
				}
*/
				Serial.printf_P(PSTR("+CIPSSLCERT,%d:deleted\r\n"), certNumberToDelete);
				Serial.printf_P(MSG_OK);
				return;
			}
			else if (certNumberToDelete > CAcert->getCount())
			{
				Serial.println(F("+CIPSSLCERT=DELETE:no certificate"));
			}
		}

		Serial.printf_P(MSG_ERROR);
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
	char *remoteSite;
	uint32_t remotePort;
	uint32_t maxLen;

	do
	{
		if (inputBuffer[13] != '=' || inputBuffer[14] != '"')
			break;

		uint16_t offset = 15;

		// Read remote address

		remoteSite = (char*)&inputBuffer[offset];  // the address is a pointer to the input string
		error = 4;

		while (inputBuffer[offset] != '"' && inputBuffer[offset] > ' ')
		{
			offset++;
		}

		if (inputBuffer[offset] != '"' || inputBuffer[offset + 1] != ',')
			break;

		inputBuffer[offset] = 0;  // End of remoteAddress
		offset += 2;

		// Read remote port

		error = 100; // Unspecified error

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
						now, info->tm_year + 1900, info->tm_mon + 1, info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);

		Serial.println(F("OK"));
	}
	else
	{
		Serial.println(F("+SNTPTIME:Enable SNTP first (AT+CIPSNTPCFG)"));
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
			// Potentionally, we have a command

			switch (commandList[i].mode)
			{
			case MODE_EXACT_MATCH:
				if (inpLen == strlen(cmd) + 4) // Check exact length
					return commandList[i].cmd;
				break;

			case MODE_QUERY_SET:
			{
				char c = input[strlen(cmd) + 2];
				if (c == '=' || c == '?')
				{
					if (c == '?' && inpLen != strlen(cmd) + 5) // Check exact length
						return CMD_ERROR;

					return commandList[i].cmd;
				}
			}
			break;

			case MODE_NO_CHECKING:
			{
				// The input must not continue with an alphabetic character
				char c = input[strlen(cmd) + 2];

				if (!isAlpha(c))
					return commandList[i].cmd;
			}
			break;

			default:
				return CMD_ERROR; // should not be reached
			}
		}
	}

	return CMD_ERROR;
}

/*
 * Read quote delimited string from the buffer, adjust offset according to read data
 * Maximum text length is 200 characters
 */
String readStringFromBuffer(unsigned char *inpBuf, uint16_t &offset, bool escape, bool allowEmpty)
{
	String sRet;

	if (inpBuf[offset] == '"')
	{
		char s[200];
		char *ps = s;

		++offset;

		// Until the end of the string
		while (inpBuf[offset] != '"' && inpBuf[offset] >= ' ')
		{
			if (!escape || inpBuf[offset] != '\\')
				*ps++ = inpBuf[offset];
			else if (inpBuf[offset] < ' ')
				break; // Incorrect escaped char
			else
				*ps++ = inpBuf[++offset]; // Write next char

			++offset;

			if (ps - s > (int)sizeof(s))
				break; // Buffer overflow
		}

		if (inpBuf[offset] == '"' && (ps != s || allowEmpty))
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

/*
 * Prints the certificate name for a certain index
 */
void printCertificateName(uint8_t number)
{
	const br_x509_certificate *cert = &(CAcert->getX509Certs()[number]);

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

/*
 * Compare wifi networks by RSSI
 */
int compWifiRssi(const void *elem1, const void *elem2)
{
	int f = *((int *)elem1);
	int s = *((int *)elem2);
	if (WiFi.RSSI(f) > WiFi.RSSI(s)) // @suppress("Invalid arguments")
		return -1;
	if (WiFi.RSSI(f) < WiFi.RSSI(s)) // @suppress("Invalid arguments")
		return 1;
	return 0;
}

/*
 * Print scanned wifi network information
 */
void printCWLAP(int indices[], size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		bool show = false;
		if (authmodeMask & (1 << WiFi.encryptionType(indices[i])))
		{
			show = true;
		}
		else if (WiFi.encryptionType(indices[i]) > 8)
		{
			show = true;
		}

		if (show)
		{
			if (WiFi.RSSI(indices[i]) > rssiFilter) // @suppress("Invalid arguments")
			{
				String result = "+CWLAP:(";

				if (printMask & (1 << 0))
				{
					result += WiFi.encryptionType(indices[i]);
					result += ",";
				}
				if (printMask & (1 << 1))
				{
					result += WiFi.SSID(indices[i]); // @suppress("Invalid arguments")
					result += ",";
				}
				if (printMask & (1 << 2))
				{
					result += WiFi.RSSI(indices[i]); // @suppress("Invalid arguments")
					result += ",";
				}
				if (printMask & (1 << 3))
				{
					result += WiFi.BSSIDstr(indices[i]).c_str(); // @suppress("Invalid arguments") // @suppress("Method cannot be resolved")
					result += ",";
				}
				if (printMask & (1 << 4))
				{
					result += WiFi.channel(indices[i]); // @suppress("Invalid arguments")
					result += ",";
				}
				if (printMask & (1 << 5))
				{
					// freq_offset here
					result += 0;
					result += ",";
				}
				if (printMask & (1 << 6))
				{
					// freqcal_val here
					result += 0;
					result += ",";
				}
				if (printMask & (1 << 7))
				{
					// pairwise_cipher here
					result += 0;
					result += ",";
				}
				if (printMask & (1 << 8))
				{
					// group_cipher here
					result += 0;
					result += ",";
				}
				if (printMask & (1 << 9))
				{
					// bgn here
					result += 0;
					result += ",";
				}
				if (printMask & (1 << 10))
				{
					// wps here
					result += 0;
					result += ",";
				}

				// Remove trailing comma
				result.remove(result.lastIndexOf(','));
				result += ")";
				Serial.printf("%s\n", result.c_str());
			}
		}
	}

	gsFlag_Busy = false;
	Serial.printf_P(MSG_OK);
}

/*
 * Print the networks found from scanning
 */
void printScanResult(int networksFound)
{
	if (sort_enable == 0)
	{
		int indices[networksFound];
		for (int i = 0; i < networksFound; i++)
		{
			indices[i] = i;
		}

		// Print the unsorted networks
		printCWLAP(indices, sizeof(indices) / sizeof(indices[0]));
	}
	else if (sort_enable == 1)
	{
		int indices[networksFound];
		for (int i = 0; i < networksFound; i++)
		{
			indices[i] = i;
		}

		// Sort by RSSI
		qsort(indices, sizeof(indices) / sizeof(indices[0]), sizeof(indices[0]), compWifiRssi);

		// Print the sorted networks
		printCWLAP(indices, sizeof(indices) / sizeof(indices[0]));
	}
}
