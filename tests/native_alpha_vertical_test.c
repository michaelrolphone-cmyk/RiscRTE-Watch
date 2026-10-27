#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "apps/clock/nova/nova.c"
#include "alpha_reference.inc"
static uint16_t *observed;static size_t observed_n;static unsigned callbacks,fail_at;static uint64_t trace;
static bool capture_circle(void *u){(void)u;++callbacks;trace=trace*UINT64_C(1099511628211)^callbacks;for(unsigned i=0;i<17;i++)trace=trace*UINT64_C(1099511628211)^observed[(observed_n-1)*i/16];return callbacks!=fail_at;}
static uint32_t rng=1234567;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
 unsigned cases=0;uint64_t pixels=0;
 const unsigned sizes[][2]={{1,1},{17,31},{120,150},{240,240},{300,328},{466,466},{640,240},{2048,32}};
 for(unsigned i=0;i<3200;i++){
  unsigned w=sizes[i%8][0],h=sizes[i%8][1],stride=w+7;size_t n=stride*h+32;
  uint16_t *a=malloc(n*2),*b=malloc(n*2);assert(a&&b);for(size_t k=0;k<n;k++)a[k]=b[k]=(uint16_t)next();
  unsigned mw=next()%73+1,mh=next()%91+1;uint8_t *mask=malloc(mw*mh);assert(mask);for(unsigned k=0;k<mw*mh;k++)mask[k]=(uint8_t)next();
  int left=((int)(next()%640)-100)*256+(int)(next()%256),top=((int)(next()%640)-100)*256+(int)(next()%256);
  int sx=(int)(next()%1024)+1,sy=(int)(next()%1024)+1;unsigned opacity=next()%256;uint16_t color=(uint16_t)next();
  nova_watch_state state={.capture_audio=capture_circle};risc_display_surface_v1 sa={0,a+16,w,h,stride*2,stride*h*2,5},sb=sa;sb.pixels=b+16;
  const nova_watch_state *sp=&state;canvas ca=CANVAS_INIT(&sa,sp),cb=CANVAS_INIT(&sb,sp);
  fail_at=(unsigned[]){0,1,2,7,31}[i%5];callbacks=0;trace=0;observed=a;observed_n=n;native_alpha_reference(&ca,mask,mw,mh,left,top,sx,sy,color,opacity);unsigned ac=callbacks;uint64_t at=trace;
  callbacks=0;trace=0;observed=b;native_alpha(&cb,mask,mw,mh,left,top,sx,sy,color,opacity);
  assert(!memcmp(a,b,n*2)&&ca.failed==cb.failed&&ca.operations==cb.operations&&ac==callbacks&&at==trace);
  /* The selected180-degree transfer rotates both axes; compare every transformed
   * active pixel as well as the complete untransformed guard/padding buffer. */
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)assert(a[16+(h-1-y)*stride+w-1-x]==b[16+(h-1-y)*stride+w-1-x]);
  pixels+=w*h;++cases;free(a);free(b);free(mask);
 }
 printf("Glyph raster: %u generalized shapes, %llu active pixels, 0/180 transfer parity, exact buffers/padding/callback/fault prefixes PASS\n",cases,(unsigned long long)pixels);
return 0;
}
