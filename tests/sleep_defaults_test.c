#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/clock/WatchSleepDefaults.h"
#include "PortableQuickSession.h"

static uint8_t mode_bytes[4],idle_bytes[5],deep_bytes[5];
static uint32_t mode_size=4,idle_size=5,deep_size=5;
static int32_t mode_status,idle_status,deep_status;
static unsigned gets,idle_gets,writes,acquires,releases;
static bool absent,release_ok=true;
static int32_t get(void *context,const char *key,void *buffer,uint32_t capacity,uint32_t *size) {
    (void)context;gets++;
    const uint8_t *bytes=0;uint32_t length=0;int32_t status=RISC_KEY_VALUE_NOT_FOUND;
    if(!strcmp(key,PORTABLE_SLEEP_KEY)){bytes=mode_bytes;length=mode_size;status=mode_status;}
    if(!strcmp(key,PORTABLE_SLEEP_IDLE_KEY)){bytes=idle_bytes;length=idle_size;status=idle_status;idle_gets++;}
    if(!strcmp(key,PORTABLE_SLEEP_DEEP_KEY)){bytes=deep_bytes;length=deep_size;status=deep_status;}
    *size=length;
    if(status!=RISC_KEY_VALUE_OK)return status;
    if(length>capacity)return RISC_KEY_VALUE_BUFFER_SMALL;
    memcpy(buffer,bytes,length);return RISC_KEY_VALUE_OK;
}
static int32_t put(void *context,const char *key,const void *buffer,uint32_t size) {
    (void)context;(void)key;(void)buffer;(void)size;writes++;return RISC_KEY_VALUE_IO;
}
static risc_key_value_v1 kv={.api_version=1,.struct_size=sizeof(kv),.get=get,.put=put};
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
    assert(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&version==1&&instance==1);
    acquires++;
    if(absent)return false;
    grant->api=&kv;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
    assert(grant->api==&kv);releases++;return release_ok;
}
static const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
static void mode_record(unsigned mode) {
    mode_status=RISC_KEY_VALUE_OK;mode_size=4;
    mode_bytes[0]=0x53;mode_bytes[1]=1;mode_bytes[2]=(uint8_t)mode;mode_bytes[3]=(uint8_t)(mode^0xa5u);
}
static void timer_record(bool deep,unsigned seconds) {
    uint8_t *bytes=deep?deep_bytes:idle_bytes;
    if(deep){deep_status=RISC_KEY_VALUE_OK;deep_size=5;}
    else {idle_status=RISC_KEY_VALUE_OK;idle_size=5;}
    bytes[0]=0x54;bytes[1]=1;bytes[2]=(uint8_t)seconds;bytes[3]=(uint8_t)(seconds>>8);
    bytes[4]=(uint8_t)(bytes[2]^bytes[3]^0xa5u);
}
static void policy(void) {
    unsigned mode=77;uint32_t ms=99;
    assert(watch_sleep_load(0,&mode)==PORTABLE_SLEEP_UNAVAILABLE&&mode==WATCH_DEFAULT_SLEEP_MODE);
    assert(watch_sleep_timer_load(0,false,&ms)==PORTABLE_SLEEP_UNAVAILABLE&&ms==WATCH_DEFAULT_IDLE_MS);
    assert(watch_sleep_load(&kv,0)==PORTABLE_SLEEP_INVALID);
    assert(watch_sleep_timer_load(&kv,false,0)==PORTABLE_SLEEP_INVALID);
    for(unsigned saved=0;saved<=PORTABLE_SLEEP_HYBRID;saved++){
        mode_record(saved);assert(watch_sleep_load(&kv,&mode)==PORTABLE_SLEEP_LOADED&&mode==saved);
    }
    const int32_t failures[]={RISC_KEY_VALUE_NOT_FOUND,RISC_KEY_VALUE_BUFFER_SMALL,RISC_KEY_VALUE_IO,RISC_KEY_VALUE_BUSY};
    const int expected[]={PORTABLE_SLEEP_MISSING,PORTABLE_SLEEP_INVALID,PORTABLE_SLEEP_UNAVAILABLE,PORTABLE_SLEEP_UNAVAILABLE};
    for(unsigned i=0;i<sizeof(failures)/sizeof(*failures);i++){
        mode_status=idle_status=deep_status=failures[i];mode=99;ms=99;
        assert(watch_sleep_load(&kv,&mode)==expected[i]&&mode==WATCH_DEFAULT_SLEEP_MODE);
        assert(watch_sleep_timer_load(&kv,false,&ms)==expected[i]&&ms==WATCH_DEFAULT_IDLE_MS);
        assert(watch_sleep_timer_load(&kv,true,&ms)==expected[i]&&ms==PORTABLE_SLEEP_LIGHT_MS);
        const char *line=watch_sleep_fallback_diagnostic(expected[i]);
        assert(line&&strstr(line,"default=" WATCH_DEFAULT_SLEEP_NAME));
    }
    assert(!watch_sleep_fallback_diagnostic(PORTABLE_SLEEP_LOADED));
    for(unsigned byte=0;byte<4;byte++){
        mode_record(PORTABLE_SLEEP_LIGHT);mode_bytes[byte]^=0x80;
        assert(watch_sleep_load(&kv,&mode)==PORTABLE_SLEEP_INVALID&&mode==WATCH_DEFAULT_SLEEP_MODE);
    }
    mode_record(9);assert(watch_sleep_load(&kv,&mode)==PORTABLE_SLEEP_INVALID&&mode==WATCH_DEFAULT_SLEEP_MODE);
    for(unsigned size=0;size<4;size++){
        mode_record(1);mode_size=size;assert(watch_sleep_load(&kv,&mode)==PORTABLE_SLEEP_INVALID&&mode==WATCH_DEFAULT_SLEEP_MODE);
    }
    for(unsigned size=0;size<5;size++){
        timer_record(false,60);idle_size=size;
        assert(watch_sleep_timer_load(&kv,false,&ms)==PORTABLE_SLEEP_INVALID&&ms==WATCH_DEFAULT_IDLE_MS);
    }
    for(unsigned byte=0;byte<5;byte++){
        timer_record(false,60);idle_bytes[byte]^=0x80;
        assert(watch_sleep_timer_load(&kv,false,&ms)==PORTABLE_SLEEP_INVALID&&ms==WATCH_DEFAULT_IDLE_MS);
    }
    const unsigned valid[]={5,60,90,3600};
    for(unsigned i=0;i<sizeof(valid)/sizeof(*valid);i++){
        timer_record(false,valid[i]);assert(watch_sleep_timer_load(&kv,false,&ms)==PORTABLE_SLEEP_LOADED&&ms==valid[i]*1000u);
    }
    const unsigned invalid[]={0,4,3601,65535};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++){
        timer_record(false,invalid[i]);assert(watch_sleep_timer_load(&kv,false,&ms)==PORTABLE_SLEEP_INVALID&&ms==WATCH_DEFAULT_IDLE_MS);
    }
    const unsigned invalid_deep[]={0,1,59,3601,65535};
    for(unsigned i=0;i<sizeof(invalid_deep)/sizeof(*invalid_deep);i++){
        timer_record(true,invalid_deep[i]);
        assert(watch_sleep_timer_load(&kv,true,&ms)==PORTABLE_SLEEP_INVALID&&ms==PORTABLE_SLEEP_LIGHT_MS);
    }
    timer_record(true,123);assert(watch_sleep_timer_load(&kv,true,&ms)==PORTABLE_SLEEP_LOADED&&ms==123000);
    kv.api_version=2;assert(watch_sleep_load(&kv,&mode)==PORTABLE_SLEEP_UNAVAILABLE&&mode==WATCH_DEFAULT_SLEEP_MODE);kv.api_version=1;
    kv.struct_size=0;assert(watch_sleep_timer_load(&kv,false,&ms)==PORTABLE_SLEEP_UNAVAILABLE&&ms==WATCH_DEFAULT_IDLE_MS);kv.struct_size=sizeof(kv);
}
static void quick(void) {
    pqa_session session;pqa_session_init(&session);
    timer_record(true,123);
    /* One invocation reloads changing records. Never infer absence from 60000,
     * never reuse a stale saved value, and never perform a second idle read. */
    for(unsigned pass=0;pass<3;pass++){
        for(unsigned i=0;i<6;i++){
            timer_record(false,i?90:60);absent=false;
            if(i==2)idle_status=RISC_KEY_VALUE_NOT_FOUND;
            if(i==3)idle_status=RISC_KEY_VALUE_IO;
            if(i==4)idle_bytes[0]=0;
            if(i==5)absent=true;
            unsigned before=idle_gets;
            assert(pqa_session_load(&session,&runtime));
            assert(idle_gets-before==(absent?0u:1u));
            assert(session.idle_ms==(i<2?(i?90000u:60000u):WATCH_DEFAULT_IDLE_MS));
            assert(session.deep_ms==(absent?PORTABLE_SLEEP_LIGHT_MS:123000u));
        }
    }
    absent=false;release_ok=false;
    assert(!pqa_session_load(&session,&runtime));
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
    assert(session.retained);
    unsigned before=acquires;assert(!pqa_session_load(&session,&runtime));assert(before==acquires);
#endif
    release_ok=true;pqa_session_init(&session);idle_status=RISC_KEY_VALUE_NOT_FOUND;
    assert(pqa_session_load(&session,&runtime)&&session.idle_ms==WATCH_DEFAULT_IDLE_MS);
    assert(!writes&&gets&&releases);
}
int main(void) {
    policy();quick();
    printf("Sleep defaults %s/%us: valid preferences, missing/invalid/read failures, independent deep timer, repeated loads, one read, release failure and no writes PASS\n",WATCH_DEFAULT_SLEEP_NAME,(unsigned)WATCH_DEFAULT_IDLE_SECONDS);
    return 0;
}
