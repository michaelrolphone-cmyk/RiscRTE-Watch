#include <assert.h>
#include <stdio.h>
#include "../apps/clock/performance_trace.h"
static bool live=true,poison;static unsigned calls;static uint32_t phases[32],values[32];
static bool safe(void){return live;}
static uint32_t emit(uint32_t id,uint32_t phase,uint32_t value){assert(live&&!id);phases[calls]=phase;values[calls++]=value;if(poison)live=false;return 1;}
int main(void){
 risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.trace=emit};watch_home_performance p={0};
 whp_frame_begin(&p,&rt,safe);assert(calls==2);for(unsigned n=0;n<10000;n++)whp_count(&p.checkpoints);assert(calls==2);
 whp_sample(&p,&rt,safe,true,true);assert(calls==3&&phases[2]==RISC_PERF_INTERACTION_BEGIN);whp_sample(&p,&rt,safe,true,true);assert(calls==3);
 whp_emit(&rt,safe,RISC_PERF_DISPATCHED,0x401);whp_emit(&rt,safe,RISC_PERF_SPAN_END,0x1001);whp_emit(&rt,safe,RISC_PERF_PRESENT_SUBMIT,0x400);whp_complete(&p,&rt,safe);assert(calls==9&&values[7]==((0x41u<<16)|10000)&&values[8]==((0x42u<<16)|2));
 live=false;unsigned before=calls;whp_frame_begin(&p,&rt,safe);whp_complete(&p,&rt,safe);assert(calls==before);
 live=true;poison=true;whp_frame_begin(&p,&rt,safe);assert(calls==before+1);poison=false;live=true;
 rt.struct_size=RISC_RUNTIME_TRACE_V1_SIZE-1;before=calls;whp_frame_begin(&p,&rt,safe);assert(calls==before);
 p.checkpoints=UINT32_MAX;whp_count(&p.checkpoints);assert(p.checkpoints==UINT32_MAX);
 puts("Home RAM phases: frame/input ordering, cheap local counts, held-contact coalescing, saturation, legacy ABI and retained callback fences PASS");return 0;
}
