/*
 * error.h
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

#ifndef DEBUG_H_
#define DEBUG_H_


/*
 * Debug flag
 */
//#define AT_DEBUG

#if defined(AT_DEBUG)

#define AT_DEBUG_PRINTF(format, args...) do { \
	char buf[200];	\
	sprintf(buf, format, args); \
	Serial.print(buf); \
} while(0);

#define AT_DEBUG_PRINT(string) Serial.print(string);

#else

#define AT_DEBUG_PRINTF(format, args...) do {} while(0);
#define AT_DEBUG_PRINT(string) do {} while(0);

#endif


#endif /* DEBUG_H_ */
