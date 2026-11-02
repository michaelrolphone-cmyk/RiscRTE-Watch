#pragma once
#include <stdint.h>
#include <stddef.h>
#define RISC_CIVIL_CLOCK_CAPABILITY "time.civil"
/* Read-only local-calendar policy. No UI, alarm programming, clock seeding or
 * global timezone mutation. Days/minutes are local civil values, not durations. */
typedef struct {uint32_t struct_size;int32_t day;uint32_t minute;char zone[40];} risc_civil_time_v1;
typedef struct {
 uint32_t struct_size;
 int32_t year;
 uint8_t month,day,hour,minute,second,weekday;
 uint16_t reserved;
 uint32_t nanoseconds;
 char zone[40];
} risc_civil_calendar_v1;
/* Append-only optional full-clock policy. project converts a caller-owned UTC
 * snapshot without reading/setting a clock. interpret_rtc converts a raw calendar
 * using the installed RTC basis policy; neither operation writes preferences,
 * RTC or native time. Output is unchanged on failure. */
typedef struct {uint32_t api_version,struct_size;void *context;int32_t (*read)(void *,risc_civil_time_v1 *);} risc_civil_clock_api_v1;
#define RISC_CIVIL_CLOCK_POLICY_TAG UINT32_C(0x43495031)
typedef struct {
 uint32_t api_version,struct_size;void *context;
 int32_t (*read)(void *,risc_civil_time_v1 *);
 uint32_t policy_version,policy_tag;
 int32_t (*project)(void *,int64_t,uint32_t,risc_civil_calendar_v1 *);
 int32_t (*interpret_rtc)(void *,const risc_civil_calendar_v1 *,int64_t *);
} risc_civil_clock_policy_v1;
#define RISC_CIVIL_CLOCK_BASE_SIZE sizeof(risc_civil_clock_api_v1)
#define RISC_CIVIL_CLOCK_POLICY_SIZE sizeof(risc_civil_clock_policy_v1)
static inline const risc_civil_clock_policy_v1 *risc_civil_clock_get_policy(const void*ptr,size_t bytes){
 if(!ptr||bytes<RISC_CIVIL_CLOCK_POLICY_SIZE)return NULL;
 const risc_civil_clock_policy_v1*a=ptr;
 return a->api_version==1&&a->struct_size>=RISC_CIVIL_CLOCK_POLICY_SIZE&&a->struct_size<=bytes&&
  a->policy_version==1&&a->policy_tag==RISC_CIVIL_CLOCK_POLICY_TAG&&a->project&&a->interpret_rtc?a:NULL;
}
/* 0=valid, -1=unavailable/invalid time, -9=retained; do no further I/O on -9. */
