#include <stdint.h>
#include <stdbool.h>
#include NOVA_SOURCE
#define EXPORTED __attribute__((visibility("default")))
EXPORTED void *probe_create(unsigned w,unsigned h){return nova_watch_picker_cache_create(w,h);}
EXPORTED void probe_destroy(void*p){nova_watch_picker_cache_destroy(p);nova_watch_raster_cache_release();}
EXPORTED bool probe_render(uint16_t*p,unsigned w,unsigned h,const nova_watch_state*s,unsigned selected,int category,const int*positions,unsigned pulse,unsigned scale,void*cache){risc_display_surface_v1 f={0,p,w,h,w*2,w*h*2,5};return nova_watch_picker_collections_render(&f,s,selected,category,positions,NULL,pulse,scale,cache);}
