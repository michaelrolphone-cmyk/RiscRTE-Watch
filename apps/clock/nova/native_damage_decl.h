/* Optional completed-image owner. The normal renderer never tests a dirty mask
 * per pixel: its primitive calls specialize clipped=false at compile time. */
#ifdef WATCH_NATIVE_DAMAGE_REPLAY
#ifndef WATCH_CONTEXTS_CLIENT
#error "Damage replay requires the existing capture/custody canvas contract"
#endif
static bool damage_allowed(unsigned,unsigned);
static bool damage_record(canvas*,unsigned,const intptr_t*,const char*);
static bool damage_barrier(canvas*);
static void damage_release(void);
#define WATCH_DAMAGE_BARRIER(c) do {if(damage_barrier(c))return;} while(0)
#else
#define WATCH_DAMAGE_BARRIER(c) ((void)0)
#endif
