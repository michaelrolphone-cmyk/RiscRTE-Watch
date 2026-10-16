/* Exercise real shared session/spool with an optional capability-selected
 * export volume. No card path or physical provider ID is selected by Home. */
#define main inherited_spool_main
#include "crash_report_spool_test.c"
#undef main
#include "CrashReportSession.h"

static unsigned volume_mode,data_grants,volume_grants,releases,retains,observations;
static uint64_t tick;
static risc_storage_volume_api_v1_state test_volume;
static bool session_safe(void *c) {(void)c;return true;}
static uint64_t session_now(void *c) {(void)c;return tick;}
static void session_retain(void *c) {(void)c;retains++;}
static int32_t session_observe(void *c) {
 (void)c;observations++;
 return volume_mode==3?RISC_STORAGE_STATE_RETAINED:RISC_STORAGE_STATE_UNAVAILABLE;
}
static bool session_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 assert(version==1);
 if(!strcmp(name,RISC_APP_DATA_CAPABILITY)) {
  assert(instance==61);data_grants++;grant->api=&api;grant->slot=1;grant->generation=1;return true;
 }
 assert(!strcmp(name,"storage.volume")&&instance==0);
 volume_grants++;
 if(!volume_mode)return false;
 grant->api=&test_volume;grant->slot=2;grant->generation=1;return true;
}
static bool session_release(risc_runtime_capability_v1 *grant) {
 assert(grant->api);releases++;memset(grant,0,sizeof(*grant));return true;
}
static const risc_runtime_api_v1 session_runtime={.api_version=1,.struct_size=sizeof(session_runtime),.acquire=session_acquire,.release=session_release};
int main(void) {
 for(volume_mode=0;volume_mode<4;volume_mode++) {
  reset();assert(append(1,"EXACT REPORT\n")==CRASH_REPORT_SPOOL_COMMITTED);save_disk();
  data_grants=volume_grants=releases=retains=observations=0;tick=0;
  memset(&test_volume,0,sizeof(test_volume));
  risc_storage_volume_api_v1 *base=&test_volume.prepared.base.sleep.terminal.power.volume.base;
  base->api_version=1;base->struct_size=volume_mode==1?sizeof(*base):sizeof(test_volume);
  test_volume.state_tag=RISC_STORAGE_STATE_TAG;test_volume.state_version=1;test_volume.observe=session_observe;
  crash_report_session session;
  crash_report_session_hooks hooks={.runtime=&session_runtime,.safe=session_safe,.prepare_storage=session_safe,
   .now=session_now,.retain=session_retain,.namespace_id=61,.volume_instance=0,.sd_enabled=true};
  crash_session_init(&session,&hooks);
  unsigned writes=backend.writes;
  assert(crash_session_checkpoint(&session)==(volume_mode!=3));
  assert(data_grants==1&&volume_grants==1&&backend.writes==writes);assert_unchanged();
  assert(session.retained==(volume_mode==3)&&retains==(volume_mode==3));
  assert(releases==(volume_mode==1||volume_mode==2?2u:1u));
  tick=1;
  assert(crash_session_checkpoint(&session)==(volume_mode!=3));
  assert(data_grants==1&&volume_grants==1&&backend.writes==writes);assert_unchanged();
  printf("Optional crash volume mode=%u: exact internal spool preserved, bounded retry, correct custody PASS\n",volume_mode);
 }
 return 0;
}
