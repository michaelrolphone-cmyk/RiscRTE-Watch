#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "../apps/clock/faces/annulus_spans.h"
static uint32_t seed=0x9ab312u;
static uint32_t random32(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void){
 uint64_t checked=0,kept=0;
 for(unsigned k=0;k<40000;k++){
  int side=(int[]){1,17,120,240,466,640,1024,2048}[k%8];
  int cx=(int)(random32()%(unsigned)(side*32))-side*8,cy=(int)(random32()%(unsigned)(side*32))-side*8;
  int outer=(int)(random32()%(unsigned)(side*16+1)),inner=k%7?outer-(int)(random32()%(unsigned)(outer+1)):0;
  int y=(int)(random32()%(unsigned)side),first[2],last[2];unsigned spans=watch_circle_spans(cx,cy,outer,inner,y,0,side-1,first,last);
  assert(spans<=2);if(spans==2)assert(last[0]<first[1]);
  for(int x=0;x<side;x++){
   int64_t dx=(int64_t)x*16+8-cx,dy=(int64_t)y*16+8-cy,dist=dx*dx+dy*dy;
   bool expected=dist<=((int64_t)outer+12)*(outer+12)&&!(inner>12&&dist<((int64_t)inner-12)*(inner-12));
   bool actual=false;for(unsigned s=0;s<spans;s++)actual|=x>=first[s]&&x<=last[s];assert(actual==expected);++checked;kept+=actual;
  }
 }
 unsigned tangent_rows=0;
 for(int cx=0;cx<16;cx++)for(int cy=0;cy<16;cy++)for(int outer=0;outer<=32;outer++)for(unsigned k=0;k<3;k++)for(int y=-3;y<=4;y++){
  int inner=k==0?0:k==1?outer:outer-1,first[2],last[2];unsigned spans=watch_circle_spans(cx,cy,outer,inner,y,-8,8,first,last);
  for(int x=-8;x<=8;x++){
   int64_t dx=(int64_t)x*16+8-cx,dy=(int64_t)y*16+8-cy,dist=dx*dx+dy*dy;
   bool expected=dist<=((int64_t)outer+12)*(outer+12)&&!(inner>12&&dist<((int64_t)inner-12)*(inner-12));
   bool actual=false;for(unsigned z=0;z<spans;z++)actual|=x>=first[z]&&x<=last[z];assert(actual==expected);
  }
  ++tangent_rows;
 }
 printf("Annulus tangencies:%u exhaustive fractional-center/tiny-radius/strict-inner rows PASS\n",tangent_rows);
 for(uint64_t x=0;x<100000;x++){uint64_t r=watch_circle_root(x);assert(r*r<=x&&(r+1)*(r+1)>x);}
 const uint64_t cases[]={UINT32_MAX,UINT64_C(4294967296),UINT64_C(1000000000000),UINT64_MAX};
 for(unsigned k=0;k<4;k++){uint64_t x=cases[k],r=watch_circle_root(x);assert(r*r<=x);if(r<UINT32_MAX)assert((r+1)*(r+1)>x);}
 printf("Annulus row spans:40000 generalized rows, %llu exact center classifications, %llu kept; full-range integer roots PASS\n",(unsigned long long)checked,(unsigned long long)kept);return 0;
}
