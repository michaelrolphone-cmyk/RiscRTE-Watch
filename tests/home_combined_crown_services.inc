
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
static int32_t hc_points_get(void*c,const char*key,void*bytes,uint32_t capacity,uint32_t*size){
 check_io();(void)c;(void)bytes;(void)capacity;++hc_points_reads;*size=0;
 const bool utc=hc_presentation.points_schema==RISC_HOME_POINTS_NATIVE_UTC;
 assert(!strcmp(key,utc?"points_utc_cfg":"points_cfg")||!strcmp(key,utc?"points_utc_meta":"points_meta"));
 return RISC_KEY_VALUE_NOT_FOUND;
}
static const risc_key_value_v1 hc_points_kv={1,sizeof(hc_points_kv),NULL,hc_points_get,NULL};
static unsigned hc_expected_points_loads(void){
 return hc_presentation.points_schema==RISC_HOME_POINTS_LEGACY_RTC||hc_presentation.points_schema==RISC_HOME_POINTS_NATIVE_UTC;
}
static int hc_shared_acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 if((!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY)&&!strncmp(canvas_case,"boot-",5))||
    (!strcmp(name,RISC_HOME_PRESENTATION_CAPABILITY)&&!strncmp(canvas_case,"policy-",7))||
    (!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&instance==1&&!hc_view_seen&&!strncmp(canvas_case,"view-",5))){
  if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY))hc_view_seen=true;
  if(strstr(canvas_case,"acquire-retained")){hc_runtime_lost=retained_fence=canvas_faulted=true;return 0;}
  if(strstr(canvas_case,"absent"))return 0;
  if(strstr(canvas_case,"null")){g->api=NULL;g->slot=65533;g->generation=1;++grants;return 1;}
 }
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&instance==1)hc_view_seen=true;

 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&instance==5){
  assert(version==1&&hc_expected_points_loads());++grants;++points_loads;g->api=&hc_points_kv;return 1;
 }
 if(!strcmp(name,RISC_HOME_PRESENTATION_CAPABILITY)){
  assert(version==1&&!instance);g->api=&hc_presentation;++grants;return 1;
 }
 if(!strcmp(name,RISC_CANVAS_INPUT_CAPABILITY)){
  assert(version==1&&!instance);
  if(!strcmp(canvas_case,"companion-absent"))return 0;
  if(!strcmp(canvas_case,"companion-malformed"))g->api=&hc_malformed;
  else{
   if(!hc_shared_input_started){
    risc_provider_dependency_v1 deps[]={{RISC_CANVAS_RASTER_CAPABILITY,1,&fixture_canvas_api},
     {"input.touch.raw",1,&fixture_touch},{"input.navigation",1,&hc_navigation}};
    assert(hc_input_driver->start(deps,3));hc_shared_input_started=true;
    hc_input_api.flags=((const risc_canvas_input_v1*)hc_input_driver->capability)->flags;
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
 hc_runtime_lost=hc_view_seen=false;fast_runtime.release=hc_shared_release;
 hc_live_peak=hc_capacity_refusals=0;fast_runtime.acquire=hc_bounded_acquire;
 if(!strcmp(canvas_case,"provision"))fixture_provision_mode=1;
 if(!strcmp(canvas_case,"provision-volume"))fixture_provision_mode=2;
 if(fixture_provision_mode)saved_radio(0);
 hc_points_reads=0;hc_crash_frames=hc_crash_fresh=hc_crash_started=hc_crash_last=0;
 hc_crash_inject=hc_crash_neutral=hc_crash_release=false;
 if(!strncmp(canvas_case,"crash-",6)){fast_runtime.struct_size=sizeof(fast_runtime);fast_runtime.failure_evidence=hc_evidence;}
 if(!strcmp(canvas_case,"schema-native"))hc_presentation.points_schema=RISC_HOME_POINTS_NATIVE_UTC;
 if(!strcmp(canvas_case,"schema-unspecified"))hc_presentation.points_schema=RISC_HOME_POINTS_UNSPECIFIED;
 if(!strcmp(canvas_case,"schema-catalog"))hc_presentation.points_schema=RISC_HOME_POINTS_CATALOG;
 if(!strcmp(canvas_case,"schema-malformed"))hc_presentation.points_schema=UINT32_MAX;
 if(!strcmp(canvas_case,"fraction")){boot_cause=RISC_BOOT_DEEP_GPIO;native_time.nanoseconds=123456000;}
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
 if(!strcmp(canvas_case,"fraction"))assert(native_time.nanoseconds==123456000&&rtc_second_at==rtc_sampled_at-123u);
 portable_timezone_rule rule;portable_timezone_civil expected;
 assert(!portable_timezone_resolve("America/Denver",sizeof("America/Denver"),&rule));
 assert(!portable_timezone_utc_to_local(&rule,native_time.epoch_seconds,&expected,NULL));
 assert(face.time.year==expected.year&&face.time.month==expected.month&&face.time.day==expected.day&&
  face.time.hour==expected.hour&&face.time.minute==expected.minute&&face.time.second==expected.second&&
  face.time.weekday==expected.weekday);
}
static void hc_shared_run(void){
 assert(!hc_entry_init());hc_entry_main();
 fprintf(stderr,"Home actual acquisition boundary peak=%u overflows=%u live=%u\n",hc_live_peak,hc_capacity_refusals,grants-ungrants);
 assert(!hc_capacity_refusals);
 if(!strncmp(canvas_case,"provision",9))assert(fixture_provision_scans==5&&fixture_volume_acquires==1&&stage_calls==2);

 if(!retained_fence){if(first_seen)assert(hc_points_reads==(hc_expected_points_loads()?(WATCH_POINTS_EXTENDED?2u:1u):0u));hc_entry_fini();if(hc_shared_input_started){assert(hc_input_driver->quiesce());hc_input_driver->stop();}
  assert(hc_civil_driver->quiesce());hc_civil_driver->stop();}
}

static bool hc_crash_navigation(risc_input_navigation_frame_v1*out){
 if(strncmp(canvas_case,"crash-",6)||!clock_crash_modal)return false;
 if(hc_crash_inject){hc_crash_inject=false;out->buttons=out->pressed=RISC_NAV_CONFIRM;return true;}
 if(clock_ms-hc_crash_started<300){out->buttons=RISC_NAV_CONFIRM;return true;}
 if(!hc_crash_neutral){hc_crash_neutral=true;out->released=RISC_NAV_CONFIRM;return true;}
 if(hc_crash_release){hc_crash_release=false;out->released=RISC_NAV_CONFIRM;return true;}
 if(clock_ms-hc_crash_last<300)return true;
 if(!hc_crash_fresh)assert(hc_crash_frames==1); /* inherited queued intent did not page */
 ++hc_crash_fresh;hc_crash_last=clock_ms;hc_crash_release=true;
 out->buttons=out->pressed=RISC_NAV_CONFIRM;return true;
}
static void hc_shared_diagnostic(const char*s){
 if(strncmp(canvas_case,"crash-",6))return;
 if(strstr(s,"stage=services-ready"))ended=false;
 if(strstr(s,"stage=crash-report-begin")){
  hc_crash_started=hc_crash_last=clock_ms;hc_crash_inject=true;
  assert(clock_canvas_touch.api.poll(clock_canvas_touch.api.context,1));
  assert(clock_canvas_touch.navigation_count);
 }
 if(strstr(s,"stage=crash-report-end")){assert(hc_crash_fresh&&clock_crash.presented);ended=true;}
}
static int32_t hc_evidence_read(uint64_t token,risc_failure_evidence_v1*out){
 check_io();assert(token==71);
 *out=(risc_failure_evidence_v1){.api_version=1,.struct_size=sizeof(*out),
 .flags=RISC_FAILURE_PENDING,.record_boot=7,.record_sequence=81};
 for(unsigned i=0;i<32;i++)out->firmware_sha256[i]=(uint8_t)i;
 return RISC_FAILURE_EVIDENCE_OK;
}
static int32_t hc_evidence_ack(uint64_t token,uint32_t boot,uint32_t sequence){
 (void)token;(void)boot;(void)sequence;assert(!"absent durable spool must never acknowledge");return -1;
}
static bool hc_evidence(risc_failure_evidence_client_v1*out){
 check_io();*out=(risc_failure_evidence_client_v1){.api_version=1,.struct_size=sizeof(*out),
 .invocation=71,.read=hc_evidence_read,.acknowledge=hc_evidence_ack};return true;
}

static bool hc_bounded_acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
 check_io();assert(grants>=ungrants);
 bool acquired=hc_acquire(name,version,instance,g);
 if(grants-ungrants>hc_live_peak)hc_live_peak=grants-ungrants;
 if(grants-ungrants>16){++hc_capacity_refusals;fprintf(stderr,"Home would exceed Runtime live-slot bound: %s instance=%llu peak=%u\n",name,(unsigned long long)instance,hc_live_peak);}
 assert(grants-ungrants<=16);
 return acquired;
}

static bool hc_shared_release(risc_runtime_capability_v1*g){
 check_io();
 if(g->slot==65533){
  assert(g->generation==1&&!g->api);
  if(strstr(canvas_case,"release-retained")){canvas_faulted=true;return false;}
  memset(g,0,sizeof(*g));++ungrants;return true;
 }
 return fast_release(g);
}
