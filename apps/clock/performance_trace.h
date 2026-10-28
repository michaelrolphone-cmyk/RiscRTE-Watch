#pragma once
#include "RiscPerformanceV1.h"
/* Opt-in current-ABI RAM milestones only. No allocation, health sample,
 * provider poll, formatting or transport; no raw coordinates/user values. */
typedef struct { uint32_t checkpoints,samples; bool down; } watch_home_performance;
static void whp_count(uint32_t *n){if(*n!=UINT32_MAX)++*n;}
static void whp_emit(const risc_runtime_api_v1 *api,bool (*safe)(void),uint32_t phase,uint32_t value){
 if(safe&&safe())(void)risc_perf_trace_v1(api,0,phase,value);
}
static void whp_sample(watch_home_performance *p,const risc_runtime_api_v1 *api,bool (*safe)(void),bool valid,bool down){
 whp_count(&p->samples);
 if(valid&&down&&!p->down)whp_emit(api,safe,RISC_PERF_INTERACTION_BEGIN,RISC_PERF_KIND_OTHER);
 p->down=valid&&down;
}
static void whp_frame_begin(watch_home_performance *p,const risc_runtime_api_v1 *api,bool (*safe)(void)){
 p->checkpoints=p->samples=0;
 whp_emit(api,safe,RISC_PERF_FIRST_DRAW,0x400);
 whp_emit(api,safe,RISC_PERF_SPAN_BEGIN,0x1001);
}
static void whp_complete(watch_home_performance *p,const risc_runtime_api_v1 *api,bool (*safe)(void)){
 whp_emit(api,safe,RISC_PERF_COMPLETE,0x400);
 whp_emit(api,safe,RISC_PERF_COUNTER,(0x41u<<16)|(p->checkpoints>65535?65535:p->checkpoints));
 whp_emit(api,safe,RISC_PERF_COUNTER,(0x42u<<16)|(p->samples>65535?65535:p->samples));
 p->checkpoints=p->samples=0;
}
