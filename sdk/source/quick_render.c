#include "PortableQuickRender.h"
#include <stddef.h>
#include <string.h>
#include "WatchLayout.h"
#include "PortableQuickLayout.h"

typedef struct { uint8_t code; int8_t left,top; uint8_t width,height; uint16_t advance_q8; uint32_t offset; } pqa_glyph;
typedef struct { uint8_t width,height; uint32_t offset; } pqa_icon;
#include "quick_actions_assets.inc"
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
#include "quick_native_fonts.inc"
#endif
typedef struct { uint16_t *pixels; int y,width; } pqa_row;
#define RGB(r,g,b) (uint16_t)((((r)>>3)<<11)|(((g)>>2)<<5)|((b)>>3))
#define CYAN RGB(25,227,255)
#define MUTED RGB(138,164,170)
#define LINE RGB(18,38,43)
#define TILE RGB(12,16,19)
#define GREEN RGB(61,255,154)
static uint16_t mix(uint16_t a,uint16_t b,unsigned n) {
    unsigned m=256u-n;
    return (uint16_t)(((((a>>11)*m+(b>>11)*n+128u)>>8)<<11)|
        (((((a>>5)&63u)*m+((b>>5)&63u)*n+128u)>>8)<<5)|
        (((a&31u)*m+(b&31u)*n+128u)>>8));
}
static void pixel(pqa_row *r,int x,uint16_t color,unsigned alpha) {
    if(x>=0 && x<r->width) r->pixels[x]=mix(r->pixels[x],color,alpha);
}
static void rect(pqa_row *r,int x,int y,int w,int h,int radius,uint16_t color,unsigned alpha) {
    if(r->y<y || r->y>=y+h) return;
    int yy=r->y-y, inset=0;
    /* Antialiased circular corners from four subpixel samples per pixel. */
    for(int xx=0;xx<w;++xx) {
        unsigned coverage=4;
        int cx=xx<radius?radius-1:xx>=w-radius?w-radius:xx;
        int cy=yy<radius?radius-1:yy>=h-radius?h-radius:yy;
        if(cx!=xx && cy!=yy) {
            coverage=0;
            for(int sy=1;sy<=3;sy+=2) for(int sx=1;sx<=3;sx+=2) {
                int dx=4*xx+sx-(4*cx+2),dy=4*yy+sy-(4*cy+2);
                if(dx*dx+dy*dy<=16*radius*radius) ++coverage;
            }
        }
        if(coverage) pixel(r,x+xx+inset,color,alpha*coverage/4);
    }
}
static const pqa_glyph *glyph(const pqa_glyph *g,size_t n,char ch) {
    for(size_t i=0;i<n;++i) if(g[i].code==(uint8_t)ch)return g+i;
    return NULL;
}
static int text_width(const pqa_glyph *g,size_t n,const char *s,unsigned limit) {
    int width=0;
    for(unsigned i=0;i<limit && s[i];++i) { const pqa_glyph *c=glyph(g,n,s[i]); if(c)width+=c->advance_q8; }
    return (width+128)/256;
}
static void text(pqa_row *r,const pqa_glyph *g,size_t n,const char *s,unsigned limit,
                 int x,int baseline,uint16_t color,unsigned alpha) {
    int cursor=x*256;
    for(unsigned i=0;i<limit && s[i];++i) {
        const pqa_glyph *c=glyph(g,n,s[i]);if(!c)continue;
        int yy=r->y-baseline-c->top;
        if(yy>=0 && yy<c->height) for(int xx=0;xx<c->width;++xx) {
            unsigned a=pqa_alpha[c->offset+(unsigned)yy*c->width+(unsigned)xx];
            if(a) pixel(r,(cursor+128)/256+c->left+xx,color,(a*alpha+127)/255);
        }
        cursor+=c->advance_q8;
    }
}
#define GLYPHS(set) pqa_##set##_glyphs,sizeof(pqa_##set##_glyphs)/sizeof(pqa_##set##_glyphs[0])
static void center_text(pqa_row *r,const pqa_glyph *g,size_t n,const char *s,int cx,int baseline,uint16_t color,unsigned alpha) {
    text(r,g,n,s,32,cx-text_width(g,n,s,32)/2,baseline,color,alpha);
}
static void icon(pqa_row *r,const pqa_icon *ic,int cx,int y,uint16_t color,unsigned alpha) {
    int yy=r->y-y;
    if(yy<0 || yy>=ic->height)return;
    for(int xx=0;xx<ic->width;++xx) {
        unsigned a=pqa_alpha[ic->offset+(unsigned)yy*ic->width+(unsigned)xx];
        if(a)pixel(r,cx-ic->width/2+xx,color,(a*alpha+127)/255);
    }
}
static void percentage(char out[5],bool valid,unsigned value) {
    if(!valid || value>100u){memcpy(out,"--%",4);return;}
    unsigned i=0;
    if(value>=100)out[i++]='1';
    if(value>=10)out[i++]=(char)('0'+(value/10)%10);
    out[i++]=(char)('0'+value%10);out[i++]='%';out[i]=0;
}
static void slider(pqa_row *r,int y,const pqa_icon *ic,unsigned value,bool valid,bool muted,bool error) {
    unsigned a=muted?115u:256u;
    icon(r,ic,28,y-1,error?RGB(255,135,65):CYAN,a);
    for(int i=0;i<10;++i) {
        /* Exactly 138px flex track, 3px gaps, 11.1px bars. */
        int x=44+(i*141)/10, next=44+((i+1)*141)/10;
        rect(r,x,y,next-x-3,14,3,CYAN,(valid && (unsigned)i*10u<value)?a:a*41u/256u);
    }
    char label[5];percentage(label,valid,value);
    int width=text_width(GLYPHS(percent),label,4);
    text(r,GLYPHS(percent),label,4,220-width,y+11,error?RGB(255,135,65):RGB(207,233,238),a);
}
static void panel_row(pqa_row *r,const pqa_state *s,const char *time,bool bv,unsigned bp) {
    int shift=s->contexts_controls?24:0;
    text(r,GLYPHS(time),time?time:"--:--",8,20,s->contexts_controls?19:31,RGB(255,255,255),256);
    char battery[5]; percentage(battery,bv,bp);
    int bw=text_width(GLYPHS(percent),battery,4);
    text(r,GLYPHS(percent),battery,4,146-bw,s->contexts_controls?18:30,bv?GREEN:MUTED,256);
    for(int i=0;i<10;++i)rect(r,152+i*7,s->contexts_controls?12:24,5,4,1,GREEN,bv && (unsigned)i*10<bp?256:41);
    slider(r,55-shift,&pqa_icon_sun,s->brightness,s->brightness_valid,false,(s->error_flags&PQA_ERROR_BRIGHTNESS)!=0);
    text(r,GLYPHS(caption),"NOTIFICATIONS",13,44,80-shift,MUTED,210);
    slider(r,85-shift,s->volume_valid && !s->volume?&pqa_icon_muted:&pqa_icon_volume,
           s->volume,s->volume_valid,s->volume_valid && !s->volume,(s->error_flags&PQA_ERROR_VOLUME)!=0);
    rect(r,20,108-shift,200,1,0,LINE,256);
    static const char *const labels[]={"SILENT","DND","AIRPLANE","WI-FI SETUP","BLUETOOTH","TORCH"};
    const pqa_icon *const icons[]={&pqa_icon_silent,&pqa_icon_dnd,&pqa_icon_airplane,&pqa_icon_wifi,&pqa_icon_bluetooth,&pqa_icon_torch};
    for(int i=0;i<6;++i) {
        int x=20+(i%3)*71,y=118+(i/3)*48-(s->contexts_controls?30:0);
        bool disabled=(i==1 && !s->dnd_valid) || ((i==2||i==4||(i==3&&s->radio_controls)) && !s->radios_valid) || (i==0 && !s->volume_valid);
        bool on=(i==1 && s->dnd_valid && s->dnd_enabled) || (i==0 && s->volume_valid && !s->volume) || (i==5 && s->torch) || (s->radios_valid && ((i==2&&s->airplane)||(i==3&&s->wifi_enabled)||(i==4&&s->bluetooth_enabled)));
        rect(r,x,y,62,42,12,on?CYAN:LINE,256);
        if(!on)rect(r,x+1,y+1,60,40,11,TILE,256);
        uint16_t color=on?RGB(0,20,24):disabled?RGB(66,82,88):MUTED;
        if(i==3 && (s->error_flags&PQA_ERROR_WIFI))color=RGB(255,135,65);
        if(i==1 && (s->error_flags&PQA_ERROR_DND))color=RGB(255,135,65);
        icon(r,icons[i],x+31,y+6+(18-icons[i]->height)/2,color,256);
        center_text(r,GLYPHS(label),(i==3&&s->radio_controls)?"WI-FI":labels[i],x+31,y+36,color,256);
        /* Small unavailable dash reinforces dim disabled tiles, no fake state. */
        if(disabled)rect(r,x+49,y+6,5,1,0,RGB(66,82,88),256);
    }
    if(s->contexts_controls){
        bool on=s->contexts_valid&&s->contexts_enabled;
        uint16_t color=s->contexts_valid?CYAN:MUTED;
        rect(r,20,182,200,40,12,color,256);rect(r,21,183,198,38,11,TILE,256);
        text(r,GLYPHS(label),"CONTEXT",7,31,198,color,256);
        text(r,GLYPHS(label),"DETECTION",9,31,212,color,256);
        rect(r,143,191,65,22,10,LINE,256);
        rect(r,on?176:145,193,30,18,8,on?CYAN:LINE,256);
        center_text(r,GLYPHS(label),s->contexts_valid?"OFF":"?",160,205,on?MUTED:RGB(255,255,255),256);
        center_text(r,GLYPHS(label),"ON",192,205,on?RGB(0,20,24):MUTED,256);
    }
    rect(r,104,(int)pqa_height(s)-(s->contexts_controls?13:24),32,4,2,CYAN,128);
    if(s->error_flags&PQA_ERROR_SAVE)
        center_text(r,GLYPHS(caption),"SAVE UNCONFIRMED",120,(int)pqa_height(s)-9,RGB(255,135,65),256);
}
static uint16_t read_pixel(const uint8_t *p) { return (uint16_t)(p[0]|((uint16_t)p[1]<<8)); }
static void write_pixel(uint8_t *p,uint16_t v) {p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}
static unsigned torch_edge(int v,int extent) {
    int d=v<extent/2?v:extent-1-v;
    if(d>=46)return 256;
    if(d<14)return (unsigned)d*102u/14u;
    return 102u+(unsigned)(d-14)*154u/32u;
}
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
#include "quick_native_render.inc"
#endif
static bool render_rows(risc_display_surface_v1 *f,const pqa_state *s,const char *time,bool bv,uint8_t bp,unsigned first,unsigned count) {
    if(!f || !s || !f->pixels || f->pixel_format!=RISC_DISPLAY_FORMAT_RGB565)return false;
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    if(s->native_width && (
#if defined(PORTABLE_QUICK_SAFE_CONTENT) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
        true ||
#endif
        s->native_width!=240 || s->logical_height!=240))
        return native_render_rows(f,s,time,bv,bp,first,count);
#endif
    watch_layout layout;
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
    if(!watch_layout_open(&layout,f->width,f->height,NULL) || layout.height!=pqa_height(s))return false;
#else
    if(f->width!=240 || f->height!=240 || !watch_layout_open(&layout,240,240,NULL))return false;
#endif
    if(first>f->height || count>f->height-first)return false;
    if(layout.height!=pqa_height(s) || f->stride_bytes<(uint32_t)f->width*2u)return false;
    uint64_t span=(uint64_t)(f->height-1)*f->stride_bytes+(uint32_t)f->width*2u;
    if(span>f->size_bytes)return false;
    if(!pqa_visible(s))return true;
    bv=bv && bp<=100;
    const int logical_height=(int)layout.height,open=pqa_open_q8(s);
    int position=s->position_q8<0?0:s->position_q8>open?open:s->position_q8;
    uint16_t pixels[240];pqa_row row={pixels,0,240};
    if(s->torch) {
        for(unsigned py=first;py<first+count;++py) {
            int y=(int)(py*240u/f->width);row.y=y;
            for(int x=0;x<240;++x)row.pixels[x]=RGB(255,243,214);
            center_text(&row,GLYPHS(torch),"TAP TO TURN OFF",120,logical_height-30,RGB(106,90,48),256);
            uint8_t *dst=(uint8_t*)f->pixels+(size_t)py*f->stride_bytes;
            for(unsigned px=0;px<f->width;++px) {
                unsigned x=px*240u/f->width;
                unsigned a=torch_edge((int)x,240)*torch_edge(y,logical_height)/256u;
                write_pixel(dst+2*px,mix(0,row.pixels[x],a));
            }
        }
        return true;
    }
    int offset=(position-open)/256;
    unsigned face_alpha=(unsigned)(position>=190*256?0:256-position/190);
    for(unsigned py=first;py<first+count;++py) {
        int y=(int)(py*240u/f->width);
        uint8_t *dst=(uint8_t*)f->pixels+(size_t)py*f->stride_bytes;
        int local=y-offset;
        bool in_panel=local>=0 && local<logical_height;
        unsigned a=in_panel?(local<logical_height-22?256u:(unsigned)(logical_height-local)*256u/22u):0;
        if(in_panel){memset(pixels,0,sizeof(pixels));row.y=local;panel_row(&row,s,time,bv,bp);}
        for(unsigned px=0;px<f->width;++px) {
            unsigned x=px*240u/f->width;
            uint16_t behind=mix(0,read_pixel(dst+2*px),face_alpha);
            write_pixel(dst+2*px,in_panel?mix(behind,row.pixels[x],a):behind);
        }
    }
    return true;
}

bool pqa_render(risc_display_surface_v1 *f,const pqa_state *s,const char *time,bool bv,uint8_t bp) {
 return f&&render_rows(f,s,time,bv,bp,0,f->height);
}
#ifdef PORTABLE_RASTER_SNAPSHOT
bool pqa_render_rows(risc_display_surface_v1 *f,const pqa_state *s,const char *time,bool bv,uint8_t bp,unsigned first,unsigned count) {
 return render_rows(f,s,time,bv,bp,first,count);
}
#endif

void pqa_paper_icon(unsigned index,int x,int y,int size,bool black,
                    void (*draw)(int,int,uint32_t,unsigned)) {
    const pqa_icon *const icons[]={&pqa_icon_silent,&pqa_icon_dnd,&pqa_icon_airplane,
      &pqa_icon_wifi,&pqa_icon_bluetooth,&pqa_icon_torch,&pqa_icon_sun,&pqa_icon_volume};
    if(index>=sizeof(icons)/sizeof(icons[0]) || !draw || size<1 || size>64)return;
    const pqa_icon *ic=icons[index];int largest=ic->width>ic->height?ic->width:ic->height;
    int w=ic->width*size/largest,h=ic->height*size/largest;
    for(int j=0;j<h;j++)for(int i=0;i<w;i++)
      if(pqa_alpha[ic->offset+(unsigned)(j*ic->height/h)*ic->width+(unsigned)(i*ic->width/w)]>=128)
        draw(x+(size-w)/2+i,y+(size-h)/2+j,black?0:0xffffff,255);
}
