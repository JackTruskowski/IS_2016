/*
 max.jit.boids3d.c  12/15/2005 wesley smith
 
 adapted from:
 boids3d 08/2005 a.sier / jasch adapted from boids by eric singer © 1995-2003 eric l. singer
 free for non-commercial use
 */

#include "jit.common.h"
#include "max.jit.mop.h"

typedef struct _max_jit_boids3d
{
	t_object		ob;
	void			*obex;
} t_max_jit_boids3d;

t_jit_err jit_boids3d_init(void);

void *max_jit_boids3d_new(t_symbol *s, long argc, t_atom *argv);
void max_jit_boids3d_free(t_max_jit_boids3d *x);
void max_jit_boids3d_outputmatrix(t_max_jit_boids3d *x);
void *max_jit_boids3d_class;

int C74_EXPORT main (void)
{
	void *p,*q;
	
	jit_boids3d_init();
	setup(&max_jit_boids3d_class, max_jit_boids3d_new, (method)max_jit_boids3d_free, (short)sizeof(t_max_jit_boids3d),
          0L, A_GIMME, 0);
    
	p = max_jit_classex_setup(calcoffset(t_max_jit_boids3d,obex));
	q = jit_class_findbyname(gensym("jit_boids3d"));
    max_jit_classex_mop_wrap(p,q,MAX_JIT_MOP_FLAGS_OWN_OUTPUTMATRIX|MAX_JIT_MOP_FLAGS_OWN_JIT_MATRIX);
    max_jit_classex_standard_wrap(p,q,0);
	max_addmethod_usurp_low((method)max_jit_boids3d_outputmatrix, "outputmatrix");
    addmess((method)max_jit_mop_assist, "assist", A_CANT,0);
}

void max_jit_boids3d_outputmatrix(t_max_jit_boids3d *x)
{
	t_atom a;
	long outputmode=max_jit_mop_getoutputmode(x);
	void *mop=max_jit_obex_adornment_get(x,_jit_sym_jit_mop);
	t_jit_err err;
	
	if (outputmode&&mop) { //always output unless output mode is none
		if (outputmode==1)  {
			if (err=(t_jit_err)jit_object_method(
                                                 max_jit_obex_jitob_get(x),
                                                 _jit_sym_matrix_calc,
                                                 jit_object_method(mop,_jit_sym_getinputlist),
                                                 jit_object_method(mop,_jit_sym_getoutputlist)))
			{
				jit_error_code(x,err);
			} else {
				max_jit_mop_outputmatrix(x);
			}
		}
	}
}

void max_jit_boids3d_free(t_max_jit_boids3d *x)
{
	max_jit_mop_free(x);
	jit_object_free(max_jit_obex_jitob_get(x));
	max_jit_obex_free(x);
}

void *max_jit_boids3d_new(t_symbol *s, long argc, t_atom *argv)
{
	t_max_jit_boids3d *x;
	void *o;
    
	if (x=(t_max_jit_boids3d *)max_jit_obex_new(max_jit_boids3d_class,gensym("jit_boids3d"))) {
		if (o=jit_object_new(gensym("jit_boids3d"))) {
			max_jit_mop_setup_simple(x,o,argc,argv);			
			max_jit_attr_args(x,argc,argv);
		} else {
			error("jit.boids3d: could not allocate object");
			freeobject(x);
		}
	}
	return (x);
}