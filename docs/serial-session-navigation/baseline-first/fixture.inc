/* Production Home, touch recognizer and renderer, real Files lease/parser.
 * Launch scheduling and peripherals are explicit modeled boundaries. */
extern void serial_probe_init(int,int,int);
extern int serial_probe_checkpoint(uint32_t);
extern unsigned serial_probe_requests(void);
extern uint32_t serial_probe_expired_at(void),serial_probe_disconnected_at(void);
extern int serial_probe_exit_safe(void),serial_probe_pending(void);
extern void serial_probe_path(uint32_t,int);
static const char *nav_case;
static bool nav_baseline,nav_done,nav_path,nav_none;
static bool nav_pending,nav_undrained,nav_disconnect;
static uint32_t nav_at,nav_safe_at,nav_fresh_at;
static unsigned nav_checkpoints,nav_busy,nav_frames,nav_polls,nav_feedback,nav_launches,nav_busy_launches,nav_closes;
static bool nav_safe=true;
static bool nav_health(risc_runtime_health_v1*h){check_io();h->uptime_ms=clock_ms;return !nav_done&&clock_ms<70000;}
static int32_t nav_checkpoint(void){
 check_io();assert(first_seen&&!held&&!pending&&!owned&&last_complete==submitted);++nav_checkpoints;
 nav_safe=serial_probe_checkpoint(clock_ms);
 if(!nav_safe)++nav_busy;
 else if(nav_busy&&!nav_safe_at)nav_safe_at=clock_ms;
 return nav_safe?RISC_RUNTIME_CHECKPOINT_OK:RISC_RUNTIME_CHECKPOINT_BUSY;
}
static bool nav_key(void*c,uint32_t*out){check_io();(void)c;*out=0;return true;}
static bool nav_snapshot(void*c,risc_touch_snapshot_v1*out){
 check_io();(void)c;*out=(risc_touch_snapshot_v1){.width=TEST_WIDTH,.height=TEST_HEIGHT};
 if(!ready||clock_ms-ready_time<6000)return true;
 if(!nav_at){nav_at=clock_ms;nav_polls=polls;serial_probe_path(clock_ms,nav_pending?1:nav_undrained?2:0);nav_path=true;}
 uint32_t age=clock_ms-nav_at;
 bool down=true;unsigned x=age<120?120:60;
 if(nav_safe_at&&clock_ms-nav_safe_at>=600){down=false;if(clock_ms-nav_safe_at>=1200){if(!nav_fresh_at)nav_fresh_at=clock_ms;down=true;x=clock_ms-nav_fresh_at<120?120:60;}}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){1,0,x*TEST_WIDTH/240,TEST_HEIGHT/2};}
 return true;
}
static bool nav_unsubscribe(void*c,uint64_t h){if(nav_at&&!serial_probe_exit_safe())++nav_closes;return unsubscribe(c,h);}
static bool nav_launch(const char*path){
 check_io();assert(!held&&!owned&&!pending&&!touch.subscription&&!strcmp(path,"springboard.elf"));++nav_launches;
 if(!serial_probe_exit_safe()){++nav_busy_launches;if(!nav_baseline)return false;}
 ++launches;nav_done=true;return true;
}
static bool nav_status(void*c,uint64_t token,risc_display_present_status_v1*out){
 bool ok=fast_status(c,token,out);
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE&&nav_at&&!serial_probe_exit_safe()){assert(hardware_brightness);++nav_frames;}
 return ok;
}
static bool nav_diagnostic(const char*s){
 if(strstr(s,"navigation=busy"))++nav_feedback;
 return fast_diagnostic(s);
}
int main(int argc,char**argv){
 assert(argc==3);nav_case=argv[1];nav_baseline=!strcmp(argv[2],"baseline");
 nav_pending=!strcmp(nav_case,"swipe-pending");nav_undrained=!strcmp(nav_case,"swipe-undrained");nav_disconnect=!strcmp(nav_case,"swipe-disconnect");nav_none=!strcmp(nav_case,"swipe-no-session");
 assert(nav_pending||nav_undrained||nav_disconnect||nav_none||!strcmp(nav_case,"swipe-active"));
 reset_case();probe_long=true;fixture_provision_mode=2;saved_radio(0);
 const uint8_t idle_record[]={0x54,1,0,0,0xa5};memcpy(low_bytes[1],idle_record,5);low_lengths[1]=5;
 serial_probe_init(!nav_none,0,nav_disconnect);
 fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.health=nav_health;fast_runtime.diagnostic=nav_diagnostic;
 fast_runtime.service_checkpoint=nav_checkpoint;fast_runtime.request_launch=nav_launch;
 pa.key_events=nav_key;ta.snapshot=nav_snapshot;ta.unsubscribe=nav_unsubscribe;da.base.present_status=nav_status;
 app_main();check_clean();assert(first_seen&&ready&&!retains&&hardware_brightness>0&&grants==ungrants&&subscribed==unsubscribed&&!allocations);
 assert(nav_at&&nav_path&&nav_done&&launches==1&&!sleeps);
 if(nav_none){assert(!nav_busy_launches&&!nav_busy&&!nav_closes&&!nav_feedback);}
 else if(nav_baseline){assert(nav_busy_launches==1&&nav_closes==1&&!nav_feedback&&!nav_fresh_at);}
 else {assert(!nav_busy_launches&&!nav_closes&&nav_launches==1&&nav_feedback==1&&nav_fresh_at&&nav_safe_at);assert(nav_frames>20&&polls>nav_polls+100&&nav_checkpoints>20&&!launcher_swipe_pending);}
 printf("HOME_SERIAL_NAV %ux%u variant=%s case=%s requests=%u checkpoints=%u busy=%u visible_busy_frames=%u input_polls=%u busy_closes=%u launch_calls=%u busy_launches=%u launches=%u feedback=%u gesture_ms=%u safe_ms=%u fresh_gesture_ms=%u expiry_ms=%u disconnect_ms=%u PASS\n",TEST_WIDTH,TEST_HEIGHT,argv[2],nav_case,serial_probe_requests(),nav_checkpoints,nav_busy,nav_frames,polls,nav_closes,nav_launches,nav_busy_launches,launches,nav_feedback,nav_at,nav_safe_at,nav_fresh_at,serial_probe_expired_at(),serial_probe_disconnected_at());
 return 0;
}
