#define main inherited_quick_main
#include "quick_clock_test.c"
#undef main
int main(void) {
 scenario=8;rt=&runtime;picker.selected=0;pqa_session_init(&clock_quick);
 assert(clock_face_begin());assert(clock_face_save(2));assert(saved_face&&saved_blob[2]==2&&face_writes==1);assert(clock_face_close());
 picker.selected=2;assert(clock_face_begin());assert(clock_face_save(2));assert(face_writes==2);assert(clock_face_close());
 assert(clock_face_begin());++atomic_revision;assert(!clock_face_save(3));assert(saved_blob[2]==2&&face_writes==2);assert(clock_face_close());
 assert(clock_face_begin());assert(!clock_face_save(WATCH_FACE_COUNT));assert(clock_face_close());
 assert(clock_face_begin());assert(clock_face_close());assert(face_writes==2);
 picker.selected=0;assert(!clock_face_begin());assert(clock_face_close());
 assert(grants==ungrants&&!atomic_token&&!clock_atomic_retained);
 puts("Shared Home face USER saves: complete value, same-value intent, stale refusal, invalid selection, cancel and displayed-base mismatch PASS");
}
