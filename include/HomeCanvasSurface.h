#pragma once
/* Shared logical RGB565 raster adapter. Physical format, rotation, quantization
 * and refresh ownership stay in ui.canvas. This is not a device selector.
 * One frame and one receipt at a time, matching the Home renderer's cadence. */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "RiscCanvasV1.h"
#include "RiscDisplayOutputV1.h"
typedef struct {
 const risc_canvas_api_v1 *canvas;
 uint64_t session,lease,receipt;
 risc_canvas_geometry_v1 geometry;
 risc_display_output_api_v1 display;
 bool retained;
 bool (*brightness)(void*,uint16_t,uint16_t);
 void *brightness_context;
} home_canvas_surface;
static inline bool home_canvas_result(home_canvas_surface*s,int32_t rc){
 if(rc==RISC_CANVAS_RETAINED || rc<RISC_CANVAS_UNSUPPORTED || rc>RISC_CANVAS_AGAIN)s->retained=true;
 return rc==RISC_CANVAS_OK;
}
static inline bool home_canvas_geometry(home_canvas_surface*s){
 if(s->retained||!s->session)return false;
 risc_canvas_geometry_v1 g={.struct_size=sizeof(g)};
 if(!home_canvas_result(s,s->canvas->geometry(s->canvas->context,s->session,&g)))return false;
 if(g.struct_size<sizeof(g)||!g.revision||!g.width||!g.height||g.width>UINT16_MAX||g.height>UINT16_MAX){s->retained=true;return false;}
 s->geometry=g;return true;
}
static bool home_canvas_info(void*ctx,risc_display_info_v1*out){
 home_canvas_surface*s=ctx;if(!out||!home_canvas_geometry(s))return false;
 *out=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*out),
  .width=s->geometry.width,.height=s->geometry.height,
  .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),
  .preferred_format=RISC_DISPLAY_FORMAT_RGB565,
  .supported_rotations=RISC_DISPLAY_ROTATION_0,
  .flags=RISC_DISPLAY_INFO_ASYNC_PRESENT};
 return true;
}
static bool home_canvas_acquire(void*ctx,uint32_t format,risc_display_surface_v1*out){
 home_canvas_surface*s=ctx;
 if(!out||s->retained||!s->session||s->lease||s->receipt||format!=RISC_DISPLAY_FORMAT_RGB565)return false;
 if(!home_canvas_geometry(s))return false;
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};
 if(!home_canvas_result(s,s->canvas->begin_frame(s->canvas->context,s->session,&f)))return false;
 /* Malformed ownership cannot be released speculatively. */
 if(f.struct_size<sizeof(f)||!f.lease||!f.pixels||f.encoding!=RISC_CANVAS_RGB565_ADAPTIVE||
    f.geometry_revision!=s->geometry.revision||f.width!=s->geometry.width||f.height!=s->geometry.height||
    (f.stride&1u)||f.stride<(uint64_t)f.width*2||f.byte_count>UINT32_MAX||
    f.height>f.byte_count/f.stride||((uintptr_t)f.pixels&1u)){s->retained=true;return false;}
 s->lease=f.lease;
 *out=(risc_display_surface_v1){f.lease,f.pixels,f.width,f.height,f.stride,(uint32_t)f.byte_count,RISC_DISPLAY_FORMAT_RGB565};
 return true;
}
static void home_canvas_release(void*ctx,risc_display_frame_v1 lease){
 home_canvas_surface*s=ctx;if(s->retained||!lease||lease!=s->lease)return;
 if(home_canvas_result(s,s->canvas->cancel_frame(s->canvas->context,s->session,lease)))s->lease=0;
 else s->retained=true; /* void facade cannot report unproved cancellation */
}
static bool home_canvas_submit(void*ctx,risc_display_frame_v1 lease,const risc_display_rect_v1*damage,size_t count,
 const risc_display_present_options_v1*options,risc_display_present_token_v1*out){
 home_canvas_surface*s=ctx;
 if(!out||s->retained||!lease||lease!=s->lease||s->receipt||count>RISC_DISPLAY_MAX_DAMAGE_RECTS||(count&&!damage))return false;
 /* Do not silently discard an unsupported semantic presentation request. */
 if(options&&(options->intent!=RISC_DISPLAY_PRESENT_DEFAULT||options->queue_policy!=RISC_DISPLAY_QUEUE_FIFO||options->reserved))return false;
 for(size_t i=0;i<count;i++)if(damage[i].x<0||damage[i].y<0||!damage[i].width||!damage[i].height||
  (uint64_t)(uint32_t)damage[i].x+damage[i].width>s->geometry.width||
  (uint64_t)(uint32_t)damage[i].y+damage[i].height>s->geometry.height)return false;
 uint64_t receipt=0;
 if(!home_canvas_result(s,s->canvas->submit(s->canvas->context,s->session,lease,&receipt)))return false;
 if(!receipt){s->retained=true;return false;}
 s->lease=0;s->receipt=receipt;*out=receipt;return true;
}
static bool home_canvas_status(void*ctx,risc_display_present_token_v1 receipt,risc_display_present_status_v1*out){
 home_canvas_surface*s=ctx;if(!out||s->retained||!receipt||receipt!=s->receipt)return false;
 risc_canvas_receipt_v1 r={.struct_size=sizeof(r)};
 if(!home_canvas_result(s,s->canvas->poll(s->canvas->context,s->session,receipt,&r)))return false;
 if(r.struct_size<sizeof(r)||r.receipt!=receipt||r.state<RISC_CANVAS_QUEUED||r.state>RISC_CANVAS_FAILED){s->retained=true;return false;}
 if(r.state>=RISC_CANVAS_COMPLETE){
  if(!home_canvas_result(s,s->canvas->forget_receipt(s->canvas->context,s->session,receipt)))return false;
  s->receipt=0;
 }
 *out=(risc_display_present_status_v1){.state=(uint8_t)r.state};return true;
}
static bool home_canvas_wait(void*ctx,risc_display_present_token_v1 receipt,uint32_t timeout,risc_display_present_status_v1*out){
 (void)timeout;return home_canvas_status(ctx,receipt,out); /* bounded nonblocking poll */
}
static bool home_canvas_brightness(void*ctx,uint16_t value,uint16_t maximum){
 home_canvas_surface*s=ctx;return !s->retained&&s->brightness&&s->brightness(s->brightness_context,value,maximum);
}
static inline bool home_canvas_open(home_canvas_surface*s,const risc_canvas_api_v1*api,
 bool(*brightness)(void*,uint16_t,uint16_t),void*context){
 if(!s||!api)return false;
 *s=(home_canvas_surface){.canvas=api,.brightness=brightness,.brightness_context=context};
 risc_canvas_open_v1 request={sizeof(request),RISC_CANVAS_RGB565_ADAPTIVE,RISC_CANVAS_RESPONSIVE,0,0};
 if(!home_canvas_result(s,api->open(api->context,&request,&s->session)))return false;
 if(!s->session){s->retained=true;return false;}
 if(!home_canvas_geometry(s))return false;
 s->display=(risc_display_output_api_v1){1,sizeof(s->display),s,home_canvas_info,home_canvas_acquire,
  home_canvas_release,home_canvas_submit,home_canvas_status,home_canvas_wait,home_canvas_brightness};return true;
}
static inline bool home_canvas_quiesce(home_canvas_surface*s){
 return !s->retained&&!s->lease&&!s->receipt&&home_canvas_result(s,s->canvas->quiesce(s->canvas->context,s->session));
}
static inline bool home_canvas_resume(home_canvas_surface*s){
 return !s->retained&&home_canvas_result(s,s->canvas->resume(s->canvas->context,s->session))&&home_canvas_geometry(s);
}
static inline bool home_canvas_close(home_canvas_surface*s){
 if(s->retained||s->lease||s->receipt)return false;
 if(s->session&&!home_canvas_result(s,s->canvas->close(s->canvas->context,s->session)))return false;
 *s=(home_canvas_surface){0};return true;
}
