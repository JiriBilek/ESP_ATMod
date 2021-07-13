/*
 * WifiEvents.c
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

#include "WifiEvents.h"

/*
 * Feedback when connected to AP
 */
void onStationConnected(const WiFiEventStationModeConnected &evt)
{
	(void)evt;
	Serial.println(F("WIFI CONNECTED"));
}

/*
 * Feedback when got IP address
 */
void onStationGotIP(const WiFiEventStationModeGotIP &evt)
{
	(void)evt;
	Serial.println(F("WIFI GOT IP"));
}

/*
 * Feedback when disconnected
 * FIXME: print only reason 8
 */
void onStationDisconnected(const WiFiEventStationModeDisconnected &evt)
{
	Serial.printf_P(PSTR("WIFI DISCONNECT (%d)\r\n"), evt.reason);
}
