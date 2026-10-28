#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define WATCH_NATIVE_COVERAGE_CACHE
#include "../apps/clock/nova/native_reuse_admission.h"
int main(void){assert(!native_reuse_allowed(466,466));native_reuse_width(466,466);native_reuse_width(466,466);assert(!native_reuse_allowed(466,466));native_reuse_width(466,466);assert(native_reuse_allowed(466,466));for(unsigned i=0;i<32;i++)native_reuse_width(300+i,300+i);assert(!native_reuse_allowed(466,466));for(unsigned i=0;i<3;i++)native_reuse_width(466,466);assert(native_reuse_allowed(466,466));native_reuse_reset();assert(!native_reuse_allowed(466,466));reuse_serial=UINT32_MAX-1;for(unsigned i=0;i<3;i++)native_reuse_width(466,466);assert(native_reuse_allowed(466,466));assert(reuse_serial==1);printf("PASS cold admission, geometry churn, expiry, reset, uint32 wrap; state=%zu bytes\n",sizeof(reuse_geometries)+sizeof(reuse_serial));}
