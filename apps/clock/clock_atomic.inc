/* Explicit color Clock controllers. Shared Quick/automatic encoders retain
 * their original origin and copied-transaction contracts. Default off. */
#include "RiscRuntimeHostControlV1.h"
static bool clock_atomic_retained;
static bool quick_atomic_running,home_pending,handoff_requested;
static unsigned quick_atomic_contact,navigation_pending;
static bool clock_quick_scope;
#define T5_APP_BUTTON_BACK 1u
static void clock_atomic_retain(void) {
 clock_atomic_retained=true;
#ifdef WATCH_RUNTIME_FEATURES
 if(!native_clock.halted)watch_runtime_halt(&native_clock);
#else
 if(rt&&rt->retain_invocation)(void)rt->retain_invocation();
#endif
}
static void clock_atomic_service(void) {
 if(risc_runtime_exit_requested_v1(rt))navigation_pending|=T5_APP_BUTTON_BACK;
}
static void clock_atomic_dispatch(void) {}
#define quick clock_quick
#define quick_radios clock_radios
#ifdef WATCH_RUNTIME_FEATURES
#define native_custody_retained (clock_atomic_retained || held || native_clock.halted)
#else
#define native_custody_retained (clock_atomic_retained || held)
#endif
#define failed clock_atomic_retained
#define portable_adapter_retain clock_atomic_retain
#define input_service clock_atomic_service
#define input_dispatch clock_atomic_dispatch
#include "quick_atomic.inc"
#ifdef WATCH_CONTEXTS_CLIENT
#define PORTABLE_CONTEXTS_CLIENT
#define contexts_before_storage() clock_background_pause()
#define contexts_before_storage_read() clock_background_pause()
#endif
#ifdef WATCH_BLE_BROADCAST
#define PORTABLE_BLE_BROADCAST
#define broadcast_before_storage() clock_background_pause()
#define broadcast_before_storage_read() clock_background_pause()
#endif
#define low_battery clock_low_battery
#include "home_automatic.inc"
#undef low_battery
#ifdef WATCH_BLE_BROADCAST
#undef PORTABLE_BLE_BROADCAST
#undef broadcast_before_storage
#undef broadcast_before_storage_read
#endif
#ifdef WATCH_CONTEXTS_CLIENT
#undef PORTABLE_CONTEXTS_CLIENT
#undef contexts_before_storage
#undef contexts_before_storage_read
#endif
#undef quick
#undef quick_radios
#undef native_custody_retained
#undef failed
#undef portable_adapter_retain
#undef input_service
#undef input_dispatch

static risc_runtime_capability_v1 clock_face_grant;
static portable_preference_atomic clock_face_edit;
static bool clock_face_close(void) {
 if(!quick_atomic_guard(NULL)||clock_face_edit.token)return false;
 if(clock_face_grant.api) {
  bool released=rt->release(&clock_face_grant);
  if(!released||clock_face_grant.api||clock_face_grant.slot||clock_face_grant.generation){clock_atomic_retain();return false;}
 }
 clock_face_grant=(risc_runtime_capability_v1){0};clock_face_edit=(portable_preference_atomic){0};return true;
}
static bool clock_face_begin(void) {
 if(!clock_face_close())return false;
 clock_face_grant=(risc_runtime_capability_v1){.struct_size=sizeof(clock_face_grant)};
 bool got=rt->acquire(PORTABLE_PREFERENCE_CAPABILITY,1,1,&clock_face_grant);
 if(!got){if(clock_face_grant.api||clock_face_grant.slot||clock_face_grant.generation)clock_atomic_retain();return false;}
 if(clock_face_grant.struct_size!=sizeof(clock_face_grant)||!clock_face_grant.slot||!clock_face_grant.generation||!portable_preference_atomic_api_valid(clock_face_grant.api)){clock_atomic_retain();return false;}
 int rc=portable_preference_atomic_begin(&clock_face_edit,clock_face_grant.api,WATCH_FACE_KEY,PORTABLE_PREFERENCE_USER,quick_atomic_guard,NULL);
 if(rc!=PORTABLE_PREFERENCE_READY){quick_atomic_result(rc);return false;}
 uint8_t prior[]={0x46,1,picker.selected,(uint8_t)(picker.selected^0xa5u)};
 if(clock_face_edit.present?(clock_face_edit.expected_size!=4||memcmp(clock_face_edit.expected,prior,4)):picker.selected!=0){clock_face_edit.valid=false;return false;}
 return true;
}
static bool clock_face_save(unsigned selected) {
 if(selected>=WATCH_FACE_COUNT)return false;
 uint8_t value[]={0x46,1,(uint8_t)selected,(uint8_t)(selected^0xa5u)};
 int rc=portable_preference_atomic_prepare(&clock_face_edit,value,4);
 if(rc!=PORTABLE_PREFERENCE_READY)return false;
 for(;;){
  rc=risc_runtime_exit_requested_v1(rt)?portable_preference_atomic_cancel(&clock_face_edit):clock_face_edit.token?portable_preference_atomic_collect(&clock_face_edit):portable_preference_atomic_submit(&clock_face_edit);
  if(rc!=RISC_ATOMIC_PENDING&&rc!=PORTABLE_PREFERENCE_READY)break;
  rt->yield_ms(1);
 }
 if(!quick_atomic_result(rc))return false;
 return rc==RISC_ATOMIC_OK;
}
static bool clock_atomic_finish(void) {
 if(!quick_atomic_guard(NULL)||!quick_atomic_close()||!home_automatic_close()||!clock_face_close()){clock_atomic_retain();return false;}
 clock_quick_scope=false;return true;
}
