#include "PortableTimeZone.h"

/* Bounded text primitives avoid libc imports in native applications. */
static size_t text_length(const char *s,size_t capacity,size_t limit) {
    size_t i=0;
    if (!s) return limit;
    for (;i<capacity && i<limit;++i) if (!s[i]) return i;
    return limit;
}
static int equal(const char *a,const char *b,size_t n) {
    for (size_t i=0;i<n;++i) if (a[i]!=b[i]) return 0;
    return b[n]==0;
}
int portable_timezone_find(const char *id,size_t capacity) {
    size_t n=text_length(id,capacity,PORTABLE_TIMEZONE_ID_BYTES);
    if (!n || n==PORTABLE_TIMEZONE_ID_BYTES) return -1;
    if (equal(id,"Etc/UTC",n) || equal(id,"Etc/GMT",n)) return 0;
    for (unsigned i=0;i<portable_timezone_count();++i)
        if (equal(id,portable_timezone_get(i)->id,n)) return (int)i;
    return -1;
}
const char *portable_timezone_region_name(unsigned region) {
    static const char *const names[]={"UTC","Africa","America","Antarctica","Arctic",
        "Asia","Atlantic","Australia","Europe","Indian Ocean","Pacific"};
    return region<PORTABLE_TIMEZONE_REGION_COUNT?names[region]:0;
}
unsigned portable_timezone_region_count(unsigned region) {
    unsigned count=0;
    for (unsigned i=0;i<portable_timezone_count();++i)
        if ((unsigned)portable_timezone_get(i)->region==region) ++count;
    return count;
}
int portable_timezone_city_index(unsigned region,unsigned city) {
    for (unsigned i=0;i<portable_timezone_count();++i)
        if ((unsigned)portable_timezone_get(i)->region==region && !city--) return (int)i;
    return -1;
}
int portable_timezone_display_name(unsigned index,char *out,size_t capacity) {
    const portable_timezone_entry *entry=portable_timezone_get(index);
    if (!out || !capacity || !entry) return PORTABLE_TIMEZONE_INVALID;
    const char *src=entry->id;
    for (size_t i=0;src[i];++i) if (src[i]=='/') { src+=i+1; break; }
    size_t n=text_length(src,PORTABLE_TIMEZONE_ID_BYTES,PORTABLE_TIMEZONE_ID_BYTES);
    if (n>=capacity) { out[0]=0; return PORTABLE_TIMEZONE_RANGE; }
    for (size_t i=0;i<n;++i) out[i]=src[i]=='_'?' ':src[i];
    out[n]=0;
    return PORTABLE_TIMEZONE_OK;
}

typedef struct { const char *p,*end; } parser;
static int letter(char c) { return (c>='A' && c<='Z') || (c>='a' && c<='z'); }
static int digit(char c) { return c>='0' && c<='9'; }
static int consume(parser *p,char c) {
    if (p->p==p->end || *p->p!=c) return 0;
    ++p->p; return 1;
}
static int number(parser *p,int maximum,int *out) {
    int value=0,count=0;
    while (p->p<p->end && digit(*p->p)) {
        if (++count>3) return 0;
        value=value*10+(*p->p++-'0');
        if (value>maximum) return 0;
    }
    if (!count) return 0;
    *out=value; return 1;
}
static int abbreviation(parser *p) {
    unsigned count=0;
    if (consume(p,'<')) {
        while (p->p<p->end && (letter(*p->p)||digit(*p->p)||*p->p=='+'||*p->p=='-'||*p->p==':')) {
            ++p->p; if (++count>16) return 0;
        }
        return count>=2 && consume(p,'>');
    }
    while (p->p<p->end && letter(*p->p)) { ++p->p; if (++count>16) return 0; }
    return count>=3;
}
static int clock_seconds(parser *p,int max_hour,int32_t *out) {
    int sign=1,hour=0,minute=0,second=0;
    if (consume(p,'-')) sign=-1; else (void)consume(p,'+');
    if (!number(p,max_hour,&hour)) return 0;
    if (consume(p,':')) {
        if (!number(p,59,&minute)) return 0;
        if (consume(p,':') && !number(p,59,&second)) return 0;
    }
    if (max_hour==24 && hour==24 && (minute||second)) return 0;
    *out=sign*(hour*3600+minute*60+second); return 1;
}
static int transition(parser *p,portable_timezone_transition *out) {
    int month,week,weekday;
    if (!consume(p,'M') || !number(p,12,&month) || month<1 || !consume(p,'.') ||
        !number(p,5,&week) || week<1 || !consume(p,'.') || !number(p,6,&weekday)) return 0;
    out->month=(uint8_t)month; out->week=(uint8_t)week; out->weekday=(uint8_t)weekday;
    out->seconds=7200;
    return !consume(p,'/') || clock_seconds(p,167,&out->seconds);
}
static int valid_transition(const portable_timezone_transition *t) {
    return t->month>=1 && t->month<=12 && t->week>=1 && t->week<=5 &&
        t->weekday<=6 && t->seconds>=-604799 && t->seconds<=604799;
}
static int valid_rule(const portable_timezone_rule *r) {
    return r && r->standard_offset>=-86400 && r->standard_offset<=86400 &&
        r->daylight_offset>=-86400 && r->daylight_offset<=86400 && r->has_daylight<=1 &&
        (!r->has_daylight || (valid_transition(&r->start) && valid_transition(&r->end))) &&
        (!r->era_year || (r->has_daylight && r->era_year>=PORTABLE_TIMEZONE_MIN_YEAR &&
         r->era_year<=PORTABLE_TIMEZONE_MAX_YEAR && valid_transition(&r->era_start) && valid_transition(&r->era_end)));
}
int portable_timezone_parse(const char *posix,size_t capacity,portable_timezone_rule *out) {
    if (!out) return PORTABLE_TIMEZONE_INVALID;
    size_t n=text_length(posix,capacity,PORTABLE_TIMEZONE_RULE_BYTES);
    if (!n || n==PORTABLE_TIMEZONE_RULE_BYTES) return PORTABLE_TIMEZONE_BAD_RULE;
    portable_timezone_rule r={0}; parser p={posix,posix+n}; int32_t offset;
    if (!abbreviation(&p) || !clock_seconds(&p,24,&offset)) return PORTABLE_TIMEZONE_BAD_RULE;
    r.standard_offset=-offset; r.daylight_offset=r.standard_offset;
    if (p.p<p.end) {
        if (!abbreviation(&p)) return PORTABLE_TIMEZONE_BAD_RULE;
        r.has_daylight=1; r.daylight_offset=r.standard_offset+3600;
        if (p.p<p.end && *p.p!=',') {
            if (!clock_seconds(&p,24,&offset)) return PORTABLE_TIMEZONE_BAD_RULE;
            r.daylight_offset=-offset;
        }
        if (!consume(&p,',') || !transition(&p,&r.start) || !consume(&p,',') || !transition(&p,&r.end))
            return PORTABLE_TIMEZONE_BAD_RULE;
    }
    if (p.p!=p.end || !valid_rule(&r)) return PORTABLE_TIMEZONE_BAD_RULE;
    *out=r; return PORTABLE_TIMEZONE_OK;
}
static int retained_rule(const char *id,size_t capacity,portable_timezone_rule *out) {
    size_t n=text_length(id,capacity,PORTABLE_TIMEZONE_ID_BYTES);
    if(!n||n==PORTABLE_TIMEZONE_ID_BYTES)return PORTABLE_TIMEZONE_BAD_RULE;
    parser p={id,id+n};portable_timezone_rule r={0};int year=0;int32_t offset;
    if(!consume(&p,'@'))return PORTABLE_TIMEZONE_BAD_RULE;
    for(unsigned i=0;i<4;i++) {
        if(p.p==p.end||!digit(*p.p))return PORTABLE_TIMEZONE_BAD_RULE;
        year=year*10+(*p.p++-'0');
    }
    if(year<PORTABLE_TIMEZONE_MIN_YEAR||year>PORTABLE_TIMEZONE_MAX_YEAR||!consume(&p,':')||
       !clock_seconds(&p,24,&offset))return PORTABLE_TIMEZONE_BAD_RULE;
    r.standard_offset=-offset;
    if(!consume(&p,',')||!clock_seconds(&p,24,&offset))return PORTABLE_TIMEZONE_BAD_RULE;
    r.daylight_offset=-offset;r.has_daylight=1;r.era_year=(uint16_t)year;
    if(!consume(&p,';')||!transition(&p,&r.start)||!consume(&p,',')||!transition(&p,&r.end)||
       !consume(&p,';')||!transition(&p,&r.era_start)||!consume(&p,',')||!transition(&p,&r.era_end)||
       p.p!=p.end||!valid_rule(&r))return PORTABLE_TIMEZONE_BAD_RULE;
    *out=r;return PORTABLE_TIMEZONE_OK;
}
int portable_timezone_resolve(const char *id,size_t capacity,portable_timezone_rule *out) {
    if (!out) return PORTABLE_TIMEZONE_INVALID;
    if(id&&capacity&&id[0]=='@')return retained_rule(id,capacity,out);
    int index=portable_timezone_find(id,capacity);
    const portable_timezone_entry *entry=portable_timezone_get(index<0?0u:(unsigned)index);
    int rc=portable_timezone_parse(entry->posix,PORTABLE_TIMEZONE_RULE_BYTES,out);
    return rc?rc:(index<0?PORTABLE_TIMEZONE_FALLBACK:PORTABLE_TIMEZONE_OK);
}

static int leap(int32_t year) { return year%4==0 && (year%100!=0 || year%400==0); }
static unsigned month_days(int32_t year,unsigned month) {
    static const uint8_t days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month-1]+(month==2 && leap(year));
}
/* Integer Gregorian decomposition; no libc time_t or target 64-bit divide. */
static int32_t civil_days(int32_t year,unsigned month,unsigned day) {
    year-=month<=2;
    int32_t era=year/400,yoe=year-era*400;
    int32_t mp=(int32_t)month+(month>2?-3:9);
    int32_t doy=(153*mp+2)/5+(int32_t)day-1;
    return era*146097+yoe*365+yoe/4-yoe/100+doy-719468;
}
static int civil_valid(const portable_timezone_civil *c) {
    return c && c->year>=PORTABLE_TIMEZONE_MIN_YEAR && c->year<=PORTABLE_TIMEZONE_MAX_YEAR &&
        c->month>=1 && c->month<=12 && c->day>=1 && c->day<=month_days(c->year,c->month) &&
        c->hour<24 && c->minute<60 && c->second<60;
}
int portable_timezone_civil_to_epoch(const portable_timezone_civil *civil,int64_t *out) {
    if (!out || !civil_valid(civil)) return PORTABLE_TIMEZONE_INVALID;
    *out=(int64_t)civil_days(civil->year,civil->month,civil->day)*86400+
        civil->hour*3600+civil->minute*60+civil->second;
    return PORTABLE_TIMEZONE_OK;
}
static int weekday(int32_t days) { int w=(days+4)%7; return w<0?w+7:w; }
int portable_timezone_epoch_to_civil(int64_t epoch,portable_timezone_civil *out) {
    if (!out) return PORTABLE_TIMEZONE_INVALID;
    int32_t lo=civil_days(PORTABLE_TIMEZONE_MIN_YEAR,1,1);
    int32_t hi=civil_days(PORTABLE_TIMEZONE_MAX_YEAR+1,1,1);
    if (epoch<(int64_t)lo*86400 || epoch>=(int64_t)hi*86400) return PORTABLE_TIMEZONE_RANGE;
    /* At most 22 comparisons over the declared 8400-year domain. This avoids
     * compiler __divdi3 imports and blanket libgcc native-link dependencies. */
    while (hi-lo>1) { int32_t mid=lo+(hi-lo)/2; if ((int64_t)mid*86400<=epoch) lo=mid; else hi=mid; }
    int32_t seconds=(int32_t)(epoch-(int64_t)lo*86400),z=lo+719468;
    int32_t era=z/146097,doe=z-era*146097;
    int32_t yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
    int32_t year=yoe+era*400,doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;
    portable_timezone_civil c;
    c.day=(uint8_t)(doy-(153*mp+2)/5+1); c.month=(uint8_t)(mp+(mp<10?3:-9));
    c.year=year+(c.month<=2); c.weekday=(uint8_t)weekday(lo);
    c.hour=(uint8_t)(seconds/3600); c.minute=(uint8_t)((seconds%3600)/60); c.second=(uint8_t)(seconds%60);
    *out=c; return PORTABLE_TIMEZONE_OK;
}
static int64_t transition_epoch(int32_t year,const portable_timezone_transition *t,int32_t before_offset) {
    unsigned day=1u+(t->weekday+7u-(unsigned)weekday(civil_days(year,t->month,1)))%7u+7u*(t->week-1u);
    if (day>month_days(year,t->month)) day-=7;
    return (int64_t)civil_days(year,t->month,day)*86400+t->seconds-before_offset;
}
static int rule_offset(const portable_timezone_rule *r,int64_t epoch,int32_t *offset,uint8_t *daylight) {
    portable_timezone_civil utc;
    int status=portable_timezone_epoch_to_civil(epoch,&utc);
    if (status) return status;
    *daylight=0; *offset=r->standard_offset;
    if (r->has_daylight) {
        int64_t latest=INT64_MIN;
        /* Check adjacent years as offsets/signed transition times can cross
         * New Year; southern seasons then follow naturally from last event. */
        for (int32_t year=utc.year-1;year<=utc.year+1;++year) {
            const portable_timezone_transition *start_rule=r->era_year&&year>=r->era_year?&r->era_start:&r->start;
            const portable_timezone_transition *end_rule=r->era_year&&year>=r->era_year?&r->era_end:&r->end;
            int64_t start=transition_epoch(year,start_rule,r->standard_offset);
            int64_t end=transition_epoch(year,end_rule,r->daylight_offset);
            if (start<=epoch && start>latest) { latest=start; *daylight=1; }
            if (end<=epoch && end>=latest) { latest=end; *daylight=0; }
        }
        if (*daylight) *offset=r->daylight_offset;
    }
    return PORTABLE_TIMEZONE_OK;
}
int portable_timezone_utc_to_local(const portable_timezone_rule *rule,int64_t epoch,
        portable_timezone_civil *out,portable_timezone_candidate *details) {
    if (!out) return PORTABLE_TIMEZONE_INVALID;
    if (!valid_rule(rule)) return PORTABLE_TIMEZONE_BAD_RULE;
    int32_t offset; uint8_t daylight;
    int rc=rule_offset(rule,epoch,&offset,&daylight);
    if (rc) return rc;
    rc=portable_timezone_epoch_to_civil(epoch+offset,out);
    if (!rc && details) { details->epoch=epoch; details->offset=offset; details->daylight=daylight; }
    return rc;
}
static int civil_equal(const portable_timezone_civil *a,const portable_timezone_civil *b) {
    return a->year==b->year && a->month==b->month && a->day==b->day &&
        a->hour==b->hour && a->minute==b->minute && a->second==b->second;
}
int portable_timezone_local_to_utc(const portable_timezone_rule *rule,
        const portable_timezone_civil *civil,portable_timezone_inverse *out) {
    if (!out) return PORTABLE_TIMEZONE_INVALID;
    out->count=0;
    if (!valid_rule(rule)) return PORTABLE_TIMEZONE_BAD_RULE;
    int64_t wall; int rc=portable_timezone_civil_to_epoch(civil,&wall);
    if (rc) return rc;
    unsigned range_failures=0,attempts=rule->has_daylight && rule->daylight_offset!=rule->standard_offset?2u:1u;
    for (unsigned i=0;i<attempts;++i) {
        int64_t epoch=wall-(i?rule->daylight_offset:rule->standard_offset);
        portable_timezone_civil back; portable_timezone_candidate candidate;
        rc=portable_timezone_utc_to_local(rule,epoch,&back,&candidate);
        if (rc==PORTABLE_TIMEZONE_RANGE) ++range_failures;
        if (!rc && civil_equal(civil,&back)) out->candidate[out->count++]=candidate;
    }
    if (out->count==2 && out->candidate[0].epoch>out->candidate[1].epoch) {
        portable_timezone_candidate swap=out->candidate[0]; out->candidate[0]=out->candidate[1]; out->candidate[1]=swap;
    }
    if (out->count==2) return PORTABLE_TIMEZONE_FOLD;
    if (out->count==1) return PORTABLE_TIMEZONE_OK;
    return range_failures?PORTABLE_TIMEZONE_RANGE:PORTABLE_TIMEZONE_GAP;
}
static int64_t distance(int64_t a,int64_t b) { return a>b?a-b:b-a; }
int portable_timezone_interpret_rtc(const portable_timezone_rule *rule,
        const portable_timezone_civil *rtc,unsigned utc_stores_hint,uint32_t reference_epoch,
        int fold_choice,portable_timezone_rtc_result *out) {
    if (!out || utc_stores_hint>1 || fold_choice< -1 || fold_choice>1) return PORTABLE_TIMEZONE_INVALID;
    if (!valid_rule(rule)) return PORTABLE_TIMEZONE_BAD_RULE;
    portable_timezone_rtc_result r={0}; portable_timezone_inverse local;
    int rc=portable_timezone_civil_to_epoch(rtc,&r.utc_epoch);
    if (rc) return rc;
    r.utc_usable=r.utc_epoch>=PORTABLE_TIMEZONE_USABLE_EPOCH;
    r.local_status=portable_timezone_local_to_utc(rule,rtc,&local);
    if (r.local_status==PORTABLE_TIMEZONE_OK || (r.local_status==PORTABLE_TIMEZONE_FOLD && fold_choice>=0)) {
        r.local_epoch=local.candidate[r.local_status==PORTABLE_TIMEZONE_FOLD?fold_choice:0].epoch;
        r.local_usable=r.local_epoch>=PORTABLE_TIMEZONE_USABLE_EPOCH;
    }
    r.stores_utc=(uint8_t)utc_stores_hint;
    if (r.utc_usable && r.local_usable && reference_epoch>=PORTABLE_TIMEZONE_USABLE_EPOCH) {
        int64_t utc_delta=distance(r.utc_epoch,reference_epoch),local_delta=distance(r.local_epoch,reference_epoch);
        if (utc_delta+1800<local_delta) r.stores_utc=1;
        else if (local_delta+1800<utc_delta) r.stores_utc=0;
    } else if (r.utc_usable && !r.local_usable) r.stores_utc=1;
    else if (r.local_usable && !r.utc_usable) r.stores_utc=0;
    r.mode_changed=r.stores_utc!=utc_stores_hint;
    r.epoch=r.stores_utc?r.utc_epoch:r.local_epoch;
    *out=r;
    return (r.stores_utc?r.utc_usable:r.local_usable)?PORTABLE_TIMEZONE_OK:PORTABLE_TIMEZONE_UNUSABLE;
}
