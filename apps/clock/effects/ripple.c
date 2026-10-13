/* T5S3 NativeVideoBootScrub command sequence, mapped to RGB565.
 * Caller reconstructs the outgoing frame before applying each scan.
 * Retain leaves it intact; ends with every pixel black. */
#include "effects.h"
#include "ripple-map.h"
bool watch_ripple_render(const risc_display_surface_v1 *s,uint32_t scan) {
 if(!s || !s->pixels || s->width!=240 || s->height<240 || s->height>2048 || (s->stride_bytes&1u) || s->stride_bytes<480 || s->stride_bytes>UINT32_MAX/s->height || s->size_bytes<s->stride_bytes*s->height || s->pixel_format!=5) return false;
 unsigned top=(s->height-240)/2;
 for(unsigned y=0;y<s->height;y++)for(unsigned x=0;x<240;x++){
  unsigned yy=y<top?0:y-top;if(yy>239)yy=239;
  unsigned i=yy*240+x;
  uint16_t *p=(uint16_t*)((uint8_t*)s->pixels+y*s->stride_bytes)+x;
  unsigned arrival=(arrivals[i/2]>>((i&1)*4))&15;
  if(scan>=arrival)*p=scan<arrival+3u?0xffff:0;
 }
 return true;
}
