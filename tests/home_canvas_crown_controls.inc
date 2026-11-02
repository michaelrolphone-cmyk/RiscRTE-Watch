/* Controlled services around the real production crown and Canvas provider. */
#include "canvas_provider.h"
static canvas_provider fixture_canvas;
static risc_canvas_api_v1 fixture_canvas_api;
static risc_display_output_api_v1 fixture_physical;
static risc_touch_api_v1 fixture_touch;
static const char *canvas_case;
static unsigned physical_format,physical_width,physical_height,physical_stride;
static unsigned char *physical_pixels;
static unsigned canvas_grants,canvas_calls,canvas_submits,converted_frames,canvas_rotation,raw_touch_acquires;
static void hc_yield(uint32_t milliseconds){check_io();yield(milliseconds);}
static bool canvas_faulted,canvas_missing_touch;
static unsigned navigation_phase,navigation_at;static bool navigation_release,canvas_controls_emitted;
static int32_t hc_touch_next(void*c,uint64_t id,risc_touch_event_v1*out){
 check_io();
 if(!strcmp(canvas_case,"controls-button")&&first_seen&&clock_ms>=300&&!canvas_controls_emitted){
  canvas_controls_emitted=true;*out=(risc_touch_event_v1){.sequence=1,.kind=RISC_TOUCH_EVENT_BUTTON_DOWN,.id=0};return 1;
 }
 return fast_touch_next(c,id,out);
}
static bool hc_snapshot(void*c,risc_touch_snapshot_v1*out){
 check_io();bool ok=fast_snapshot(c,out);if(canvas_controls_emitted)out->sequence=1;
 if(!strcmp(canvas_case,"multitouch")&&first_seen&&clock_ms>=300&&clock_ms<1800){
  out->sequence=2;out->contact_count=2;
  out->contacts[0]=(risc_touch_contact_v1){.id=10,.x=TEST_WIDTH/2,.y=TEST_HEIGHT/2};
  out->contacts[1]=(risc_touch_contact_v1){.id=11,.x=TEST_WIDTH/2+1,.y=TEST_HEIGHT/2+1};
 }else if(!strcmp(canvas_case,"multitouch")&&first_seen&&clock_ms>=1800)out->sequence=3;
 return ok;
}
static bool hc_touch_poll(void*c,size_t n){check_io();assert(n&&n<=RISC_TOUCH_QUEUE_LENGTH);return poll(c,1);}
static bool hc_navigation_foreground(void*c,const risc_input_foreground_v1*claims,size_t count){check_io();(void)c;(void)claims;assert(count<=1);return true;}
static bool hc_navigation_reset(void*c){check_io();(void)c;return true;}
static bool hc_navigation_poll(void*c,risc_input_navigation_frame_v1*out){
 check_io();(void)c;*out=(risc_input_navigation_frame_v1){0};
 if(strncmp(canvas_case,"nav-",4)||!first_seen)return true;
 if(navigation_release){navigation_release=false;return true;}
 if(clock_ms-navigation_at<300&&strcmp(canvas_case,"nav-repeat"))return true;
 if(!strcmp(canvas_case,"nav-repeat")&&(!navigation_phase?clock_ms<300:!picker.open))return true;
 const uint32_t home[]={RISC_NAV_HOME};
 const uint32_t picker_keys[]={RISC_NAV_CONFIRM,RISC_NAV_RIGHT,RISC_NAV_CONFIRM,RISC_NAV_BACK};
 const uint32_t repeat_keys[]={RISC_NAV_CONFIRM,RISC_NAV_RIGHT,RISC_NAV_RIGHT};
 const uint32_t quick_keys[]={RISC_NAV_UP,RISC_NAV_DOWN,RISC_NAV_RIGHT,RISC_NAV_CONFIRM,RISC_NAV_BACK};
 const uint32_t *keys=!strcmp(canvas_case,"nav-home")?home:!strcmp(canvas_case,"nav-picker")?picker_keys:!strcmp(canvas_case,"nav-repeat")?repeat_keys:quick_keys;
 unsigned count=!strcmp(canvas_case,"nav-home")?1:!strcmp(canvas_case,"nav-picker")?4:!strcmp(canvas_case,"nav-repeat")?3:5;
 if(navigation_phase>=count){if(strcmp(canvas_case,"nav-repeat")||picker.target==2)ended=true;return true;}
 out->buttons=out->pressed=keys[navigation_phase++];navigation_at=clock_ms;navigation_release=true;return true;
}
static const risc_input_navigation_api_v1 hc_navigation={1,sizeof(hc_navigation),NULL,hc_navigation_poll,hc_navigation_foreground,hc_navigation_reset};
static bool hc_diagnostic(const char*s){check_io();bool ok=fast_diagnostic(s);if(!strncmp(canvas_case,"nav-",4)&&strstr(s,"stage=services-ready"))ended=false;return ok;}
static bool canvas_fault(const char *operation){
 check_io();++canvas_calls;
 if(!strcmp(canvas_case,operation)){canvas_faulted=true;return true;}
 return false;
}
static bool physical_info(void*c,risc_display_info_v1*out){
 check_io();(void)c;*out=(risc_display_info_v1){.width=physical_width,.height=physical_height,
  .supported_formats=RISC_DISPLAY_FORMAT_BIT(physical_format)};return true;
}
static bool physical_acquire(void*c,uint32_t format,risc_display_surface_v1*out){
 check_io();(void)c;assert(format==physical_format&&!owned&&!pending);owned=true;
 *out=(risc_display_surface_v1){1,physical_pixels,physical_width,physical_height,physical_stride,
 (uint32_t)((size_t)physical_stride*physical_height),physical_format};return true;
}
static bool physical_submit(void*c,uint64_t id,const risc_display_rect_v1*damage,size_t n,
 const risc_display_present_options_v1*options,uint64_t*out){
 check_io();assert(id==1);canvas_mapping *m=&fixture_canvas.frames.mapping;
 for(unsigned y=0;y<TEST_HEIGHT;y++)for(unsigned x=0;x<TEST_WIDTH;x++){
  unsigned px,py;canvas_mapping_physical(m,x,y,&px,&py);
  uint16_t expected=pixels[y*TEST_WIDTH+x];unsigned char *row=physical_pixels+(size_t)py*physical_stride;
  if(physical_format==RISC_DISPLAY_FORMAT_RGB565)assert((uint16_t)(row[px*2]|(uint16_t)row[px*2+1]<<8)==expected);
  else assert(!!(row[px/8]&(0x80u>>(px&7)))==!canvas_rgb565_white(expected,px,py));
 }
 ++converted_frames;return submit_frame(c,id,damage,n,options,out);
}
static int32_t hc_open(void*c,const risc_canvas_open_v1*r,uint64_t*s){
 if(canvas_fault("open-retained"))return RISC_CANVAS_RETAINED;
 if(canvas_fault("open-unknown"))return -42;
 return canvas_provider_open(c,r,s);
}
static int32_t hc_geometry(void*c,uint64_t s,risc_canvas_geometry_v1*g){
 if(canvas_fault("geometry-retained"))return RISC_CANVAS_RETAINED;
 return canvas_provider_geometry(c,s,g);
}
static int32_t hc_begin(void*c,uint64_t s,risc_canvas_frame_v1*f){
 if(canvas_fault("begin-retained"))return RISC_CANVAS_RETAINED;
 if(canvas_fault("begin-unknown"))return -42;
 int32_t rc=canvas_provider_begin(c,s,f);
 if(!strcmp(canvas_case,"malformed-frame")&&rc==0){f->geometry_revision=0;canvas_faulted=true;}
 return rc;
}
static int32_t hc_submit(void*c,uint64_t s,uint64_t lease,uint64_t*out){
 if(canvas_fault("submit-retained"))return RISC_CANVAS_RETAINED;
 assert(fixture_canvas.frames.bytes==sizeof(pixels));memcpy(pixels,fixture_canvas.frames.writable,sizeof(pixels));
 ++canvas_submits;return canvas_provider_submit(c,s,lease,out);
}
static int32_t hc_cancel(void*c,uint64_t s,uint64_t lease){check_io();return canvas_provider_cancel(c,s,lease);}
static int32_t hc_poll(void*c,uint64_t s,uint64_t receipt,risc_canvas_receipt_v1*out){
 if(canvas_fault("poll-retained"))return RISC_CANVAS_RETAINED;
 if(canvas_fault("poll-unknown"))return -42;
 return canvas_provider_poll(c,s,receipt,out);
}
static int32_t hc_forget(void*c,uint64_t s,uint64_t receipt){check_io();return canvas_provider_forget(c,s,receipt);}
static int32_t hc_next(void*c,uint64_t s,risc_canvas_event_v1*out){
 if(canvas_fault("input-retained")&&first_seen)return RISC_CANVAS_RETAINED;
 return canvas_provider_next(c,s,out);
}
static int32_t hc_contacts(void*c,uint64_t s,risc_canvas_event_v1*out,size_t cap,size_t*n,uint64_t*sequence){
 check_io();return canvas_provider_contacts(c,s,out,cap,n,sequence);
}
static int32_t hc_quiesce(void*c,uint64_t s){
 if(canvas_fault("quiesce-retained"))return RISC_CANVAS_RETAINED;
 return canvas_provider_quiesce(c,s);
}
static int32_t hc_resume(void*c,uint64_t s){check_io();return canvas_provider_resume(c,s);}
static int32_t hc_close(void*c,uint64_t s){
 if(canvas_fault("close-retained"))return RISC_CANVAS_RETAINED;
 return canvas_provider_close(c,s);
}
static const struct {uint32_t version,size;} hc_malformed={2,8};
static bool hc_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 check_io();
 if(!strcmp(name,"ui.canvas")){
  assert(version==1&&instance==0);
  if(!strcmp(canvas_case,"absent"))return false;
  g->api=!strcmp(canvas_case,"malformed-api")?(const void*)&hc_malformed:(const void*)&fixture_canvas_api;
  ++grants;++canvas_grants;return true;
 }
 if(!strcmp(name,"time.civil"))return false;
 if(!strcmp(name,"input.touch.raw")){++raw_touch_acquires;if(canvas_missing_touch)return false;}
 return fast_acquire(name,version,instance,g);
}
static void hc_reset(unsigned format,unsigned rotation){
 reset_case();canvas_rotation=rotation;physical_format=format;raw_touch_acquires=canvas_calls=canvas_grants=canvas_submits=converted_frames=0;canvas_faulted=false;
 physical_width=rotation%2?TEST_HEIGHT:TEST_WIDTH;physical_height=rotation%2?TEST_WIDTH:TEST_HEIGHT;
 physical_stride=(format==RISC_DISPLAY_FORMAT_RGB565?physical_width*2:(physical_width+7)/8)+4;
 physical_pixels=calloc(physical_height,physical_stride);assert(physical_pixels);
 fixture_physical=da.base;fixture_physical.get_info=physical_info;fixture_physical.acquire=physical_acquire;fixture_physical.submit=physical_submit;
 fixture_touch=ta;fixture_touch.poll=hc_touch_poll;fixture_touch.next=hc_touch_next;fixture_touch.snapshot=hc_snapshot;
 navigation_phase=navigation_at=0;navigation_release=canvas_controls_emitted=false;
 canvas_provider_init(&fixture_canvas,&fixture_physical,&fixture_touch,rotation);
 fixture_canvas.navigation=&hc_navigation;
 fixture_canvas_api=fixture_canvas.api;
 fixture_canvas_api.open=hc_open;fixture_canvas_api.geometry=hc_geometry;fixture_canvas_api.begin_frame=hc_begin;
 fixture_canvas_api.submit=hc_submit;fixture_canvas_api.cancel_frame=hc_cancel;fixture_canvas_api.poll=hc_poll;
 fixture_canvas_api.forget_receipt=hc_forget;fixture_canvas_api.next=hc_next;fixture_canvas_api.contacts=hc_contacts;
 fixture_canvas_api.quiesce=hc_quiesce;fixture_canvas_api.resume=hc_resume;fixture_canvas_api.close=hc_close;
 fast_runtime.acquire=hc_acquire;fast_runtime.diagnostic=hc_diagnostic;fast_runtime.yield_ms=hc_yield;
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"face-count")){printf("%u\n",(unsigned)WATCH_FACE_COUNT);return 0;}
 assert(argc==4);canvas_case=argv[1];unsigned format=(unsigned)strtoul(argv[2],NULL,10),rotation=(unsigned)strtoul(argv[3],NULL,10);
 hc_reset(format,rotation);
 canvas_missing_touch=!strcmp(canvas_case,"no-raw-touch");
 if(!strcmp(canvas_case,"cold"))initial_default_entry=true;
 if(!strcmp(canvas_case,"deep"))boot_cause=RISC_BOOT_DEEP_GPIO;
 if(!strcmp(canvas_case,"sleep")||!strcmp(canvas_case,"quiesce-retained"))requested_wakes=1;
 if(!strcmp(canvas_case,"saved")){scenario=17;saved_face=true;memcpy(saved_blob,(uint8_t[]){0x46,1,1,1^0xa5},4);quick_saved[0]=true;quick_values[0]=60;}
 if(!strncmp(canvas_case,"face-",5)){unsigned selected=(unsigned)strtoul(canvas_case+5,NULL,10);assert(selected<WATCH_FACE_COUNT);saved_face=true;memcpy(saved_blob,(uint8_t[]){0x46,1,(uint8_t)selected,(uint8_t)(selected^0xa5)},4);}
 if(!strncmp(canvas_case,"alarm-",6)){alarm_touch_test=true;alarm_touch_wrong_y=!strcmp(canvas_case,"alarm-retry")?watch_home_alarm_retry_at(TEST_WIDTH,TEST_HEIGHT):watch_home_alarm_dismiss_at(TEST_WIDTH,TEST_HEIGHT)-1u;alarm_view.state=ALARM_STATE_ALERT;alarm_view.occurrence.generation=1;}
 if(!strcmp(canvas_case,"physical-retained"))fail_display=true;
 app_main();
 bool retained=strstr(canvas_case,"retained")||strstr(canvas_case,"unknown")||!strcmp(canvas_case,"malformed-frame");
 if(retained){assert(retained_fence&&retains&&native_clock.halted);assert(canvas_faulted||!strcmp(canvas_case,"physical-retained"));}
 else {
  check_clean();
  if(!strcmp(canvas_case,"malformed-api")){assert(!first_seen&&!canvas_submits&&!confirmations);free(physical_pixels);puts("Malformed optional Canvas safely refuses startup PASS");return 0;}
  assert(first_seen&&ready);
  if(!strncmp(canvas_case,"nav-",4)){assert(navigation_phase);if(!strcmp(canvas_case,"nav-home"))assert(launches==1);if(!strcmp(canvas_case,"nav-repeat"))assert(navigation_phase==3&&picker.target==2);}
  else assert(confirmations==1+wake_count);
  if(!strcmp(canvas_case,"absent"))assert(!canvas_submits);
  else assert(canvas_submits&&converted_frames==canvas_submits&&!raw_touch_acquires&&!fixture_canvas.frames.session&&!fixture_canvas.frames.writable&&!fixture_canvas.frames.receipts);
  if(!strcmp(canvas_case,"cold"))assert(first_frames>10&&first_at>2000);
  if(!strcmp(canvas_case,"deep"))assert(first_frames==1&&first_at==95);
  if(!strcmp(canvas_case,"sleep"))assert(sleeps==1&&wake_count==1&&soft_frames==1);
  if(!strcmp(canvas_case,"multitouch"))assert(!launches&&!face_writes&&!quick_writes&&!picker.open&&!pqa_visible(&clock_quick.ui));
  if(!strcmp(canvas_case,"controls-button"))assert(canvas_controls_emitted&&pqa_visible(&clock_quick.ui));
  if(!strncmp(canvas_case,"alarm-",6))assert(alarm_touch_phase==4&&!alarm_view.occurrence.generation&&alarm_frames);
  if(!strncmp(canvas_case,"face-",5))assert(picker.selected==(unsigned)strtoul(canvas_case+5,NULL,10));
  if(!strcmp(canvas_case,"saved"))assert(picker.selected==1&&face.hour_24&&hardware_brightness==60);
  free(physical_pixels);
 }
 printf("Actual crown Canvas %s format=%u rotation=%u frames=%u: PASS\n",canvas_case,format,rotation,converted_frames);
 return 0;
}
