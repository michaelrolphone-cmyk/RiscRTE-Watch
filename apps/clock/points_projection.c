/* Watch presentation of the Utilities-owned recurrence model. No I/O, output or
 * scheduling authority here. Called with copied catalog/metadata and actual RTC time. */
#include "points_projection.h"
#include "PointsSchedule.h"
#ifdef WATCH_SHARED_HOME_CIVIL
#define POINTS_UTC_PROJECTION_ONLY
#include "PointsUtcSchedule.h"
#endif
#include <string.h>
#if WATCH_POINTS_EXTENDED
static bool watch_points_custom(unsigned kind) {
    return kind==POINTS_CUSTOM_1||kind==POINTS_CUSTOM_2;
}
#endif
static void watch_points_style(const points_meta *meta,unsigned kind,nova_point_event *out) {
    out->color_index=6;out->label[0]=0;
#if WATCH_POINTS_EXTENDED
    if(!watch_points_custom(kind))return;
    unsigned index=kind-POINTS_CUSTOM_1;
    if(meta&&index<POINTS_CUSTOM_COUNT&&meta->custom[index].name[0]) {
        out->color_index=meta->custom[index].color<POINTS_COLOR_COUNT?meta->custom[index].color:6;
        unsigned n=0;while(n<NOVA_POINT_LABEL_MAX&&meta->custom[index].name[n])n++;
        if(n)memcpy(out->label,meta->custom[index].name,n);
        out->label[n]=0;
    } else {
        const char *fallback=index?"CUSTOM 2":"CUSTOM 1";unsigned n=0;
        while(n<NOVA_POINT_LABEL_MAX&&fallback[n]){out->label[n]=fallback[n];n++;}out->label[n]=0;
    }
#else
    (void)meta;(void)kind;
#endif
}

typedef struct {
#ifdef WATCH_SHARED_HOME_CIVIL
    const portable_timezone_rule *rule;
    uint32_t maximum;
    const points_utc_projection_clock *clock;
#else
    unsigned unused;
#endif
} watch_points_time;
static bool watch_points_day(const watch_points_time *time,uint32_t now,uint32_t *day) {
#ifdef WATCH_SHARED_HOME_CIVIL
    if(time&&time->rule)return points_utc_local_day_clock(time->rule,now,day,time->clock);
#else
    (void)time;
#endif
    return points_local_day(now,day);
}
static bool watch_points_for_day(const watch_points_time *time,const points_config *c,unsigned slot,uint32_t day,unsigned edge,points_event *e,uint32_t *flags) {
#ifdef WATCH_SHARED_HOME_CIVIL
    if(time&&time->rule)return points_utc_event_for_day_clock(time->rule,c,slot,day,edge,e,flags,time->maximum,time->clock);
#else
    (void)time;
#endif
    return points_event_for_day(c,slot,day,edge,e,flags);
}
static bool watch_points_project(const watch_points_time *time,const points_config *c,uint32_t now,points_projection *p) {
#ifdef WATCH_SHARED_HOME_CIVIL
    if(time&&time->rule)return points_utc_project_clock(time->rule,c,now,p,time->maximum,time->clock);
#else
    (void)time;
#endif
    return points_project(c,now,p);
}
static bool watch_points_calendar(const watch_points_time *time,uint32_t at,twatch_rtc_time_v1 *local) {
#ifdef WATCH_SHARED_HOME_CIVIL
    if(time&&time->rule) {
        portable_timezone_civil v;
        bool ok=time->clock&&time->clock->project?
            time->clock->project(time->clock->context,INT64_C(946684800)+at,&v):
            portable_timezone_utc_to_local(time->rule,INT64_C(946684800)+at,&v,NULL)==PORTABLE_TIMEZONE_OK;
        if(!ok)return false;
        *local=(twatch_rtc_time_v1){(uint16_t)v.year,v.month,v.day,v.weekday,v.hour,v.minute,v.second};
        return true;
    }
#else
    (void)time;
#endif
    twatch_rtc_time_v1 raw;
    return points_calendar(at,&raw)&&portable_time_forward(&raw,local);
}

static bool watch_points_event(const watch_points_time *time,const points_event *e,const points_meta *meta,uint32_t today,nova_point_event *out) {
    twatch_rtc_time_v1 local;uint32_t day;
    if(!watch_points_calendar(time,e->deadline,&local)||
       !watch_points_day(time,e->deadline,&day))return false;
    *out=(nova_point_event){.at_rtc=e->deadline,.hour=local.hour,.minute=local.minute,.kind=e->kind,
        .source_slot=e->slot,.is_end=e->edge!=0,.day_offset=(int16_t)((int32_t)day-(int32_t)today)};
    watch_points_style(meta,e->kind,out);return true;
}
static bool watch_points_projection_core(const watch_points_time *time,const points_config *config,const points_meta *meta,uint32_t now,nova_points_state *out) {
    nova_points_state v={.now_rtc=now,.status=NOVA_POINTS_EMPTY};
    uint32_t today;points_projection projection;
    if(!config||!watch_points_day(time,now,&today)||!watch_points_project(time,config,now,&projection)) {
        v.status=NOVA_POINTS_ERROR;*out=v;return false;
    }
    if(!config->revision){*out=v;return true;}
    v.status=NOVA_POINTS_READY;
    v.next_count=(uint8_t)projection.count;v.previous_valid=projection.has_previous;
    for(unsigned i=0;i<v.next_count;i++)if(!watch_points_event(time,&projection.next[i],meta,today,&v.next[i]))goto invalid;
    if(v.previous_valid) {
        if(!watch_points_event(time,&projection.previous,meta,today,&v.previous))goto invalid;
        unsigned kind=projection.previous.kind;
        v.phase=projection.previous.edge?NOVA_PHASE_UNKNOWN:
            kind==POINTS_WORK_START?NOVA_PHASE_WORKING:kind==POINTS_WORK_END?NOVA_PHASE_OFF_WORK:
            kind==POINTS_LUNCH?NOVA_PHASE_LUNCH:kind==POINTS_BREAK?NOVA_PHASE_BREAK:
            kind==POINTS_BEDTIME?NOVA_PHASE_WIND_DOWN:NOVA_PHASE_UNKNOWN;
    }
    /* Endpoints after midnight belong to yesterday's selected start weekday. */
    for(int offset=-1;offset<=0;offset++) {
        int day=(int)today+offset;if(day<1)continue;
        for(unsigned slot=0;slot<POINTS_MAX;slot++)for(unsigned edge=0;edge<2;edge++) {
            points_event e;nova_point_event item;
            if(!watch_points_for_day(time,config,slot,(uint32_t)day,edge,&e,NULL)||
               !watch_points_event(time,&e,meta,today,&item)||item.day_offset!=0)continue;
            if(v.today_count>=NOVA_POINTS_TODAY_MAX)goto invalid;
            unsigned at=v.today_count++;
            while(at&&v.today[at-1].at_rtc>item.at_rtc){v.today[at]=v.today[at-1];--at;}
            v.today[at]=item;
        }
    }
    /* Use a real current/upcoming Work Start followed by its first Work End.
     * An unpaired start never produces an invented workday progress bar. */
    points_event best_start={0},best_end={0};bool paired=false;unsigned best_rank=5;
    for(int offset=-1;offset<=7;offset++) {
        int day=(int)today+offset;if(day<1||day>36525)continue;
        for(unsigned slot=0;slot<POINTS_MAX;slot++) {
            if(config->points[slot].kind!=POINTS_WORK_START)continue;
            points_event start,end={0};bool found_end=false;
            if(!watch_points_for_day(time,config,slot,(uint32_t)day,0,&start,NULL))continue;
            for(unsigned ahead=0;ahead<=1;ahead++)for(unsigned j=0;j<POINTS_MAX;j++) {
                points_event e;if(config->points[j].kind!=POINTS_WORK_END||
                    !watch_points_for_day(time,config,j,(uint32_t)day+ahead,0,&e,NULL)||e.deadline<=start.deadline)continue;
                if(!found_end||e.deadline<end.deadline){end=e;found_end=true;}
            }
            if(!found_end)continue;
            unsigned rank=start.deadline<=now&&end.deadline>now?0:
                (uint32_t)day==today&&start.deadline>now?1:
                (uint32_t)day==today&&end.deadline<=now?2:start.deadline>now?3:4;
            if(rank==4)continue;
            if(!paired||rank<best_rank||(rank==best_rank&&
               (rank==2?start.deadline>best_start.deadline:start.deadline<best_start.deadline))) {
                best_start=start;best_end=end;paired=true;best_rank=rank;
            }
        }
    }
    if(paired) {
        v.work_valid=watch_points_event(time,&best_start,meta,today,&v.work_start)&&watch_points_event(time,&best_end,meta,today,&v.work_end);
    }
    if(v.previous_valid&&projection.previous.edge)
        v.phase=v.work_valid&&v.work_start.at_rtc<=now&&now<v.work_end.at_rtc?NOVA_PHASE_WORKING:NOVA_PHASE_UNKNOWN;
    /* A shorter overlapping duration ending must reveal any still-active
     * duration. Custom durations remain semantically custom rather than BREAK. */
    points_event active_start={0};bool has_active_duration=false;
    for(int offset=-1;offset<=0;offset++) {
        int day=(int)today+offset;if(day<1)continue;
        for(unsigned slot=0;slot<POINTS_MAX;slot++) {
            points_event start,end;
            if(!config->points[slot].duration_minutes||
               !watch_points_for_day(time,config,slot,(uint32_t)day,0,&start,NULL)||
               !watch_points_for_day(time,config,slot,(uint32_t)day,1,&end,NULL)||
               start.deadline>now||end.deadline<=now)continue;
            if(!has_active_duration||points_event_before(&active_start,&start)) {
                active_start=start;has_active_duration=true;
            }
        }
    }
    if(has_active_duration)v.phase=active_start.kind==POINTS_LUNCH?NOVA_PHASE_LUNCH:
        active_start.kind==POINTS_BREAK?NOVA_PHASE_BREAK:NOVA_PHASE_UNKNOWN;
    if(!v.next_count&&!v.previous_valid&&!v.today_count)v.status=NOVA_POINTS_EMPTY;
    /* Include metadata revision in the presentation stamp so custom label/color
     * edits invalidate schedule cards without changing recurrence. */
    uint32_t meta_revision=0;
#if WATCH_POINTS_EXTENDED
    if(meta)meta_revision=meta->revision;
#else
    (void)meta;
#endif
    v.revision=config->revision ^ (meta_revision*2166136261u) ^ (today*16777619u) ^
        (v.previous_valid?v.previous.at_rtc:0) ^ (v.next_count?v.next[0].at_rtc:0) ^ (uint32_t)v.status;
    *out=v;return true;
invalid:
    v=(nova_points_state){.now_rtc=now,.status=NOVA_POINTS_ERROR};*out=v;return false;
}

bool watch_points_projection(const points_config *config,const points_meta *meta,uint32_t now,nova_points_state *out) {
    if(!out)return false;
    return watch_points_projection_core(NULL,config,meta,now,out);
}
#ifdef WATCH_SHARED_HOME_CIVIL
typedef struct {
    const risc_civil_clock_policy_v1 *policy;
    const risc_civil_clock_schedule_v1 *schedule;
    int32_t status;
    bool (*safe)(void *);
    void *safe_context;
} watch_points_policy_context;
static bool watch_points_policy_safe(watch_points_policy_context *c) {
    if(c->status)return false;
    if(c->safe&&!c->safe(c->safe_context)){c->status=-9;return false;}
    return true;
}
static int32_t watch_points_policy_interpret(watch_points_policy_context *c,
    const risc_civil_calendar_v1 *value,int64_t *epoch) {
    if(!watch_points_policy_safe(c))return c->status;
    int32_t rc=c->policy->interpret_rtc(c->policy->context,value,epoch);
    /* Test custody before accepting even an ordinary successful result. */
    if(!watch_points_policy_safe(c))return c->status;
    c->status=rc;return rc;
}
static bool watch_points_policy_project(void *context,int64_t epoch,portable_timezone_civil *out) {
    watch_points_policy_context *c=context;if(!watch_points_policy_safe(c))return false;
    risc_civil_calendar_v1 value={.struct_size=sizeof(value)};
    int32_t rc=c->policy->project(c->policy->context,epoch,0,&value);
    if(!watch_points_policy_safe(c))return false;
    c->status=rc;if(c->status)return false;
    portable_timezone_civil civil={value.year,value.month,value.day,value.hour,value.minute,value.second,value.weekday};
    int64_t checked;
    if(value.struct_size<sizeof(value)||value.reserved||value.nanoseconds||value.year<2000||value.year>2099||
       portable_timezone_civil_to_epoch(&civil,&checked)!=PORTABLE_TIMEZONE_OK){c->status=-1;return false;}
    *out=civil;return true;
}
static int32_t watch_points_policy_inverse(void *context,const portable_timezone_civil *local,int64_t epoch[2],uint32_t *count) {
    watch_points_policy_context *c=context;if(!watch_points_policy_safe(c))return c->status;
    risc_civil_calendar_v1 value={.struct_size=sizeof(value),.year=local->year,.month=local->month,
        .day=local->day,.hour=local->hour,.minute=local->minute,.second=local->second,.weekday=local->weekday};
    risc_civil_inverse_v1 result={.struct_size=sizeof(result)};
    int32_t rc=c->schedule->inverse(c->policy->context,&value,&result);
    if(!watch_points_policy_safe(c))return c->status;
    c->status=rc;if(c->status)return c->status;
    if(result.struct_size<sizeof(result)||result.count>2||
       (result.count==2&&result.epoch[0]>=result.epoch[1])){c->status=-1;return -1;}
    *count=result.count;for(unsigned i=0;i<result.count;i++)epoch[i]=result.epoch[i];
    return 0;
}
bool watch_points_projection_utc_guarded(const points_config *config,const points_meta *meta,
    watch_points_record_basis basis,int64_t epoch,const risc_civil_calendar_v1 *civil,
    const risc_civil_clock_policy_v1 *policy,nova_points_state *out,int32_t *policy_status,
    bool (*safe)(void *),void *safe_context) {
    if(policy_status)*policy_status=0;
    if(!out)return false;
    *out=(nova_points_state){.status=NOVA_POINTS_ERROR};
    if(!config||!civil||!policy_status||epoch<INT64_C(946684800)||
       epoch>INT64_C(946684800)+ALARM_RTC_MAX||
       (basis!=WATCH_POINTS_RECORD_RTC&&basis!=WATCH_POINTS_RECORD_UTC))return false;
    if(basis==WATCH_POINTS_RECORD_RTC&&!policy) {
        out->status=NOVA_POINTS_UNAVAILABLE;return false;
    }
    if(basis==WATCH_POINTS_RECORD_UTC&&(epoch>INT32_MAX||
       config->created>(uint32_t)(INT32_MAX-INT64_C(946684800))-ALARM_RECOVERY_SECONDS))return false;
    portable_timezone_rule rule={0};
    if(basis==WATCH_POINTS_RECORD_UTC&&
       portable_timezone_resolve(civil->zone,sizeof(civil->zone),&rule)!=PORTABLE_TIMEZONE_OK)return false;
    watch_points_policy_context provider={.policy=policy,.safe=safe,.safe_context=safe_context};
    if(!watch_points_policy_safe(&provider)){*policy_status=provider.status;return false;}
    if(basis==WATCH_POINTS_RECORD_RTC) {
        provider.schedule=risc_civil_clock_get_schedule(policy,policy->struct_size);
        if(!provider.schedule){out->status=NOVA_POINTS_UNAVAILABLE;return false;}
    }
    points_config copy=*config;
    if(basis==WATCH_POINTS_RECORD_RTC&&copy.created) {
        twatch_rtc_time_v1 raw;
        if(!policy||!policy->interpret_rtc||!points_calendar(copy.created,&raw))return false;
        risc_civil_calendar_v1 value={.struct_size=sizeof(value),.year=raw.year,.month=raw.month,
            .day=raw.day,.hour=raw.hour,.minute=raw.minute,.second=raw.second,.weekday=raw.weekday};
        int64_t created=0;
        *policy_status=watch_points_policy_interpret(&provider,&value,&created);
        if(*policy_status||created>INT64_C(946684800)+ALARM_RTC_MAX)return false;
        /* A cutoff before the representable UTC recurrence domain excludes no
         * representable occurrence. Preserve the original record untouched. */
        copy.created=created<=INT64_C(946684800)?0u:(uint32_t)(created-INT64_C(946684800));
    }
    uint32_t maximum=(uint32_t)(INT32_MAX-INT64_C(946684800));
    if(basis==WATCH_POINTS_RECORD_RTC) {
        /* Derive the legacy absolute record domain from provider policy.
         * Never assume a device-specific RTC offset inside the application. */
        if(!policy||!policy->interpret_rtc)return false;
        risc_civil_calendar_v1 last={.struct_size=sizeof(last),.year=2099,.month=12,.day=31,
            .hour=23,.minute=59,.second=59};
        int64_t upper=0;
        *policy_status=watch_points_policy_interpret(&provider,&last,&upper);
        if(*policy_status||upper<INT64_C(946684800)||upper>INT64_C(946684800)+ALARM_RTC_MAX)return false;
        maximum=(uint32_t)(upper-INT64_C(946684800));
    }
    const points_utc_projection_clock clock={&provider,watch_points_policy_project,watch_points_policy_inverse};
    const watch_points_time time={.rule=&rule,.maximum=maximum,
        .clock=basis==WATCH_POINTS_RECORD_RTC?&clock:NULL};
    bool ok=watch_points_projection_core(&time,&copy,meta,(uint32_t)(epoch-INT64_C(946684800)),out);
    (void)watch_points_policy_safe(&provider);
    if(provider.status) {
        *policy_status=provider.status;
        *out=(nova_points_state){.status=NOVA_POINTS_ERROR};return false;
    }
    return ok;
}
bool watch_points_projection_utc(const points_config *config,const points_meta *meta,
    watch_points_record_basis basis,int64_t epoch,const risc_civil_calendar_v1 *civil,
    const risc_civil_clock_policy_v1 *policy,nova_points_state *out,int32_t *policy_status) {
    return watch_points_projection_utc_guarded(config,meta,basis,epoch,civil,policy,out,
        policy_status,NULL,NULL);
}
#endif
