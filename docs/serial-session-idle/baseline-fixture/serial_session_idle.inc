/* Actual Home caller and renderer plus actual RiscSerial::Files. Runtime's
 * service/sleep API adapter and all peripherals are modeled host boundaries. */
extern void serial_probe_init(int,int,int);
extern int serial_probe_checkpoint(uint32_t);
extern unsigned serial_probe_requests(void);
extern uint32_t serial_probe_last_request(void),serial_probe_expired_at(void),serial_probe_disconnected_at(void),serial_probe_timeout(void);
static const char *probe_case;
static bool probe_baseline,probe_old_signal,probe_manual,probe_key_sent,probe_done;
static bool probe_session,probe_renew,probe_disconnect;
static unsigned probe_checkpoints,probe_busy,probe_begins,probe_ends,probe_refusals,probe_blanks;
static unsigned probe_blanks_while_busy,probe_frames_while_busy,probe_polls_before_busy;
static uint32_t probe_first_begin,probe_first_sleep,probe_safe_at;
static bool probe_serial_safe=true;
static bool probe_health(risc_runtime_health_v1*h){check_io();h->uptime_ms=clock_ms;return !probe_done&&clock_ms<(probe_manual?12000u:70000u);}
static int32_t probe_checkpoint(void){
 check_io();assert(first_seen&&!held&&!pending&&!owned&&last_complete==submitted);++probe_checkpoints;
 probe_serial_safe=serial_probe_checkpoint(clock_ms);
 if(!probe_serial_safe){++probe_busy;if(!probe_polls_before_busy)probe_polls_before_busy=polls;}
 else if(probe_busy&&!probe_safe_at)probe_safe_at=clock_ms;
 return !probe_serial_safe&&!probe_old_signal?RISC_RUNTIME_CHECKPOINT_BUSY:RISC_RUNTIME_CHECKPOINT_OK;
}
static int32_t probe_prepare(uint64_t*token){
 check_io();assert(!held&&!pending&&!owned&&!touch.subscription&&last_complete==submitted);
 assert(!hardware_brightness);++probe_begins;if(!probe_first_begin)probe_first_begin=clock_ms;
 *token=0;probe_serial_safe=serial_probe_checkpoint(clock_ms);
 if(!probe_serial_safe)return RISC_RUNTIME_SLEEP_BUSY;
 *token=probe_begins;return RISC_RUNTIME_SLEEP_OK;
}
static int32_t probe_resume(uint64_t token){check_io();assert(token==probe_begins);++probe_ends;return RISC_RUNTIME_SLEEP_OK;}
static bool probe_key(void*c,uint32_t*out){
 check_io();(void)c;*out=0;
 if(probe_manual&&ready&&!probe_key_sent&&clock_ms-ready_time>=3000){*out=2;probe_key_sent=true;}
 return true;
}
static bool probe_diagnostic(const char*s){
 if(!strncmp(s,"WATCH_CLOCK sleep=refused",25)){
  assert(!strcmp(s,"WATCH_CLOCK sleep=refused stage=SERVICE detail=1"));
  assert(!strcmp(sleep_status,"SLEEP ERR SERVICE 1"));
  assert(!hardware_brightness&&!owned&&!pending&&!touch.subscription);
  ++probe_refusals;++refused_cycles;return true;
 }
 bool result=fast_diagnostic(s);
 if(strstr(s,"stage=services-ready")&&sleeps)probe_done=true;
 return result;
}
static bool probe_brightness(void*c,uint16_t level,uint16_t max){
 if(ready&&!level){++probe_blanks;if(!probe_serial_safe)++probe_blanks_while_busy;}
 return fast_brightness(c,level,max);
}
static bool probe_status(void*c,uint64_t token,risc_display_present_status_v1*out){
 bool result=fast_status(c,token,out);
 if(out->state==RISC_DISPLAY_PRESENT_COMPLETE&&!probe_serial_safe&&hardware_brightness)++probe_frames_while_busy;
 return result;
}
static int32_t probe_sleep(void*c,uint32_t ms,risc_light_sleep_result_v1*out){
 assert(probe_serial_safe);if(!probe_first_sleep)probe_first_sleep=clock_ms;
 return motion_sleep(c,ms,out);
}
int main(int argc,char**argv){
 assert(argc==3);probe_case=argv[1];probe_baseline=!strcmp(argv[2],"baseline");
 probe_old_signal=!strcmp(probe_case,"old-signal");probe_manual=!strcmp(probe_case,"manual-busy");
 probe_session=strcmp(probe_case,"no-session")!=0;probe_renew=!strcmp(probe_case,"renewed-session");probe_disconnect=!strcmp(probe_case,"disconnect");
 assert(probe_old_signal||probe_manual||!strcmp(probe_case,"lease-timeout")||!strcmp(probe_case,"no-session")||probe_renew||probe_disconnect);
 reset_case();probe_long=true;fixture_provision_mode=2;saved_radio(0);
 const uint8_t idle_record[]={0x54,1,10,0,10^0xa5};memcpy(low_bytes[1],idle_record,5);low_lengths[1]=5;
 serial_probe_init(probe_session,probe_renew,probe_disconnect);
 fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.health=probe_health;fast_runtime.diagnostic=probe_diagnostic;
 fast_runtime.service_checkpoint=probe_checkpoint;fast_runtime.sleep_prepare=probe_prepare;fast_runtime.sleep_resume=probe_resume;
 pa.key_events=probe_key;pa.light_sleep_set=probe_sleep;da.base.set_brightness=probe_brightness;da.base.present_status=probe_status;
 app_main();check_clean();assert(first_seen&&ready&&!retains&&hardware_brightness>0&&grants==ungrants&&subscribed==unsubscribed&&!allocations);
 assert(clock_quick.idle_ms==10000&&probe_checkpoints>10&&polls>probe_polls_before_busy);
 if(probe_session)assert(probe_busy>10&&probe_frames_while_busy>10&&serial_probe_requests()>=(probe_renew?4u:3u));
 if(probe_manual){assert(probe_key_sent&&trace_manual&&probe_begins==1&&probe_refusals==1&&!sleeps&&!probe_ends);}
 else {
  assert(sleeps==1&&probe_ends==1&&probe_first_sleep>10000u&&probe_done);
  if(probe_disconnect)assert(serial_probe_disconnected_at()>=15000&&probe_first_sleep>=serial_probe_disconnected_at()&&!serial_probe_expired_at());
  else if(probe_session){assert(serial_probe_expired_at()>=serial_probe_last_request()+serial_probe_timeout());assert(probe_first_sleep>=serial_probe_expired_at());}
  if(probe_session&&(probe_baseline||probe_old_signal)){assert(probe_refusals>=1&&probe_blanks_while_busy>=1&&probe_begins>1);}
  else {assert(!probe_refusals&&!probe_blanks_while_busy&&probe_begins==1&&!sleep_status[0]);}
 }
 printf("HOME_SERIAL_IDLE %ux%u variant=%s case=%s checkpoints=%u busy=%u visible_busy_frames=%u input_polls=%u blank_while_busy=%u refused=%u prepare=%u resumes=%u sleep=%u first_prepare_ms=%u first_sleep_ms=%u last_request_ms=%u expired_ms=%u disconnected_ms=%u visible=%d PASS\n",TEST_WIDTH,TEST_HEIGHT,argv[2],probe_case,probe_checkpoints,probe_busy,probe_frames_while_busy,polls,probe_blanks_while_busy,probe_refusals,probe_begins,probe_ends,sleeps,probe_first_begin,probe_first_sleep,serial_probe_last_request(),serial_probe_expired_at(),serial_probe_disconnected_at(),hardware_brightness>0);
 return 0;
}
