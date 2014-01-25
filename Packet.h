#include <pcap.h>

#ifndef __PACKET_H__
#define __PACKET_H__

#define DATA_SIZE  1602

typedef struct Packet{
	struct pcap_pkthdr header;
	u_char             data[DATA_SIZE];
} PACKET;


#endif