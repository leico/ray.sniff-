#include "raysniff_core.h"

namespace raysniff_core{
	
	static t_class *g_class;
	unsigned int i, j;
	
	void InitObj(void){
		t_class *c;
	
		c = class_new("__ray.sniff~_core__", (method)NewObj,
				(method)DelObj, (long)sizeof(OBJECT), 0L, A_GIMME, 0);
		class_addmethod(c, (method)LoopOn,     "loopon"    , A_CANT, 0);
		class_addmethod(c, (method)LoopOff,    "loopoff"   , A_CANT, 0);
		class_addmethod(c, (method)GetData,    "getdata"   , A_CANT, 0);
		class_addmethod(c, (method)GetNoise,   "getnoise"  , A_CANT, 0);
		class_addmethod(c, (method)GetData64,  "getdata64" , A_CANT, 0);
		class_addmethod(c, (method)GetNoise64, "getnoise64", A_CANT, 0);
		class_addmethod(c, (method)ClearBuff,  "clearbuff" , A_CANT, 0);
		class_register(CLASS_NOBOX, c);
		g_class = c;
		
		return;
	}
	
	
	void *NewObj(t_symbol *s, long argc, t_atom *argv){
		
		post("arguments:");
		post("must [long bitdepth] |any [string devicename] [long ipaddress] [long subnetmask] [string[] filter]");
		
		if (atom_gettype(argv) != A_LONG){
			post("1st argument is bit depth");
			return NULL;
		}
				
		OBJECT *x = NULL;
		x = (OBJECT *)object_alloc(g_class);
				
		if (x == NULL) {
			post("cant allocate object");
			return x;
		}
		
		switch (atom_getlong(argv)) {
			case 8  :	x -> maxdata = 0x80;       x -> bytes = 1; break;
			case 16 :	x -> maxdata = 0x8000;     x -> bytes = 2; break;
			case 24 :	x -> maxdata = 0x800000;   x -> bytes = 3; break;
			case 32 :	x -> maxdata = 0x80000000; x -> bytes = 4; break;
/*				
			case 8  :	x -> maxdata = 0xFF;       x -> bytes = 1; x -> peak = 0x7F;       break;
			case 16 :	x -> maxdata = 0xFFFF;     x -> bytes = 2; x -> peak = 0x7FFF;     break;
			case 24 :	x -> maxdata = 0xFFFFFF;   x -> bytes = 3; x -> peak = 0x7FFFFF;   break;
			case 32 :	x -> maxdata = 0xFFFFFFFF; x -> bytes = 4; x -> peak = 0x7FFFFFFF; break;
 */
			default:
				post("I cant use %ld bit", atom_getlong(argv));
				post("please set 8 or 16 or 24 or 32 at 1st argument");
				post("use default 16");
				x -> maxdata = 0x80; x -> bytes   = 2;
				break;
		}
		
		PACKET pkt = {{{0}, 0, 0}, {0}};
		x -> mute  = pkt;
		
		if (PcapInit(x, argc - 1, (argv + 1)) == MAX_ERR_NONE)
			post("Success PcapInit!");
		else
			post("Can't PcapInit");
		
		x -> queue = qelem_new((t_object *)x, (method)ObjQueue);
		x -> data  = new list<PACKET>;

		return x;
	}
	
	void  DelObj(OBJECT *x){

		qelem_unset(x -> queue);
		qelem_free (x -> queue);
		if(x -> pcaphandle != NULL)
			pcap_close (x -> pcaphandle);
		
		x -> data -> clear();
		delete x -> data;
		
		return;
	}
	
	void LoopOn(OBJECT *x, long argc, t_atom *argv){
	
		post("On!");
		qelem_set(x -> queue);
		return;
	}
	
	void LoopOff(OBJECT *x, long argc, t_atom *argv){
		
		post("Off!");
		qelem_unset(x -> queue);
		
		post("%ld", x -> data -> size());
		
		return;
	}
	
	void ObjQueue(OBJECT *x){
		
		x -> packet = pcap_next(x -> pcaphandle, &(x -> header));
		if (x -> packet == NULL)
			x -> data -> push_back(x -> mute);
		else{
			PACKET pkt = {x -> header, {0}};
			memcpy(pkt.data, x -> packet, pkt.header.caplen);
			x -> data -> push_back(pkt);
		}
		qelem_set(x -> queue);
		return;
	}
		
	void GetData(OBJECT *x, unsigned long size, t_float *sound){

		x -> it = x -> data -> begin();
		for (i = 0 ; i < size; i ++) {
				
			if (x -> data -> empty()) {
				*(sound + i) = (t_float)0.0;
			}
			else{
				
				PACKET pkt = * (x -> it);
				memcpy(x -> buff.str, &(pkt.data[j]), x -> bytes);
				j += x -> bytes;
				*(sound + i) = (t_float)(       ((x -> buff.num)  >= (x -> maxdata))   ? 
							  -( x -> maxdata - ((x -> buff.num)  -  (x -> maxdata)) ) :
											     (x -> buff.num)) /   x -> maxdata;
				if (j >= DATA_SIZE){
					x -> data -> pop_front();
					j = 0;
				}
			}
		}
		
		return;
	}
	
	void GetNoise(OBJECT *x, unsigned long size, t_float *sound){
		
		x -> it = x -> data -> begin();
		for (i = 0 ;  i < size; i ++) {
			
			if (x -> data -> empty()) {
				*(sound + i) = (t_float)0.0;
			}
			else{
				
				PACKET pkt = * (x -> it);
				if (j >= pkt.header.caplen){
					x -> data -> pop_front();
					j = 0;
					*(sound + i) = (t_float)0.0;
				}
				else{
					memcpy(x -> buff.str, &(pkt.data[j]), x -> bytes);
					j += x -> bytes;
					*(sound + i) = (t_float)(       ((x -> buff.num)  >= (x -> maxdata))   ? 
								  -( x -> maxdata - ((x -> buff.num)  -  (x -> maxdata)) ) :
										 			 (x -> buff.num)) /   x -> maxdata;
				}
			}
		}
		
		return;
	}

	void GetData64(OBJECT *x, unsigned long size, t_double *sound){

		x -> it = x -> data -> begin();
		for (i = 0 ; i < size; i ++) {

			if (x -> data -> empty()) {
				*(sound + i) = (t_double)0.0;
			}
			else{

				PACKET pkt = * (x -> it);
				memcpy(x -> buff.str, &(pkt.data[j]), x -> bytes);
				j += x -> bytes;
				*(sound + i) = (t_double)(      ((x -> buff.num)  >= (x -> maxdata))   ? 
						          -( x -> maxdata - ((x -> buff.num)  -  (x -> maxdata)) ) :
                                         (x -> buff.num)) /   x -> maxdata;
				if (j >= DATA_SIZE){
					x -> data -> pop_front();
					j = 0;
				}
			}
		}

		return;
	}

	void GetNoise64(OBJECT *x, unsigned long size, t_double *sound){

		x -> it = x -> data -> begin();
		for (i = 0 ;  i < size; i ++) {

			if (x -> data -> empty()) {
				*(sound + i) = (t_double)0.0;
			}
			else{

				PACKET pkt = * (x -> it);
				if (j >= pkt.header.caplen){
					x -> data -> pop_front();
					j = 0;
					*(sound + i) = (t_double)0.0;
				}
				else{
					memcpy(x -> buff.str, &(pkt.data[j]), x -> bytes);
					j += x -> bytes;
					*(sound + i) =  (t_double)(      ((x -> buff.num)  >= (x -> maxdata))   ? 
                         -( x -> maxdata - ((x -> buff.num)  -  (x -> maxdata)) ) :
                                            (x -> buff.num)) /   x -> maxdata;
				}
			}
		}

		return;
	}

	void ClearBuff(OBJECT *x){
		x -> data -> clear();
		return;
	}
	
	
	t_max_err PcapInit(OBJECT *x, long argc, t_atom *argv) {
		
		
		
		post("PacketCaptureInit....");
		
		char *dev = NULL;
		
		
		if (argc > 0) {
			if (atom_gettype(argv) == A_SYM)
				dev = atom_getsym(argv) -> s_name;
		}
		
		//If not args, Set Device Automatically
		char errbuf[PCAP_ERRBUF_SIZE];
		if (NULL == dev){
			dev = pcap_lookupdev(errbuf);
			if (dev == NULL) {
				post("Device not found");
				post("%s", errbuf);
				return MAX_ERR_GENERIC;
			}
		}
		post("Device : %s", dev);
		
		//Get Ip and Netmask
		bpf_u_int32 netmask = 0;
		bpf_u_int32 ip      = 0;
		
		if(argc >= 3){
			if ((atom_gettype(argv + 1) == A_LONG) && (atom_gettype(argv + 2) == A_LONG)) {
				ip      = atom_getlong(argv + 1);
				netmask = atom_getlong(argv + 2);
			}
		}
		else{
			if (pcap_lookupnet(dev, &ip, &netmask, errbuf) == -1) {
				post("can't get netmasks");
				post("%s", errbuf);
				ip     = 0;
				netmask = 0;
			}
		}
		post("ip = %ld, netmask = %ld", ip, netmask);
		
		//get Device Handle
		pcap_t *handle = pcap_open_live(dev, 4096, 1, 1, errbuf);
		if (handle == NULL) {
			post("Device Can't Open");
			post("%s", errbuf);
			return MAX_ERR_GENERIC;
		}
		post("Success get Device Handle");
		
		//packet filter
		//makefilter
		using namespace std;
		string buff;
		for (int i = 3 ; i < argc ; i ++) {
			stringstream ss;
			switch (atom_gettype(argv + i)) {
				case A_LONG:
					ss << atom_getlong(argv + i);
					buff.append(ss.str());
					buff.append(" ");
					ss.clear();
					break;
				case A_SYM:
					buff.append(atom_getsym(argv + i) -> s_name);
					buff.append(" ");
					break;
				default:
					post("Wrong Type or No Filter");
					break;
			}
		}
		char filter[buff.length()];
		strcpy(filter, buff.c_str());
		buff.clear();
		
		struct bpf_program fp;
		if(pcap_compile(handle, &fp, filter, 0, ip) == -1){
			post("can't compile filter"); 
			post("%s", filter);
			post("%s", pcap_geterr(handle));
			return MAX_ERR_GENERIC;
		}
		post("Success Compile Filter");
		
		//setfilter
		if (pcap_setfilter(handle, &fp) == -1) {
			post("can't setting filter");
			post("%s", filter);
			post("%s", pcap_geterr(handle));
			return MAX_ERR_GENERIC;
		}
		post("Success Set Filter");
		post("Filter : %s", filter);
		
		x ->pcaphandle = handle;
		return MAX_ERR_NONE;
	}
}
