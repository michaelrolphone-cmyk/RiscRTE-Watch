#include "apps/clock/nova/nova.h"
#include "apps/clock/crash_text.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static unsigned char pixels[240*502*2+64];
int main(void){
 const char text[]="CRASH / RESTART\nRecord: boot 7 / sequence 11\n0123456789012345678901234567890123456789\nRAW PC / STACK\n#0 PC 42012345 SP 3fca0000\nEvery character: !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\n";
 for(unsigned h=240;h<=294;h+=54){
  memset(pixels,0xa5,sizeof(pixels));risc_display_surface_v1 s={.pixels=pixels+32,.width=240,.height=h,.stride_bytes=480,.size_bytes=480*h,.pixel_format=RISC_DISPLAY_FORMAT_RGB565};
  uint32_t at=0,next=0;do{assert(nova_watch_crash_report(&s,text,sizeof(text)-1,at,&next));assert(next==crash_text_page(text,sizeof(text)-1,at,(h-48)/16));assert(next>at);at=next;}while(at<sizeof(text)-1);
  for(unsigned i=0;i<32;++i){assert(pixels[i]==0xa5);assert(pixels[32+480*h+i]==0xa5);}
 }
 /* Wrap consumes all content, preserving case, punctuation and spaces. */
 uint32_t at=0;char line[29];
 while(at<sizeof(text)-1){uint32_t before=at;at=crash_text_line(text,sizeof(text)-1,at,line);size_t n=strlen(line);assert(!memcmp(line,text+before,n));assert(at==before+n || (at==before+n+1 && text[at-1]=='\n'));}
 puts("Crash renderer PASS: exact report bytes, hard-wrap parity, 240/294 height and framebuffer guards");return 0;
}
