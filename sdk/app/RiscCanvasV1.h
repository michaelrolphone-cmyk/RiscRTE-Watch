#pragma once
/* Proposed ui.canvas@1 definition, not an installed provider.
 * Grant-owner serialized; handles/leases/receipts are invocation-local.
 * No callback or caller metadata pointer survives a call. */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "RiscInputNavigationV1.h"
#define RISC_CANVAS_CAPABILITY "ui.canvas"
/* Same table ABI, separately selectable input-free dependency closure. */
#define RISC_CANVAS_RASTER_CAPABILITY "ui.canvas.raster"
#define RISC_CANVAS_API_V1 1u
#define RISC_CANVAS_TAG UINT32_C(0x43415631)
enum { RISC_CANVAS_OK=0, RISC_CANVAS_IDLE=1, RISC_CANVAS_AGAIN=2,
 RISC_CANVAS_INVALID=-1, RISC_CANVAS_STALE=-2, RISC_CANVAS_BUSY=-3,
 RISC_CANVAS_UNAVAILABLE=-4, RISC_CANVAS_UNSUPPORTED=-5,
 RISC_CANVAS_RETAINED=-9 };
/* Logical content encodings, never physical panel choices. MONO1 is MSB-first,
 * one=white; RGBA8 is byte-order R,G,B,A, straight alpha, sRGB color channels.
 * Providers report unsupported encodings, not implicit lossy conversion. */
/* RGB565_ADAPTIVE explicitly permits provider-owned grayscale/dither on a
 * monochrome output. RGB565 bytes are little-endian. Color-capable providers
 * preserve every RGB565 sample exactly. Existing RGBA8 remains strict color;
 * adding this intent does not silently change its unsupported-format result. */
enum { RISC_CANVAS_MONO1=1, RISC_CANVAS_RGBA8=2,
       RISC_CANVAS_RGB565_ADAPTIVE=3 };
enum { RISC_CANVAS_FIT=1, RISC_CANVAS_FILL=2, RISC_CANVAS_RESPONSIVE=3 };
enum { RISC_CANVAS_QUEUED=1, RISC_CANVAS_PRESENTING=2,
 RISC_CANVAS_COMPLETE=3, RISC_CANVAS_SUPERSEDED=4,
 RISC_CANVAS_FAILED=5, RISC_CANVAS_CUSTODY_RETAINED=6 };
enum { RISC_CANVAS_CONTACT=1, RISC_CANVAS_NAVIGATION=2,
 RISC_CANVAS_GEOMETRY_CHANGED=3, RISC_CANVAS_INPUT_GAP=4,
 RISC_CANVAS_CONTROLS_REQUEST=5 };
enum { RISC_CANVAS_DOWN=1, RISC_CANVAS_MOVE=2, RISC_CANVAS_UP=3,
 RISC_CANVAS_CANCEL=4 };
typedef struct { int32_t x,y; uint32_t width,height; } risc_canvas_rect_v1;
typedef struct {
 uint32_t struct_size, encoding, layout, logical_width, logical_height;
} risc_canvas_open_v1;
/* Responsive uses zero requested dimensions; geometry supplies logical extent.
 * FIT/FILL use positive content dimensions. The visible rectangle may clip
 * content for FILL. Coordinates refer to content, not a physical display. */
typedef struct {
 uint32_t struct_size, revision, width, height;
 risc_canvas_rect_v1 visible;
} risc_canvas_geometry_v1;
typedef struct {
 uint32_t struct_size, geometry_revision, encoding, width, height, stride;
 uint64_t lease;
 void *pixels;
 size_t byte_count;
} risc_canvas_frame_v1;
typedef struct {
 uint32_t struct_size, state;
 uint64_t receipt;
 int32_t error;
 uint32_t reserved;
} risc_canvas_receipt_v1;
typedef struct {
 uint32_t struct_size, kind, geometry_revision, phase;
 uint64_t sequence, contact;
 int32_t x,y;
 risc_input_navigation_frame_v1 navigation;
 uint32_t reserved;
} risc_canvas_event_v1;
typedef struct {
 uint32_t api_version, struct_size, tag;
 void *context;
 int32_t (*open)(void*,const risc_canvas_open_v1*,uint64_t*);
 int32_t (*geometry)(void*,uint64_t,risc_canvas_geometry_v1*);
 int32_t (*begin_frame)(void*,uint64_t,risc_canvas_frame_v1*);
 int32_t (*submit)(void*,uint64_t,uint64_t,uint64_t*);
 int32_t (*cancel_frame)(void*,uint64_t,uint64_t);
 int32_t (*poll)(void*,uint64_t,uint64_t,risc_canvas_receipt_v1*);
 int32_t (*forget_receipt)(void*,uint64_t,uint64_t);
 int32_t (*next)(void*,uint64_t,risc_canvas_event_v1*);
 /* Two-call snapshot: NULL/zero queries count; insufficient capacity returns
  * AGAIN and required count without partial records. Sequence binds snapshot
  * to next-event position. No fixed contact or queue capacity in this ABI. */
 int32_t (*contacts)(void*,uint64_t,risc_canvas_event_v1*,size_t,size_t*,uint64_t*);
 int32_t (*quiesce)(void*,uint64_t);
 int32_t (*resume)(void*,uint64_t);
 int32_t (*close)(void*,uint64_t);
} risc_canvas_api_v1;
static inline const risc_canvas_api_v1* risc_canvas_get_v1(const void *p,size_t bytes){
 if(!p||bytes<sizeof(risc_canvas_api_v1))return NULL;
 const risc_canvas_api_v1 *a=(const risc_canvas_api_v1*)p;
 if(a->api_version!=1||a->struct_size<sizeof(*a)||a->struct_size>bytes||a->tag!=RISC_CANVAS_TAG)return NULL;
 return a->open&&a->geometry&&a->begin_frame&&a->submit&&a->cancel_frame&&a->poll&&a->forget_receipt&&a->next&&a->contacts&&a->quiesce&&a->resume&&a->close?a:NULL;
}

/* Optional presentation semantics negotiated during open. The v1 table and
 * base request remain byte-for-byte unchanged; old providers ignore the tail
 * and leave presentation_out NULL. No caller pointer survives open(). */
#define RISC_CANVAS_PRESENTATION_TAG UINT32_C(0x43505231)
enum { RISC_CANVAS_PRESENT_DEFAULT=0,RISC_CANVAS_PRESENT_LOW_LATENCY=1,
 RISC_CANVAS_PRESENT_QUALITY=2,RISC_CANVAS_PRESENT_CLEAN=3 };
typedef struct {
 uint32_t struct_size,intent;
 const risc_canvas_rect_v1 *damage;
 size_t damage_count;
} risc_canvas_present_v1;
enum { RISC_CANVAS_PRESENT_RETAINS_IMAGE=1u,RISC_CANVAS_PRESENT_CLEAN_SUPPORTED=2u,
 RISC_CANVAS_PRESENT_HISTORY_SUPPORTED=4u };
enum { RISC_CANVAS_ORIENTATION_DEFAULT=0,RISC_CANVAS_ORIENTATION_PORTRAIT=1,RISC_CANVAS_ORIENTATION_LANDSCAPE=2 };
typedef struct {
 uint32_t struct_size,version,tag;
 void *context;
 int32_t (*submit)(void*,uint64_t,uint64_t,const risc_canvas_present_v1*,uint64_t*);
 /* Copy reconstructed prior visible content into provider history, without
  * refreshing. The same logical lease remains writable and must later be
  * submitted or cancelled. Unsupported is explicit; never invent history. */
 int32_t (*seed_previous)(void*,uint64_t,uint64_t);
 uint32_t flags; /* Actual semantic support for the opened session. */
 /* Idle leases/receipts only. Provider maps semantic orientation and reverse180
  * (0/1) to native raster/input geometry. Base v1 consumers remain unchanged. */
 int32_t (*orientation)(void*,uint64_t,uint32_t,uint32_t);
 /* Latest physically completed image in current logical encoding/geometry;
  * no lease or presentation, unchanged destination on clean refusal. */
 int32_t (*copy_completed)(void*,uint64_t,uint32_t,void*,size_t,uint32_t);
} risc_canvas_presentation_v1;
typedef struct {
 risc_canvas_open_v1 base;
 uint32_t presentation_tag;
 const risc_canvas_presentation_v1 **presentation_out;
} risc_canvas_open_presentation_v1;
static inline const risc_canvas_presentation_v1*risc_canvas_presentation_get_v1(const void*p,size_t bytes){
 const size_t prefix=offsetof(risc_canvas_presentation_v1,flags)+sizeof(uint32_t);
 if(!p||bytes<prefix)return NULL;
 const risc_canvas_presentation_v1*a=p;
 return a->struct_size>=prefix&&a->struct_size<=bytes&&a->version==1&&
  a->tag==RISC_CANVAS_PRESENTATION_TAG&&a->submit&&a->seed_previous?a:NULL;
}

static inline bool risc_canvas_has_orientation(const risc_canvas_presentation_v1*p){
 return p&&p->struct_size>=offsetof(risc_canvas_presentation_v1,orientation)+sizeof(p->orientation)&&p->orientation;
}

static inline bool risc_canvas_has_snapshot(const risc_canvas_presentation_v1*p){
 return p&&p->struct_size>=sizeof(*p)&&p->copy_completed;
}
