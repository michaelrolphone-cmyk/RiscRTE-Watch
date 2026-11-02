#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../apps/clock/points_projection.h"
#include "PointsSchedule.h"
#include "PortableTimeZone.h"
static unsigned checks,interpret_calls;
static int32_t failure;
static int32_t interpret(void *ctx,const risc_civil_calendar_v1 *v,int64_t *out) {
    (void)ctx;interpret_calls++;
    if(failure)return failure;
    uint32_t raw;
    if(!alarm_calendar_seconds(v->year,v->month,v->day,v->hour,v->minute,v->second,&raw))return -1;
    *out=INT64_C(946684800)+raw-28800;return 0;
}
static risc_civil_clock_policy_v1 policy={.interpret_rtc=interpret};
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
    assert(old==newer&&rc==0);
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
    failure=0;cfg.created=0;unsigned before=interpret_calls;
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
