#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../apps/clock/interaction_trace.h"
static bool safe=true,health_ok=true,poison_health,poison_write,poison_client;
static int32_t checkpoint_result;
static unsigned calls,writes,logs,clock_calls;static uint32_t now;static char saved[200];
static bool guard(void){return safe;}
static bool health(risc_runtime_health_v1 *h){++calls;h->uptime_ms=now;if(poison_health)safe=false;return health_ok;}
static int32_t checkpoint(uint64_t token,const char *s,uint32_t n){assert(safe&&token==7);++writes;assert(n<sizeof(saved));memcpy(saved,s,n);saved[n]=0;if(poison_write)safe=false;return checkpoint_result;}
static bool monotonic(uint32_t*out){++clock_calls;*out=now;if(poison_health)safe=false;return health_ok;}
static bool client(risc_diagnostic_checkpoint_client_v1 *c){++calls;*c=(risc_diagnostic_checkpoint_client_v1){1,sizeof(*c),7,checkpoint};if(poison_client)safe=false;return true;}
static bool log_line(const char*s){assert(safe&&(strstr(s,"custody=prechecked")||strstr(s,"WATCH_IDLE reason=")));++logs;return true;}
int main(void){
 risc_runtime_api_v1 rt={.api_version=RISC_RUNTIME_API_V1,.struct_size=sizeof(rt),.health=health,.diagnostic=log_line,.diagnostic_checkpoint_client=client};watch_interaction_trace t;
 wit_init(&t,&rt,guard);now=100;wit_mark(&t,"quick-modal-begin",false,true,false);assert(writes==1&&logs==1&&strstr(saved,"last=none elapsed_ms=0"));
 now=145;wit_mark(&t,"quick-modal-done",true,false,false);assert(strstr(saved,"last=quick-modal-done elapsed_ms=45"));
 wit_exit(&t,1234);assert(strstr(saved,"last=quick-modal-done")&&strstr(saved,"WATCH_HOME_EXIT line=1234"));
 now=180;wit_mark(&t,"quick-frame-begin",false,false,true);assert(strstr(saved,"last=quick-modal-done elapsed_ms=80")&&strstr(saved,"frame_held=1"));
 safe=false;unsigned before=calls;wit_mark(&t,"forbidden",true,true,false);assert(calls==before&&writes==4&&logs==3);wit_exit(&t,9999);assert(writes==4);
 safe=true;poison_health=true;wit_mark(&t,"health-retains",false,false,false);assert(writes==4&&logs==3);poison_health=false;safe=true;
 poison_write=true;wit_mark(&t,"checkpoint-retains",false,false,false);assert(writes==5&&logs==3);poison_write=false;safe=true;
 now=UINT32_MAX-5;wit_mark(&t,"wrap",false,true,false);now=4;wit_mark(&t,"wrapped",true,false,false);assert(strstr(saved,"elapsed_ms=10"));
 for(unsigned i=0;i<50;i++){wit_mark(&t,"bounded",true,false,false);}assert(t.left==0);before=calls;wit_mark(&t,"no-spam",false,false,false);assert(calls==before);
 health_ok=false;before=writes;wit_mark(&t,"bad-health",false,true,false);assert(writes==before);health_ok=true;
 rt.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;wit_init(&t,&rt,guard);assert(!t.client.write);wit_mark(&t,"legacy",true,true,false);assert(logs>0);
 rt.struct_size=sizeof(rt);poison_client=true;wit_init(&t,&rt,guard);before=calls;wit_mark(&t,"client-retains",true,true,false);assert(calls==before);poison_client=false;safe=true;
 wit_init(&t,&rt,guard);before=calls;unsigned bw=writes,bl=logs;
 wit_sleep(&t,"idle",45000,15000,30000);assert(calls==before&&writes==bw+1&&logs==bl+1);
 assert(strstr(saved,"reason=idle now_ms=45000 last_activity_ms=15000 elapsed_ms=30000 idle_ms=30000"));
 wit_sleep(&t,"wake-baseline",UINT32_MAX,UINT32_MAX,15000);assert(strstr(saved,"elapsed_ms=0 idle_ms=15000"));
 wit_sleep(&t,"idle",4,UINT32_MAX-5,30);assert(strstr(saved,"elapsed_ms=10"));
 for(unsigned i=0;i<40;i++)wit_sleep(&t,"bounded",1,1,30);
 assert(t.sleep_left==0&&calls==before);bw=writes;bl=logs;wit_sleep(&t,"exhausted",1,1,30);assert(writes==bw&&logs==bl);
 wit_init(&t,&rt,guard);safe=false;before=calls;wit_sleep(&t,"forbidden",1,1,30);assert(calls==before&&writes==bw&&logs==bl);
 safe=true;poison_write=true;wit_sleep(&t,"write-retains",1,1,30);assert(writes==bw+1&&logs==bl);poison_write=false;safe=true;
 checkpoint_result=RISC_DIAGNOSTIC_CHECKPOINT_DENIED;wit_sleep(&t,"denied",1,1,30);assert(logs==bl);checkpoint_result=0;
 rt.struct_size=sizeof(rt);rt.monotonic_ms=monotonic;wit_init(&t,&rt,guard);
 before=calls;unsigned cw=clock_calls;now=0;wit_mark(&t,"cheap",false,true,false);
 assert(clock_calls==cw+1&&calls==before&&strstr(saved,"elapsed_ms=0"));
 health_ok=false;before=calls;unsigned wl=writes,ll=logs;wit_mark(&t,"clock-failure",false,true,false);
 assert(calls==before&&writes==wl&&logs==ll);health_ok=true;
 safe=false;cw=clock_calls;wit_mark(&t,"retained-cheap",false,true,false);assert(clock_calls==cw);safe=true;
 puts("interaction trace: timing, wrap, budget, legacy, health and retained callback fences PASS");return 0;
}
