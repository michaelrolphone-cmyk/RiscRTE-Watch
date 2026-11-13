/* Production shared Home controller/rendering with Runtime/hardware doubles. */
#define WATCH_RESIDENT_HOST
#define WATCH_TEST_RUNTIME_EXTERN
#define main inherited_quick_fixture_main
#include "tests/quick_clock_test.c"
#undef main
#include "PortableQuickLayout.h"
static bool resident_touch(void *context,risc_touch_snapshot_v1 *out) {
 (void)context;*out=(risc_touch_snapshot_v1){.width=TEST_WIDTH,.height=TEST_HEIGHT};
 if(!ready)return true;
 unsigned age=clock_ms-ready_time;pqa_layout layout=pqa_layout_for(&clock_quick.ui);
 int x=0,y=0;bool down=false;
 if(age>=650&&age<730){down=true;x=layout.slider_right;y=layout.slider_y[0]+pqa_layout_size(&layout,7);}
 if(age>=1000&&age<1080){down=true;x=layout.handle.x+layout.handle.w/2;y=layout.handle.y+layout.handle.h/2;}
 if(down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){1,0,(uint16_t)x,(uint16_t)y};}
 return true;
}
static risc_runtime_api_v1 host_runtime;
static risc_resident_callbacks_v1 host_callbacks;
static unsigned host_registered,host_runs,host_retains,host_exits;
static bool host_replaced;
static const char *host_case="controls";
static uint32_t resident_key_event;
static bool resident_key(void *context,uint32_t*out){(void)context;*out=resident_key_event;resident_key_event=0;return true;}
static bool is_case(const char *name){return !strcmp(host_case,name);}
static int32_t resident_sleep_begin(uint64_t *token){assert(!held&&!owned&&!pending&&!touch.subscription);*token=77;return RISC_RUNTIME_SLEEP_OK;}
static int32_t resident_sleep_end(uint64_t token){assert(token==77&&!held&&!owned&&!pending);return RISC_RUNTIME_SLEEP_OK;}
static bool retain_host(void){host_retains++;return true;}
static int32_t register_host(uint64_t invocation,const risc_resident_callbacks_v1 *callbacks){
 assert(invocation==7&&callbacks&&callbacks->dispatch);host_callbacks=*callbacks;host_registered++;return RISC_RESIDENT_OK;
}
static int32_t exit_child(uint64_t invocation){assert(invocation==7);host_exits++;return RISC_RESIDENT_OK;}
static int32_t run_child(uint64_t invocation,const char *path,risc_resident_result_v1 *result){
 assert(invocation==7&&!path&&result->struct_size==sizeof(*result));
 assert(!owned&&!pending&&!touch.subscription);host_runs++;
 if(is_case("no-pending"))return RISC_RESIDENT_NO_PENDING;
 if(is_case("retained"))return RISC_RESIDENT_RETAINED;
 if(is_case("unknown"))return -99;
 if(is_case("failed")) {host_callbacks.failed(NULL,NULL);clock_resident_sleep_pending=true;return RISC_RESIDENT_FAILED;}
 if(is_case("replaced")){host_replaced=true;return RISC_RESIDENT_OK;}
 if(host_runs>1)return RISC_RESIDENT_NO_PENDING;
 ready=true;ready_time=clock_ms;
 if(is_case("short-key")||is_case("long-key")) {
  risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLL,RISC_RESIDENT_POLL_INHIBIT_POLICY|RISC_RESIDENT_POLL_INHIBIT_IDLE,0};
  risc_resident_reply_v1 reply={sizeof(reply),0};
  resident_key_event=2;clock_ms+=100;
  assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&!clock_resident_keys&&!reply.flags);
  clock_ms+=200;resident_key_event=is_case("short-key")?2u:1u;
  assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&!reply.flags&&clock_resident_keys==(is_case("short-key")?2u:1u));
  assert(!sleeps&&!host_exits&&!submitted&&!touch.subscription);
  request.flags=0;
  assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&(reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST));
  request.reason=RISC_RESIDENT_CHECKPOINT_POLICY;
  int32_t rc=host_callbacks.dispatch(NULL,&request,&reply);
  assert(!clock_resident_keys&&!touch.subscription&&!owned&&!pending);
  if(is_case("short-key"))assert(rc==RISC_RESIDENT_OK&&sleeps==1&&!host_exits);
  else assert(rc==RISC_RESIDENT_EXIT&&!sleeps&&host_exits==1);
  launched=true;result->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
 }
 risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLL,RISC_RESIDENT_POLL_ACTIVITY,0};
 risc_resident_reply_v1 reply={sizeof(reply),0};
 assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&reply.flags==0);
 request.reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;request.flags=0;
 assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK);
 assert(reply.flags==(RISC_RESIDENT_REPLY_REDRAW|RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED));
 assert(!owned&&!pending&&!touch.subscription&&submitted);
 launched=true;result->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
}
static bool get_host(struct risc_resident_client_v1 *client){
 assert(client->struct_size>=sizeof(*client));
 *client=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*client),.invocation=7,.role=RISC_RESIDENT_ROLE_HOST,.register_shell=register_host,.run_foreground=run_child,.request_foreground_exit=exit_child};return true;
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return host_replaced?&runtime:&host_runtime;}
int main(int argc,char **argv){
 if(argc>1)host_case=argv[1];
 host_runtime=runtime;host_runtime.struct_size=sizeof(host_runtime);host_runtime.resident_shell=get_host;host_runtime.retain_invocation=retain_host;host_runtime.sleep_prepare=resident_sleep_begin;host_runtime.sleep_resume=resident_sleep_end;
 scenario=23;ta.snapshot=resident_touch;if(is_case("short-key")||is_case("long-key"))pa.key_events=resident_key;app_main();
 assert(host_registered==1&&host_runs==1&&host_exits==(is_case("long-key")?1u:0u));
 if(is_case("retained")||is_case("unknown")||is_case("replaced")) {
  assert(clock_resident_terminal&&grants>ungrants&&!owned&&!pending);
  assert(host_retains==(is_case("replaced")?0u:1u));
 } else {
  assert(!host_retains&&!owned&&!pending&&grants==ungrants&&subscribed==unsubscribed&&!allocations);
  if(is_case("controls")) {
#ifdef WATCH_ATOMIC_PREFERENCES
   assert(quick_writes==0&&!quick_saved[0]&&(clock_quick.ui.error_flags&PQA_ERROR_SAVE)&&!atomic_token&&!quick_atomic_running&&!clock_quick_scope);
#else
   assert(quick_writes==1&&!quick_saved[0]&&(clock_quick.ui.error_flags&PQA_ERROR_SAVE));
#endif
  }
  if(is_case("failed"))assert(!clock_resident_sleep_pending&&!strcmp(sleep_status,"APP FAILED TO START"));
  host_replaced=true;clock_resident_terminal=false;assert(!clock_resident_alive());
  assert(clock_resident_retain()==RISC_RESIDENT_RETAINED&&!host_retains);
 }
 printf("Resident Home %ux%u %s: native controller/custody PASS (Runtime doubles)\n",TEST_WIDTH,TEST_HEIGHT,host_case);
 return 0;
}
