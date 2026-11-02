/* Optional logical presentation/input. No physical display classification. */
#ifdef WATCH_HOME_CANVAS
#include "HomeCanvasSurface.h"
#include "HomeCanvasTouch.h"
static home_canvas_surface clock_canvas;
static home_canvas_touch clock_canvas_touch;
static risc_runtime_capability_v1 clock_canvas_grant;
static bool clock_canvas_selected;
static bool clock_canvas_live(void){return !clock_canvas.retained;}
static void clock_canvas_retained(void*context){
 (void)context;
#ifdef WATCH_RUNTIME_FEATURES
 watch_runtime_halt(&native_clock);
#else
 if(rt->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE&&rt->retain_invocation)(void)rt->retain_invocation();
#endif
}
static void clock_canvas_reset(void){
 clock_canvas=(home_canvas_surface){0};
 clock_canvas_touch=(home_canvas_touch){0};
 clock_canvas_grant=(risc_runtime_capability_v1){.struct_size=sizeof(clock_canvas_grant)};
 clock_canvas_selected=false;
}
static bool clock_canvas_open(const risc_display_output_api_v1*physical){
 if(!rt->acquire(RISC_CANVAS_CAPABILITY,1,0,&clock_canvas_grant))return true;
 const risc_canvas_api_v1*api=clock_canvas_grant.api;
 api=api?risc_canvas_get_v1(api,api->struct_size):NULL;
 if(!api)return false; /* Malformed advertised API is not capability absence. */
 if(!home_canvas_open_owned(&clock_canvas,api,physical->set_brightness,physical->context,
       clock_canvas_retained,NULL)){
  if(clock_canvas.retained)return false;
  /* Only no-session rejection can use the original raw fallback. A partially
   * opened session must be closed first and must not hide a provider failure. */
  if(clock_canvas.session||(clock_canvas.last_result!=RISC_CANVAS_UNSUPPORTED&&clock_canvas.last_result!=RISC_CANVAS_UNAVAILABLE))return false;
  if(!rt->release(&clock_canvas_grant)){home_canvas_retain(&clock_canvas);
 return false;}
  clock_canvas_grant=(risc_runtime_capability_v1){.struct_size=sizeof(clock_canvas_grant)};
  clock_canvas=(home_canvas_surface){0};
 return true;
 }
 clock_canvas_selected=true;
 home_canvas_touch_init(&clock_canvas_touch,&clock_canvas);
 return true;
}
static bool clock_canvas_pause(void){
 if(!clock_canvas_selected)return clock_canvas_live();
 if(home_canvas_quiesce(&clock_canvas))return true;
 home_canvas_retain(&clock_canvas);
 return false;
}
static bool clock_canvas_wake(void){
 if(!clock_canvas_selected)return clock_canvas_live();
 if(home_canvas_resume(&clock_canvas))return true;
 home_canvas_retain(&clock_canvas);
 return false;
}
static bool clock_canvas_close(void){
 if(!clock_canvas_live())return false;
 if(clock_canvas.session&&!home_canvas_close(&clock_canvas)){home_canvas_retain(&clock_canvas);
 return false;}
 clock_canvas.on_retained=clock_canvas_retained;
 if(clock_canvas_grant.api&&!rt->release(&clock_canvas_grant)){home_canvas_retain(&clock_canvas);
 return false;}
 clock_canvas_grant=(risc_runtime_capability_v1){0};
 clock_canvas_selected=false;
 return true;
}
#else
static bool clock_canvas_live(void){return true;}
static bool clock_canvas_pause(void){return true;}
static bool clock_canvas_wake(void){return true;}
#endif
