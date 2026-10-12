#include "WatchTapPolicy.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 portable_tap_settings s=portable_tap_defaults();uint16_t value=999;
 twatch_tap_info_v1 info={sizeof(info),TWATCH_TAP_BMA423,0,7,3};
 assert(watch_tap_select(&s,&info,&value)&&value==3);s.bma423=6;assert(watch_tap_select(&s,&info,&value)&&value==6);
 info=(twatch_tap_info_v1){sizeof(info),TWATCH_TAP_BMA456H,0,15,12};assert(watch_tap_select(&s,&info,&value)&&value==12);
 info=(twatch_tap_info_v1){sizeof(info),3,100,4000,800};assert(watch_tap_select(&s,&info,&value)&&value==800);
 info.profile=77;info.minimum=3;info.maximum=30;info.default_value=13;assert(watch_tap_select(&s,&info,&value)&&value==13);
 info.default_value=31;assert(!watch_tap_select(&s,&info,&value));info.default_value=2;assert(!watch_tap_select(&s,&info,&value));
 info.default_value=13;info.minimum=31;assert(!watch_tap_select(&s,&info,&value));info.minimum=3;info.profile=0;assert(!watch_tap_select(&s,&info,&value));
 info.profile=3;info.struct_size--;assert(!watch_tap_select(&s,&info,&value));
 assert(!watch_tap_select(NULL,&info,&value)&&!watch_tap_select(&s,NULL,&value)&&!watch_tap_select(&s,&info,NULL));
 puts("Watch tap policy: legacy persisted settings and arbitrary driver-declared profiles PASS");
}
