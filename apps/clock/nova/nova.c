#include "nova.h"
#include "WatchHomeLayout.h"
#include <string.h>
#include <stdlib.h>

typedef struct { uint16_t pixel, count, data; } nova_span;
typedef struct { uint16_t pixel, angle, color; } nova_ring_pixel;
typedef struct {
    uint8_t character;
    int8_t left, top;
    uint8_t width, height;
    uint16_t advance, data;
} nova_glyph;
#include "assets.inc"
#ifdef WATCH_COMPACT_NOVA_ASSETS
#include "compact_assets.inc"
static bool compact_assets_ready;
static bool unpack_alpha(uint8_t *out,size_t size,const uint8_t *in,size_t length) {
 size_t i=0,j=0;
 while(i<length){unsigned code=in[i++],count=(code&127u)+1u;
  if(count>size-j)return false;
  if(code&128u){if(i==length)return false;memset(out+j,in[i++],count);}
  else {if(count>length-i)return false;memcpy(out+j,in+i,count);i+=count;}
  j+=count;
 }
 return j==size;
}
static bool prepare_compact_assets(void) {
 if(compact_assets_ready)return true;
 compact_assets_ready=unpack_alpha(nova_glyph_alpha,sizeof(nova_glyph_alpha),nova_glyph_alpha_packed,sizeof(nova_glyph_alpha_packed)) &&
  unpack_alpha(nova_dot_alpha,sizeof(nova_dot_alpha),nova_dot_alpha_packed,sizeof(nova_dot_alpha_packed)) &&
  unpack_alpha(points_alpha,sizeof(points_alpha),points_alpha_packed,sizeof(points_alpha_packed));
 return compact_assets_ready;
}
#else
static bool prepare_compact_assets(void){return true;}
#endif
#define COUNT(x) (sizeof(x)/sizeof((x)[0]))
#define CYAN 0x1f1fu
#define GREEN 0x3ff3u
#define MUTED 0x6bf0u
#define ORANGE 0xfce3u

typedef struct { uint8_t *pixels; uint32_t stride,height,width;
#ifdef WATCH_CONTEXTS_CLIENT
    bool (*capture)(void *);void *context;unsigned operations;bool failed;uint8_t sink[2];
#endif
} canvas;
#ifdef WATCH_CONTEXTS_CLIENT
#define CANVAS_INIT(s,state) {(uint8_t*)(s)->pixels,(s)->stride_bytes,(s)->height,(s)->width,(state)?(state)->capture_audio:NULL,(state)?(state)->capture_context:NULL,0,false,{0,0}}
#define CANVAS_OK(c) (!(c).failed)
static bool canvas_checkpoint(canvas*c) {
    if(c->failed)return false;
    if(c->capture&&!c->capture(c->context))c->failed=true;
    return !c->failed;
}
static bool canvas_clear(canvas*c) {
    for(unsigned y=0;y<c->height;y++){
        if(!(y&7u)&&!canvas_checkpoint(c))return false;
        memset(c->pixels+y*c->stride,0,c->width*2u);
    }
    return true;
}
#define CANVAS_CLEAR(c) do {if(!canvas_clear(&(c)))return false;}while(0)
#else
#define CANVAS_INIT(s,state) {(uint8_t*)(s)->pixels,(s)->stride_bytes,(s)->height,(s)->width}
#define CANVAS_OK(c) true
#define CANVAS_CLEAR(c) for(unsigned y=0;y<(c).height;y++)memset(at(&(c),0,y),0,(c).width*2u)
#endif
#define CANVAS_HEIGHT(c) ((c)->height)
#define CANVAS_WIDTH(c) ((c)->width)
/* Design geometry is resolved before rasterization. The canvas and every
 * destination pixel remain native; there is no completed-view resampling. */
static int native_q(const canvas *c,int value) { return (int)((int64_t)value*c->width/240); }
static int floor_q8(int value) { return value>=0?value/256:(value-255)/256; }
static bool home_surface_valid(const risc_display_surface_v1 *s) {
    return s && s->pixels && s->pixel_format==RISC_DISPLAY_FORMAT_RGB565 &&
        s->width>0 && s->width<=WATCH_LAYOUT_MAX_SIDE && s->height>0 && s->height<=WATCH_LAYOUT_MAX_SIDE &&
        !(s->stride_bytes&1u) && s->stride_bytes>=s->width*2u &&
        s->stride_bytes<=UINT32_MAX/s->height && s->size_bytes>=s->stride_bytes*s->height;
}
typedef bool (*home_square_renderer)(risc_display_surface_v1 *,const nova_watch_state *,unsigned);
static bool home_face_render(risc_display_surface_v1 *,const nova_watch_state *,unsigned,home_square_renderer);
static bool home_classic_render(risc_display_surface_v1 *s,const nova_watch_state *state,unsigned id) {
    (void)id;return nova_watch_render(s,state);
}
static void two(char *s, unsigned n) { s[0]=(char)('0'+n/10); s[1]=(char)('0'+n%10); }
void nova_watch_format(const nova_watch_state *s, nova_watch_labels *out) {
    if (!out) return;
    memcpy(out->hour_minute,"--:--",6); memcpy(out->meridiem,"--",3);
    memcpy(out->seconds,"--",3); memcpy(out->date,"TIME UNSET",11);
    memcpy(out->status,"UNSET",6); memcpy(out->battery,"--%",4);
    out->time_valid=s && s->time_valid && tw_valid_time(&s->time);
    out->battery_valid=s && s->battery_valid && s->battery_percent<=100;
    if (out->time_valid) {
        const twatch_rtc_time_v1 *t=&s->time;
        unsigned h=s->hour_24?t->hour:t->hour%12; if (!s->hour_24&&!h) h=12;
        two(out->hour_minute,h);two(out->hour_minute+3,t->minute);two(out->seconds,t->second);
        memcpy(out->meridiem,s->hour_24?"":t->hour<12?"AM":"PM",s->hour_24?1:3);memcpy(out->status,"RTC",4);
        static const char weekdays[]="SUNMONTUEWEDTHUFRISAT";
        static const char months[]="JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
        static const uint8_t offsets[]={0,3,2,5,0,3,5,1,4,6,2,4};
        unsigned y=t->year-(t->month<3);
        unsigned wd=(y+y/4-y/100+y/400+offsets[t->month-1]+t->day)%7;
        memcpy(out->date,weekdays+3*wd,3);out->date[3]=' ';two(out->date+4,t->day);
        out->date[6]=' ';memcpy(out->date+7,months+3*(t->month-1),3);out->date[10]=0;
    }
    if (out->battery_valid) {
        unsigned n=s->battery_percent;
        if (n==100) memcpy(out->battery,"100%",5);
        else if (n>=10) { two(out->battery,n);out->battery[2]='%';out->battery[3]=0; }
        else { out->battery[0]=(char)('0'+n);out->battery[1]='%';out->battery[2]=0; }
    }
}
static uint8_t *at(canvas *c, unsigned x, unsigned y) {
#ifdef WATCH_CONTEXTS_CLIENT
    if(c->failed||(!(++c->operations&255u)&&!canvas_checkpoint(c)))return c->sink;
#endif
    return c->pixels+y*c->stride+2*x;
}
static uint8_t *indexed(canvas *c, unsigned i) {
    #ifdef WATCH_CONTEXTS_CLIENT
    return at(c,i%240,i/240);
#else
    return c->stride==480 ? c->pixels+2*i : at(c,i%240,i/240);
#endif
}
static void put(uint8_t *p, uint16_t color) { p[0]=(uint8_t)color;p[1]=(uint8_t)(color>>8); }
static void blend(uint8_t *p, uint16_t color, unsigned alpha) {
    if (!alpha) return;
    if (alpha==255) { put(p,color); return; }
    unsigned old=p[0]|((unsigned)p[1]<<8),inv=255-alpha;
    unsigned r=(((old>>11)*inv+(color>>11)*alpha+127)/255);
    unsigned g=((((old>>5)&63)*inv+((color>>5)&63)*alpha+127)/255);
    unsigned b=(((old&31)*inv+(color&31)*alpha+127)/255);
    put(p,(uint16_t)((r<<11)|(g<<5)|b));
}
/* Reconstruct each glyph's coverage directly at its physical destination.
 * This operates on a glyph mask, never on a rendered application framebuffer. */
static void native_alpha(canvas *c,const uint8_t *mask,unsigned w,unsigned h,
        int left,int top,int sx,int sy,uint16_t color,unsigned opacity) {
    if(sx<=0 || sy<=0 || !w || !h)return;
    int x0=floor_q8(left),x1=floor_q8(left+(int)w*sx+255);
    int y0=floor_q8(top),y1=floor_q8(top+(int)h*sy+255);
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>(int)c->width)x1=(int)c->width;
    if(y1>(int)c->height)y1=(int)c->height;
    for(int y=y0;y<y1;y++) {
        int v=(int)(((int64_t)y*256+128-top)*256/sy)-128;
        int iy=floor_q8(v);unsigned fy=(unsigned)(v-iy*256);
        for(int x=x0;x<x1;x++) {
            int u=(int)(((int64_t)x*256+128-left)*256/sx)-128;
            int ix=floor_q8(u);unsigned fx=(unsigned)(u-ix*256);
            unsigned a=0,b=0,d=0,e=0;
            if(iy>=0 && iy<(int)h) {
                if(ix>=0 && ix<(int)w)a=mask[iy*w+ix];
                if(ix+1>=0 && ix+1<(int)w)b=mask[iy*w+ix+1];
            }
            if(iy+1>=0 && iy+1<(int)h) {
                if(ix>=0 && ix<(int)w)d=mask[(iy+1)*w+ix];
                if(ix+1>=0 && ix+1<(int)w)e=mask[(iy+1)*w+ix+1];
            }
            unsigned hi=(a*(256-fx)+b*fx+128)>>8;
            unsigned lo=(d*(256-fx)+e*fx+128)>>8;
            unsigned coverage=((hi*(256-fy)+lo*fy+128)>>8)*opacity/255;
            if(coverage)blend(at(c,(unsigned)x,(unsigned)y),color,coverage);
        }
    }
}
static const nova_glyph *glyph(const nova_glyph *set, unsigned count, char ch) {
    for (unsigned i=0;i<count;i++) if (set[i].character==(uint8_t)ch) return &set[i];
    return NULL;
}
/* All coordinates and font advances use Q8, except native pixel y. */
static int text_width(const nova_glyph *set,unsigned count,const char *s,int spacing) {
    int width=0;
    for (;*s;s++) { const nova_glyph *g=glyph(set,count,*s);if (g) width+=g->advance; width+=spacing; }
    return width;
}
static void text(canvas *c,const nova_glyph *set,unsigned count,const char *s,
                 int x,int baseline,int spacing,unsigned scale,uint16_t color,
                 unsigned alpha,bool colon) {
    if(!prepare_compact_assets())return;
    int pen=x;
    for (;*s;s++) {
        const nova_glyph *g=glyph(set,count,*s);
        if (!g) { pen+=(spacing*(int)scale)/256;continue; }
        int left=pen+g->left*(int)scale;
        int first=left>=0?left/256:(left-255)/256;
        int last=(left+g->width*(int)scale+255)/256;
        uint16_t ink=colon && *s==':'?CYAN:color;
        unsigned opacity=colon && *s==':'?alpha:255;
        if(c->width!=240u) {
            native_alpha(c,nova_glyph_alpha+g->data,g->width,g->height,native_q(c,left),
                native_q(c,(baseline+g->top)*256),native_q(c,(int)scale),native_q(c,256),ink,opacity);
            pen+=((int)g->advance+spacing)*(int)scale/256;continue;
        }
        for (int dx=first;dx<last;dx++) {
            if (dx<0 || dx>=240) continue;
            int source=((dx*256+128-left)*256)/(int)scale-128;
            int sx=source>=0?source/256:(source-255)/256;
            unsigned frac=(unsigned)(source-sx*256);
            for (unsigned gy=0;gy<g->height;gy++) {
                int dy=baseline+g->top+(int)gy;if (dy<0 || dy>=(int)c->height) continue;
                const uint8_t *row=nova_glyph_alpha+g->data+gy*g->width;
                unsigned a=sx>=0 && sx<g->width?row[sx]:0;
                unsigned b=sx+1>=0 && sx+1<g->width?row[sx+1]:0;
                unsigned coverage=((a*(256-frac)+b*frac+128)>>8)*opacity/255;
                blend(at(c,(unsigned)dx,(unsigned)dy),ink,coverage);
            }
        }
        pen+=((int)g->advance+spacing)*(int)scale/256;
    }
}
static void centered(canvas *c,const nova_glyph *set,unsigned count,const char *s,
                     int y,int spacing,uint16_t color) {
    int width=text_width(set,count,s,spacing);
    text(c,set,count,s,120*256-width/2,y,spacing,256,color,255,false);
}
static void dashed(canvas *c,const nova_ring_pixel *points,unsigned count,
                   unsigned rotation,unsigned circumference,unsigned period,bool segments) {
    for (unsigned i=0;i<count;i++) {
        const nova_ring_pixel *p=points+i;
        unsigned angle=(p->angle-rotation)&65535;
        unsigned distance=(angle*circumference)>>16;
        unsigned phase=distance%period;
        bool on=segments?(phase<384 || (phase>=1344 && phase<1440)):phase<32;
        if (on) put(indexed(c,p->pixel),p->color);
    }
}
static int sine(unsigned q8) {
    unsigned deg=(q8>>8)%360,frac=q8&255;
    return (nova_sine[deg]*(int)(256-frac)+nova_sine[deg+1]*(int)frac)/256;
}
static void seconds(canvas *c,unsigned milliseconds) {
    unsigned angle=milliseconds*65536u/60000u;
    for (unsigned i=0;i<COUNT(nova_seconds);i++) {
        const nova_ring_pixel *p=nova_seconds+i;
        if (((p->angle+16384u)&65535u)<=angle) blend(indexed(c,p->pixel),CYAN,p->color);
    }
    for (unsigned y=0;y<16;y++) for (unsigned x=0;x<8;x++)
        blend(at(c,112+x,14+y),CYAN,nova_start_cap[y*16+x]);
    unsigned degrees=milliseconds*1536u/1000u;
    int qx=480+sine(degrees)*392/32767;
    int qy=480-sine((degrees+90*256)%(360*256))*392/32767;
    int x=qx/4-10,y=qy/4-10;
    unsigned phase=(unsigned)(qy&3)*4+(unsigned)(qx&3);
    const uint8_t *sprite=nova_dot_alpha+phase*22*22;
    for (int dy=0;dy<22;dy++) for (int dx=0;dx<22;dx++)
        if (x+dx>=0 && x+dx<240 && y+dy>=0 && y+dy<240)
            blend(at(c,(unsigned)(x+dx),(unsigned)(y+dy)),0xffff,sprite[dy*22+dx]);
}
bool nova_watch_render(risc_display_surface_v1 *s,const nova_watch_state *state) {
    if(!prepare_compact_assets())return false;
    if(s && s->width!=240)return nova_watch_face_render(s,state,0);
    if(s && s->height!=240)return home_face_render(s,state,0,home_classic_render);
    if (!s || !s->pixels || s->pixel_format!=RISC_DISPLAY_FORMAT_RGB565 ||
        s->width!=240 || s->height!=240 || s->stride_bytes<480 ||
        s->stride_bytes>UINT32_MAX/240 || s->size_bytes<s->stride_bytes*240) return false;
    nova_watch_state empty={0};if (!state) state=&empty;
    nova_watch_labels labels;nova_watch_format(state,&labels);
    canvas c=CANVAS_INIT(s,state);
    CANVAS_CLEAR(c);
    for (unsigned i=0;i<COUNT(nova_base_spans);i++) {
        const nova_span *span=nova_base_spans+i;
#ifdef WATCH_CONTEXTS_CLIENT
        for(unsigned j=0;j<span->count;j++)put(indexed(&c,span->pixel+j),nova_base_pixels[span->data+j]);
#else
        uint8_t *p=indexed(&c,span->pixel);
        for(unsigned j=0;j<span->count;j++)put(p+2*j,nova_base_pixels[span->data+j]);
#endif
    }
    unsigned rot82=(state->animation_ms%90000u)*32768u/45000u;
    unsigned rot76=0u-(state->animation_ms%40000u)*65536u/40000u;
    dashed(&c,nova_ring82,COUNT(nova_ring82),rot82,8244,144,false);
    dashed(&c,nova_ring76,COUNT(nova_ring76),rot76,7640,2400,true);
    unsigned sub=state->subsecond_ms<1000?state->subsecond_ms:999;
    if (labels.time_valid) seconds(&c,state->time.second*1000u+sub);
    centered(&c,nova_date_glyphs,COUNT(nova_date_glyphs),labels.date,62,640,labels.time_valid?0x15b9:ORANGE);
    int width=text_width(nova_big_glyphs,COUNT(nova_big_glyphs),labels.hour_minute,0);
    unsigned scale=(unsigned)(132*65536/width);
    text(&c,nova_big_glyphs,COUNT(nova_big_glyphs),labels.hour_minute,54*256,126,0,scale,
         labels.time_valid?0xffff:0x7bef,sub<500?255:64,true);
    text(&c,nova_small_glyphs,COUNT(nova_small_glyphs),labels.meridiem,66*256,147,0,256,
         labels.time_valid?CYAN:MUTED,255,false);
    text(&c,nova_status_glyphs,COUNT(nova_status_glyphs),labels.status,110*256,147,512,256,MUTED,255,false);
    for (unsigned y=142;y<146;y++) for (unsigned x=101;x<105;x++)
        if (!((x==101 || x==104) && (y==142 || y==145))) put(at(&c,x,y),labels.time_valid?GREEN:ORANGE);
    text(&c,nova_small_glyphs,COUNT(nova_small_glyphs),labels.seconds,161*256,147,0,256,0x15b9,255,false);
    centered(&c,nova_battery_glyphs,COUNT(nova_battery_glyphs),labels.battery,185,0,
             labels.battery_valid?GREEN:MUTED);
    if (labels.battery_valid) {
        unsigned fill=state->battery_percent*30u;
        for (unsigned x=0;x<30;x++) {
            unsigned alpha=fill>=100?255:fill*255/100;
            for (unsigned y=190;y<192;y++) blend(at(&c,105+x,y),GREEN,alpha);
            fill=fill>=100?fill-100:0;
        }
    }
    return CANVAS_OK(c);
}

#include "../faces/render.inc"

/* Temporary sleep-refusal diagnosis uses the existing NOVA typeface. */
bool nova_watch_sleep_status(risc_display_surface_v1 *s,const char *label){
    if(!home_surface_valid(s)||!label)return false;
    for(unsigned n=0;label[n];n++)if(n==31)return false;
    canvas full=CANVAS_INIT(s,((const nova_watch_state*)NULL));canvas c=home_ui_canvas(full);
    frect(&c,10,ui_y(&c,(int)s->height)-37,220,28,0);
    centered(&c,nova_date_glyphs,COUNT(nova_date_glyphs),label,ui_y(&c,(int)s->height)-18,128,ORANGE);return true;
}

bool nova_watch_context_message(risc_display_surface_v1*s,const char*message){
 if(!home_surface_valid(s)||!message)return false;
 canvas c=CANVAS_INIT(s,((const nova_watch_state*)NULL));CANVAS_CLEAR(c);c=home_ui_canvas(c);
 int extra=ui_y(&c,(int)s->height)-240;
 char first[17]={0},second[17]={0};unsigned n=0;while(message[n]&&n<16){first[n]=message[n];n++;}unsigned j=0;while(message[n]&&j<16)second[j++]=message[n++];
 centered(&c,nova_date_glyphs,COUNT(nova_date_glyphs),"CONTEXT",65,256,CYAN);
 centered(&c,nova_date_glyphs,COUNT(nova_date_glyphs),first,112+extra/2,0,0xffff);
 centered(&c,nova_date_glyphs,COUNT(nova_date_glyphs),second,142+extra/2,0,0xffff);
 centered(&c,nova_date_glyphs,COUNT(nova_date_glyphs),"TAP TO DISMISS",ui_y(&c,(int)s->height)-50,0,MUTED);return CANVAS_OK(c);
}
#ifdef WATCH_CRASH_REPORT
#include "../crash_text.h"
#include "crash_font.inc"
static void crash_literal(canvas *c,const char *s,unsigned y) {
 for(unsigned i=0;s[i] && i<28;++i){unsigned ch=(unsigned char)s[i];if(ch<32 || ch>126)ch='?';
  for(unsigned dy=0;dy<16 && y+dy<c->height;++dy)for(unsigned dx=0;dx<8;++dx)
   if(8+i*8+dx<c->width && (crash_font[ch-32][dy]&(1u<<dx)))put(at(c,8+i*8+dx,y+dy),0xffff);
 }
}
bool nova_watch_crash_report(risc_display_surface_v1 *s,const char *report,uint32_t size,uint32_t offset,uint32_t *next) {
 if(!home_surface_valid(s) || !report || !next || offset>size)return false;
 const nova_watch_state *state=NULL;canvas c=CANVAS_INIT(s,state);CANVAS_CLEAR(c);
 unsigned rows=s->height>48?(s->height-48)/16:0;char line[29];uint32_t at=offset;
 for(unsigned row=0;row<rows && at<size;++row){at=crash_text_line(report,size,at,line);crash_literal(&c,line,8+row*16);}
 crash_literal(&c,at<size?"Tap or key: next page":"Tap or key: continue",s->height>30?s->height-30:0);
 *next=at;return CANVAS_OK(c);
}
#endif
