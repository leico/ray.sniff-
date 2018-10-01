
#include <iostream>
#include <pcap.h>
#include <sstream>
#include <string>
#include <list>
#include <vector>

extern "C" {
#include "ext.h"
#include "ext_obex.h"
#include "ext_systhread.h"
#include "z_dsp.h"
}

typedef union {
  unsigned int num;
  u_char str[4];
} DATA;

typedef struct _raysniff{
  t_pxobject _ob;
  
  t_systhread       _systhread;
  t_systhread_mutex _mutex;
  bool _systhread_cancel;

  void *_qelem;
  void *_outlet;

  pcap_t *_pcap_handle;
  unsigned int _peak;
           int _byte;

  std :: list< std :: vector<u_char> > *_datapool;
  int _datapos;

  int _count;
  
} t_raysniff;

//void   raysniff_dsp      (t_raysniff *x, t_signal **sp, short *count); //deprecated
//t_int* raysniff_perform  (t_int *w);                                   //deprecated
void   raysniff_dsp64    (t_raysniff *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void   raysniff_perform64(t_raysniff *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam);

void  raysniff_int      (t_raysniff *x, long num);
void  raysniff_stop     (t_raysniff *x);
void* raysniff_thread   (t_raysniff *x);
void  raysniff_qfx      (t_raysniff *x);
void  raysniff_size     (t_raysniff *x);
void  raysniff_assist   (t_raysniff *x, void *b, long m, long a, char *s);
void  raysniff_free     (t_raysniff *x);
void* raysniff_new      (t_symbol *s, int argc, t_atom *argv); 

static t_class *raysniff = NULL;

void ext_main(void *r){
  
  t_class *c;

  c = class_new("ray.sniff~", (method)raysniff_new, (method)raysniff_free, sizeof(t_raysniff), 0L, A_GIMME, 0);
 
  class_addmethod(c, (method)raysniff_int,       "int",       A_DEFLONG, 0);
  class_addmethod(c, (method)raysniff_size,      "size",      A_NOTHING, 0);
//  class_addmethod(c, (method)raysniff_dsp,       "dsp",       A_CANT,    0); //deprecated
  class_addmethod(c, (method)raysniff_dsp64,     "dsp64",     A_CANT,    0);
  class_addmethod(c, (method)raysniff_assist,    "assist",    A_CANT,    0);

  class_dspinit(c);
  class_register(CLASS_BOX, c);

  raysniff = c;
	return;
}


/* ==================================================== *
 * raysniff_dsp64                                       *
 * ==================================================== */
void raysniff_dsp64(t_raysniff *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags){

  object_method(dsp64, gensym("dsp_add64"), x, raysniff_perform64, 0, NULL);
}

/* ==================================================== *
 * raysniff_perform64                                   *
 * ==================================================== */

double raysniff_getsample64(t_raysniff *x);
void   raysniff_getoutput64(t_raysniff *x, double **outs, long sampleframes);


void raysniff_perform64(t_raysniff *x, t_object *dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam){


  raysniff_getoutput64(x, outs, sampleframes);

}

/* ==================================================== *
 * raysniff_getsample64                                 *
 * ==================================================== */
double raysniff_getsample64(t_raysniff *x){


  if( systhread_mutex_trylock(x -> _mutex) == 0){

    DATA data = {0};
    int n = x -> _byte;
    while(n){
      if(x -> _datapool -> empty()){
        data.str[x -> _byte - n] = 0;
        n --;
        continue;
      }

      if(x -> _datapos >= x -> _datapool -> front().size()){
        x -> _datapos = 0;
        x -> _datapool -> pop_front();
        continue;
      }

      data.str[x -> _byte - n] = x -> _datapool -> front().at(x -> _datapos);
      x -> _datapos ++;
      n --;
    }

    systhread_mutex_unlock(x -> _mutex);

    return (data.num == 0) ? data.num : -((double)data.num - x -> _peak) / x -> _peak;
  }

  return 0;
}


/* ==================================================== *
 * raysniff_getoutput64                                 *
 * ==================================================== */
void raysniff_getoutput64(t_raysniff *x, double **outs, long sampleframes){

  t_double *out1 = outs[0];
  int n = sampleframes;

  while(n --)
    *out1 ++ = raysniff_getsample64(x);
}


/* ==================================================== *
 * raysniff_dsp     DEPRECATED                          *
 * ==================================================== */
/*
void raysniff_dsp(t_raysniff *x, t_signal **sp, short *count){

  dsp_add(raysniff_perform, 3, x, sp[0] -> s_vec, sp[0] -> s_n);
}
*/

/* ==================================================== *
 * raysniff_perform   DEPRECATED                        *
 * ==================================================== */
/*
float raysniff_getsample32(t_raysniff *x);
void  raysniff_getoutput32(t_raysniff *x, t_float **outs, int sampleframes);
*/
/*
t_int* raysniff_perform(t_int *w){
  t_raysniff *x    = (t_raysniff *)(w[1]);
  t_float    *out1 = (t_float    *)(w[2]);
  int            n = (int)          w[3];

  raysniff_getoutput32(x, &out1, n);

  return w + 4;
}
*/
/* ==================================================== *
 * raysniff_getsample32  DEPRECATED                     *
 * ==================================================== */
/*
t_float raysniff_getsample32(t_raysniff *x){

  if( systhread_mutex_trylock(x -> _mutex) == 0){

    DATA data = {0};
    int n = x -> _byte;
    while(n){
      if(x -> _datapool -> empty()){
        data.str[x -> _byte - n] = 0;
        n --;
        continue;
      }

      if(x -> _datapos >= x -> _datapool -> front().size()){
        x -> _datapos = 0;
        x -> _datapool -> pop_front();
        continue;
      }

      data.str[x -> _byte - n] = x -> _datapool -> front().at(x -> _datapos);
      x -> _datapos ++;
      n --;
    }

    systhread_mutex_unlock(x -> _mutex);

    return (data.num == 0) ? data.num : -((t_float)data.num - x -> _peak) / x -> _peak;
  }

  return 0;

}
*/
/* ==================================================== *
 * raysniff_getoutput32   DEPRECATED                    *
 * ==================================================== */
/*
void raysniff_getoutput32(t_raysniff *x, t_float **outs, int sampleframes){

  t_float *out1 = outs[0];
  int n = sampleframes;

  while(n --)
    *out1 ++ = raysniff_getsample32(x);
}
*/

/* ==================================================== *
 * raysniff_int                                         *
 * ==================================================== */
void raysniff_int(t_raysniff *x, long num){

  if(num <= 0){
    raysniff_stop(x);
    return;
  }

  if(x -> _systhread == NULL){
    post("starting a pcap thread");
    systhread_create((method)raysniff_thread, x, 0, 0, 0, &x -> _systhread);
  }
}




/* ==================================================== *
 * raysniff_size                                        *
 * ==================================================== */
void raysniff_size(t_raysniff *x){

  qelem_set(x -> _qelem);
}




/* ==================================================== *
 * raysniff_stop                                        *
 * ==================================================== */
void raysniff_stop(t_raysniff *x){

  unsigned int ret;

  if(x -> _systhread){
    post("stopping pcap thread");
    x -> _systhread_cancel = true;
    systhread_join(x -> _systhread, &ret);
    x -> _systhread = NULL;
  }
}




/* ==================================================== *
 * raysniff_thread                                      *
 * ==================================================== */
void* raysniff_thread(t_raysniff *x){

  struct pcap_pkthdr  header;
  const  u_char      *packet;

  while(1){

    if(x -> _systhread_cancel) break;

    while(systhread_mutex_trylock(x -> _mutex) != 0);

    packet = pcap_next(x -> _pcap_handle, &header);
    if(packet != NULL){

      u_char data[header.caplen];
      u_char *pos = data;
      memcpy(data, packet, header.caplen);

      x -> _datapool -> push_back( std :: vector<u_char>(pos, pos + header.caplen) );
    }
		
		systhread_mutex_unlock(x -> _mutex);

    //systhread_sleep(1);
    usleep( 1000 );
  }

  x -> _systhread_cancel = false;

  systhread_exit(0);
  return NULL;
}







/* ==================================================== *
 * raysniff_qfx                                         *
 * ==================================================== */
void raysniff_qfx(t_raysniff *x){

  while( systhread_mutex_trylock(x -> _mutex) != 0);
  int size = x -> _datapool -> size();
  systhread_mutex_unlock(x -> _mutex);

  outlet_int(x -> _outlet, size);

}



/* ==================================================== *
 * raysniff_assist                                      *
 * ==================================================== */
void raysniff_assist(t_raysniff *x, void *b, long m, long a, char *s){

  switch(m){
    case ASSIST_INLET:
      {
        switch(a){
          case 0:
            sprintf(s, "(toggle)on/off a new thread");
            break;
        }
      }
      break;
    case ASSIST_OUTLET:
      {
        switch(a){
          case 0:
            sprintf(s, "(signal)network signal");
            break;
          case 1:
            sprintf(s, "(int)report quantity of packet");
            break;
        }
      }
      break;
  }
}




/* ==================================================== *
 * raysniff_free                                        *
 * ==================================================== */
void  raysniff_free(t_raysniff *x){

  dsp_free((t_pxobject *)x);

  raysniff_stop(x);

  if(x -> _qelem) qelem_free          (x -> _qelem);
  if(x -> _mutex) systhread_mutex_free(x -> _mutex);

  if(x -> _pcap_handle) pcap_close(x -> _pcap_handle);

  delete x -> _datapool;
}



/* ==================================================== *
 * raysniff_new                                         *
 * ==================================================== */

//Function prototype
t_max_err raysniff_bitdepth(t_atom *data, unsigned int *maxdata, int *bytes);
t_max_err raysniff_pcapinit(long argc, t_atom *argv, pcap_t **handle);

//raysniff_new function
void* raysniff_new (t_symbol *s, int argc, t_atom *argv){

  post("arguments:");
  post("must [long bitdepth] |any [string devicename] [long ipaddress] [long subnetmask] [string[] filter]");

  unsigned int peak;
  int byte;
  if(raysniff_bitdepth(argv, &peak, &byte) != MAX_ERR_NONE){
    error("error : in 1st argument");
    return NULL;
  }

  pcap_t *handle = NULL;
  if(raysniff_pcapinit(argc - 1, argv + 1, &handle) != MAX_ERR_NONE){
    error("error : pcap_init");
    return NULL;
  }


  t_raysniff *x;

  x = (t_raysniff *)object_alloc(raysniff);

  if(x){

    x -> _outlet    = outlet_new(x, NULL);
    x -> _qelem     =  qelem_new(x, (method)raysniff_qfx);
    x -> _systhread = NULL;
		x -> _systhread_cancel = false;
    systhread_mutex_new(&(x -> _mutex), 0);

    x -> _pcap_handle = handle;
    x -> _peak        = peak;
    x -> _byte        = byte;

    x -> _datapool    = new std :: list < std :: vector<u_char> >;
		x -> _datapos     = 0;

    dsp_setup((t_pxobject *)x, 0);
    outlet_new(x, "signal");
  }

  return (x);
}



/* ==================================================== *
 * raysniff_bitdepth                                    *
 * ==================================================== */
t_max_err raysniff_bitdepth(t_atom *data, unsigned int *maxdata, int *bytes){

  if (atom_gettype(data) != A_LONG){
    error("Not Long Type: %ld", atom_gettype(data));
    return MAX_ERR_GENERIC;
  }


  switch (atom_getlong(data)) {
    case 8  :	*maxdata = 0x80;       *bytes = 1; break;
    case 16 :	*maxdata = 0x8000;     *bytes = 2; break;
    case 24 :	*maxdata = 0x800000;   *bytes = 3; break;
    case 32 :	*maxdata = 0x80000000; *bytes = 4; break;

    default:
              error("I cant use %ld bit", atom_getlong(data));
              error("please set 8 or 16 or 24 or 32");
              return MAX_ERR_GENERIC;
              break;
  }

  return MAX_ERR_NONE;
}

/* ==================================================== *
 * raysniff_pcapinit                                    *
 * ==================================================== */
t_max_err raysniff_pcapinit(long argc, t_atom *argv, pcap_t **handle) {

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
      error("Device not found");
      error("%s", errbuf);
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
      error("can't get netmasks");
      error("%s", errbuf);
      ip      = 0;
      netmask = 0;
    }
  }
  post("ip = %ld, netmask = %ld", ip, netmask);

  /******** Packet Capture Device settings *******/
    //get Device Handle
  *handle = pcap_create( dev, errbuf );
  if (*handle == NULL) {
    error("Can't get device handle");
    error("%s", errbuf);
    return MAX_ERR_GENERIC;
  }

    //set snap length : max length of a captured packet
  int status = 0;
  status = pcap_set_snaplen( *handle, 65535);
  if( status != 0 ) { 
    error( "set_snaplen status != 0" );
    error( "%s", errbuf);
    return MAX_ERR_GENERIC;
  }

    // set buffer timeout : if set 0, infinity waiting
  status = pcap_set_timeout( *handle, 1 );
  if( status != 0 ) {
    error( "set_timeout status != 0" );
    error( "%s", errbuf);
    return MAX_ERR_GENERIC;
  }

    // set promiscuous mode : disable -> only capture related packets
  status = pcap_set_promisc( *handle, 1 );
  if( status != 0 ) { 
    error( "set_promisc != 0" );
    error( "%s", errbuf );
    return MAX_ERR_GENERIC;
  }

    // set buffer size : prepare enough data buffer size for multi media contents
  status = pcap_set_buffer_size( *handle, 1024 * 1024 * 500 ); // 500 MB
  if( status != 0 ) { 
    error( "set_buffer_size != 0" );
    error( "%s", errbuf );
    return MAX_ERR_GENERIC;
  }

  status = pcap_activate( *handle );
  if( status != 0 ) { 
    error( "pcap_activate != 0" );
    error( "%s", errbuf );
    return MAX_ERR_GENERIC;
  }

  //*handle = pcap_open_live(dev, 4096, 1, 1, errbuf);
  //post("Success get Device Handle");
  
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
        error("Wrong Type or No Filter");
        break;
    }
  }
  char filter[buff.length()];
  strcpy(filter, buff.c_str());
  buff.clear();

  struct bpf_program fp;
  if(pcap_compile(*handle, &fp, filter, 0, ip) == -1){
    error("can't compile filter"); 
    error("%s", filter);
    error("%s", pcap_geterr(*handle));
    return MAX_ERR_GENERIC;
  }
  post("Success Compile Filter");

  //setfilter
  if (pcap_setfilter(*handle, &fp) == -1) {
    error("can't setting filter");
    error("%s", filter);
    error("%s", pcap_geterr(*handle));
    return MAX_ERR_GENERIC;
  }
  post("Success Set Filter");
  post("Filter : %s", filter);

  post("Success Pcap Init");

  return MAX_ERR_NONE;
}
