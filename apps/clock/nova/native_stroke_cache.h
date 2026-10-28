/* Exact, optional four-sample stroke coverage. Owned memory, not a lease. */
#define STROKE_BYTES 65536u
#define STROKE_ENTRIES 1024u
/* Large sparse strokes are cheaper on the original row-bounded path and must
 * not evict a frame of small invariant strokes. This only limits caching. */
#define STROKE_ENTRY_BYTES 4096u
#define STROKE_HASH 2048u
typedef struct {int32_t x,y,dx,dy,r,w,h;} stroke_key;
typedef struct {stroke_key key;uint32_t begin;} stroke_entry;
typedef struct {stroke_entry entries[STROKE_ENTRIES];uint16_t hash[STROKE_HASH];uint8_t bytes[STROKE_BYTES];unsigned count,used;} stroke_store;
static stroke_store *stroke_cache;static bool stroke_attempted;
#ifndef WATCH_STROKE_ALLOC
#define WATCH_STROKE_ALLOC malloc
#define WATCH_STROKE_FREE free
#endif
#ifdef WATCH_STROKE_TEST
static bool stroke_enabled=true;static unsigned stroke_hits,stroke_misses;
#define STROKE_COUNT(n) (stroke_##n++)
#else
#define STROKE_COUNT(n) ((void)0)
#endif
static void stroke_release(void){if(stroke_cache)WATCH_STROKE_FREE(stroke_cache);stroke_cache=NULL;stroke_attempted=false;}
static unsigned stroke_hash_key(const stroke_key *k){uint32_t h=2166136261u;const unsigned char*p=(const unsigned char*)k;for(unsigned i=0;i<sizeof(*k);i++)h=(h^p[i])*16777619u;return h&(STROKE_HASH-1);}
static stroke_entry *stroke_begin(const stroke_key *key,bool *hit,unsigned *slot){
    *hit=false;
#ifdef WATCH_STROKE_TEST
    if(!stroke_enabled)return NULL;
#endif
    if(key->w<=0||key->h<=0)return NULL;
    uint64_t area=(uint64_t)(unsigned)key->w*(unsigned)key->h;
    if(area>(STROKE_ENTRY_BYTES*8u)/3u)return NULL;
    unsigned bytes=((unsigned)area*3u+7u)/8u;
    if(!stroke_attempted){stroke_attempted=true;stroke_cache=WATCH_STROKE_ALLOC(sizeof(*stroke_cache));if(stroke_cache)memset(stroke_cache,0,sizeof(*stroke_cache));}
    if(!stroke_cache)return NULL;
    unsigned s=stroke_hash_key(key);
    while(stroke_cache->hash[s]){stroke_entry *e=&stroke_cache->entries[stroke_cache->hash[s]-1];if(!memcmp(&e->key,key,sizeof(*key))){*hit=true;STROKE_COUNT(hits);return e;}s=(s+1)&(STROKE_HASH-1);}
    if(stroke_cache->count==STROKE_ENTRIES||bytes>STROKE_BYTES-stroke_cache->used){memset(stroke_cache->hash,0,sizeof(stroke_cache->hash));stroke_cache->count=stroke_cache->used=0;s=stroke_hash_key(key);}
    stroke_entry *e=&stroke_cache->entries[stroke_cache->count++];e->key=*key;e->begin=stroke_cache->used;stroke_cache->used+=bytes;memset(stroke_cache->bytes+e->begin,0,bytes);*slot=s;STROKE_COUNT(misses);return e;
}
/* Counts0..4 need three bits; preserve exact four-sample rounding at replay. */
static void stroke_put(stroke_entry *e,unsigned x,unsigned y,unsigned a){unsigned bit=(y*(unsigned)e->key.w+x)*3u;unsigned shift=bit&7u;uint8_t*p=stroke_cache->bytes+e->begin+bit/8u;unsigned mask=7u<<shift,value=(a&7u)<<shift;p[0]=(uint8_t)((p[0]&~mask)|value);if(shift>5u)p[1]=(uint8_t)((p[1]&~(mask>>8))|(value>>8));}
static unsigned stroke_get(const stroke_entry*e,unsigned x,unsigned y){unsigned bit=(y*(unsigned)e->key.w+x)*3u;unsigned shift=bit&7u;const uint8_t*p=stroke_cache->bytes+e->begin+bit/8u;unsigned value=p[0];if(shift>5u)value|=(unsigned)p[1]<<8;return (value>>shift)&7u;}
