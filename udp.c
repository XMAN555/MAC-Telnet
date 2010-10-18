#ifndef __APPLE_CC__
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <malloc.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <netinet/in.h>
#include <linux/udp.h>
#include <linux/if_arp.h>
#include <stdlib.h>
#include <stdio.h>

unsigned short in_cksum(unsigned short *addr, int len)
{
	int nleft = len;
	int sum = 0;
	unsigned short *w = addr;
	unsigned short answer = 0;
	
	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}
	
	if (nleft == 1) {
		*(unsigned char *) (&answer) = *(unsigned char *) w;
		sum += answer;
	}
	
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	answer = ~sum;
	return (answer);
}

int sendCustomUDP(const int socket, const int ifindex, const unsigned char *sourcemac, const unsigned char *destmac, const struct in_addr *sourceip, const int sourceport, const struct in_addr *destip, const int destport, const unsigned char *data, const int datalen) {
	struct sockaddr_ll socket_address;

	/*
	 * Create a buffer for the full ethernet frame
	 * and align header pointers to the correct positions.
	*/
	void* buffer = (void*)malloc(ETH_FRAME_LEN);
	struct ethhdr *eh = (struct ethhdr *)buffer;
	struct iphdr *ip = (struct iphdr *)(buffer+14);
	struct udphdr *udp = (struct udphdr *)(buffer+14+20);
	unsigned char *rest = (unsigned char *)(buffer+20+14+sizeof(struct udphdr));

	static unsigned int id = 1;
	int send_result = 0;

	/* Abort if we couldn't allocate enough memory */
	if (buffer == NULL) {
		perror("malloc");
		exit(1);
	}

	/* Init ethernet header */
	memcpy(eh->h_source, sourcemac, ETH_ALEN);
	memcpy(eh->h_dest, destmac, ETH_ALEN);
	eh->h_proto = 8;

	/* Init SendTo struct */
	socket_address.sll_family   = PF_PACKET;	
	socket_address.sll_protocol = htons(ETH_P_IP);	
	socket_address.sll_ifindex  = ifindex;
	socket_address.sll_hatype   = ARPHRD_ETHER;
	socket_address.sll_pkttype  = PACKET_OTHERHOST;
	socket_address.sll_halen    = ETH_ALEN;		

	memcpy(socket_address.sll_addr, eh->h_source, ETH_ALEN);
	socket_address.sll_addr[6]  = 0x00;/*not used*/
	socket_address.sll_addr[7]  = 0x00;/*not used*/

	/* Init IP Header */
	ip->version = 4;
	ip->ihl = 5;
	ip->tos = 0x10;
	ip->tot_len = htons(datalen + 8 + 20);
	ip->id = htons(id++);
	ip->frag_off = 0x0040;
	ip->ttl = 64;
	ip->protocol = 17; /* UDP */
	ip->check = 0x0000;
	ip->saddr = sourceip->s_addr;
	ip->daddr = destip->s_addr;
	/* Calculate checksum for IP header */
	ip->check = in_cksum((unsigned short *)ip, sizeof(struct iphdr));

	/* Init UDP Header */
	udp->source = htons(sourceport);
	udp->dest = htons(destport);
	udp->check = 0;
	udp->len = htons(sizeof(struct udphdr) + datalen);

	/* Insert actual data */
	memcpy(rest, data, datalen);

	/* Send the packet */
	send_result = sendto(socket, buffer, datalen+8+14+20, 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
	free(buffer);

	/* Return amount of _data_ bytes sent */
	return send_result-8-14-20;
}
#endif
