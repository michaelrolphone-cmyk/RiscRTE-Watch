#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
static unsigned damage_allocs,damage_frees,fail_allocation,retain_allocation;
static bool authority_lost;
#ifdef WATCH_NATIVE_DAMAGE_REPLAY
static void *test_damage_alloc(size_t bytes) {
    assert(!authority_lost);unsigned n=++damage_allocs;
    void *p=n==fail_allocation?NULL:malloc(bytes);
    if(n==retain_allocation)authority_lost=true;
    return p;
}
static void test_damage_free(void*p){assert(!authority_lost);if(p)damage_frees++;free(p);}
#define WATCH_DAMAGE_ALLOC test_damage_alloc
#define WATCH_DAMAGE_FREE test_damage_free
#endif
#ifndef NOVA_SOURCE
#define NOVA_SOURCE "../apps/clock/nova/nova.c"
#endif
#include NOVA_SOURCE
#define EXPORT __attribute__((visibility("default")))
EXPORT bool probe_face(risc_display_surface_v1*s,const nova_watch_state*t,unsigned id){return nova_watch_face_render(s,t,id);}
EXPORT void probe_release(void){nova_watch_raster_cache_release();}
EXPORT void probe_control(unsigned fail,unsigned retained,bool lost){fail_allocation=fail;retain_allocation=retained;authority_lost=lost;}
EXPORT void probe_info(uint64_t*out){
 out[0]=damage_allocs;out[1]=damage_frees;out[2]=authority_lost;
#ifdef WATCH_NATIVE_DAMAGE_REPLAY
 out[7]=damage_owner?damage_owner->unsupported_backoff:0;out[3]=damage_owner&&damage_owner->valid;out[4]=sizeof(damage_store);out[5]=damage_owner&&damage_owner->image?(uint64_t)damage_owner->width*damage_owner->height*2:0;out[6]=damage_attempted;
#else
 out[3]=out[4]=out[5]=out[6]=0;
#endif
}
EXPORT const void*probe_image(void){
#ifdef WATCH_NATIVE_DAMAGE_REPLAY
 return damage_owner?damage_owner->image:NULL;
#else
 return NULL;
#endif
}
EXPORT bool probe_picker(risc_display_surface_v1*s,const nova_watch_state*t,unsigned n){
 nova_watch_picker_cache*p=nova_watch_picker_cache_create(s->width,s->height);if(!p)return false;
 int positions[WATCH_FACE_CATEGORY_COUNT]={0};positions[0]=-(int)(n*256);
 bool ok=nova_watch_picker_collections_render(s,t,0,0,positions,NULL,0,256,p);
 nova_watch_picker_cache_destroy(p);return ok;
}

/* Model a future conditional stream becoming supported under the same renderer
 * selection. Actual current unknown/barrier observations are tested separately. */
EXPORT void probe_admission_selection(unsigned id) {
#ifdef WATCH_NATIVE_DAMAGE_REPLAY
 assert(damage_owner && damage_owner->unsupported_backoff);
 damage_owner->admission_id=id;
#else
 (void)id;
#endif
}
