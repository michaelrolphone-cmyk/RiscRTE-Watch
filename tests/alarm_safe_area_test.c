#include "apps/clock/nova/nova.h"
#include "WatchHomeLayout.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void exercise(unsigned width,unsigned height,watch_layout_rect safe) {
    const risc_display_rect_v1 bounds={safe.x,safe.y,safe.width,safe.height};
    unsigned stride=(width+4)*2,bytes=stride*height;
    uint8_t *pixels=malloc(bytes),*reference=malloc(bytes);assert(pixels&&reference);
    risc_display_surface_v1 surface={.pixels=pixels,.width=width,.height=height,
        .stride_bytes=stride,.size_bytes=bytes,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
    for(unsigned mode=0;mode<32;mode++) {
        memset(pixels,0xa5,bytes);
        assert(nova_watch_alarm_label_render_safe(&surface,&bounds,"WAKE UP",mode&1,mode&2,mode&4,mode&8,mode&16,true));
        unsigned lit=0;
        for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width+4;x++) {
            uint16_t pixel=((uint16_t*)(pixels+y*stride))[x];
            if(x>=width){assert(pixel==0xa5a5);continue;}
            if(!watch_layout_contains(&safe,x,y))assert(!pixel);
            else if(pixel)lit++;
        }
        assert(lit);
        if(safe.x==0&&safe.y==0&&safe.width==width&&safe.height==height) {
            memcpy(reference,pixels,bytes);memset(pixels,0xa5,bytes);
            assert(nova_watch_alarm_label_render(&surface,"WAKE UP",mode&1,mode&2,mode&4,mode&8,mode&16,true));
            assert(!memcmp(reference,pixels,bytes));
        }
    }
    unsigned dismiss=safe.y+watch_home_alarm_dismiss_at(safe.width,safe.height);
    unsigned retry=safe.y+watch_home_alarm_retry_at(safe.width,safe.height);
    for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++) {
        unsigned expected=!watch_layout_contains(&safe,x,y)?0:y>=retry?2:y>=dismiss?1:0;
        assert(watch_home_alarm_action(&safe,x,y)==expected);
    }
    assert(!watch_home_alarm_action(&safe,width,height));
    risc_display_rect_v1 bad[]={ {-1,0,1,1},{0,-1,1,1},{0,0,0,1},{0,0,1,0},
        {0,0,width+1,height},{0,0,width,height+1},{1,1,UINT32_MAX,1} };
    for(unsigned n=0;n<sizeof(bad)/sizeof(bad[0]);n++) {
        memset(pixels,0xa5,bytes);memcpy(reference,pixels,bytes);
        assert(!nova_watch_alarm_label_render_safe(&surface,&bad[n],NULL,false,false,false,false,false,false));
        assert(!memcmp(reference,pixels,bytes));
    }
    free(reference);free(pixels);
}
int main(void) {
    exercise(466,466,(watch_layout_rect){69,69,328,328});
    exercise(410,502,(watch_layout_rect){19,11,378,474});
    exercise(502,410,(watch_layout_rect){11,19,474,378});
    exercise(240,240,(watch_layout_rect){0,0,240,240});
    exercise(410,502,(watch_layout_rect){0,0,410,502});
    puts("alarm safe content: 160 actual renders, inset gutters, padding, physical hit boundaries, zero-inset parity PASS");
}
