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

#include "ESP_ATMod.h"

/*
 * Defines
 */

#define EEPROM_DATA_SIZE 64

/*
 * Types
 */

typedef struct
{
	uint32_t uartBaudRate;
	uint16_t uartConfig;
	uint8_t dhcpMode;
	ipConfig_t netConfig;
	dnsConfig_t dnsConfig;
	ipConfig_t apIpConfig;
	int maximumCertificates;

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
	static ipConfig_t getNetConfig();
	static ipConfig_t getApIpConfig();
	static dnsConfig_t getDnsConfig();
	static int getMaximumCertificates();

	static void setUartBaudRate(uint32_t baudRate);
	static void setUartConfig(SerialConfig config);
	static void setDhcpMode(uint8_t mode);
	static void setNetConfig(ipConfig_t netCfg);
	static void setApIpConfig(ipConfig_t apIpCfg);
	static void setDnsConfig(dnsConfig_t dnsCfg);
	static void setMaximumCertificates(int maximumCertificates);

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
		ipConfig_t getNetConfig() { return data.netConfig; }
		ipConfig_t getApIpConfig() { return data.apIpConfig; }
		dnsConfig_t getDnsConfig() { return data.dnsConfig; }
		int getMaximumCertificates() { return data.maximumCertificates; }

		void setUartBaudRate(uint32_t baudRate) { data.uartBaudRate = baudRate; }
		void setUartConfig(SerialConfig config) { data.uartConfig = config; }
		void setDhcpMode(uint8_t mode) { data.dhcpMode = mode; }
		void setNetConfig(ipConfig_t netCfg) { data.netConfig = netCfg; }
		void setApIpConfig(ipConfig_t apIpCfg) { data.apIpConfig = apIpCfg; }
		void setDnsConfig(dnsConfig_t dnsCfg) { data.dnsConfig = dnsCfg; }
		void setMaximumCertificates(int maximumCertificates) { data.maximumCertificates = maximumCertificates; }

		eepromData_t *getDataPtr() { return &data; }
	};
};

#endif /* SETTINGS_H_ */
