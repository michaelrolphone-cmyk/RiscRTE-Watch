/* Exact dual-schema native boundary tests, no hardware or provider startup. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "twatch_caps.h"
#include "../apps/runtime_features/clock_runtime.h"
static unsigned acquires,releases,legacy_reads,desk_reads,legacy_stages,desk_stages;
static uint32_t boot_cause=RISC_BOOT_DEEP_TIMER;
static int read_result,stage_result,mode;
static bool extension=true,corrupt;
static uint8_t payload[PORTABLE_DESK_CLOCK_RECORD_BYTES],staged[PORTABLE_DESK_CLOCK_RECORD_BYTES];
static int32_t read_time(void*p,risc_realtime_snapshot_v1*out){(void)p;*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=RISC_REALTIME_VALID,.epoch_seconds=1704067237};return 0;}
static int32_t seed_time(void*p,int64_t epoch,uint32_t ns){(void)p;(void)epoch;(void)ns;return 0;}
static const risc_realtime_control_api_v1 time_api={.api_version=1,.struct_size=sizeof(time_api),.read=read_time,.seed=seed_time};
static int32_t read_legacy(void*p,uint32_t type,uint32_t schema,risc_retained_wake_record_v1*out,uint32_t*cause){
 (void)p;assert(type==WATCH_CLOCK_CHECKPOINT_TYPE&&schema==1);++legacy_reads;*cause=boot_cause;
 if(mode!=1)return mode==2?RISC_RETAINED_WAKE_ABSENT:RISC_RETAINED_WAKE_MISMATCH;
 *out=(risc_retained_wake_record_v1){.struct_size=sizeof(*out),.type=type,.schema_version=schema,.size=1,.payload={1}};return 0;
}
static int32_t stage_legacy(void*p,const risc_retained_wake_record_v1*r){(void)p;assert(r->type==WATCH_CLOCK_CHECKPOINT_TYPE&&r->schema_version==1&&r->size==1&&r->payload[0]==1);++legacy_stages;return 0;}
static int32_t clear_wake(void*p){(void)p;return 0;}
static int32_t read_desk(void*p,uint32_t type,uint32_t schema,void*out,uint32_t capacity,uint32_t*used,uint32_t*cause){
 (void)p;assert(type==PORTABLE_DESK_CLOCK_RECORD_TYPE&&schema==3&&capacity==408);++desk_reads;*cause=boot_cause;
 if(read_result)return read_result;
 memcpy(out,payload,408);*used=corrupt?40:408;return 0;
}
static int32_t stage_desk(void*p,uint32_t type,uint32_t schema,const void*in,uint32_t used){
 (void)p;assert(type==PORTABLE_DESK_CLOCK_RECORD_TYPE&&schema==3&&used==408);++desk_stages;memcpy(staged,in,408);return stage_result;
}
static risc_retained_wake_api_v1_extended wake_api={
 .base={.api_version=1,.struct_size=sizeof(wake_api),.read=read_legacy,.stage=stage_legacy,.clear=clear_wake},
 .extension_tag=RISC_RETAINED_WAKE_EXTENDED_TAG,.extension_version=1,.max_payload_bytes=512,.read_bytes=read_desk,.stage_bytes=stage_desk};
static bool acquire(const char*name,uint32_t api,uint64_t instance,risc_runtime_capability_v1*out){
 assert(api==1&&instance==0);++acquires;out->slot=1;out->generation=1;
 if(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY))out->api=&time_api;
 else{assert(!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY));wake_api.base.struct_size=extension?sizeof(wake_api):sizeof(wake_api.base);out->api=&wake_api;}
 return true;
}
static bool release(risc_runtime_capability_v1*g){++releases;memset(g,0,sizeof(*g));return true;}
static bool diagnostic(const char*s){(void)s;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.diagnostic=diagnostic};
static void reset(void){acquires=releases=legacy_reads=desk_reads=legacy_stages=desk_stages=0;mode=0;extension=true;corrupt=false;read_result=stage_result=0;boot_cause=RISC_BOOT_DEEP_TIMER;}
int main(void){
 portable_desk_record record={.config={.time_format=1},.displayed_minute=1704067200,.refresh_modulo=1,.has_image=true};strcpy(record.config.time_zone,"UTC");
 assert(portable_desk_encode(&record,payload,sizeof(payload)));
 watch_clock_runtime c;
 reset();mode=1;assert(watch_runtime_open(&c,&runtime,true));assert(c.resume&&!c.desk_resume&&desk_reads==0&&acquires==2);assert(watch_runtime_stage(&c)&&legacy_stages==1);assert(watch_runtime_abandon(&c));assert(watch_runtime_close(&c)&&releases==2);
 reset();assert(watch_runtime_open(&c,&runtime,true));assert(c.desk_supported&&c.desk_resume&&!c.resume&&desk_reads==1&&legacy_reads==1&&acquires==2);
 assert(watch_runtime_stage_desk(&c,&c.desk_record)==1&&desk_stages==1&&!memcmp(staged,payload,408));assert(watch_runtime_close(&c));
 reset();extension=false;assert(watch_runtime_open(&c,&runtime,true));assert(!c.desk_supported&&!c.desk_resume&&desk_reads==0&&watch_runtime_stage_desk(&c,&record)==0);assert(watch_runtime_close(&c));
 reset();boot_cause=RISC_BOOT_DEEP_GPIO;assert(watch_runtime_open(&c,&runtime,true)&&!c.desk_resume);assert(watch_runtime_close(&c));
 reset();wake_api.extension_tag=0;assert(watch_runtime_open(&c,&runtime,true)&&!c.desk_supported&&!c.desk_resume&&desk_reads==0);assert(watch_runtime_close(&c));wake_api.extension_tag=RISC_RETAINED_WAKE_EXTENDED_TAG;
 reset();wake_api.max_payload_bytes=256;assert(watch_runtime_open(&c,&runtime,true)&&!c.desk_supported&&watch_runtime_stage_desk(&c,&record)==0);assert(watch_runtime_close(&c));wake_api.max_payload_bytes=512;
 reset();read_result=RISC_RETAINED_WAKE_INVALID;assert(watch_runtime_open(&c,&runtime,true)&&!c.desk_resume&&!c.halted);assert(watch_runtime_close(&c));
 reset();corrupt=true;assert(watch_runtime_open(&c,&runtime,true)&&!c.desk_resume);assert(watch_runtime_close(&c));
 reset();mode=2;assert(watch_runtime_open(&c,&runtime,true)&&!c.desk_resume&&desk_reads==0);assert(watch_runtime_close(&c));
 for(int i=0;i<2;i++){reset();read_result=i?-99:RISC_RETAINED_WAKE_CONTEXT;assert(!watch_runtime_open(&c,&runtime,true)&&c.halted);assert(!watch_runtime_close(&c)&&releases==0);}
 reset();assert(watch_runtime_open(&c,&runtime,true));stage_result=RISC_RETAINED_WAKE_INVALID;assert(watch_runtime_stage_desk(&c,&record)==0&&!c.halted);stage_result=-99;assert(watch_runtime_stage_desk(&c,&record)==-2&&c.halted&&!watch_runtime_close(&c)&&releases==0);
 (void)watch_runtime_seed;(void)watch_runtime_read;(void)watch_runtime_promote;
 puts("shared Home native checkpoint: original one-byte marker, schema3 408-byte TIMER, GPIO, malformed, absent extension, retained/unknown custody PASS");
 return 0;
}
