#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../apps/clock/nova/nova.c"
int main(void){static uint16_t a[480*480],b[480*480],src[280*280];uint32_t columns[280];unsigned cases=0;
 for(unsigned i=0;i<280*280;i++)src[i]=(uint16_t)(i*137u+41u);
 for(unsigned n=0;n<600;n++){unsigned w=n%2?466:240,h=n%3?466:240,stride=w+9,size=1+n*19%278;int x=(int)(n*97%(w+size))-(int)size,y=(int)(n*53%(h+size))-(int)size;unsigned radius=n%(size/2+1),border=1+n%7;
 for(unsigned i=0;i<480*480;i++)a[i]=b[i]=(uint16_t)(i*13u+n);
 canvas ca={(uint8_t*)a,stride*2,h,w},cb={(uint8_t*)b,stride*2,h,w};
 picker_composite_reference(&ca,src,size,x,y,radius,border,n%256,0x34f1,n&1);
 picker_composite_planned(&cb,src,size,x,y,radius,border,n%256,0x34f1,n&1,n%13?columns:NULL,280);
 assert(!memcmp(a,b,sizeof(a)));cases++;}
 printf("PASS %u non-context full-buffer/guard/scaled-source/fallback cases\n",cases);}
