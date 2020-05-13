/*
 * settings.h
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

#ifndef SETTINGS_H_
#define SETTINGS_H_

#include "Arduino.h"

/*
 * Defines
 */

#define EEPROM_DATA_SIZE	64

/*
 * Types
 */

typedef struct
{
	uint32_t uartBaudRate;
	uint16_t uartConfig;
	uint8_t dhcpMode;

	uint32_t crc32;
} eepromData_t;

/*
 * Class for settings
 */

class Settings
{
public:
	static uint32_t getUartBaudRate();
	static SerialConfig getUartConfig();
	static uint8_t getDhcpMode();

	static void setUartBaudRate(uint32_t baudRate);
	static void setUartConfig(SerialConfig config);
	static void setDhcpMode(uint8_t mode);

	static void reset();

protected:
	static void resetData(eepromData_t *dataPtr);

protected:
	class EEPROMData
	{
	private:
		eepromData_t data;

	public:
		EEPROMData();
		~EEPROMData();

		uint32_t getUartBaudRate() { return data.uartBaudRate; }
		SerialConfig getUartConfig() { return (SerialConfig)(data.uartConfig); }
		uint8_t getDhcpMode() { return data.dhcpMode; }

		void setUartBaudRate(uint32_t baudRate) { data.uartBaudRate = baudRate; }
		void setUartConfig(SerialConfig config) { data.uartConfig = config; }
		void setDhcpMode(uint8_t mode) { data.dhcpMode = mode; }

		eepromData_t *getDataPtr() { return &data; }
	};
};


#endif /* SETTINGS_H_ */
