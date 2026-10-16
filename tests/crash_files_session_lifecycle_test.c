/* Compose real report formatting, durable spool, generic export and ack gates.
 * The two existing doubles supply AppData and backend-neutral file operations. */
#define main inherited_spool_main
#include "crash_report_spool_test.c"
#undef main
#define main inherited_files_main
#include "crash_report_files_test.c"
#undef main
#include "CrashReportSession.h"

static fake destination;
static crash_report_session *active_session;
static unsigned acknowledgements,removed_spools,grants,released_grants;
static uint64_t session_time;
static bool lifecycle_safe(void *c) {(void)c;return true;}
static bool lifecycle_prepare(void *c) {(void)c;return true;}
static uint64_t lifecycle_now(void *c) {(void)c;return session_time;}
static void lifecycle_retain(void *c) {(void)c;assert(!"unexpected retained session");}
static void assert_exported(void) {
 unsigned finals=0;
 for(unsigned i=0;i<FILES;i++)if(destination.files[i].used&&strstr(destination.files[i].path,".txt")) {
  file *f=&destination.files[i];finals++;
  assert(f->synced&&f->size==active_session->text_size&&!memcmp(f->data,active_session->text,f->size));
 }
 assert(finals==1&&!destination.handle);
 assert(destination.counts[RENAME]==1&&destination.counts[CLOSE]>=3);
 assert(destination.calls&&destination.events[destination.calls-1]==CLOSE);
}
static int32_t lifecycle_replace(void *c,const char *name,uint64_t expected,const void *bytes,uint32_t size) {
 if(size>=CRASH_REPORT_SPOOL_HEADER_SIZE&&!crash_report_spool_u32((const uint8_t*)bytes+16)) {
  assert_exported();removed_spools++;
 }
 return fake_replace(c,name,expected,bytes,size);
}
static int32_t evidence_read(uint64_t invocation,risc_failure_evidence_v1 *out) {
 assert(invocation==71);
 *out=(risc_failure_evidence_v1){.api_version=1,.struct_size=sizeof(*out),.flags=RISC_FAILURE_PENDING,.record_boot=7,.record_sequence=81};
 for(unsigned i=0;i<32;i++)out->firmware_sha256[i]=(uint8_t)i;
 return RISC_FAILURE_EVIDENCE_OK;
}
static int32_t acknowledge(uint64_t invocation,uint32_t boot,uint32_t sequence) {
 assert(invocation==71&&boot==7&&sequence==81);
 assert(active_session->presented&&active_session->committed&&backend.present);
 assert(crash_report_spool_reload(&spool)==CRASH_REPORT_SPOOL_OK&&spool.count==1);
 acknowledgements++;return RISC_FAILURE_EVIDENCE_OK;
}
static bool evidence(risc_failure_evidence_client_v1 *out) {
 *out=(risc_failure_evidence_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=71,.read=evidence_read,.acknowledge=acknowledge};return true;
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
 assert(version==1);grants++;
 if(!strcmp(name,RISC_APP_DATA_CAPABILITY)){assert(instance==61);grant->api=&api;}
 else {assert(!strcmp(name,RISC_STORAGE_FILES_CAPABILITY)&&instance==0);grant->api=&destination.api;}
 grant->slot=1;grant->generation=1;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
 assert(grant->api&&!destination.handle);memset(grant,0,sizeof(*grant));released_grants++;return true;
}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.failure_evidence=evidence};
int main(void) {
 unsigned final_read_call=0;
 for(unsigned scenario=0;scenario<3;scenario++) {
  bool failed_save=scenario==1;
  reset();init(&destination);api.replace=lifecycle_replace;
  acknowledgements=removed_spools=grants=released_grants=0;session_time=0;
  crash_report_session session;active_session=&session;
  crash_report_session_hooks hooks={.runtime=&runtime,.safe=lifecycle_safe,.prepare_storage=lifecycle_prepare,
   .now=lifecycle_now,.retain=lifecycle_retain,.namespace_id=61,.files_instance=0,.files_enabled=true};
  crash_session_init(&session,&hooks);
  if(failed_save)backend.write_fault=RISC_APP_DATA_IO;
  assert(crash_session_read(&session)&&session.pending&&!session.presented&&!acknowledgements);
  assert(crash_session_ack(&session)&&!acknowledgements);
  if(failed_save) {
   assert(!session.committed);session.presented=true;
   assert(crash_session_ack(&session)&&!acknowledgements);
   assert(crash_session_save(&session)&&session.committed);
  } else assert(session.committed);
  session.presented=true;assert(crash_session_ack(&session)&&acknowledgements==1&&!session.pending);
  assert(!destination.calls&&!removed_spools);
  if(scenario==2){assert(final_read_call);destination.corrupt_call=final_read_call;}
  assert(crash_session_checkpoint(&session));
  if(scenario==2) {
   assert(!removed_spools&&acknowledgements==1&&grants==released_grants);
   assert(crash_report_spool_reload(&spool)==CRASH_REPORT_SPOOL_OK&&spool.count==1);
   unsigned calls=destination.calls,writes=destination.counts[WRITE];
   session_time=1;assert(crash_session_checkpoint(&session)&&destination.calls==calls);
   destination.corrupt_call=0;session_time=30000;
   assert(crash_session_checkpoint(&session)&&destination.counts[WRITE]==writes);
  }
  assert_exported();
  if(!scenario) {
   bool renamed=false;
   for(unsigned i=0;i<destination.calls;i++) {
    if(destination.events[i]==RENAME)renamed=true;
    else if(renamed&&destination.events[i]==READ){final_read_call=i+1;break;}
   }
   assert(final_read_call);
  }
  assert(removed_spools==1&&acknowledgements==1&&grants==released_grants);
  assert(crash_report_spool_reload(&spool)==CRASH_REPORT_SPOOL_OK&&!spool.count);
  printf("Generic crash session scenario=%u (normal, initial-save failure, corrupt-final-read retry): durable save + presentation gate acknowledgement; final verified close precedes spool removal PASS\n",scenario);
 }
 return 0;
}
