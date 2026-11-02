#pragma once
/* Native Quick geometry shared by rasterization and hit testing. Individual
 * controls use the shorter side for size; extra width/height is layout space.
 * No logical framebuffer or final-frame resampling is involved. */
#include "PortableQuickActions.h"
typedef struct { int x,y,w,h; } pqa_rect;
typedef struct {
    int width,height,unit,extra_y;
    int origin_x,origin_y,content_width,content_height;
    int slider_left,slider_right,slider_y[2];
    pqa_rect slider_hit[2],tiles[6],contexts,handle;
} pqa_layout;
static inline int pqa_layout_scale(const pqa_layout *l,int value) {
    return (value*l->unit+(value<0?-120:120))/240;
}
static inline int pqa_layout_size(const pqa_layout *l,int value) {
    int n=pqa_layout_scale(l,value);return n>0?n:1;
}
static inline int pqa_layout_x(const pqa_layout *l,int value) {
    return l->origin_x+(value*l->content_width+120)/240;
}
static inline int pqa_layout_y(const pqa_layout *l,int value) {
    return l->origin_y+pqa_layout_scale(l,value);
}
static inline bool pqa_rect_contains(pqa_rect r,int x,int y) {
    return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h;
}
static inline pqa_layout pqa_layout_for(const pqa_state *s) {
    pqa_layout l={0};
    l.width=(int)pqa_width(s);l.height=(int)pqa_height(s);
    l.content_width=l.width;l.content_height=l.height;
#ifdef PORTABLE_QUICK_SAFE_CONTENT
    if(s->safe_width && s->safe_height) {
        l.origin_x=s->safe_left;l.origin_y=s->safe_top;
        l.content_width=s->safe_width;l.content_height=s->safe_height;
    }
#endif
    l.unit=l.content_width<l.content_height?l.content_width:l.content_height;
    l.extra_y=l.content_height-l.unit;
    int shift=s->contexts_controls?24:0;
    l.slider_left=pqa_layout_x(&l,44);l.slider_right=pqa_layout_x(&l,182);
    for(int i=0;i<2;++i) {
        l.slider_y[i]=pqa_layout_y(&l,55+30*i-shift)+l.extra_y*(i+1)/6;
        l.slider_hit[i]=(pqa_rect){pqa_layout_x(&l,16),l.slider_y[i]-pqa_layout_size(&l,7),
            pqa_layout_x(&l,224)-pqa_layout_x(&l,16),pqa_layout_size(&l,29)};
    }
    for(int i=0;i<6;++i) {
        int x=20+(i%3)*71,y=118+(i/3)*48-(s->contexts_controls?30:0);
        l.tiles[i]=(pqa_rect){pqa_layout_x(&l,x),pqa_layout_y(&l,y)+l.extra_y*(3+i/3)/6,
            pqa_layout_x(&l,x+62)-pqa_layout_x(&l,x),pqa_layout_size(&l,42)};
    }
    l.contexts=(pqa_rect){pqa_layout_x(&l,20),pqa_layout_y(&l,182)+l.extra_y*5/6,
        pqa_layout_x(&l,220)-pqa_layout_x(&l,20),pqa_layout_size(&l,40)};
    l.handle=(pqa_rect){pqa_layout_x(&l,76),l.origin_y+l.content_height-pqa_layout_size(&l,s->contexts_controls?18:33),
        pqa_layout_x(&l,164)-pqa_layout_x(&l,76),pqa_layout_size(&l,s->contexts_controls?17:32)};
    return l;
}
