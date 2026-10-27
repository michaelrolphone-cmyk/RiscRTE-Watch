#include "picker_compositor_module.c"
EXPORTED bool probe_face(uint16_t*p,unsigned w,unsigned h,const nova_watch_state*s,unsigned id){risc_display_surface_v1 f={0,p,w,h,w*2,w*h*2,5};return nova_watch_face_render(&f,s,id);}
