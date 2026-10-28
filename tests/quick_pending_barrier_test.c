static _Bool clock_runtime_unavailable;
static unsigned clock_fault_call,clock_calls;
#include "PortableQuickActions.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define PORTABLE_QUICK_CAPABILITY_SAFE 1
#define WATCH_ATOMIC_PREFERENCES 1
#define WATCH_CLOCK_LAUNCHER 1
#define WATCH_CLOCK_ALARMS 1
/* Provider completion is modeled; controller/helper are production code. */
typedef struct {unsigned frame;} risc_display_surface_v1;
static struct {pqa_state ui;unsigned brightness,volume,restore_volume;bool dnd_enabled;} clock_quick;
static struct {bool open;} picker;
static void *rt,*display;static struct {void*api;}clock_alarm;
static bool clock_alarm_modal,retained,exiting,completed,mutated,clock_touch_retained;
static unsigned stage,fail_stage,cancel_kind,frames,draws,presents,restores,discards,writes;
static bool step(void){assert(!retained);return ++stage!=fail_stage;}
static void clock_atomic_retain(void){assert(!retained);retained=true;}
static bool alive(uint32_t*n){assert(!clock_runtime_unavailable);if(clock_fault_call&&++clock_calls==clock_fault_call){clock_runtime_unavailable=true;return false;}*n=stage;return step();}
static bool frame(risc_display_surface_v1*f){frames++;f->frame=1;return step();}
static bool draw_clock(uint32_t n,risc_display_surface_v1*f){(void)n;assert(f->frame&&clock_quick.ui.save_pending&&!mutated&&!writes);draws++;if(cancel_kind==7){retained=clock_touch_retained=true;return false;}return step();}
static bool present(void){presents++;if(!step())return false;/* submit */if(!step())return false;/* completion */completed=true;return true;}
static void sample_launcher_touch(uint32_t n,bool active){assert(completed&&active&&!retained);if(cancel_kind==6){retained=clock_touch_retained=true;return;}if(cancel_kind==1)(void)pqa_input(&clock_quick.ui,n,true,1,9,100,100,true);if(cancel_kind==2)(void)pqa_input(&clock_quick.ui,n,false,0,0,0,0,true);}
static bool clock_key_read(uint32_t*e,const char*s){(void)s;assert(completed&&!retained);*e=cancel_kind==3?1:0;return true;}
static bool risc_runtime_exit_requested_v1(void*p){(void)p;return exiting;}
static bool clock_alarm_foreground(void){assert(completed&&!retained);clock_alarm_modal=cancel_kind==5;return true;}
static void quick_atomic_discard(void){assert(!retained);discards++;}
static bool pqa_session_restore(void*p,void*d){(void)p;(void)d;assert(!retained);restores++;return true;}
#include "../apps/clock/quick_pending.inc"
static void reset(void){clock_runtime_unavailable=false;clock_fault_call=clock_calls=0;memset(&clock_quick,0,sizeof(clock_quick));pqa_init(&clock_quick.ui);clock_quick.ui.position_q8=clock_quick.ui.target_q8=pqa_open_q8(&clock_quick.ui);clock_quick.brightness=30;clock_quick.volume=40;clock_quick.restore_volume=40;clock_quick.ui.brightness=70;clock_quick.ui.volume=80;clock_quick.ui.dnd_enabled=true;clock_alarm.api=&clock_alarm;clock_alarm_modal=retained=exiting=completed=mutated=clock_touch_retained=false;stage=fail_stage=cancel_kind=frames=draws=presents=restores=discards=writes=0;}
int main(void){const uint32_t actions[]={PQA_BRIGHTNESS_COMMIT,PQA_VOLUME_COMMIT,PQA_SILENT,PQA_DND,PQA_CONTEXTS,PQA_WIFI,PQA_BLUETOOTH,PQA_AIRPLANE};
 for(unsigned i=0;i<sizeof(actions)/sizeof(actions[0]);i++){
  reset();uint32_t e=0;assert(clock_quick_pending_frame(actions[i],&e)==1);assert(completed&&frames==1&&draws==1&&presents==1&&clock_quick.ui.save_pending&&!writes&&!restores);mutated=true;writes++;clock_quick_pending_done();assert(!clock_quick.ui.save_pending&&!clock_quick.ui.save_cancelled);
 }
 reset();uint32_t e=0;assert(clock_quick_pending_frame(PQA_TORCH,&e)==1&&!frames&&!clock_quick.ui.save_pending);
 for(unsigned failure=1;failure<=5;failure++){reset();fail_stage=failure;e=0;assert(clock_quick_pending_frame(PQA_DND,&e)==-1);assert(retained&&clock_quick.ui.save_pending&&!writes&&!restores&&!discards);}
 for(unsigned why=1;why<=5;why++){reset();cancel_kind=why;exiting=why==4;e=0;assert(clock_quick_pending_frame(PQA_DND,&e)==0);assert(completed&&!writes&&restores==1&&discards==1&&!clock_quick.ui.save_pending&&clock_quick.ui.brightness==30&&clock_quick.ui.volume==40&&!clock_quick.ui.dnd_enabled);}
 reset();e=1;assert(clock_quick_pending_frame(PQA_DND,&e)==0&&!frames&&!writes&&restores==1);
 reset();e=0;assert(clock_quick_pending_frame(PQA_WIFI,&e)==1);clock_atomic_retain();assert(clock_quick.ui.save_pending&&!writes&&!restores);/* No done call after retained result. */
 for(unsigned kind=6;kind<=7;kind++){reset();cancel_kind=kind;e=0;assert(clock_quick_pending_frame(PQA_DND,&e)==-1);assert(retained&&clock_touch_retained&&!writes&&!restores&&!discards);}
 for(unsigned n=1;n<=2;n++){reset();clock_fault_call=n;e=0;assert(clock_quick_pending_frame(PQA_DND,&e)==-1);assert(clock_runtime_unavailable&&!retained&&!clock_touch_retained&&!writes&&!restores&&!discards);}
 puts("PASS actual pending helper: eight persistence actions, pre-mutation completed frame, nonpersistence, presentation failures, contact/gap/PWR/exit/alarm cancel restoration, safe done and retained pending; provider completion modeled");}
