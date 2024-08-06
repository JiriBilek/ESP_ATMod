#ifndef _LWIPINTFDEVPACTH_H
#define _LWIPINTFDEVPATCH_H

/*
 * This patch is required for esp8266 Arduino version 3.1.2 end older.
 * When a version following 3.1.2 is released, the patch can be removed.
 */


#include <LwipIntfDev.h>

template<class RawDev>
class LwipIntfDevPatch: public LwipIntfDev<RawDev> {
public:

	LwipIntfDevPatch(int8_t cs = SS, SPIClass &spi = SPI, int8_t intr = -1) :
			LwipIntfDev<RawDev>(cs, spi, intr)
	{

	}

	void end()
	{
		if (LwipIntfDev<RawDev>::_started)
		{
			netif_remove(&(LwipIntfDev<RawDev>::_netif));
			LwipIntfDev<RawDev>::_started = false;
			RawDev::end();
		}
	}

    uint8_t* macAddress(uint8_t* mac)
    {
        memcpy(mac, LwipIntfDev<RawDev>::_netif.hwaddr, 6);
        return mac;
    }
};


#endif