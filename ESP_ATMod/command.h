/*
 * command.h *
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

#ifndef COMMAND_H_
#define COMMAND_H_

/*
 * Constants for implemented commands
 */
enum commands_t
{
	CMD_ERROR = 0,
	// Basic AT Commands
	CMD_AT,
	CMD_AT_RST,
	CMD_AT_GMR,
	CMD_ATE,
	CMD_AT_RESTORE,
	CMD_AT_UART,
	CMD_AT_UART_CUR,
	CMD_AT_UART_DEF,
	CMD_AT_SYSRAM,
	// Wi-Fi AT Commands
	CMD_AT_CWMODE,
	CMD_AT_CWMODE_CUR,
	CMD_AT_CWMODE_DEF,
	CMD_AT_CWJAP,
	CMD_AT_CWJAP_CUR,
	CMD_AT_CWJAP_DEF,
	CMD_AT_CWLAPOPT,
	CMD_AT_CWLAP,
	CMD_AT_CWQAP,
	CMD_AT_CWDHCP,
	CMD_AT_CWDHCP_CUR,
	CMD_AT_CWDHCP_DEF,
	CMD_AT_CWAUTOCONN,
	CMD_AT_CIPSTA,
	CMD_AT_CIPSTA_CUR,
	CMD_AT_CIPSTA_DEF,
	CMD_AT_CWHOSTNAME,
	// TCP/IP AT Commands
	CMD_AT_CIPSTATUS,
	CMD_AT_CIPSTART,
	CMD_AT_CIPSSLSIZE,
	CMD_AT_CIPSEND,
	CMD_AT_CIPCLOSEMODE,
	CMD_AT_CIPCLOSE,
	CMD_AT_CIFSR,
	CMD_AT_CIPMUX,
	CMD_AT_CIPSERVER,
	CMD_AT_CIPSERVERMAXCONN,
	CMD_AT_CIPSTO,
	CMD_AT_CIPDINFO,
	CMD_AT_CIPRECVMODE, // v 1.7
	CMD_AT_CIPRECVDATA, // v 1.7
	CMD_AT_CIPRECVLEN,	// v 1.7
	CMD_AT_CIPSNTPCFG,
	CMD_AT_CIPSNTPTIME,
	CMD_AT_CIPDNS,
	CMD_AT_CIPDNS_CUR,
	CMD_AT_CIPDNS_DEF,
	// New commands
	CMD_AT_SYSCPUFREQ,	  // New command
	CMD_AT_RFMODE,		  // New command
	CMD_AT_CIPSSLAUTH,	  // New command
	CMD_AT_CIPSSLFP,	  // New command
	CMD_AT_CIPSSLCERTMAX, // New command
	CMD_AT_CIPSSLCERT,	  // New command
	CMD_AT_CIPSSLMFLN,	  // New command
	CMD_AT_CIPSSLSTA,	  // New command
	CMD_AT_SNTPTIME,	  // New command
};

/*
 * Public functions
 */

void processCommandBuffer();

#endif /* COMMAND_H_ */
