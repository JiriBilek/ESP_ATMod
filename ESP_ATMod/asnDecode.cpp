/*
 * asnDecode.cpp
 *
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

#include "asnDecode.h"

/*
 * Note: this parser reads only the necessary information
 *       to retrieve the CN (Common Name) field from the DER certificate.
 */

/*
 * X.509 certificate structure (partial):
 *
 * Certificate ::= SEQUENCE {
 *   tbsCertificate          TBSCertificate,
 *   signatureAlgorithm      AlgorithmIdentifier,
 *   signature               BIT STRING
 *   }
 *
 * TBSCertificate ::= SEQUENCE {
 *   version          [ 0 ]  Version DEFAULT v1(0),
 *   serialNumber            CertificateSerialNumber,
 *   signature               AlgorithmIdentifier,
 *   issuer                  Name,
 *   validity                Validity,
 *   subject                 Name,
 *   subjectPublicKeyInfo    SubjectPublicKeyInfo,
 *   issuerUniqueID    [ 1 ] IMPLICIT UniqueIdentifier OPTIONAL,
 *   subjectUniqueID   [ 2 ] IMPLICIT UniqueIdentifier OPTIONAL,
 *   extensions        [ 3 ] Extensions OPTIONAL
 *   }
 *
 * Name ::= SEQUENCE OF RelativeDistinguishedName
 *
 * RelativeDistinguishedName ::= SET OF AttributeValueAssertion
 *
 * AttributeValueAssertion ::= SEQUENCE {
 *   attributeType             OBJECT IDENTIFIER,
 *   attributeValue            ANY
 *   }
 */

/*
 * Debug
 */

//#define DBG(x)		Serial.println(F(x));
#define DBG(x)

/*
 * Defines
 */

enum asnTypes {
	ASN_INTEGER = 0x02,
	ASN_OBJECT_IDENTIFER = 0x06,
	ASN_SEQUENCE = 0x10,
	ASN_SET = 0x11,
	ASN_PRINTABLE_STRING = 0x13
};

enum asnMethods {
	ASN_CONSTRUCTED = 0x20,
	ASN_UNIVERSAL = 0x00,
	ASN_APPLICATION = 0x40,
	ASN_CONTEXT_SPECIFIC = 0x80,
	ASN_PRIVATE = 0xc0
};

/*
 * Local types
 */

typedef struct {
	uint8_t tag;
	uint16_t length;
	uint16_t dataPos;
} asnHeader_t;

/*
 * Static functions
 */

static asnHeader_t readHeader(uint8_t *der, uint16_t &pos, uint16_t length);

/*
 * Public functions
 */

/*
 * Read CN field from DER certificate. The CN field should be part of the Name field.
 * Returns the pointer to the string value (first byte is length followed by UTF-8 characters) or nullptr
 */
uint8_t *getCnFromDer(uint8_t *der, uint16_t length)
{
	// Input check
	if (der == nullptr)
		return nullptr;

	uint16_t pos = 0;
	asnHeader_t header;

	// 'Certificate' - sequence
	header = readHeader(der, pos, length);

	if (header.dataPos == 0 || header.tag != (ASN_SEQUENCE | ASN_CONSTRUCTED))
		return nullptr;

	DBG("SEQUENCE Certificate");

	// 'TBSCertificate' - sequence

	header = readHeader(der, header.dataPos, header.dataPos + header.length);

	if (header.dataPos == 0 || header.tag != (ASN_SEQUENCE | ASN_CONSTRUCTED))
		return nullptr;

	DBG("--SEQUENCE TBSCertificate");

	// Go inside the sequence
	uint16_t tbsCertPos = header.dataPos;
	uint16_t tbsCertEnd = tbsCertPos + header.length;

	// 'version'

	header = readHeader(der, tbsCertPos, tbsCertEnd);

	if (header.dataPos == 0 || header.tag != (ASN_CONTEXT_SPECIFIC | ASN_CONSTRUCTED))
		return nullptr;

	DBG("----[0] version");

	// 'serialNumber' - integer
	header = readHeader(der, tbsCertPos, tbsCertEnd);

	if (header.dataPos == 0 || header.tag != (ASN_INTEGER))
		return nullptr;

	DBG("----INTEGER serialNumber");

	// 'signature' - sequence
	header = readHeader(der, tbsCertPos, tbsCertEnd);

	if (header.dataPos == 0 || header.tag != (ASN_SEQUENCE | ASN_CONSTRUCTED))
		return nullptr;

	DBG("----SEQUENCE signature");

	// 'issuer' - sequence
	header = readHeader(der, tbsCertPos, tbsCertEnd);

	if (header.dataPos == 0 || header.tag != (ASN_SEQUENCE | ASN_CONSTRUCTED))
		return nullptr;

	DBG("----SEQUENCE issuer");

	// Go inside the sequence
	uint16_t issuerPos = header.dataPos;
	uint16_t issuerEnd = issuerPos + header.length;

	// Scan the issuer sequence
	while (issuerPos < issuerEnd)
	{
		// 'RelativeDistinguishedName' - SET
		header = readHeader(der, issuerPos, issuerEnd);

		if (header.dataPos == 0 || header.tag != (ASN_SET | ASN_CONSTRUCTED))
			return nullptr;

		DBG("------SET");

		// Go inside the set
		uint16_t setPos = header.dataPos;
		uint16_t setEnd = setPos + header.length;

		// 'AttributeValueAssertion' - SEQUENCE
		header = readHeader(der, setPos, setEnd);

		if (header.dataPos == 0 || header.tag != (ASN_SEQUENCE | ASN_CONSTRUCTED))
			return nullptr;

		DBG("--------SEQUENCE");

		// Go inside the sequence
		uint16_t attrPos = header.dataPos;
		uint16_t attrEnd = attrPos + header.length;

		// 'attributeType'
		header = readHeader(der, attrPos, attrEnd);

		if (header.dataPos == 0 || header.tag != ASN_OBJECT_IDENTIFER)
			return nullptr;

		DBG("--------OBJECT_IDENTIFIER");

		// Check the ID 2.5.4.3 - commonName

		if (header.length == 3 && ! memcmp(der + header.dataPos, "\x55\x04\x03", 3))
		{
			// 'attributeValue'
			header = readHeader(der, attrPos, attrEnd);

			if (header.dataPos == 0)
				return nullptr;

			DBG("--------Value");

			if (header.tag != ASN_PRINTABLE_STRING)
				return nullptr;

			return (der + header.dataPos - 1);  // the last byte before the string should be the length - assume max 127 bytes
		}
	}

	return nullptr;
}

/*
 * Static functions
 */

/*
 * Read ASN header record
 */
static asnHeader_t readHeader(uint8_t *der, uint16_t &pos, uint16_t length)
{
	asnHeader_t hdr = { 0, 0, 0 };

	// Checking
	if (der == nullptr || pos >= length)
		return hdr;

	hdr.tag = der[pos];

	uint8_t hdrsize;

	if (der[pos + 1] < 0x80)
	{
		hdr.length = der[pos + 1];
		hdrsize = 2;
	}
	else if (der[pos + 1] == 0x82)  // Omitting other length on purpose
	{
		hdr.length = (der[pos + 2] << 8) | der[pos + 3];
		hdrsize = 4;
	}
	else
		return hdr;

	// Check the length
	if (pos + hdrsize > length)
		return hdr;  // out of buffer

	hdr.dataPos = pos + hdrsize;
	pos += hdrsize + hdr.length;  // advance to the next tag in the same level

	return hdr;
}
