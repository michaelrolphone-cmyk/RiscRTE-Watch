#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define WATCH_PICKER_COST_TEST
#include "../apps/clock/nova/nova.c"
static bool capture(void*p){(void)p;return true;}
static uint16_t a[466*466];
static nova_watch_state state={.time={.year=2026,.month=10,.day=13,.hour=21,.minute=37,.second=49},.time_valid=true,.battery_valid=true,.battery_percent=73,.subsecond_ms=725,.animation_ms=13579,.capture_audio=capture};
int main(void){unsigned speeds[]={1,2,8,32};for(unsigned speed=0;speed<4;speed++){nova_watch_raster_cache_release();nova_watch_picker_cache*c=nova_watch_picker_cache_create(466,466);assert(c);int positions[WATCH_FACE_CATEGORY_COUNT]={0};double total=0;picker_face_ticks=picker_composite_ticks=0;for(unsigned n=0;n<100;n++){unsigned extent=(watch_face_page_for(0)->count-1)*(466u*108u/240u),phase=(n*speeds[speed])%(2*extent);positions[0]=-(int)(phase<=extent?phase:2*extent-phase)*256;state.time.second=(48+n/20)%60;state.subsecond_ms=(n%20)*50;risc_display_surface_v1 f={0,a,466,466,932,sizeof(a),5};clock_t t=clock();assert(nova_watch_picker_collections_render(&f,&state,0,0,positions,NULL,0,256,c));total+=1000.0*(clock()-t)/CLOCKS_PER_SEC;}printf("speed%u total_ms=%.3f face_ms=%.3f compose_ms=%.3f other_ms=%.3f\n",speeds[speed],total/100,1000.0*picker_face_ticks/CLOCKS_PER_SEC/100,1000.0*picker_composite_ticks/CLOCKS_PER_SEC/100,(total-1000.0*(picker_face_ticks+picker_composite_ticks)/CLOCKS_PER_SEC)/100);nova_watch_picker_cache_destroy(c);}nova_watch_raster_cache_release();}
