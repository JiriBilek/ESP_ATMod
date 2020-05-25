/*
 * settings.cpp
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
#include "coredecls.h"

#include <EEPROM.h>

#include "ESP_ATMod.h"
#include "settings.h"
#include "debug.h"

/*
 * Class Settings
 */

/*
 * Getters and setters
 */

void Settings::setUartBaudRate(uint32_t baudRate)
{
	EEPROMData eeprom;

	eeprom.setUartBaudRate(baudRate);
}

uint32_t Settings::getUartBaudRate()
{
	EEPROMData eeprom;

	return eeprom.getUartBaudRate();
}

void Settings::setUartConfig(SerialConfig config)
{
	EEPROMData eeprom;

	eeprom.setUartConfig(config);
}

SerialConfig Settings::getUartConfig()
{
	EEPROMData eeprom;

	return eeprom.getUartConfig();
}

void Settings::setDhcpMode(uint8_t mode)
{
	EEPROMData eeprom;

	eeprom.setDhcpMode(mode);
}

uint8_t Settings::getDhcpMode()
{
	EEPROMData eeprom;

	return eeprom.getDhcpMode();
}

void Settings::setNetConfig(ipConfig_t netCfg)
{
	EEPROMData eeprom;

	eeprom.setNetConfig(netCfg);
}

ipConfig_t Settings::getNetConfig()
{
	EEPROMData eeprom;

	return eeprom.getNetConfig();
}

void Settings::setDnsConfig(dnsConfig_t dnsCfg)
{
	EEPROMData eeprom;

	eeprom.setDnsConfig(dnsCfg);
}

dnsConfig_t Settings::getDnsConfig()
{
	EEPROMData eeprom;

	return eeprom.getDnsConfig();
}

/*
 * Other functions
 */

void Settings::reset()
{
	EEPROMData eeprom;

	resetData(eeprom.getDataPtr());
}

void Settings::resetData(eepromData_t *dataPtr)
{
	dataPtr->uartBaudRate = 115200;
	dataPtr->uartConfig = SERIAL_8N1;
	dataPtr->dhcpMode = 3;  // for AT+CWDHCP command
	dataPtr->netConfig = ipConfig_t({0, 0, 0});
	dataPtr->dnsConfig = dnsConfig_t({0, 0});
}

/*
 * Inner Class EEPROMData
 */

Settings::EEPROMData::EEPROMData()
{
	AT_DEBUG_PRINT("\r\n--- EEPROM on\r\n");

	EEPROM.begin(EEPROM_DATA_SIZE);

	EEPROM.get(0, data);

	unsigned int crc = crc32(&data, sizeof(data) - 4);

	// Check the data is valid

	if (crc != data.crc32)
	{
		AT_DEBUG_PRINT("--- EEPROM reset\r\n");

		Settings::resetData(&data);
	}
}

Settings::EEPROMData::~EEPROMData()
{
	AT_DEBUG_PRINT("\r\n--- EEPROM off\r\n");

	// Check for data update
	if (memcmp(&data, EEPROM.getConstDataPtr(), sizeof(data)))
	{
		AT_DEBUG_PRINT("--- EEPROM write\r\n");

		data.crc32 = crc32(&data, sizeof(data) - 4);
		EEPROM.put(0, data);
	}

	EEPROM.end();
}
