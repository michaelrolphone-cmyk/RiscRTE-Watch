#pragma once
#include "RiscDiagnosticCheckpointV1.h"
#include "RiscRuntimeClockV1.h"
#include <string.h>
/* Optional RAM checkpoint and sparse diagnostic queue entries. Never provider
 * I/O, allocation, polling, or a change to the operation's result. */
typedef struct {
 const risc_runtime_api_v1 *runtime;
 risc_diagnostic_checkpoint_client_v1 client;
 bool (*safe)(void);
 uint32_t began;
 unsigned left,sleep_left;
 char completed[32],record[192];
} watch_interaction_trace;
static void wit_text(char *out,unsigned *n,const char *s) {
 while(*s && *n<190u)out[(*n)++]=*s++;
}
static void wit_number(char *out,unsigned *n,uint32_t v) {
 char digits[10];unsigned count=0;
 do{digits[count++]=(char)('0'+v%10u);v/=10u;}while(v);
 while(count && *n<190u)out[(*n)++]=digits[--count];
}
static void wit_init(watch_interaction_trace *t,const risc_runtime_api_v1 *rt,bool (*safe)(void)) {
 memset(t,0,sizeof(*t));t->runtime=rt;t->safe=safe;t->sleep_left=16;
 if(!rt||!safe||!safe())return;
 t->client.struct_size=sizeof(t->client);
 if(!risc_runtime_diagnostic_checkpoint_client(rt,&t->client)||
    t->client.api_version!=1||t->client.struct_size<sizeof(t->client)||!t->client.write)
  t->client=(risc_diagnostic_checkpoint_client_v1){0};
}
static void wit_mark(watch_interaction_trace *t,const char *phase,bool completed,bool begin,bool frame_held) {
 if(!t->runtime||!t->safe||!t->safe()||(!begin&&!t->left))return;
 uint32_t now=0;
 if(!risc_runtime_monotonic_ms_v1(t->runtime,&now)||!t->safe())return;
 if(begin){t->began=now;t->left=24;memcpy(t->completed,"none",5);}
 --t->left;
 if(completed){unsigned n=0;while(phase[n]&&n<sizeof(t->completed)-1){t->completed[n]=phase[n];++n;}t->completed[n]=0;}
 unsigned n=0;wit_text(t->record,&n,"WATCH_INTERACTION phase=");wit_text(t->record,&n,phase);
 wit_text(t->record,&n," last=");wit_text(t->record,&n,t->completed);
 wit_text(t->record,&n," elapsed_ms=");wit_number(t->record,&n,now-t->began);
 wit_text(t->record,&n," custody=prechecked frame_held=");wit_number(t->record,&n,frame_held?1u:0u);t->record[n]=0;
 if(t->client.write)(void)t->client.write(t->client.invocation,t->record,n);
 if(t->safe()&&t->runtime->diagnostic)(void)t->runtime->diagnostic(t->record);
}

static void wit_exit(watch_interaction_trace *t,unsigned line) {
 if(!t->record[0]||!t->safe||!t->safe()||!t->client.write)return;
 char detail[192];unsigned n=0;
 wit_text(detail,&n,t->record);wit_text(detail,&n," WATCH_HOME_EXIT line=");
 wit_number(detail,&n,line);detail[n]=0;
 (void)t->client.write(t->client.invocation,detail,n);
}

/* Separate bounded sleep decisions. All timestamps are existing samples: no
 * health/clock/provider callback is performed merely to report elapsed time. */
static void wit_sleep(watch_interaction_trace *t,const char *reason,uint32_t now,
                      uint32_t last_activity,uint32_t idle_ms) {
 if(!t->runtime||!t->safe||!t->safe()||!t->sleep_left)return;
 --t->sleep_left;
 char record[192];unsigned n=0;
 wit_text(record,&n,"WATCH_IDLE reason=");wit_text(record,&n,reason);
 wit_text(record,&n," now_ms=");wit_number(record,&n,now);
 wit_text(record,&n," last_activity_ms=");wit_number(record,&n,last_activity);
 wit_text(record,&n," elapsed_ms=");wit_number(record,&n,now-last_activity);
 wit_text(record,&n," idle_ms=");wit_number(record,&n,idle_ms);record[n]=0;
 if(t->client.write&&t->client.write(t->client.invocation,record,n)<0)return;
 if(t->safe()&&t->runtime->diagnostic)(void)t->runtime->diagnostic(record);
}
