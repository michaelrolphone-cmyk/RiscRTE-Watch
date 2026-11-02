/* Derived-fixture controls: real crown and full production compiler flags. */
static const char *malformed_capability;
static const struct {uint32_t version,size;} malformed_api={2,8};
static bool optional_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant){
 bool acquired=fast_acquire(name,version,instance,grant);
 if(acquired&&malformed_capability&&!strcmp(name,malformed_capability))grant->api=&malformed_api;
 return acquired;
}
int main(int argc,char **argv){
 assert(argc==3);reset_case();
 bool malformed=!strcmp(argv[1],"malformed");
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
 if(!malformed&&!strcmp(capability,"board.battery"))assert(!face.battery_valid);
 printf("Full crown %s %s: %s; checked custody PASS\n",argv[1],capability,stops?"refused":"continued");
 return 0;
}
