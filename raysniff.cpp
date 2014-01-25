#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "raysniff.h"
#include "raysniff_core.h"


namespace raysniff{
	
	static t_class  *g_class	= NULL;
	static t_object *coreobject = NULL;
	
	void InitObj(void){
		
		t_class *c;
		
		c = class_new("ray.sniff~", (method)NewObj, (method)DelObj, (long)sizeof(OBJECT), 0L, A_GIMME, 0);
		
		class_addmethod(c, (method)ObjInInt,     "int",    A_LONG, 0);
		class_addmethod(c, (method)ObjInSig,     "dsp",    A_CANT, 0);
		class_addmethod(c, (method)ObjInSig64,   "dsp64",  A_CANT, 0);
		class_addmethod(c, (method)GlitchSwitch, "glitch", A_LONG, 0);
		class_addmethod(c, (method)ClearBuff,    "clear", 0);
		class_dspinit(c);
		class_register(CLASS_BOX, c);
		g_class = c;
		return;
	}
	
	void *NewObj(t_symbol *s, long argc, t_atom *argv){
		
		OBJECT *x = NULL;
		x = (OBJECT *)object_alloc(g_class);
		
		if (x != NULL){
			dsp_setup ((t_pxobject *)x, 0);

			outlet_new((t_pxobject *)x, "signal");

			x -> glitched = true;
			coreobject = (t_object *)object_new_typed(CLASS_NOBOX,
					gensym("__ray.sniff~_core__"), argc, argv);
  		}
		else
			post("Can't Allocate Object");
		
		return x;
	}	
	
	
	void DelObj(OBJECT *x){
		
		dsp_free((t_pxobject *)x);
		
		object_method(coreobject, gensym("clearbuff"));
		object_method(coreobject, gensym("loopoff"));

		object_free(coreobject);
		return;
	}
	
	void ObjInInt(OBJECT *x, long num){
		
		if (num > 0) object_method(coreobject, gensym("loopon"));
		else         object_method(coreobject, gensym("loopoff"));
		
		return;
	}
	void GlitchSwitch(OBJECT *x, long num){
		
		if (num > 0)
			x -> glitched = true;
		else
			x -> glitched = false;
		
		return;
	}
	
	void ClearBuff(OBJECT *x){
		object_method(coreobject, gensym("clearbuff"));
		return;
	}
	
	void ObjInSig(OBJECT *x, t_signal **sp, short *count){
		
		dsp_add((t_perfroutine)ObjDsp, 3, x, sp[0] -> s_vec, sp[0] -> s_n);
		return;
	}
	
	
	t_int *ObjDsp(t_int *args){
		
		OBJECT  *x     = (OBJECT *)  (args[1]);
		t_float *out   = (t_float *) (args[2]);
		int      vsize = (int)       (args[3]);
		
		if(x ->glitched) object_method(coreobject, gensym("getdata"),  vsize, out);
		else             object_method(coreobject, gensym("getnoise"), vsize, out);
		
		return args + 4;
	}


	void ObjInSig64(OBJECT *x, t_object *dsp64, short  count, double  samplerate, long   maxvectorsize, long  flags){

		object_method(dsp64, gensym("dsp_add64"), x, ObjDsp64, 0, NULL);
		return;
	}

	void ObjDsp64  (OBJECT *x, t_object *dsp64, double **ins, 
			long numins      , double **outs, long numouts,
			long sampleframes, long    flags, void *userparam)   {

		double *out = outs[0];

		if(x -> glitched) object_method(coreobject, gensym("getdata64") , sampleframes, out);
		else              object_method(coreobject, gensym("getnoise64"), sampleframes, out);

		return;
	}

}
