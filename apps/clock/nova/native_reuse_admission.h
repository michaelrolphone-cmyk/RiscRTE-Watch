/* Predict reuse from actual canvas construction, not face/device identity.
 * A picker constructs its destination and each rerendered card independently.
 * Geometry metadata is only a performance hint; denied admission renders fully. */
#if defined(WATCH_NATIVE_COVERAGE_CACHE) || defined(WATCH_NATIVE_STROKE_CACHE)
#define REUSE_GEOMETRY_SLOTS 32u
#define REUSE_GEOMETRY_OBSERVATIONS 3u
#define REUSE_GEOMETRY_RECENCY 8u
typedef struct {uint32_t width,height,seen;unsigned repeats;} reuse_geometry;
static reuse_geometry reuse_geometries[REUSE_GEOMETRY_SLOTS];static uint32_t reuse_serial;
#if defined(WATCH_COVERAGE_TEST) || defined(WATCH_STROKE_TEST)
static bool reuse_test_observe=true;
#endif
static unsigned reuse_geometry_slot(unsigned w,unsigned h){uint32_t key=w*2654435761u;key^=h*2246822519u;key^=key>>16;return key&(REUSE_GEOMETRY_SLOTS-1u);}
static unsigned native_reuse_width(unsigned w,unsigned h){
#if defined(WATCH_COVERAGE_TEST) || defined(WATCH_STROKE_TEST)
    if(!reuse_test_observe)return w;
#endif
    uint32_t now=++reuse_serial;reuse_geometry*g=&reuse_geometries[reuse_geometry_slot(w,h)];
    if(g->width==w&&g->height==h&&(uint32_t)(now-g->seen)<=REUSE_GEOMETRY_RECENCY){if(g->repeats<REUSE_GEOMETRY_OBSERVATIONS)g->repeats++;}
    else {g->width=w;g->height=h;g->repeats=1;}
    g->seen=now;return w;
}
static bool native_reuse_allowed(unsigned w,unsigned h){const reuse_geometry*g=&reuse_geometries[reuse_geometry_slot(w,h)];return g->width==w&&g->height==h&&g->repeats>=REUSE_GEOMETRY_OBSERVATIONS&&(uint32_t)(reuse_serial-g->seen)<=REUSE_GEOMETRY_RECENCY;}
static void native_reuse_reset(void){memset(reuse_geometries,0,sizeof(reuse_geometries));reuse_serial=0;}
#else
#define native_reuse_width(w,h) (w)
#endif
