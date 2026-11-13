#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "canvas_provider.h"
#include "HomeCanvasSurface.h"
static unsigned width,height,format,stride;
static unsigned char *allocation,*pixels;
static uint64_t sequence;
static unsigned polls,physical_state;
static bool brightness(void*c,uint16_t v,uint16_t m){(void)c;return m&&v<=m;}
static bool info(void*c,risc_display_info_v1*out){(void)c;*out=(risc_display_info_v1){.width=width,.height=height,.supported_formats=RISC_DISPLAY_FORMAT_BIT(format)};return true;}
static bool acquire(void*c,uint32_t encoding,risc_display_surface_v1*out){(void)c;assert(encoding==format);*out=(risc_display_surface_v1){1,pixels,width,height,stride,(size_t)stride*height,format};return true;}
static void release(void*c,uint64_t id){(void)c;assert(id==1);}
static bool submit(void*c,uint64_t id,const risc_display_rect_v1*r,size_t count,const risc_display_present_options_v1*o,uint64_t*out){(void)c;(void)r;(void)count;(void)o;assert(id==1);*out=++sequence;return true;}
static bool status(void*c,uint64_t id,risc_display_present_status_v1*out){(void)c;assert(id==sequence);out->state=physical_state?physical_state:(polls++?RISC_DISPLAY_PRESENT_COMPLETE:RISC_DISPLAY_PRESENT_QUEUED);return true;}
static risc_touch_snapshot_v1 raw;
static uint64_t subscribe(void*c){(void)c;return 1;}
static bool unsubscribe(void*c,uint64_t id){(void)c;return id==1;}
static bool touch_poll(void*c,size_t n){(void)c;(void)n;return true;}
static int32_t touch_next(void*c,uint64_t id,risc_touch_event_v1*out){(void)c;(void)id;(void)out;return 0;}
static bool snapshot(void*c,risc_touch_snapshot_v1*out){(void)c;*out=raw;return true;}
static uint16_t color(unsigned x,unsigned y){return (uint16_t)(x+256u*y);}
static void run(unsigned w,unsigned h,unsigned output,unsigned rotation){
 width=w;height=h;format=output;stride=(format==RISC_DISPLAY_FORMAT_RGB565?width*2:(width+7)/8)+3;
 size_t bytes=(size_t)stride*height;allocation=malloc(bytes+32);assert(allocation);memset(allocation,0xa5,bytes+32);pixels=allocation+16;
 risc_display_output_api_v1 display={.api_version=1,.struct_size=sizeof(display),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&display,NULL,rotation);
 home_canvas_surface bridge;assert(home_canvas_open(&bridge,&p.api,brightness,NULL));
 const risc_display_output_api_v1*a=&bridge.display;risc_display_info_v1 logical;
 assert(a->get_info(a->context,&logical));
 assert(logical.width==(rotation%2?height:width)&&logical.height==(rotation%2?width:height));
 risc_display_surface_v1 frame={0};
 assert(!a->acquire(a->context,RISC_DISPLAY_FORMAT_MONO1,&frame));
 assert(a->acquire(a->context,RISC_DISPLAY_FORMAT_RGB565,&frame));
 assert(!home_canvas_close(&bridge)&&!home_canvas_quiesce(&bridge));
 risc_display_present_options_v1 unsupported={RISC_DISPLAY_PRESENT_CLEAN,0,0};uint64_t receipt=0;
 assert(!a->submit(a->context,frame.frame,NULL,0,&unsupported,&receipt)&&bridge.lease==frame.frame);
 a->release(a->context,frame.frame);assert(!bridge.lease);
 assert(a->acquire(a->context,RISC_DISPLAY_FORMAT_RGB565,&frame));
 for(unsigned y=0;y<frame.height;y++)for(unsigned x=0;x<frame.width;x++){uint16_t v=color(x,y);unsigned char*b=(unsigned char*)frame.pixels+(size_t)y*frame.stride_bytes+x*2;b[0]=(unsigned char)v;b[1]=(unsigned char)(v>>8);}
 assert(a->submit(a->context,frame.frame,NULL,0,NULL,&receipt));polls=0;
 assert(!bridge.lease&&bridge.receipt==receipt&&!home_canvas_close(&bridge));
 assert(!a->acquire(a->context,RISC_DISPLAY_FORMAT_RGB565,&frame));
 risc_display_present_status_v1 pending={0};assert(a->present_status(a->context,receipt,&pending)&&pending.state==RISC_DISPLAY_PRESENT_QUEUED);

 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
  unsigned lx=x,ly=y;switch(rotation){case 1:lx=y;ly=width-1-x;break;case 2:lx=width-1-x;ly=height-1-y;break;case 3:lx=height-1-y;ly=x;break;default:break;}
  uint16_t expected=color(lx,ly);
  if(output==RISC_DISPLAY_FORMAT_RGB565){unsigned char*b=pixels+(size_t)y*stride+x*2;assert((uint16_t)(b[0]|(uint16_t)b[1]<<8)==expected);}
  else {bool black=(pixels[(size_t)y*stride+x/8]&(0x80u>>(x&7)))!=0;assert(black!=canvas_rgb565_white(expected,x,y));}
 }
 for(unsigned n=0;n<16;n++)assert(allocation[n]==0xa5&&allocation[16+bytes+n]==0xa5);
 risc_display_present_status_v1 settled={0};assert(a->present_status(a->context,receipt,&settled)&&settled.state==RISC_DISPLAY_PRESENT_COMPLETE);
 assert(!bridge.receipt&&!p.frames.receipts);
 assert(home_canvas_quiesce(&bridge)&&home_canvas_resume(&bridge));
 assert(a->set_brightness(a->context,30,100)&&!a->set_brightness(a->context,101,100));
 assert(home_canvas_close(&bridge));free(allocation);
}
int main(void){
 unsigned previous=0;
 for(unsigned gray=0;gray<256;gray++){
  uint16_t p=(uint16_t)((gray*31/255)<<11|(gray*63/255)<<5|(gray*31/255));unsigned white=0;
  for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++)white+=canvas_rgb565_white(p,x,y);
  assert(white>=previous);previous=white;if(!gray)assert(!white);if(gray==255)assert(white==16);
 }
 unsigned red=0,green=0,blue=0;
 for(unsigned y=0;y<4;y++)for(unsigned x=0;x<4;x++){red+=canvas_rgb565_white(0xf800,x,y);green+=canvas_rgb565_white(0x07e0,x,y);blue+=canvas_rgb565_white(0x001f,x,y);}
 assert(green>red&&red>blue);
 const unsigned sizes[][2]={{7,11},{240,240},{410,502},{480,800},{256,256}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)for(unsigned rotation=0;rotation<4;rotation++){
  run(sizes[i][0],sizes[i][1],RISC_DISPLAY_FORMAT_RGB565,rotation);
  run(sizes[i][0],sizes[i][1],RISC_DISPLAY_FORMAT_MONO1,rotation);
 }


 width=17;height=23;format=RISC_DISPLAY_FORMAT_RGB565;stride=34;pixels=calloc(height,stride);
 risc_display_output_api_v1 input_display={.api_version=1,.struct_size=sizeof(input_display),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
 raw=(risc_touch_snapshot_v1){.sequence=1,.width=17,.height=23};
 canvas_provider input_provider;canvas_provider_init(&input_provider,&input_display,&touch,1);
 home_canvas_surface input_bridge;assert(home_canvas_open(&input_bridge,&input_provider.api,brightness,NULL));
 home_canvas_observation observation=home_canvas_capture(&input_bridge);assert(!observation.valid);
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&!observation.count);
 raw.sequence++;raw.contact_count=1;raw.contacts[0]=(risc_touch_contact_v1){.id=19,.x=3,.y=7};
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.count==1&&observation.x==4&&observation.y==5);
 uint8_t identity=observation.id;
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.id==identity);
 raw.sequence++;raw.contacts[0].id=20;
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.id!=identity);
 raw.sequence++;raw.contact_count=5;
 for(unsigned i=0;i<5;i++)raw.contacts[i]=(risc_touch_contact_v1){.id=(uint8_t)(30+i),.x=(uint16_t)i,.y=2};
 observation=home_canvas_capture(&input_bridge);assert(observation.valid&&observation.count==2);
 raw.sequence++;raw.contact_count=0;observation=home_canvas_capture(&input_bridge);assert(observation.valid&&!observation.count);
 assert(home_canvas_close(&input_bridge));free(pixels);
 /* A lower provider's failed transfer is uncertain custody, never success. */
 width=height=4;format=RISC_DISPLAY_FORMAT_RGB565;stride=8;pixels=calloc(1,32);
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 canvas_provider p;canvas_provider_init(&p,&d,NULL,0);home_canvas_surface bridge;
 assert(home_canvas_open(&bridge,&p.api,brightness,NULL));risc_display_surface_v1 f;
 assert(bridge.display.acquire(&bridge,RISC_DISPLAY_FORMAT_RGB565,&f));
 uint64_t receipt;assert(bridge.display.submit(&bridge,f.frame,NULL,0,NULL,&receipt));
 physical_state=RISC_DISPLAY_PRESENT_FAILED;risc_display_present_status_v1 r;
 assert(!bridge.display.present_status(&bridge,receipt,&r)&&bridge.retained);
 assert(!home_canvas_close(&bridge)&&!home_canvas_resume(&bridge)&&!home_canvas_quiesce(&bridge));
 assert(!bridge.display.acquire(&bridge,RISC_DISPLAY_FORMAT_RGB565,&f));
 free(pixels);
 puts("Home Canvas adapter:40 real-provider raster geometry/rotation/format cases, receipt lifecycle, cancellation, quiesce/resume and custody fence PASS");
}
