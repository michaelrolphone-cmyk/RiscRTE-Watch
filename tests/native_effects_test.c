#include "apps/clock/effects/effects.h"
#include "apps/clock/transitions/PortableTransition.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void guards(const uint16_t *storage,unsigned width,unsigned height,unsigned stride) {
    for(unsigned n=0;n<16;n++){assert(storage[n]==0xa5a5);assert(storage[16+stride*height+n]==0xa5a5);}
    for(unsigned y=0;y<height;y++)for(unsigned x=width;x<stride;x++)assert(storage[16+y*stride+x]==0xa5a5);
}
static void exercise(unsigned width,unsigned height) {
    unsigned stride=width+7;
    size_t count=(size_t)stride*height+32,bytes=(size_t)width*height*2;
    uint16_t *storage=malloc(count*2),*old=malloc(bytes),*scratch=malloc(bytes);assert(storage&&old&&scratch);
    memset(storage,0xa5,count*2);
    /* Deliberately omit padding after the final row from the accessible span. */
    risc_display_surface_v1 s={0,storage+16,width,height,stride*2,(size_t)(height-1)*stride*2+width*2,5};
    for(unsigned age=0;age<=1900;age+=13){assert(watch_boot_render(&s,age));guards(storage,width,height,stride);}
    unsigned ink=0;
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)ink+=storage[16+y*stride+x]!=0;
    if(width>=120&&height>=120)assert(ink);
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)old[y*width+x]=(uint16_t)(x+7*y);
    assert(portable_transition_rgb565(s.pixels,s.stride_bytes,old,width*2,scratch,bytes,width,height,0));
    for(unsigned y=0;y<height;y++)assert(!memcmp(storage+16+y*stride,old+y*width,width*2));
    guards(storage,width,height,stride);
    for(unsigned alpha=1;alpha<256;alpha+=37) {
        assert(watch_boot_render(&s,1800));
        assert(portable_transition_rgb565(s.pixels,s.stride_bytes,old,width*2,scratch,bytes,width,height,alpha));
        guards(storage,width,height,stride);
    }
    assert(watch_boot_render(&s,1800));
    for(unsigned y=0;y<height;y++)memcpy(old+y*width,storage+16+y*stride,width*2);
    assert(portable_transition_rgb565(s.pixels,s.stride_bytes,old,width*2,scratch,bytes,width,height,256));
    for(unsigned y=0;y<height;y++)assert(!memcmp(old+y*width,storage+16+y*stride,width*2));
    for(unsigned scan=0;scan<=18;scan++) {
        assert(watch_boot_render(&s,1800));assert(watch_ripple_render(&s,scan));guards(storage,width,height,stride);
    }
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)assert(!storage[16+y*stride+x]);
    risc_display_surface_v1 invalid=s;invalid.size_bytes--;assert(!watch_boot_render(&invalid,0)&&!watch_ripple_render(&invalid,0));
    invalid=s;invalid.width=0;assert(!watch_boot_render(&invalid,0)&&!watch_ripple_render(&invalid,0));
    invalid=s;invalid.width=2049;assert(!watch_boot_render(&invalid,0)&&!watch_ripple_render(&invalid,0));
    free(scratch);free(old);free(storage);
}
int main(void) {
    const unsigned sizes[][2]={{240,240},{410,502},{502,410},{120,150},{160,160},{241,295},{1,1},{1,64},{64,1}};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++)exercise(sizes[i][0],sizes[i][1]);
    puts("Native effects: boot primitives, arrival field, retained transition endpoints, padded guards and short final rows PASS");
    return 0;
}
