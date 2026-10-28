#pragma once
#include <stdint.h>
/* Exact integer center bounds for the existing Q4 circle rejection predicate.
 * These skip only centers that the old outer/inner test rejects. The original
 * four coverage samples, blending and pixel callback order remain authoritative.
 */
static uint32_t watch_circle_root32(uint32_t n) {
 uint32_t result=0,bit=UINT32_C(1)<<30;
 while(bit>n)bit>>=2;
 while(bit){if(n>=result+bit){n-=result+bit;result=(result>>1)+bit;}else result>>=1;bit>>=2;}
 return result;
}
static uint64_t watch_circle_root(uint64_t n) {
 if(n<=UINT32_MAX)return watch_circle_root32((uint32_t)n);
 uint64_t result=0,bit=UINT64_C(1)<<62;
 while(bit>n)bit>>=2;
 while(bit){if(n>=result+bit){n-=result+bit;result=(result>>1)+bit;}else result>>=1;bit>>=2;}
 return result;
}
static int64_t watch_circle_floor16(int64_t n){return n>=0?n/16:-((-n+15)/16);}
static int64_t watch_circle_ceil16(int64_t n){return n>=0?(n+15)/16:-((-n)/16);}
static unsigned watch_circle_spans(int cx,int cy,int outer,int inner,int y,int minx,int maxx,int first[2],int last[2]) {
 int64_t dy=(int64_t)y*16+8-cy,r=(int64_t)outer+12;
 int64_t limit=r*r-dy*dy;
 if(limit<0||minx>maxx)return 0;
 int64_t bound=(int64_t)watch_circle_root((uint64_t)limit),center=(int64_t)cx-8;
 int64_t lo=watch_circle_ceil16(center-bound),hi=watch_circle_floor16(center+bound);
 if(lo<minx)lo=minx;
 if(hi>maxx)hi=maxx;
 if(lo>hi)return 0;
 first[0]=(int)lo;last[0]=(int)hi;
 if(inner>12){
  r=(int64_t)inner-12;limit=r*r-dy*dy;
  if(limit>0){
   /* dist < inner^2: for integer centers, equality remains included. */
   bound=(int64_t)watch_circle_root((uint64_t)limit-1u);
   int64_t a=watch_circle_ceil16(center-bound),b=watch_circle_floor16(center+bound);
   if(a<lo)a=lo;
   if(b>hi)b=hi;
   if(a<=b){
    unsigned count=0;
    if(lo<a){first[count]=(int)lo;last[count++]=(int)a-1;}
    if(b<hi){first[count]=(int)b+1;last[count++]=(int)hi;}
    return count;
   }
  }
 }
 return 1;
}
