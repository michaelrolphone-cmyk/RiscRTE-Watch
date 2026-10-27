#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../apps/clock/nova/nova.c"
typedef struct {canvas*c;uint8_t*alternate;unsigned calls,kind,change;} owner;
static bool boundary(void*arg){owner*o=arg;if(++o->calls!=o->change)return true;
 switch(o->kind){case 0:o->c->pixels=o->alternate;break;case 1:o->c->stride=800;break;
 case 2:o->c->width=317;o->c->height=91;break;case 3:o->c->pixels=o->alternate;o->c->stride=800;break;
 case 4:o->c->pixels=NULL;return false;case 5:return false;}return true;}
int main(void){unsigned cases=0;static uint8_t a[2][100000],b[2][100000];
 for(unsigned kind=0;kind<6;kind++)for(unsigned event=1;event<8;event++)for(unsigned initial=0;initial<4;initial++){
 memset(a,0x67,sizeof(a));memcpy(b,a,sizeof(a));canvas ca={a[0],700,100,340,boundary,NULL,initial==0?0:initial==1?253:initial==2?254:UINT32_MAX-2,false,{0}},cb=ca;cb.pixels=b[0];
 owner oa={&ca,a[1],0,kind,event},ob={&cb,b[1],0,kind,event};ca.context=&oa;cb.context=&ob;
 for(unsigned y=0;y<12;y++){uint8_t*row=cb.failed?NULL:cb.pixels+y*cb.stride;for(unsigned x=0;x<300;x++){
  uint8_t*pa=at(&ca,x,y),*pb=picker_row_at(&cb,x,y,&row);blend(pa,(uint16_t)(x*17+y),x%256);blend(pb,(uint16_t)(x*17+y),x%256);
  if(x%19==0){pa=at(&ca,x,y);pb=picker_row_at(&cb,x,y,&row);blend(pa,0xa81f,127);blend(pb,0xa81f,127);}
 }}
 assert(!memcmp(a,b,sizeof(a)));assert(ca.failed==cb.failed&&ca.operations==cb.operations&&oa.calls==ob.calls);assert(!memcmp(ca.sink,cb.sink,2));cases++;
 }printf("PASS %u row writer callback mutation/failure/wrap cases, all pixels/guards/sinks/callbacks equal\n",cases);}
