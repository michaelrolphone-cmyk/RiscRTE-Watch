/* Link the real System Settings/adapter with the real generic key bridge. */
#define PORTABLE_SETTINGS_NAVIGATION_OVERRIDE
#define OPTIONAL_SERVICE_FIXTURE_MAIN unused_optional_service_main
#include "optional_service_adapter_test.c"
#include "RiscPowerKeyV1.h"
static unsigned bridge_case,bridge_reads,bridge_fault_read,bridge_events;
static risc_power_key_api_v1 bridge_key;
static bool bridge_events_read(void *c,uint32_t *out) {
 (void)c;assert(!fenced);bridge_reads++;
 if(bridge_fault_read && bridge_reads==bridge_fault_read){health_dead=bridge_case!=13;return false;}
 *out=bridge_events;bridge_events=0;return true;
}
static bool bridge_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
 assert(!fenced);
 if(strcmp(name,RISC_POWER_KEY_CAPABILITY))return startup_acquire(name,version,instance,g);
 assert(version==1&&!instance);acquires++;
 if(bridge_case==1)return false;
 if(bridge_case==3){health_dead=true;return false;}
 *g=(risc_runtime_capability_v1){sizeof(*g),771,1,bridge_case==15?NULL:&bridge_key};grants++;
 return bridge_case!=2&&bridge_case!=15;
}
static bool bridge_release(risc_runtime_capability_v1 *g) {
 assert(!fenced);
 if(g->slot!=771)return startup_release(g);
 releases++;assert(grants);
 if(bridge_case==11||bridge_case==14)return false;
 grants--;memset(g,0,sizeof(*g));return true;
}
static bool bridge_retain(void) {
 /* The bridge promotes immediately, then the adapter confirms idempotently. */
 if(!fenced){fenced=true;fences++;}return true;
}
void *__real_malloc(size_t);
void __real_free(void *);
void *__wrap_malloc(size_t n){assert(!fenced);return __real_malloc(n);}
void __wrap_free(void *p){assert(!fenced);__real_free(p);}
int main(int argc,char **argv) {
 assert(argc==2);bridge_case=(unsigned)atoi(argv[1]);
 configure();startup_runtime.acquire=bridge_acquire;startup_runtime.release=bridge_release;startup_runtime.retain_invocation=bridge_retain;
 bridge_key=(risc_power_key_api_v1){1,sizeof(bridge_key),NULL,bridge_events_read};
 if(bridge_case==4||bridge_case==14)bridge_key.api_version=9;
 if(bridge_case==5)bridge_fault_read=1;
 if(bridge_case==6)bridge_fault_read=2;
 int result=app_module_init();
 if(bridge_case==1||bridge_case==4) {
  assert(result==-1&&!fenced);app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
 } else if(bridge_case==2||bridge_case==3||bridge_case==5||bridge_case==6||bridge_case==14||bridge_case==15) {
  assert(result==-1);check_fenced();
 } else {
  assert(result==0&&!failed&&!fenced);
  if(bridge_case==0) {
   for(unsigned n=0;n<3;n++) {
    clear();present(false);t5_app_input_t input;assert(poll(&input,8));
    /* Boundary draining and neutral rearming occur through production calls. */
    bridge_events=RISC_POWER_KEY_SHORT_PRESS;input_navigation_reset();assert(!bridge_events&&!navigation_pending);
    input_service();bridge_events=RISC_POWER_KEY_LONG_PRESS;input_service();assert(!navigation_pending);
    input_service();bridge_events=RISC_POWER_KEY_SHORT_PRESS;input_service();assert(navigation_pending&T5_APP_BUTTON_BACK);
    input_navigation_reset();app_module_fini();assert(!grants&&!subscriptions&&!frame_count);
    if(n<2)assert(app_module_init()==0);
   }
  } else if(bridge_case==7||bridge_case==13) {
   bridge_fault_read=bridge_reads+1;input_service();assert(failed==(bridge_case==7));
   if(bridge_case==7)check_fenced();else {assert(!fenced);app_module_fini();assert(!grants);}
  } else if(bridge_case==8) {
   bridge_fault_read=bridge_reads+1;input_navigation_reset();check_fenced();
  } else if(bridge_case==9||bridge_case==10||bridge_case==11) {
   if(bridge_case!=11)bridge_fault_read=bridge_reads+(bridge_case==9?1:2);
   app_module_fini();check_fenced();
  } else assert(!"unknown bridge case");
 }
 printf("actual Settings/adapter/generic bridge case %u passed\n",bridge_case);return 0;
}
