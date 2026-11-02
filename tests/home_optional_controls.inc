/* Derived-fixture controls: real crown and full production compiler flags. */
static const char *malformed_capability;
static bool optional_preset;
static unsigned optional_applied;
static contexts_service_v1 optional_context;
static telemetry_broadcast_v1 optional_broadcast;
static int32_t optional_get(void *ctx,const char *key,void *data,uint32_t capacity,uint32_t *size){
 if(optional_preset&&!strcmp(key,"ctx_p0")){
  portable_context_preset preset;portable_context_preset_init(&preset);
  preset.source=PORTABLE_CONTEXT_RF;preset.slot=0;preset.enabled=true;
  preset.actions=PORTABLE_CONTEXT_VOLUME;preset.volume=30;strcpy(preset.name,"Test room");
  assert(capacity>=64&&portable_context_preset_encode(&preset,data));*size=64;return RISC_KEY_VALUE_OK;
 }
 return fast_get(ctx,key,data,capacity,size);
}
static const risc_key_value_v1 optional_kv={1,sizeof(optional_kv),NULL,optional_get,face_put};
static bool optional_claim(void *ctx,uint32_t source,uint32_t slot,const char *name,uint32_t generation){
 check_io();(void)ctx;(void)generation;assert(source==CONTEXTS_RADIO&&slot==0&&!strcmp(name,"Test room"));return true;
}
static bool optional_result(void *ctx,uint32_t source,uint32_t generation,uint32_t result){
 check_io();(void)ctx;(void)generation;assert(source==CONTEXTS_RADIO&&result==CONTEXTS_PRESET_APPLIED);
 ++optional_applied;contexts_view.radio.preset_entry=contexts_view.radio.room_entry;return true;
}
static bool optional_context_step(void *ctx,const contexts_policy_v1 *policy){
 check_io();(void)ctx;assert(!held&&!pending&&!clock_radios_pending&&clock_contexts_started);
 assert(!policy->enabled||contexts_enabled);contexts_live=true;return true;
}
static bool optional_broadcast_step(void *ctx,bool allowed,const telemetry_broadcast_policy_v1 *policy){
 check_io();(void)ctx;(void)policy;assert(allowed&&!held&&!pending&&!clock_radios_pending&&clock_broadcast_started);
 broadcast_live=true;return true;
}
static const struct {uint32_t version,size;} malformed_api={2,8};
static bool optional_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 bool acquired=fast_acquire(name,version,instance,grant);
 if(acquired&&optional_preset){
  if(grant->api==&fast_kv&&instance==1)grant->api=&optional_kv;
  if(grant->api==&context_api)grant->api=&optional_context;
  if(grant->api==&broadcast_api)grant->api=&optional_broadcast;
 }
 if(acquired&&malformed_capability&&!strcmp(name,malformed_capability))grant->api=&malformed_api;
 return acquired;
}
int main(int argc,char **argv){
 assert(argc==3);reset_case();
 bool malformed=!strcmp(argv[1],"malformed");
 optional_preset=!strcmp(argv[1],"preset");
 if(optional_preset){
  contexts_enabled=true;
  optional_context=context_api;optional_context.claim_preset=optional_claim;optional_context.preset_result=optional_result;optional_context.step=optional_context_step;
  optional_broadcast=broadcast_api;optional_broadcast.step=optional_broadcast_step;
  contexts_view.radio.current=true;contexts_view.radio.room_valid=true;
  contexts_view.radio.source=CONTEXTS_RADIO;contexts_view.radio.room_slot=0;
  contexts_view.radio.room_entry=1;strcpy(contexts_view.radio.room_name,"Test room");
 }
 const char *capability=argv[2];
 if(malformed)malformed_capability=capability;else fail_capability=capability;
 fast_runtime.acquire=optional_acquire;
 app_main();
 bool required=!strcmp(capability,"display.output")||!strcmp(capability,"input.touch.raw")||
  !strcmp(capability,"runtime.realtime-control")||!strcmp(capability,"runtime.provider-promotion");
 bool stops=required||(malformed&&(!strcmp(capability,"board.battery")||
  !strcmp(capability,"power.sleep")||!strcmp(capability,"input.power-key")||
  !strcmp(capability,"display.power")||!strcmp(capability,"runtime.retained-wake")||
  !strcmp(capability,"alarm.service")||!strcmp(capability,"telemetry.broadcast")||
  !strcmp(capability,"contexts.service")));
 if(!malformed&&!strcmp(capability,"runtime.provider-promotion")){
  assert(retained_fence&&retains&&native_clock.halted&&!confirmations);
 }else{
  check_clean();
  if(stops)assert(!confirmations);
  else assert(first_seen&&ready&&confirmations==1&&stage_calls==1);
 }
 if(optional_preset)assert(optional_applied==1&&quick_values[1]==30&&quick_saved[1]&&!clock_alarm.api);
 if(!malformed&&!strcmp(capability,"board.battery"))assert(!face.battery_valid);
 if(!strcmp(capability,"net.wifi")||!strcmp(capability,"bluetooth.hci")){
  bool absent_wifi=!strcmp(capability,"net.wifi");
  assert(clock_radios.available&&clock_quick.ui.radios_valid);
  assert(clock_quick.ui.wifi_valid==!absent_wifi&&clock_quick.ui.bluetooth_valid==absent_wifi);
 }
 printf("Full crown %s %s: %s; checked custody PASS\n",argv[1],capability,stops?"refused":"continued");
 return 0;
}
