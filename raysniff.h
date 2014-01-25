#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "raysniff_core.h"

#ifndef __RAYSNIFF_H__
#define __RAYSNIFF_H__

namespace raysniff {
	
	typedef struct{
		t_pxobject	 s_obj;
		bool		 glitched;
		int			 count;

		void 		*count_out;
	} OBJECT;
	
	void     InitObj(void);
	void    *NewObj (t_symbol *s, long argc, t_atom *argv);
	void     DelObj (OBJECT *x);
	
	void GlitchSwitch(OBJECT *x, long num);
	void ObjInInt    (OBJECT *x, long num);
	
	void ClearBuff(OBJECT *x);

	void   ObjInSig (OBJECT *x, t_signal **sp, short *count);
	t_int *ObjDsp   (t_int *args);

	void ObjInSig64(OBJECT *x, t_object *dsp64, short  count, double  samplerate, long   maxvectorsize, long  flags);
	void ObjDsp64  (OBJECT *x, t_object *dsp64, double **ins, long    numins    , double **outs       , long numouts,
			            long sampleframes, long flags, void *userparam);

	
}
#endif
