#include "watch_sleep.h"
#include <assert.h>
#include <stdio.h>
static unsigned phase;
static bool refuse,restore_failure;
static int32_t result;
static int power_context,panel_context;
static bool prep_display(void *ctx){assert(ctx==&panel_context&&phase==0);phase=1;return true;}
static bool prep_power(void *ctx){assert(ctx==&power_context&&phase==1);phase=2;return !refuse;}
static bool restore_power(void *ctx){assert(ctx==&power_context&&(phase==2||phase==3));phase=4;return !restore_failure;}
static bool restore_display(void *ctx){assert(ctx==&panel_context&&phase==4);phase=5;return true;}
static int32_t light(void *ctx,risc_light_sleep_result_v1 *out){assert(ctx==&power_context&&phase==2);phase=3;out->wake_cause=RISC_LIGHT_SLEEP_WAKE_GPIO;return result;}
static bool diagnostic(const char *msg){assert(msg&&*msg);return true;}
int main(void){
 const risc_power_sleep_api_v1 power={.api_version=1,.struct_size=RISC_POWER_LIGHT_SLEEP_SIZE,.context=&power_context,.prepare_sleep=prep_power,.resume=restore_power,.light_sleep=light};
 const risc_display_power_api_v1 display={.api_version=1,.struct_size=RISC_DISPLAY_POWER_LIGHT_SLEEP_SIZE,.context=&panel_context,.prepare_sleep=prep_display,.resume=restore_display};
 result=0;assert(watch_sleep_prepared(&display,&power,PORTABLE_SLEEP_LIGHT,diagnostic)==WATCH_SLEEP_WOKE&&phase==5);
 phase=0;refuse=true;assert(watch_sleep_prepared(&display,&power,PORTABLE_SLEEP_LIGHT,diagnostic)==WATCH_SLEEP_REFUSED&&phase==5);
 phase=0;refuse=false;restore_failure=true;assert(watch_sleep_prepared(&display,&power,PORTABLE_SLEEP_LIGHT,diagnostic)==WATCH_SLEEP_FAILED&&phase==5);
 phase=0;restore_failure=false;result=RISC_LIGHT_SLEEP_RETAINED;assert(watch_sleep_prepared(&display,&power,PORTABLE_SLEEP_LIGHT,diagnostic)==WATCH_SLEEP_RETAINED&&phase==3);
 puts("Generic sleep: battery-free light sleep, refusal restore, restore failure, retained transport PASS");
}
