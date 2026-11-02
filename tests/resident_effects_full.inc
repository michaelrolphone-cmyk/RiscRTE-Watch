#define main inherited_effects_idle_main
#include "resident_idle_full.inc"
#undef main
static const char*effect_case;
static unsigned effect_refreshes,effect_steps,effect_keys;
static int effect_refresh_rc,effect_step_rc;
static bool effect_started;
static int32_t effect_call(uint32_t);
static unsigned effect_wifi_off,effect_ble_set,effect_ble_status;
static bool effect_wifi_disconnect(void*c){(void)c;++effect_wifi_off;return strcmp(effect_case,"wifi-retained")!=0;}
static wifi_link_t effect_wifi_status(void*c){(void)c;return WIFI_LINK_DOWN;}
static bool effect_ble_change(void*c,bool on){(void)c;++effect_ble_set;if(!strcmp(effect_case,"ble-failure"))return false;ble_state=on?1:0;return true;}
static bool effect_ble_read(void*c,uint8_t*out){(void)c;++effect_ble_status;*out=ble_state;return true;}
static const wifi_api_v1 effect_wifi={.api_version=1,.struct_size=sizeof(effect_wifi),.status=effect_wifi_status,.disconnect_checked=effect_wifi_disconnect};
static const portable_bluetooth_control_v1 effect_ble={.api_version=1,.struct_size=sizeof(effect_ble),.set_enabled=effect_ble_change,.status=effect_ble_read};
static bool effect_key(void*c,uint32_t*out){++effect_keys;return att_key(c,out);}
static int32_t effect_refresh(void*c){(void)c;++effect_refreshes;if(!strcmp(effect_case,"reentry"))assert(effect_call(RISC_SETTINGS_APPLY(4))==RISC_RESIDENT_RETAINED);return effect_refresh_rc;}
static int32_t effect_step(void*c){if(!effect_started)return att_step(c);++effect_steps;return effect_step_rc;}
static int32_t effect_call(uint32_t reason){risc_resident_request_v1 q={sizeof(q),reason,0,0};risc_resident_reply_v1 r={sizeof(r),0};int rc=attention_host.dispatch(NULL,&q,&r);assert(!r.flags);return rc;}
static int32_t effect_run(uint64_t token,const char*path,risc_resident_result_v1*out){
 assert(token==9&&!path&&!owned&&!pending&&!touch.subscription);++attention_runs;effect_started=true;
 /* The child double has completed its own scene before this boundary. */
 if(!strcmp(effect_case,"brightness"))submitted=last_complete=1;
 unsigned keys=effect_keys,steps=attention_steps,frames=submitted,acq=grants,rel=ungrants;
 clock_resident_keys=3;attention_keys=3;
 for(unsigned e=1;e<=4;e++){assert(effect_call(RISC_SETTINGS_QUERY(e))==RISC_RESIDENT_OK);assert(effect_call(RISC_SETTINGS_RESULT(e,RISC_SETTINGS_APPLIED))==RISC_RESIDENT_BUSY);}
 assert(keys==effect_keys&&steps==attention_steps&&frames==submitted&&acq==grants&&rel==ungrants);
 if(!strcmp(effect_case,"pending"))effect_refresh_rc=ALARM_PENDING;
 if(!strcmp(effect_case,"clean-failure"))effect_refresh_rc=ALARM_STORAGE;
 if(!strcmp(effect_case,"uncertain"))effect_refresh_rc=ALARM_OUTPUT;
 if(!strcmp(effect_case,"held")){held=true;assert(effect_call(RISC_SETTINGS_APPLY(4))==RISC_RESIDENT_BUSY&&!effect_refreshes);held=false;}
 int rc=effect_call(RISC_SETTINGS_APPLY(4));
 if(!strcmp(effect_case,"uncertain")||!strcmp(effect_case,"reentry")){assert(rc==RISC_RESIDENT_RETAINED&&clock_resident_terminal&&effect_refreshes==1);assert(effect_call(RISC_SETTINGS_APPLY(4))==RISC_RESIDENT_RETAINED&&effect_refreshes==1);out->status=RISC_RESIDENT_RETAINED;return RISC_RESIDENT_RETAINED;}
 assert(rc==RISC_RESIDENT_OK&&effect_refreshes==1&&!effect_steps);
 unsigned expected=effect_refresh_rc==ALARM_PENDING?RISC_SETTINGS_PENDING:effect_refresh_rc?RISC_SETTINGS_FAILED:RISC_SETTINGS_APPLIED;
 for(unsigned n=0;n<3;n++){assert(effect_call(RISC_SETTINGS_RESULT(4,expected))==RISC_RESIDENT_OK);assert(effect_refreshes==1&&!effect_steps);}
 if(expected==RISC_SETTINGS_PENDING){effect_step_rc=ALARM_PENDING;assert(effect_call(RISC_SETTINGS_PROGRESS(4))==RISC_RESIDENT_OK);assert(effect_call(RISC_SETTINGS_RESULT(4,3))==RISC_RESIDENT_OK);effect_step_rc=ALARM_OK;assert(effect_call(RISC_SETTINGS_PROGRESS(4))==RISC_RESIDENT_OK);assert(effect_call(RISC_SETTINGS_RESULT(4,1))==RISC_RESIDENT_OK&&effect_refreshes==1&&effect_steps==2);assert(effect_call(RISC_SETTINGS_PROGRESS(4))==RISC_RESIDENT_BUSY&&effect_steps==2);}
 assert(keys==effect_keys&&steps==attention_steps&&frames==submitted&&!sleeps&&!attention_exits&&clock_resident_keys==3&&attention_keys==3);
 if(!strcmp(effect_case,"brightness")){assert(effect_call(RISC_SETTINGS_APPLY(1))==RISC_RESIDENT_OK);assert(effect_call(RISC_SETTINGS_RESULT(1,1))==RISC_RESIDENT_OK);assert(effect_call(RISC_SETTINGS_RESULT(4,expected))==RISC_RESIDENT_BUSY);assert(effect_refreshes==1&&keys==effect_keys&&steps==attention_steps&&frames==submitted);}
 if(!strcmp(effect_case,"radios")||!strcmp(effect_case,"ble-failure")||!strcmp(effect_case,"wifi-retained")){
  radio_saved=true;memcpy(radio_bytes,(uint8_t[]){0x51,1,2,2^0xa5},4);ble_state=0;
  unsigned before_refresh=effect_refreshes;
  int radio_rc=effect_call(RISC_SETTINGS_APPLY(2));
  if(!strcmp(effect_case,"wifi-retained")){assert(radio_rc==RISC_RESIDENT_RETAINED&&effect_wifi_off==1&&!effect_ble_set);out->status=RISC_RESIDENT_RETAINED;return RISC_RESIDENT_RETAINED;}
  assert(radio_rc==0&&effect_wifi_off==1&&!effect_ble_set&&effect_call(RISC_SETTINGS_RESULT(2,1))==0);
  assert(effect_call(RISC_SETTINGS_APPLY(3))==0&&effect_ble_set==1&&effect_ble_status==2);
  assert(effect_call(RISC_SETTINGS_RESULT(3,!strcmp(effect_case,"ble-failure")?2:1))==0);
  radio_bytes[2]=1;radio_bytes[3]=1^0xa5;
  assert(effect_call(RISC_SETTINGS_APPLY(2))==0&&effect_wifi_off==1); /* Wi-Fi enable never connects. */
  assert(effect_refreshes==before_refresh&&keys==effect_keys&&steps==attention_steps&&frames==submitted);
 }
 clock_effect_clear();assert(effect_call(RISC_SETTINGS_RESULT(4,expected))==RISC_RESIDENT_BUSY);
 ended=true;out->status=0;return 0;
}
static bool effect_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){bool ok=att_acquire(n,v,id,g);if(ok){g->slot=1;g->generation=1;if(effect_started&&!strcmp(n,"net.wifi"))g->api=&effect_wifi;if(effect_started&&!strcmp(n,"bluetooth.hci"))g->api=&effect_ble;}return ok;}
static bool effect_get(risc_resident_client_v1*c){if(!att_get(c))return false;c->run_foreground=effect_run;return true;}
int main(int argc,char**argv){
 assert(argc==2);effect_case=argv[1];reset_case();
 assert(alarm_calendar_seconds(2026,10,4,0,0,40,&attention_base_seconds));alarm_view.snapshot=7;
 att_alarm=alarm_api;att_alarm.base.status=att_status;att_alarm.base.step=effect_step;att_alarm.base.refresh=effect_refresh;att_alarm.base.prepare_sleep=att_deadline;
 pa.key_events=effect_key;ra.read=att_rtc;fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.resident_shell=effect_get;fast_runtime.health=att_health;fast_runtime.diagnostic=att_diagnostic;fast_runtime.acquire=effect_acquire;
 app_main();
 assert(attention_runs==1);
 if(!strcmp(effect_case,"uncertain")||!strcmp(effect_case,"reentry")||!strcmp(effect_case,"wifi-retained"))assert(clock_resident_terminal&&retains);
 else assert(!clock_effects.result&&!clock_effects.id&&!retains&&!owned&&!pending&&!held&&!touch.subscription&&grants==ungrants);
 printf("Home settings effects %s: production flags and real Home controller PASS\n",effect_case);return 0;
}
