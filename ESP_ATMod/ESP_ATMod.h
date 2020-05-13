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

/*
 * Types
 */

enum clientTypes_t {
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
} client_t;

/*
 * Globals
 */

extern client_t clients[5];

const uint16_t INPUT_BUFFER_LEN = 80;

extern uint8_t inputBuffer[INPUT_BUFFER_LEN];  // Input buffer
extern uint16_t inputBufferCnt;  // Number of bytes in inputBuffer

extern uint16_t dataRead;  // Number of bytes read from the input to a send buffer

/*
 * Global settings
 */

extern bool gsEchoEnabled;  // command ATE
extern uint8_t gsCipMux;  // command AT+CIPMUX
extern uint8_t gsCipdInfo;  // command AT+CIPDINFO
extern uint8_t gsCwDhcp;  // command AT+CWDHCP
extern bool gsFlag_Connecting;  // Connecting in progress (CWJAP) - other commands ignored
extern int8_t gsLinkIdReading;  // Link id for which are the data read
extern bool gsWasConnected;  // Connection flag for AT+CIPSTATUS

extern const char APP_VERSION[];
extern const char MSG_OK[] PROGMEM;
extern const char MSG_ERROR[] PROGMEM;

/*
 * Public functions
 */

void DeleteClient(uint8_t index);
void setDhcpMode();

#endif /* ESP_ATMOD_H_ */
