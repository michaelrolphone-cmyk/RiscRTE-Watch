#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../apps/clock/points_projection.h"
#include "PointsSchedule.h"
#include "PortableTimeZone.h"
static unsigned checks,interpret_calls;
static int32_t failure;
static unsigned custody_calls,custody_drop_at;
static bool custody_alive=true;
static void custody_tick(void) {
    custody_calls++;
    if(custody_drop_at&&custody_calls==custody_drop_at)custody_alive=false;
}
static bool custody_safe(void *context) { return *(bool *)context; }
static int32_t interpret(void *ctx,const risc_civil_calendar_v1 *v,int64_t *out) {
    (void)ctx;interpret_calls++;custody_tick();
    if(failure)return failure;
    uint32_t raw;
    if(!alarm_calendar_seconds(v->year,v->month,v->day,v->hour,v->minute,v->second,&raw))return -1;
    *out=INT64_C(946684800)+raw-28800;return 0;
}
static unsigned civil_calls,fail_at;
static int32_t civil_failure;
static int32_t project(void *ctx,int64_t epoch,uint32_t ns,risc_civil_calendar_v1 *out) {
    (void)ctx;civil_calls++;custody_tick();
    if(fail_at&&civil_calls>=fail_at)return civil_failure;
    if(epoch<INT64_C(946656000)||epoch>INT64_C(4102415999))return -1;
    twatch_rtc_time_v1 raw,local;
    if(!points_calendar((uint32_t)(epoch-INT64_C(946684800)+28800),&raw)||!portable_time_forward(&raw,&local))return -1;
    *out=(risc_civil_calendar_v1){.struct_size=sizeof(*out),.year=local.year,.month=local.month,.day=local.day,
        .hour=local.hour,.minute=local.minute,.second=local.second,.weekday=local.weekday,.nanoseconds=ns,.zone="America/Denver"};
    return 0;
}
static int32_t inverse(void *ctx,const risc_civil_calendar_v1 *v,risc_civil_inverse_v1 *out) {
    (void)ctx;civil_calls++;custody_tick();
    if(fail_at&&civil_calls>=fail_at)return civil_failure;
    twatch_rtc_time_v1 local={(uint16_t)v->year,v->month,v->day,v->weekday,v->hour,v->minute,v->second};
    portable_time_candidate candidates[2];
    *out=(risc_civil_inverse_v1){.struct_size=sizeof(*out),.count=portable_time_inverse(&local,candidates)};
    for(unsigned i=0;i<out->count;i++) {
        const twatch_rtc_time_v1 *raw=&candidates[i].rtc;uint32_t at;
        assert(alarm_calendar_seconds(raw->year,raw->month,raw->day,raw->hour,raw->minute,raw->second,&at));
        out->epoch[i]=INT64_C(946684800)+at-28800;
    }
    return 0;
}
static risc_civil_clock_schedule_v1 schedule={
    .base={.api_version=1,.struct_size=sizeof(schedule),.policy_version=1,.policy_tag=RISC_CIVIL_CLOCK_POLICY_TAG,
        .project=project,.interpret_rtc=interpret},
    .schedule_version=1,.schedule_tag=RISC_CIVIL_CLOCK_SCHEDULE_TAG,.inverse=inverse};
#define policy schedule.base
static void event_equal(const nova_point_event *a,const nova_point_event *b) {
    assert(a->at_rtc>=28800&&a->at_rtc-28800==b->at_rtc);
    assert(a->hour==b->hour&&a->minute==b->minute&&a->kind==b->kind);
    assert(a->source_slot==b->source_slot&&a->is_end==b->is_end&&a->day_offset==b->day_offset);
    assert(a->color_index==b->color_index&&!strcmp(a->label,b->label));checks++;
}
static void differential(points_config *cfg,points_meta *meta,uint32_t raw) {
    nova_points_state a,b;int32_t rc=123;
    risc_civil_calendar_v1 civil={.struct_size=sizeof(civil),.zone="America/Denver"};
    uint8_t before[64],after[64],mbefore[64],mafter[64];
    points_config_encode(cfg,before);points_meta_encode(meta,mbefore);
    bool old=watch_points_projection(cfg,meta,raw,&a);
    bool newer=watch_points_projection_utc(cfg,meta,WATCH_POINTS_RECORD_RTC,INT64_C(946684800)+raw-28800,&civil,&policy,&b,&rc);
    assert(old==newer&&(rc==0||(!old&&rc==-1)));
    if(old) {
        assert(a.status==b.status&&a.phase==b.phase&&a.next_count==b.next_count&&a.today_count==b.today_count);
        assert(a.previous_valid==b.previous_valid&&a.work_valid==b.work_valid);
        if(a.previous_valid)event_equal(&a.previous,&b.previous);
        if(a.work_valid){event_equal(&a.work_start,&b.work_start);event_equal(&a.work_end,&b.work_end);}
        for(unsigned i=0;i<a.next_count;i++)event_equal(&a.next[i],&b.next[i]);
        for(unsigned i=0;i<a.today_count;i++)event_equal(&a.today[i],&b.today[i]);
    }
    points_config_encode(cfg,after);points_meta_encode(meta,mafter);
    assert(!memcmp(before,after,64)&&!memcmp(mbefore,mafter,64));checks++;
}
int main(void) {
    points_config cfg=points_default_config();points_meta meta=points_default_meta();
    cfg.points[7]=(points_item){POINTS_CUSTOM_1,1,3,127,23,55,30,1,1};
    const unsigned years[]={2000,2024,2026,2038,2050,2099};
    for(unsigned y=0;y<sizeof(years)/sizeof(years[0]);y++)for(unsigned m=1;m<=12;m++)
      for(unsigned d=1;d<=watch_month_days(years[y],m);d+=3)for(unsigned h=0;h<24;h+=4) {
        uint32_t raw;assert(alarm_calendar_seconds(years[y],m,d,h,17,3,&raw));
        differential(&cfg,&meta,raw);
      }
    /* Exact DST gap/fold hours and elapsed overnight duration. */
    for(unsigned d=7;d<=10;d++)for(unsigned h=0;h<24;h++) {
        uint32_t raw;assert(alarm_calendar_seconds(2026,3,d,h,30,0,&raw));differential(&cfg,&meta,raw);
        assert(alarm_calendar_seconds(2026,11,d-6,h,30,0,&raw));differential(&cfg,&meta,raw);
    }
    /* Existing nonzero creation cutoffs filter exactly the same occurrences. */
    for(unsigned h=0;h<24;h++) {
        uint32_t raw;assert(alarm_calendar_seconds(2026,11,1,h,30,0,&raw));
        cfg.created=raw-1800;differential(&cfg,&meta,raw);
    }
    cfg.created=0;
    /* The final RTC day retains its provider-interpreted upper range for
       starts, duration ends and recovery margin, not a later UTC-only ceiling. */
    for(unsigned h=0;h<24;h++) {
        uint32_t raw;assert(alarm_calendar_seconds(2099,12,31,h,59,0,&raw));
        differential(&cfg,&meta,raw);
    }
    /* A nonzero legacy cutoff in the first eight raw hours remains a
       strict earlier cutoff for every representable UTC occurrence. */
    cfg.created=3600;
    for(unsigned h=0;h<72;h++)differential(&cfg,&meta,h*3600);
    uint8_t saved[64];points_config_encode(&cfg,saved);
    points_config decoded;assert(points_config_decode(&decoded,saved,64));
    saved[7]^=1;assert(!points_config_decode(&decoded,saved,64));
    nova_points_state out;int32_t rc;risc_civil_calendar_v1 civil={.zone="America/Denver"};
    for(int f=-9;f<=-1;f+=8) {
        failure=f;unsigned before=interpret_calls;
        assert(!watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_RTC,1793610000,&civil,&policy,&out,&rc));
        assert(rc==f&&out.status==NOVA_POINTS_ERROR&&interpret_calls==before+1);checks++;
    }
    failure=0;cfg.created=0;
    /* Terminal custody lost during recurrence is sticky: no further provider
       calls, including while the pure recurrence loops finish unwinding. */
    fail_at=civil_calls+7;civil_failure=-9;
    assert(!watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_RTC,1793610000,&civil,&policy,&out,&rc));
    assert(rc==-9&&civil_calls==fail_at&&out.status==NOVA_POINTS_ERROR);checks++;
    fail_at=0;
    /* Ordinary successful provider results cannot outlive caller custody.
       Exhaust every callback boundary from one actual complete projection. */
    cfg.created=3600;custody_calls=0;
    assert(watch_points_projection_utc_guarded(&cfg,&meta,WATCH_POINTS_RECORD_RTC,
        1793610000,&civil,&policy,&out,&rc,custody_safe,&custody_alive));
    unsigned total_calls=custody_calls;assert(total_calls>10);
    for(unsigned stop=1;stop<=total_calls;stop++) {
        custody_calls=0;custody_drop_at=stop;custody_alive=true;
        unsigned old_interpret=interpret_calls,old_civil=civil_calls;
        assert(!watch_points_projection_utc_guarded(&cfg,&meta,WATCH_POINTS_RECORD_RTC,
            1793610000,&civil,&policy,&out,&rc,custody_safe,&custody_alive));
        assert(rc==-9&&out.status==NOVA_POINTS_ERROR&&custody_calls==stop);
        assert(interpret_calls-old_interpret+civil_calls-old_civil==stop);checks++;
    }
    custody_calls=0;custody_drop_at=0;custody_alive=false;
    assert(!watch_points_projection_utc_guarded(&cfg,&meta,WATCH_POINTS_RECORD_RTC,
        1793610000,&civil,&policy,&out,&rc,custody_safe,&custody_alive));
    assert(rc==-9&&custody_calls==0);checks++;
    custody_alive=true;cfg.created=0;
    risc_civil_clock_policy_v1 prefix=policy;prefix.struct_size=sizeof(prefix);
    assert(!watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_RTC,1793610000,&civil,&prefix,&out,&rc));
    assert(out.status==NOVA_POINTS_UNAVAILABLE);checks++;
    unsigned before=interpret_calls;
    assert(watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_UTC,1793610000,&civil,NULL,&out,&rc));
    assert(interpret_calls==before&&rc==0);checks++;
    assert(!watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_RTC,
        1793610000,&civil,NULL,&out,&rc)&&out.status==NOVA_POINTS_UNAVAILABLE);checks++;
    /* Native UTC keys retain the existing signed-native epoch ceiling. */
    assert(!watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_UTC,
        INT64_C(2147483648),&civil,NULL,&out,&rc));checks++;
    cfg.created=ALARM_RTC_MAX-ALARM_RECOVERY_SECONDS;
    assert(!watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_UTC,
        1793610000,&civil,NULL,&out,&rc));checks++;
    cfg.created=0;
    /* Present empty configuration remains empty; no defaults or migration. */
    memset(cfg.points,0,sizeof(cfg.points));
    assert(watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_RTC,
        1793610000,&civil,&policy,&out,&rc)&&out.status==NOVA_POINTS_EMPTY);checks++;
    strcpy(civil.zone,"not/a-zone");
    assert(!watch_points_projection_utc(&cfg,&meta,WATCH_POINTS_RECORD_UTC,1793610000,&civil,NULL,&out,&rc));checks++;
    printf("Points legacy/UTC read-only differential: %u assertions; saved bytes unchanged\n",checks);
    return 0;
}
