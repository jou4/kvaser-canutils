/*
 *
 * Original code from lib.c in linux-can/can-utils
 * Copyright (c) 2002-2007 Volkswagen Group Electronic Research
 * All rights reserved.
 * Licensed under BSD-3-Clause License (full text below)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Volkswagen nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 *
 * Modified by Kazuyoshi Kamitsukasa, 2025
 *
 */

#include <string.h>
#include "linux/can.h"
#include "../lib.h"

#define CANID_DELIM '#'
#define CC_DLC_DELIM '_'
#define XL_HDR_DELIM ':'
#define DATA_SEPERATOR '.'

const char hex_asc_upper[] = "0123456789ABCDEF";

#define hex_asc_upper_lo(x) hex_asc_upper[((x)&0x0F)]
#define hex_asc_upper_hi(x) hex_asc_upper[((x)&0xF0) >> 4]

static inline void put_hex_byte(char *buf, __u8 byte)
{
	buf[0] = hex_asc_upper_hi(byte);
	buf[1] = hex_asc_upper_lo(byte);
}

static inline void _put_id(char *buf, int end_offset, __u32 id)
{
	/* build 3 (SFF) or 8 (EFF) digit CAN identifier */
	while (end_offset >= 0) {
		buf[end_offset--] = hex_asc_upper_lo(id);
		id >>= 4;
	}
}

#define put_sff_id(buf, id) _put_id(buf, 2, id)
#define put_eff_id(buf, id) _put_id(buf, 7, id)

/* CAN DLC to real data length conversion helpers */

static const unsigned char dlc2len[] = {0, 1, 2, 3, 4, 5, 6, 7,
					8, 12, 16, 20, 24, 32, 48, 64};

/* get data length from raw data length code (DLC) */
unsigned char can_fd_dlc2len(unsigned char dlc)
{
	return dlc2len[dlc & 0x0F];
}

static const unsigned char len2dlc[] = {0, 1, 2, 3, 4, 5, 6, 7, 8,		/* 0 - 8 */
					9, 9, 9, 9,				/* 9 - 12 */
					10, 10, 10, 10,				/* 13 - 16 */
					11, 11, 11, 11,				/* 17 - 20 */
					12, 12, 12, 12,				/* 21 - 24 */
					13, 13, 13, 13, 13, 13, 13, 13,		/* 25 - 32 */
					14, 14, 14, 14, 14, 14, 14, 14,		/* 33 - 40 */
					14, 14, 14, 14, 14, 14, 14, 14,		/* 41 - 48 */
					15, 15, 15, 15, 15, 15, 15, 15,		/* 49 - 56 */
					15, 15, 15, 15, 15, 15, 15, 15};	/* 57 - 64 */

/* map the sanitized data length to an appropriate data length code */
unsigned char can_fd_len2dlc(unsigned char len)
{
	if (len > 64)
		return 0xF;

	return len2dlc[len];
}

unsigned char asc2nibble(char c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';

	if ((c >= 'A') && (c <= 'F'))
		return c - 'A' + 10;

	if ((c >= 'a') && (c <= 'f'))
		return c - 'a' + 10;

	return 16; /* error */
}

int hexstring2data(char *arg, unsigned char *data, int maxdlen)
{
	int len = (int) strlen(arg);
	int i;
	unsigned char tmp;

	if (!len || len % 2 || len > maxdlen * 2)
		return 1;

	memset(data, 0, maxdlen);

	for (i = 0; i < len / 2; i++) {
		tmp = asc2nibble(*(arg + (2 * i)));
		if (tmp > 0x0F)
			return 1;

		data[i] = (tmp << 4);

		tmp = asc2nibble(*(arg + (2 * i) + 1));
		if (tmp > 0x0F)
			return 1;

		data[i] |= tmp;
	}

	return 0;
}

int parse_canframe(char *cs, can_frame *cf)
{
	/* documentation see lib.h */

	int i, idx, dlen, len;
	int maxdlen = CAN_MAX_DLEN;
	__u8 *data = cf->msg; /* fill CAN CC/FD data by default */
	__u32 tmp;

	len = (int) strlen(cs);
	//printf("'%s' len %d\n", cs, len);

	memset(cf, 0, sizeof(*cf)); /* init CAN CC/FD/XL frame, e.g. LEN = 0 */

	if (len < 4)
		return 0;

	if (cs[3] == CANID_DELIM) { /* 3 digits SFF */

		idx = 4;
		for (i = 0; i < 3; i++) {
			if ((tmp = asc2nibble(cs[i])) > 0x0F)
				return 0;
			cf->id |= tmp << (2 - i) * 4;
		}

	} else if (cs[5] == CANID_DELIM) { /* 5 digits CAN XL VCID/PRIO*/

		// idx = 6;
		// for (i = 0; i < 5; i++) {
		// 	if ((tmp = asc2nibble(cs[i])) > 0x0F)
		// 		return 0;
		// 	cu->xl.prio |= tmp << (4 - i) * 4;
		// }

		// /* the VCID starts at bit position 16 */
		// tmp = (cu->xl.prio << 4) & CANXL_VCID_MASK;
		// cu->xl.prio &= CANXL_PRIO_MASK;
		// cu->xl.prio |= tmp;

		return 0;

	} else if (cs[8] == CANID_DELIM) { /* 8 digits EFF */

		idx = 9;
		for (i = 0; i < 8; i++) {
			if ((tmp = asc2nibble(cs[i])) > 0x0F)
				return 0;
			cf->id |= tmp << (7 - i) * 4;
		}
		if (!(cf->id & CAN_ERR_FLAG)) /* 8 digits but no errorframe?  */
			cf->flag |= canMSG_EXT;   /* then it is an extended frame */

	} else
		return 0;

	if ((cs[idx] == 'R') || (cs[idx] == 'r')) { /* RTR frame */
		cf->flag |= canMSG_RTR;

		/* check for optional DLC value for CAN 2.0B frames */
		if (cs[++idx] && (tmp = asc2nibble(cs[idx++])) <= CAN_MAX_DLEN) {
			cf->dlc = tmp;

			// /* check for optional raw DLC value for CAN 2.0B frames */
			// if ((tmp == CAN_MAX_DLEN) && (cs[idx++] == CC_DLC_DELIM)) {
			// 	tmp = asc2nibble(cs[idx]);
			// 	if ((tmp > CAN_MAX_DLEN) && (tmp <= CAN_MAX_RAW_DLC))
			// 		cu->cc.len8_dlc = tmp;
			// }
		}
		return 1;
	}

	if (cs[idx] == CANID_DELIM) { /* CAN FD frame escape char '##' */
		maxdlen = CANFD_MAX_DLEN;

		/* CAN FD frame <canid>##<flags><data>* */
		if ((tmp = asc2nibble(cs[idx + 1])) > 0x0F)
			return 0;

		cf->flag |= canFDMSG_FDF;
		if (tmp & CANFD_BRS)
			cf->flag |= canFDMSG_BRS;
		if (tmp & CANFD_ESI)
			cf->flag |= canFDMSG_ESI;
		idx += 2;

	} else if (cs[idx + 14] == CANID_DELIM) { /* CAN XL frame '#80:00:11223344#' */
		// maxdlen = CANXL_MAX_DLEN;
		// mtu = CANXL_MTU;
		// data = cu->xl.data; /* fill CAN XL data */

		// if ((cs[idx + 2] != XL_HDR_DELIM) || (cs[idx + 5] != XL_HDR_DELIM))
		// 	return 0;

		// if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
		// 	return 0;
		// cu->xl.flags = tmp << 4;
		// if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
		// 	return 0;
		// cu->xl.flags |= tmp;

		// /* force CAN XL flag if it was missing in the ASCII string */
		// cu->xl.flags |= CANXL_XLF;

		// idx++; /* skip XL_HDR_DELIM */

		// if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
		// 	return 0;
		// cu->xl.sdt = tmp << 4;
		// if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
		// 	return 0;
		// cu->xl.sdt |= tmp;

		// idx++; /* skip XL_HDR_DELIM */

		// for (i = 0; i < 8; i++) {
		// 	if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
		// 		return 0;
		// 	cu->xl.af |= tmp << (7 - i) * 4;
		// }

		// idx++; /* skip CANID_DELIM */

		return 0;
	}

	for (i = 0, dlen = 0; i < maxdlen; i++) {
		if (cs[idx] == DATA_SEPERATOR) /* skip (optional) separator */
			idx++;

		if (idx >= len) /* end of string => end of data */
			break;

		if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
			return 0;
		data[i] = tmp << 4;
		if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
			return 0;
		data[i] |= tmp;
		dlen++;
	}

	cf->dlc = dlen;

	// /* check for extra DLC when having a Classic CAN with 8 bytes payload */
	// if ((maxdlen == CAN_MAX_DLEN) && (dlen == CAN_MAX_DLEN) && (cs[idx++] == CC_DLC_DELIM)) {
	// 	unsigned char dlc = asc2nibble(cs[idx]);

	// 	if ((dlc > CAN_MAX_DLEN) && (dlc <= CAN_MAX_RAW_DLC))
	// 		cu->cc.len8_dlc = dlc;
	// }

	return 1;
}
