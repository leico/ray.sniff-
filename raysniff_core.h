#include <list>
#include <string>
#include <sstream>
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "Packet.h"

#ifndef __RAYSNIFF_CORE_H__
#define __RAYSNIFF_CORE_H__

namespace raysniff_core{
	
	using namespace std;
	
	typedef union {
		int  num;
		char str[4];
	}DAT;
	
	typedef struct{
		t_object	 s_obj;
		void		*queue;

		pcap_t		 *pcaphandle;
		list<PACKET> *data;
		
		int maxdata;
		int bytes;
		
		PACKET mute;
		
		DAT buff;
		list<PACKET> :: iterator it;
		
		const u_char       *packet;
		struct pcap_pkthdr  header;
		
	} OBJECT;
	
	void  InitObj (void);
	void *NewObj  (t_symbol *s, long argc, t_atom *argv);
	void  DelObj  (OBJECT *x);
	
	void  LoopOn (OBJECT *x, long argc, t_atom *argv);
	void  LoopOff(OBJECT *x, long argc, t_atom *argv);
	
	void  BitDepth(OBJECT *x);

	void  GetData (OBJECT *x, unsigned long size, t_float *sound);
	void  GetNoise(OBJECT *x, unsigned long size, t_float *sound);

	void  GetData64 (OBJECT *x, unsigned long size, t_double *sound);
	void  GetNoise64(OBJECT *x, unsigned long size, t_double *sound);


	void  ClearBuff(OBJECT *x);
	
	void  ObjQueue(OBJECT *x);
	void  GetCount(OBJECT *x, int *count);
	
	t_max_err PcapInit(OBJECT *x, long argc, t_atom *argv);

}

#endif
