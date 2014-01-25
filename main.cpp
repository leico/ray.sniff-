#include "PACKET.h"
#include "raysniff.h"
#include "raysniff_core.h"
#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

extern "C" C74_EXPORT int main(void){
	
	raysniff      :: InitObj();
	raysniff_core :: InitObj();
	
	return 0;
}