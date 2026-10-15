/* Optional storage.files uses the production shared session and durable spool.
 * Every error leaves exact bytes queued, with no storage.volume fallback. */
#define main inherited_spool_main
#include "crash_report_spool_test.c"
#undef main
#include "CrashReportSession.h"

static unsigned mode,data_grants,file_grants,releases,retains,operations;
static uint64_t tick;
static bool safe(void *c) {(void)c;return true;}
static uint64_t now(void *c) {(void)c;return tick;}
static void retain(void *c) {(void)c;retains++;}
static int32_t files_stat(void *c,const char *path,risc_storage_files_stat_v1 *out) {
 (void)c;(void)out;assert(!strcmp(path,"/CrashReports"));operations++;
 static const int codes[]={0,0,RISC_FILES_UNAVAILABLE,RISC_FILES_RETAINED,RISC_FILES_BUSY,RISC_FILES_IO,RISC_FILES_DENIED};
 assert(mode>=2&&mode<sizeof(codes)/sizeof(*codes));return codes[mode];
}
static int32_t unexpected_open(void *c,const char *p,uint32_t f,risc_storage_files_handle_v1 *h) {(void)c;(void)p;(void)f;(void)h;assert(false);return RISC_FILES_IO;}
static int32_t unexpected_token(void *c,risc_storage_files_handle_v1 h) {(void)c;(void)h;assert(false);return RISC_FILES_IO;}
static int32_t unexpected_read(void *c,risc_storage_files_handle_v1 h,void *p,uint32_t n,uint32_t *z) {(void)c;(void)h;(void)p;(void)n;(void)z;assert(false);return RISC_FILES_IO;}
static int32_t unexpected_write(void *c,risc_storage_files_handle_v1 h,const void *p,uint32_t n,uint32_t *z) {(void)c;(void)h;(void)p;(void)n;(void)z;assert(false);return RISC_FILES_IO;}
static int32_t unexpected_info(void *c,risc_storage_files_handle_v1 h,uint64_t *n,uint64_t *o) {(void)c;(void)h;(void)n;(void)o;assert(false);return RISC_FILES_IO;}
static int32_t unexpected_path(void *c,const char *p) {(void)c;(void)p;assert(false);return RISC_FILES_IO;}
static int32_t unexpected_rename(void *c,const char *a,const char *b) {(void)c;(void)a;(void)b;assert(false);return RISC_FILES_IO;}
static risc_storage_files_v1 files;
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 assert(version==1);
 if(!strcmp(name,RISC_APP_DATA_CAPABILITY)) {
  assert(instance==61);data_grants++;grant->api=&api;grant->slot=1;grant->generation=1;return true;
 }
 assert(!strcmp(name,RISC_STORAGE_FILES_CAPABILITY)&&instance==0);file_grants++;
 if(!mode)return false;
 grant->api=&files;grant->slot=2;grant->generation=1;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
 assert(grant->api);releases++;memset(grant,0,sizeof(*grant));return true;
}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
int main(void) {
 for(mode=0;mode<7;mode++) {
  reset();assert(append(1,"EXACT REPORT\n")==CRASH_REPORT_SPOOL_COMMITTED);save_disk();
  data_grants=file_grants=releases=retains=operations=0;tick=0;
  files=(risc_storage_files_v1){.api_version=mode==1?0:1,.struct_size=sizeof(files),.open=unexpected_open,
   .close=unexpected_token,.read=unexpected_read,.write=unexpected_write,.info=unexpected_info,
   .sync=unexpected_token,.stat=files_stat,.mkdir=unexpected_path,.rename=unexpected_rename,.remove=unexpected_path};
  crash_report_session session;
  crash_report_session_hooks hooks={.runtime=&runtime,.safe=safe,.prepare_storage=safe,
   .now=now,.retain=retain,.namespace_id=61,.files_instance=0,.files_enabled=true};
  crash_session_init(&session,&hooks);unsigned writes=backend.writes;
  assert(crash_session_checkpoint(&session)==(mode!=3));
  assert(data_grants==1&&file_grants==1&&backend.writes==writes);assert_unchanged();
  assert(session.retained==(mode==3)&&retains==(mode==3));
  assert(releases==(mode&&mode!=3?2u:1u)&&operations==(mode>=2?1u:0u));
  tick=1;assert(crash_session_checkpoint(&session)==(mode!=3));
  assert(data_grants==1&&file_grants==1&&backend.writes==writes);assert_unchanged();
  printf("Optional generic files mode=%u: exact spool, bounded retry, correct custody and no volume downgrade PASS\n",mode);
 }
 return 0;
}
