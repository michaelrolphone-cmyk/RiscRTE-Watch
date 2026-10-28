#include "PortableQuickRadios.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool have_wifi,have_ble,unknown_wifi,malformed_wifi;
static uint8_t record[4],bstate;static bool saved,rf_live,fail_read,fail_write,commit_io,fail_ble,fail_off,fail_release;static unsigned gets,puts_,disconnects,opens,closes,grants,releases;
static int32_t get(void*c,const char*k,void*out,uint32_t cap,uint32_t*n){(void)c;assert(!strcmp(k,PORTABLE_RADIO_KEY)&&cap==4);gets++;*n=0;if(fail_read)return RISC_KEY_VALUE_IO;if(!saved)return RISC_KEY_VALUE_NOT_FOUND;memcpy(out,record,4);*n=4;return RISC_KEY_VALUE_OK;}
static int32_t put(void*c,const char*k,const void*in,uint32_t n){(void)c;assert(!strcmp(k,PORTABLE_RADIO_KEY)&&n==4);puts_++;if(fail_write)return RISC_KEY_VALUE_IO;memcpy(record,in,4);saved=true;return commit_io?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static wifi_link_t wstatus(void*c){(void)c;return unknown_wifi?(wifi_link_t)99:rf_live?WIFI_LINK_UP:WIFI_LINK_DOWN;}
static bool off(void*c){(void)c;disconnects++;if(fail_off)return false;rf_live=false;return true;}
static const wifi_api_v1 w={.api_version=1,.struct_size=sizeof(w),.status=wstatus,.disconnect_checked=off};
static bool benable(void*c,bool on){(void)c;if(fail_ble)return false;if(on){if(bstate!=PORTABLE_BLUETOOTH_ON)opens++;bstate=PORTABLE_BLUETOOTH_ON;}else {if(bstate!=PORTABLE_BLUETOOTH_OFF)closes++;bstate=PORTABLE_BLUETOOTH_OFF;}return true;}
static bool bstatus(void*c,uint8_t*out){(void)c;*out=bstate;return true;}
static const portable_bluetooth_control_v1 b={.api_version=1,.struct_size=sizeof(b),.set_enabled=benable,.status=bstatus};
static bool acquire(const char*cap,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(v==1);grants++;if(!strcmp(cap,RISC_KEY_VALUE_CAPABILITY)){assert(id==1);g->api=&kv;}else if(!strcmp(cap,"net.wifi")){assert(id==0);if(!have_wifi){grants--;return false;}static const wifi_api_v1 bad_wifi={0};g->api=malformed_wifi?&bad_wifi:&w;}else {assert(!strcmp(cap,"bluetooth.hci")&&id==0);if(!have_ble){grants--;return false;}g->api=&b;}return true;}
static bool release(risc_runtime_capability_v1*g){assert(g->api);if(fail_release)return false;g->api=NULL;releases++;return true;}
static char logs[16384];
static bool health(risc_runtime_health_v1*out){out->uptime_ms=1234;return true;}
static bool diagnostic(const char*line){assert(strlen(logs)+strlen(line)+2<sizeof(logs));strcat(logs,line);strcat(logs,"\n");return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.health=health,.diagnostic=diagnostic};
static void reset(void){logs[0]=0;memset(record,0,4);saved=rf_live=fail_read=fail_write=commit_io=fail_ble=fail_off=fail_release=false;bstate=0;gets=puts_=disconnects=opens=closes=grants=releases=0;}

static bool safe(void*c){return *(bool*)c;}
static bool save(void*c,const char*k,const void*v,uint32_t n,const uint8_t*previous){(void)c;(void)previous;return put(NULL,k,v,n)==0;}
int main(void){bool settled=true;for(unsigned mask=0;mask<4;mask++){
 reset();have_wifi=mask&1;have_ble=mask&2;pqa_radios s;pqa_state u={0};assert(pqa_radios_load(&s,&u,&rt));assert(u.wifi_available==have_wifi&&u.bluetooth_available==have_ble);assert(u.wifi_valid==have_wifi&&u.bluetooth_valid==have_ble&&u.radios_valid==(have_wifi&&have_ble));
 assert(pqa_radios_apply_user(&s,&u,&rt,PQA_AIRPLANE,save,&settled,safe));assert(u.airplane==(mask==3));if(mask!=3)assert(!puts_);
 assert(pqa_radios_apply_user(&s,&u,&rt,PQA_WIFI,save,&settled,safe));assert(pqa_radios_apply_user(&s,&u,&rt,PQA_BLUETOOTH,save,&settled,safe));assert(grants==releases);
 reset();pqa_radios_load(&s,&u,&rt);fail_write=true;assert(pqa_radios_apply_user(&s,&u,&rt,have_wifi?PQA_WIFI:PQA_BLUETOOTH,save,&settled,safe));assert(!saved&&!bstate);
 reset();saved=true;memset(record,255,4);assert(pqa_radios_load(&s,&u,&rt));assert(u.wifi_available==have_wifi&&u.bluetooth_available==have_ble);assert(!u.wifi_valid&&!u.bluetooth_valid&&!u.radios_valid&&!puts_);
 }
 reset();have_wifi=true;have_ble=false;unknown_wifi=true;pqa_radios s;pqa_state u={0};assert(!pqa_radios_load(&s,&u,&rt));unknown_wifi=false;assert(u.wifi_available&&!u.wifi_valid);
 reset();malformed_wifi=true;assert(!pqa_radios_load(&s,&u,&rt));assert(u.wifi_available&&!u.wifi_valid);malformed_wifi=false;
 reset();have_ble=true;assert(pqa_radios_load(&s,&u,&rt));fail_ble=true;assert(!pqa_radios_apply_user(&s,&u,&rt,PQA_BLUETOOTH,save,&settled,safe));
 reset();assert(pqa_radios_load(&s,&u,&rt));settled=false;unsigned before=releases;assert(!pqa_radios_apply_user(&s,&u,&rt,PQA_WIFI,save,&settled,safe));assert(releases==before);
 puts("PASS capability-safe radios four capability graphs, independent controls, Airplane truth, corrupt preferences, rollback, unknown status and retained save boundary");}
