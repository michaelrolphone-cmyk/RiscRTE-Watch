static const char *confirm_case;
static unsigned confirm_turns,confirm_calls,confirm_successes,confirm_first_ms,confirm_first_frame,confirm_last_frame;
static unsigned confirm_wait_lines,confirm_deadline_lines,confirm_yields;
static unsigned confirm_busy_navigation_lines,confirm_swipe_frame;
static bool confirm_swipe_sent;
static uint16_t confirm_pixels[TEST_WIDTH*TEST_HEIGHT];
static unsigned confirm_token;
static bool confirm_ready,confirm_snapshot;
static bool confirm_health(risc_runtime_health_v1*h){
 check_io();h->uptime_ms=clock_ms;
 if(confirm_successes&&submitted>=confirm_last_frame+2u)return false;
 return clock_ms<30000u;
}
static void confirm_yield(uint32_t ms){++confirm_yields;yield(ms);}
static bool confirm_diagnostic(const char*s){
 if(!strcmp(s,"WATCH_CLOCK confirm=deferred checkpoint=busy"))++confirm_wait_lines;
 if(!strcmp(s,"WATCH_CLOCK confirm=deferred deadline-ms=5000 checkpoint=busy"))++confirm_deadline_lines;
 if(!strcmp(s,"WATCH_CLOCK navigation=busy action=retry"))++confirm_busy_navigation_lines;
 return fast_diagnostic(s);
}
static int32_t confirm_checkpoint(void){
 check_io();assert(first_seen&&!held&&!pending&&!owned);
 if(clock_deferred_phase!=3)return RISC_RUNTIME_CHECKPOINT_OK;
 if(!confirm_turns){confirm_first_ms=clock_ms;confirm_first_frame=last_complete;}
 ++confirm_turns;
 if(!strcmp(confirm_case,"quiescent-refusal")||!strcmp(confirm_case,"normal-success")){confirm_ready=true;return RISC_RUNTIME_CHECKPOINT_OK;}
 if(!strcmp(confirm_case,"progress")&&confirm_turns>=8){confirm_ready=true;return RISC_RUNTIME_CHECKPOINT_OK;}
 if(!strcmp(confirm_case,"retained")&&confirm_turns>=8){retained_fence=true;return RISC_RUNTIME_CHECKPOINT_RETAINED;}
 if(!strcmp(confirm_case,"long-busy")&&!confirm_swipe_sent&&(uint32_t)(clock_ms-confirm_first_ms)>=6500u){
  confirm_swipe_sent=true;confirm_swipe_frame=last_complete;launcher_swipe_pending=true;
 }
 return RISC_RUNTIME_CHECKPOINT_BUSY;
}
static bool confirm_boot_probe(void){
 check_io();assert(first_seen&&!held&&!pending&&!owned&&clock_deferred_phase==3);++confirm_calls;
 if(!confirm_ready||!strcmp(confirm_case,"quiescent-refusal")){
  memcpy(confirm_pixels,completed_pixels,sizeof(confirm_pixels));confirm_token=last_complete;confirm_snapshot=true;return false;
 }
 ++confirm_successes;confirm_last_frame=last_complete;return fast_confirm();
}
int main(int argc,char**argv){
 assert(argc==2);confirm_case=argv[1];reset_case();probe_long=true;saved_radio(0);
 fast_runtime.health=confirm_health;fast_runtime.yield_ms=confirm_yield;fast_runtime.diagnostic=confirm_diagnostic;
 fast_runtime.service_checkpoint=confirm_checkpoint;fast_runtime.confirm_boot=confirm_boot_probe;
 app_main();assert(first_seen&&hardware_brightness>0&&confirm_turns&&confirm_yields);
 if(!strcmp(confirm_case,"baseline-busy")||!strcmp(confirm_case,"quiescent-refusal")){
  assert(confirm_calls==1&&!confirm_successes&&confirm_snapshot&&clock_deferred_phase==3);
  assert(last_complete==confirm_token&&!memcmp(confirm_pixels,completed_pixels,sizeof(confirm_pixels)));check_clean();
 }else if(!strcmp(confirm_case,"progress")||!strcmp(confirm_case,"normal-success")){
  assert(confirm_calls==1&&confirm_successes==1&&clock_deferred_phase==4&&promotions==1);check_clean();
  if(!strcmp(confirm_case,"progress"))assert(confirm_turns==8&&confirm_wait_lines==1&&last_complete>=confirm_first_frame+8);
  else assert(confirm_turns==1&&!confirm_wait_lines&&!confirm_deadline_lines);
 }else if(!strcmp(confirm_case,"long-busy")){
  /* Busy confirmation now also refuses navigation. One rejected action must
   * be consumed without ending the loop or starving deferred confirmation. */
  assert(!confirm_calls&&!confirm_successes&&!promotions&&clock_deferred_phase==3&&!launches);
  assert(confirm_swipe_sent&&!launcher_swipe_pending&&confirm_busy_navigation_lines==1);
  assert(clock_ms>=30000u&&last_complete>confirm_swipe_frame+20);
  assert(confirm_wait_lines==1&&confirm_deadline_lines==1&&clock_ms-confirm_first_ms>=6500u&&last_complete>confirm_first_frame+20);check_clean();
 }else if(!strcmp(confirm_case,"retained")){
  assert(retained_fence&&!confirm_calls&&!promotions&&grants>ungrants&&confirm_turns==8);retained_reset();
 }else assert(false);
 printf("HOME_CONFIRM case=%s turns=%u attempts=%u success=%u wait=%u deadline=%u yields=%u first_ms=%u final_ms=%u first_frame=%u final_frame=%u PASS\n",confirm_case,confirm_turns,confirm_calls,confirm_successes,confirm_wait_lines,confirm_deadline_lines,confirm_yields,confirm_first_ms,clock_ms,confirm_first_frame,last_complete);
 return 0;
}
