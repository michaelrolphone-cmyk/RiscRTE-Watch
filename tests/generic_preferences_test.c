/* Same compiled Home/Quick controllers negotiate either provider contract.
 * All persistence writes pass a copied generic operation. Legacy KV remains
 * a read view; face_put asserts that no controller silently writes through it. */
#define main inherited_quick_main
#include "quick_clock_test.c"
#undef main

/* The legacy replay fixture reports a 240px touch device. Bind its semantic
 * controls to the real native layout, as actual providers must report matching
 * display/touch dimensions. App geometry itself is never overridden here. */
#include "PortableQuickLayout.h"
static bool native_quick_snapshot(void *context,risc_touch_snapshot_v1 *out) {
 bool ok=snapshot(context,out);out->width=TEST_WIDTH;out->height=TEST_HEIGHT;
 if(!out->contact_count)return ok;
 unsigned age=clock_ms-ready_time;
 int x=(int)out->contacts[0].x,y=(int)out->contacts[0].y;
 pqa_layout layout=pqa_layout_for(&clock_quick.ui);
 if(age<120) {x=TEST_WIDTH/2;y=y*TEST_HEIGHT/240;}
 else {
  int tile=-1;
  if(y==90){x=x==44?layout.slider_left:layout.slider_right;y=layout.slider_y[1];}
  else if(y==62){x=pqa_layout_x(&layout,x);y=layout.slider_y[0];}
  else if(y==139)tile=x==51?0:2;
  else if(y==185)tile=x==51?3:x==122?4:5;
  else if(y==222){x=layout.handle.x+layout.handle.w/2;y=layout.handle.y+layout.handle.h/2;}
  else {x=x*TEST_WIDTH/240;y=y*TEST_HEIGHT/240;}
  if(tile>=0){x=layout.tiles[tile].x+layout.tiles[tile].w/2;y=layout.tiles[tile].y+layout.tiles[tile].h/2;}
 }
 out->contacts[0].x=(uint16_t)x;out->contacts[0].y=(uint16_t)y;return ok;
}

static void setup_case(void) {
 ta.snapshot=native_quick_snapshot;scenario=8;rt=&runtime;picker.selected=0;pqa_session_init(&clock_quick);
}
static void face_success(void) {
 assert(clock_face_begin());assert(clock_face_save(2));
 assert(saved_face&&saved_blob[2]==2&&face_writes==1);assert(clock_face_close());
 picker.selected=2;assert(clock_face_begin());assert(clock_face_save(2));
 assert(face_writes==2);assert(clock_face_close());
 assert(clock_face_begin());++atomic_revision;assert(!clock_face_save(3));
 assert(saved_blob[2]==2&&face_writes==2);assert(clock_face_close());
 assert(clock_face_begin());assert(!clock_face_save(WATCH_FACE_COUNT));assert(clock_face_close());
 assert(clock_face_begin());assert(clock_face_close());assert(face_writes==2);
 picker.selected=0;assert(!clock_face_begin());assert(clock_face_close());
 assert(grants==ungrants&&!atomic_token&&!clock_atomic_retained);
}
static void failure(bool quick) {
 if(quick) {
  bool opened=quick_atomic_modal_begin();
  if(opened) {
   assert(quick_atomic_capture(PQA_VOLUME_COMMIT));
   uint8_t value=60;
   assert(!quick_atomic_save(NULL,PQA_VOLUME_KEY,&value,1,NULL));
  }
  if(!clock_atomic_retained&&!clock_quick.retained)assert(quick_atomic_close());
 } else {
  if(clock_face_begin())assert(!clock_face_save(2));
  if(!clock_atomic_retained&&!clock_quick.retained)assert(clock_face_close());
 }
 /* Sync may already have persisted before a broken collect response. Never
  * replay through any other capability, and keep uncertain custody. */
 bool submitted=preference_fault==PREF_COLLECT_IO||preference_fault==PREF_BAD_RECEIPT;
 if(!submitted||preference_flags!=4)assert(!face_writes&&!quick_writes);
 if(submitted)assert(atomic_token&&(clock_atomic_retained||clock_quick.retained));
 assert(!atomic_acquires);
}
static void partial_group(void) {
 assert(preference_flags==4);
 quick_saved[1]=true;quick_values[1]=65;
 clock_quick.volume=65;
 assert(quick_atomic_modal_begin());assert(quick_atomic_capture(PQA_SILENT));
 uint8_t zero=0,restore=65;
 preference_fault=PREF_PARTIAL;
 assert(!quick_atomic_save(NULL,PQA_VOLUME_KEY,&zero,1,&restore));
 assert(quick_saved[2]&&quick_values[2]==65&&quick_values[1]==65);
 assert(quick_writes==1&&!atomic_token&&!atomic_acquires);
 assert(quick_atomic_close());
}
int main(int argc,char **argv) {
 assert(argc==3);preference_flags=(unsigned)strtoul(argv[1],NULL,10);
 assert(preference_flags==4||preference_flags==15);setup_case();
 const char *mode=argv[2];
 if(!strcmp(mode,"quick"))assert(!inherited_quick_main());
 else if(!strcmp(mode,"face"))face_success();
 else if(!strcmp(mode,"partial"))partial_group();
 else {
  bool quick=!strncmp(mode,"quick-",6);mode+=quick?6:5;
  static const char *faults[]={"", "missing","bad-api","bad-grant","bad-flags","state-io","read-io","submit-io","collect-io","bad-receipt","dirty-missing"};
  for(unsigned i=1;i<sizeof(faults)/sizeof(*faults);i++)if(!strcmp(mode,faults[i]))preference_fault=i;
  assert(preference_fault);failure(quick);
 }
 assert(generic_acquires&&!atomic_acquires);
 printf("Generic Home preferences flags=%u %s: PASS (no legacy writer downgrade)\n",preference_flags,argv[2]);
 return 0;
}
