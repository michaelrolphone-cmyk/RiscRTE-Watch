#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {uint32_t width;}canvas;
#include "native_q_under_test.inc"
static uint32_t random_state=1234567;
static uint32_t next(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
static unsigned long long checked,fast,wide;
static void check(uint32_t width,int value){canvas c={width};int64_t p=(int64_t)value*width;
 assert(native_q(&c,value)==(int)(p/240));++checked;if(p>=INT32_MIN&&p<=INT32_MAX)++fast;else ++wide;}
int main(void){
 const uint32_t widths[]={0,1,17,120,240,300,328,466,640,1024,2048,65535,UINT32_MAX};
 for(unsigned i=0;i<sizeof(widths)/sizeof(*widths);i++){
  uint32_t w=widths[i];for(int x=-32768;x<=32768;x++)check(w,x);
  const int values[]={INT32_MIN,INT32_MIN+1,-240,-239,-1,0,1,239,240,INT32_MAX-1,INT32_MAX};
  for(unsigned j=0;j<sizeof(values)/sizeof(*values);j++)check(w,values[j]);
  if(w){int64_t edges[]={INT32_MIN/(int64_t)w,INT32_MAX/(int64_t)w};for(unsigned e=0;e<2;e++)for(int d=-3;d<=3;d++)if(edges[e]+d>=INT32_MIN&&edges[e]+d<=INT32_MAX)check(w,(int)(edges[e]+d));}
 }
 for(unsigned i=0;i<1000000;i++)check(next(),(int32_t)next());
 assert(fast&&wide);printf("native_q exact: %llu cases, fast=%llu wide=%llu PASS\n",checked,fast,wide);
}
