/* T5S3 NativeVideoBootScrub command sequence, mapped to RGB565.
 * Caller reconstructs the outgoing frame before applying each scan.
 * Retain leaves it intact; ends with every pixel black. */
#include "effects.h"
#include "ripple-map.h"
bool watch_ripple_render(const risc_display_surface_v1 *s,uint32_t scan) {
 if(!s || !s->pixels || !s->width || !s->height || s->width>2048 || s->height>2048 ||
    ((uintptr_t)s->pixels&1u) || (s->stride_bytes&1u) || s->stride_bytes<s->width*2u ||
    s->stride_bytes>SIZE_MAX/s->height ||
    s->size_bytes<(size_t)s->stride_bytes*(s->height-1u)+s->width*2u ||
    s->pixel_format!=RISC_DISPLAY_FORMAT_RGB565) return false;
 unsigned side=s->width<s->height?s->width:s->height;
 unsigned top=(s->height-side)/2u,left=(s->width-side)/2u;
 /* Sample the pinned arrival-time mask, not a completed frame. Apply each
  * command to the native raster, including the surrounding content area. */
 for(unsigned y=0;y<s->height;y++)for(unsigned x=0;x<s->width;x++){
  unsigned yy=y<top?0:y-top;if(yy>=side)yy=side-1u;
  unsigned xx=x<left?0:x-left;if(xx>=side)xx=side-1u;
  unsigned i=(yy*240u/side)*240u+xx*240u/side;
  uint16_t *p=(uint16_t*)((uint8_t*)s->pixels+(size_t)y*s->stride_bytes)+x;
  unsigned arrival=(arrivals[i/2]>>((i&1)*4))&15;
  if(scan>=arrival)*p=scan<arrival+3u?0xffff:0;
 }
 return true;
}
