/*
    Mac-Telnet - Connect to RouterOS routers via MAC address
    Copyright (C) 2010, Håkon Nessjøen <haakon.nessjoen@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef __APPLE_CC__
#include <linux/if_ether.h>
#else
#define ETH_ALEN 6
#include <net/ethernet.h>
#endif
#include "protocol.h"
#include "config.h"

unsigned char mt_mactelnet_cpmagic[4] = { 0x56, 0x34, 0x12, 0xff };
unsigned char mt_mactelnet_clienttype[2] = { 0x00, 0x15 };


int initPacket(struct mt_packet *packet, unsigned char ptype, unsigned char *srcmac, unsigned char *dstmac, unsigned short sessionkey, unsigned int counter) {
	unsigned char *data = packet->data;

	/* Packet version */
	data[0] = 1;

	/* Packet type */
	data[1] = ptype;

	/* src ethernet address */
	memcpy(data + 2, srcmac, ETH_ALEN);

	/* dst ethernet address */
	memcpy(data + 8, dstmac, ETH_ALEN);

	if (mt_direction_fromserver) {
		/* Session key */
		data[16] = sessionkey >> 8;
		data[17] = sessionkey & 0xff;

		/* Client type: Mac Telnet */
		memcpy(data + 14, &mt_mactelnet_clienttype, sizeof(mt_mactelnet_clienttype));
	} else {
		/* Session key */
		data[14] = sessionkey >> 8;
		data[15] = sessionkey & 0xff;

		/* Client type: Mac Telnet */
		memcpy(data + 16, &mt_mactelnet_clienttype, sizeof(mt_mactelnet_clienttype));
	}

	/* Received/sent data counter */
	data[18] = (counter >> 24) & 0xff;
	data[19] = (counter >> 16) & 0xff;
	data[20] = (counter >> 8) & 0xff;
	data[21] = counter & 0xff;

	/* 22 bytes header */
	packet->size = 22;
	return 22;
}

int addControlPacket(struct mt_packet *packet, char cptype, void *cpdata, int data_len) {
	unsigned char *data = packet->data + packet->size;

	/* Something is really wrong. Packets should never become over 1500 bytes */
	if (packet->size + MT_CPHEADER_LEN + data_len > MT_PACKET_LEN) {
		fprintf(stderr, "addControlPacket: ERROR, too large packet. Exceeds %d bytes\n", MT_PACKET_LEN);
		return -1;
		//exit(1);
	}

	/* PLAINDATA isn't really a controlpacket, but we handle it here, since
	   parseControlPacket also parses raw data as PLAINDATA */
	if (cptype == MT_CPTYPE_PLAINDATA) {
		memcpy(data, cpdata, data_len);
		packet->size += data_len;
		return data_len;
	}

	/* Control Packet Magic id */
	memcpy(data,  mt_mactelnet_cpmagic, sizeof(mt_mactelnet_cpmagic));

	/* Control packet type */
	data[4] = cptype;

	/* Data length */
	data[5] = (data_len >> 24) & 0xff;
	data[6] = (data_len >> 16) & 0xff;
	data[7] = (data_len >> 8) & 0xff;
	data[8] = data_len & 0xff;

	/* Insert data */
	if (data_len) {
		memcpy(data + MT_CPHEADER_LEN, cpdata, data_len);
	}

	packet->size += MT_CPHEADER_LEN + data_len;
	/* Control packet header length + data length */
	return MT_CPHEADER_LEN + data_len;
}

void parsePacket(unsigned char *data, struct mt_mactelnet_hdr *pkthdr) {
	/* Packet version */
	pkthdr->ver = data[0];

	/* Packet type */
	pkthdr->ptype = data[1];

	/* src ethernet addr */
	memcpy(pkthdr->srcaddr, data+2,6);

	/* dst ethernet addr */
	memcpy(pkthdr->dstaddr, data+8,6);

	if (mt_direction_fromserver) {
		/* Session key */
		pkthdr->seskey = data[14] << 8 | data[15];

		/* server type */
		memcpy(&(pkthdr->clienttype), data+16, 2);
	} else {
		/* server type */
		memcpy(&(pkthdr->clienttype), data+14, 2);

		/* Session key */
		pkthdr->seskey = data[16] << 8 | data[17];
	}

	/* Received/sent data counter */
	pkthdr->counter = data[18] << 24 | data[19] << 16 | data[20] << 8 | data[21];

	/* Set pointer to actual data */
	pkthdr->data = data + 22;
}


int parseControlPacket(unsigned char *data, const int data_len, struct mt_mactelnet_control_hdr *cpkthdr) {

	if (data_len < 0)
		return 0;

	/* Check for valid minimum packet length & magic header */
	if (data_len >= 9 && memcmp(data, &mt_mactelnet_cpmagic, 4) == 0) {

		/* Control packet type */
		cpkthdr->cptype = data[4];

		/* Control packet data length */
		cpkthdr->length = data[5] << 24 | data[6] << 16 | data[7] << 8 | data[8];

		/* Set pointer to actual data */
		cpkthdr->data = data + 9;

		/* Return number of bytes in packet */
		return cpkthdr->length + 9;

	} else {
		/* Mark data as raw terminal data */
		cpkthdr->cptype = MT_CPTYPE_PLAINDATA;
		cpkthdr->length = data_len;
		cpkthdr->data = data;

		/* Consume the whole rest of the packet */
		return data_len;
	}
}

