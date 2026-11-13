#pragma once
#include "RiscCanvasV1.h"
#include "RiscTouchV1.h"
#define RISC_CANVAS_INPUT_CAPABILITY "ui.canvas-input"
#define RISC_CANVAS_INPUT_TAG UINT32_C(0x43494e31)
#define RISC_CANVAS_INPUT_SLEEP_SUPPORTED UINT32_C(1)
typedef struct {
 uint32_t api_version,struct_size,tag;
 void *context;
 int32_t (*activate)(void*,uint64_t);
 int32_t (*deactivate)(void*,uint64_t);
 /* Optional physical input preparation; grant/dependencies remain pinned.
  * No input power suffix means UNSUPPORTED, not invented sleep readiness. */
 int32_t (*prepare_sleep)(void*,uint64_t,uint32_t);
 int32_t (*resume_sleep)(void*,uint64_t,uint32_t);
 uint32_t flags; /* Cached actual support, not merely callback presence. */
} risc_canvas_input_v1;
static inline const risc_canvas_input_v1*risc_canvas_input_get_v1(const void*p,size_t bytes){
 const size_t prefix=offsetof(risc_canvas_input_v1,deactivate)+sizeof(((risc_canvas_input_v1*)0)->deactivate);
 if(!p||bytes<prefix)return NULL;
 const risc_canvas_input_v1*a=p;
 return a->api_version==1&&a->struct_size>=prefix&&a->struct_size<=bytes&&a->tag==RISC_CANVAS_INPUT_TAG&&a->activate&&a->deactivate?a:NULL;
}
/* Provider-to-provider binding, never application hardware selection.
 * The input companion holds canonical dependencies until deactivation.
 * Existing ui.canvas v1 prefix is unchanged. */
#define RISC_CANVAS_BINDING_TAG UINT32_C(0x43424931)
typedef struct {
 uint32_t struct_size,version,tag;
 void *context;
 int32_t (*bind_inputs)(void*,uint64_t,const risc_touch_api_v1*,const risc_input_navigation_api_v1*);
 int32_t (*unbind_inputs)(void*,uint64_t);
} risc_canvas_binding_v1;
/* Query an existing session through open's tagged extension mechanism. No
 * new session, input activation, or caller pointer retention occurs. Old
 * providers reject the unknown tag cleanly; base v1 table/size are unchanged. */
typedef struct {
 risc_canvas_open_v1 base;
 uint32_t binding_tag;
 const risc_canvas_binding_v1**binding_out;
 uint64_t session;
} risc_canvas_open_input_binding_v1;
static inline const risc_canvas_binding_v1*risc_canvas_binding_get_v1(const risc_canvas_binding_v1*p){
 return p&&p->struct_size>=sizeof(*p)&&p->tag==RISC_CANVAS_BINDING_TAG&&p->version==1&&p->bind_inputs&&p->unbind_inputs?p:NULL;
}

static inline bool risc_canvas_input_sleep_supported(const risc_canvas_input_v1*p){
 return p&&p->struct_size>=sizeof(*p)&&p->prepare_sleep&&p->resume_sleep&&
  (p->flags&RISC_CANVAS_INPUT_SLEEP_SUPPORTED);
}
