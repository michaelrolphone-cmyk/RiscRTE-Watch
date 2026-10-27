#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "nova-candidate.c"
#include "line_reference.inc"
static uint16_t *observed;static size_t observed_n;static unsigned callbacks,fail_at;static uint64_t trace;
static bool capture_circle(void *u){(void)u;++callbacks;trace=trace*UINT64_C(1099511628211)^callbacks;for(unsigned i=0;i<17;i++)trace=trace*UINT64_C(1099511628211)^observed[(observed_n-1)*i/16];return callbacks!=fail_at;}
static uint32_t rng=1234567;
static unsigned next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
 unsigned cases=0;uint64_t pixels=0;
 /* Exact squared predicate at and around every sampled root boundary. */
 for(unsigned i=0;i<200000;i++){
  int r=(int)(next()%46341),len=(int)(next()&INT32_MAX);
  uint64_t n=(uint64_t)r*r*(unsigned)len,root=watch_circle_root(n);
  assert(root*root<=n && (root+1)*(root+1)>n);
  for(int off=-1;off<=1;off++){
   int64_t cross=(int64_t)root+off;if(cross>INT32_MAX)continue;
   for(int sign=-1;sign<=1;sign+=2){int64_t v=cross*sign;
    int32_t lim=root>INT32_MAX?INT32_MAX:(int32_t)root;
    assert((v*v<=(int64_t)n)==(v>=-lim&&v<=lim));
   }
  }
 }

 const unsigned sizes[][2]={{1,1},{17,31},{120,150},{240,240},{300,328},{466,466},{640,240},{2048,32}};
 for(unsigned i=0;i<3200;i++){
  unsigned w=sizes[i%8][0],h=sizes[i%8][1],stride=w+7;size_t n=stride*h+32;
  uint16_t *a=malloc(n*2),*b=malloc(n*2);assert(a&&b);for(size_t k=0;k<n;k++)a[k]=b[k]=(uint16_t)next();
  int cx=w>640?1920:((int)(next()%369)-64)*16+(int)(next()%16),cy=w>640?1920:((int)(next()%369)-64)*16+(int)(next()%16);
  int width=i%9?(int)(next()%1601):0;uint16_t color=(uint16_t)next();
  int x2=((int)(next()%401)-80)*16+(int)(next()%16),y2=((int)(next()%401)-80)*16+(int)(next()%16);
  if(i%13==0){x2=cx;y2=cy;}else if(i%13==1)x2=cx;else if(i%13==2)y2=cy;
  if(i%19==0){cx=-16000;cy=1920;x2=40000;y2=1920;}
  if(w==2048 && i%3==0){cx=3600;cy=1920;x2=cx;y2=cy;width=9000;}
  nova_watch_state state={.capture_audio=capture_circle};risc_display_surface_v1 sa={0,a+16,w,h,stride*2,stride*h*2,5},sb=sa;sb.pixels=b+16;
  const nova_watch_state *sp=&state;canvas ca=CANVAS_INIT(&sa,sp),cb=CANVAS_INIT(&sb,sp);
  fail_at=(unsigned[]){0,1,2,7,31}[i%5];callbacks=0;trace=0;observed=a;observed_n=n;fline_reference(&ca,cx,cy,x2,y2,width,color);unsigned ac=callbacks;uint64_t at=trace;
  callbacks=0;trace=0;observed=b;fline(&cb,cx,cy,x2,y2,width,color);
  assert(!memcmp(a,b,n*2)&&ca.failed==cb.failed&&ca.operations==cb.operations&&ac==callbacks&&at==trace);
  /* The selected180-degree transfer rotates both axes; compare every transformed
   * active pixel as well as the complete untransformed guard/padding buffer. */
  for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)assert(a[16+(h-1-y)*stride+w-1-x]==b[16+(h-1-y)*stride+w-1-x]);
  pixels+=w*h;++cases;free(a);free(b);
 }
 printf("Line raster: %u generalized shapes, %llu active pixels, 0/180 transfer parity, exact buffers/padding/callback/fault prefixes PASS\n",cases,(unsigned long long)pixels);
return 0;
}
