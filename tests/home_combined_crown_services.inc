
void home_faces_app_main(void){app_main();}
void home_overview_app_main(void){assert(!"overview selected in native-faces qualification");}
int home_overview_app_module_init(void){assert(!"overview selected in native-faces qualification");return -1;}
void home_overview_app_module_fini(void){assert(!"overview selected in native-faces qualification");}
static int32_t hc_input_activate(void*c,uint64_t session){
 check_io();if(!strcmp(canvas_case,"companion-retained")){canvas_faulted=true;return RISC_CANVAS_RETAINED;}
 return ((const risc_canvas_input_v1*)hc_input_driver->capability)->activate(c,session);
}
static int32_t hc_input_deactivate(void*c,uint64_t session){
 check_io();if(!strcmp(canvas_case,"companion-close-retained")){canvas_faulted=true;return RISC_CANVAS_RETAINED;}
 return ((const risc_canvas_input_v1*)hc_input_driver->capability)->deactivate(c,session);
}
static int32_t hc_civil_project(void*c,int64_t epoch,uint32_t fraction,risc_civil_calendar_v1*out){
 check_io();assert(!owned&&!pending);
 if(!strcmp(canvas_case,"civil-retained")){canvas_faulted=true;return -9;}
 const risc_civil_clock_policy_v1*p=hc_civil_driver->capability;
 return p->project(c,epoch,fraction,out);
}
static int32_t hc_civil_interpret(void*c,const risc_civil_calendar_v1*raw,int64_t*out){
 check_io();assert(!owned&&!pending);
 const risc_civil_clock_policy_v1*p=hc_civil_driver->capability;
 return p->interpret_rtc(c,raw,out);
}
static int hc_shared_acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 if(!strcmp(name,RISC_HOME_PRESENTATION_CAPABILITY)){
  assert(version==1&&!instance);g->api=&hc_presentation;++grants;return 1;
 }
 if(!strcmp(name,RISC_CANVAS_INPUT_CAPABILITY)){
  assert(version==1&&!instance);
  if(!strcmp(canvas_case,"companion-absent"))return 0;
  if(!strcmp(canvas_case,"companion-malformed"))g->api=&hc_malformed;
  else{
   if(!hc_shared_input_started){
    risc_provider_dependency_v1 deps[]={{RISC_CANVAS_CAPABILITY,1,&fixture_canvas_api},
     {"input.touch.raw",1,&fixture_touch},{"input.navigation",1,&hc_navigation}};
    assert(hc_input_driver->start(deps,3));hc_shared_input_started=true;
   }
   g->api=&hc_input_api;
  }
  ++grants;return 1;
 }
 if(!strcmp(name,RISC_CIVIL_CLOCK_CAPABILITY)){
  assert(version==1&&!instance);
  if(!strcmp(canvas_case,"civil-absent"))return 0;
  g->api=!strcmp(canvas_case,"civil-malformed")?(const void*)&hc_malformed:(const void*)&hc_civil_api;
  ++grants;return 1;
 }
 return -1;
}
static void hc_shared_start(void){
 hc_presentation=(risc_home_presentation_v1){1,sizeof(hc_presentation),RISC_HOME_PRESENTATION_TAG,RISC_HOME_VIEW_FACES,61,0,RISC_HOME_POINTS_LEGACY_RTC,0};
 hc_input_driver=hc_input_driver_get(2);hc_civil_driver=hc_civil_driver_get(2);
 assert(hc_input_driver&&hc_civil_driver);hc_input_api=*(const risc_canvas_input_v1*)hc_input_driver->capability;
 hc_input_api.activate=hc_input_activate;hc_input_api.deactivate=hc_input_deactivate;
 hc_civil_api=*(const risc_civil_clock_schedule_v1*)hc_civil_driver->capability;
 hc_civil_api.base.project=hc_civil_project;hc_civil_api.base.interpret_rtc=hc_civil_interpret;
 risc_provider_dependency_v1 deps[]={{"rtc.clock",2,&ra}};
 assert(hc_civil_driver->start(deps,1));
 hc_shared_input_started=false;
}
static void hc_shared_expected(void){
 portable_timezone_rule rule;portable_timezone_civil expected;
 assert(!portable_timezone_resolve("America/Denver",sizeof("America/Denver"),&rule));
 assert(!portable_timezone_utc_to_local(&rule,native_time.epoch_seconds,&expected,NULL));
 assert(face.time.year==expected.year&&face.time.month==expected.month&&face.time.day==expected.day&&
  face.time.hour==expected.hour&&face.time.minute==expected.minute&&face.time.second==expected.second&&
  face.time.weekday==expected.weekday);
}
static void hc_shared_run(void){
 assert(!hc_entry_init());hc_entry_main();
 if(!retained_fence){hc_entry_fini();if(hc_shared_input_started){assert(hc_input_driver->quiesce());hc_input_driver->stop();}
  assert(hc_civil_driver->quiesce());hc_civil_driver->stop();}
}
