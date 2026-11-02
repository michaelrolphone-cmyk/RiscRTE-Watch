#pragma once
#include <stdint.h>
#include <stddef.h>
#define RISC_HOME_PRESENTATION_CAPABILITY "ui.home-presentation"
#define RISC_HOME_PRESENTATION_TAG UINT32_C(0x484f4d31)
enum { RISC_HOME_VIEW_FACES=1,RISC_HOME_VIEW_OVERVIEW=2 };
enum { RISC_HOME_POINTS_UNSPECIFIED=0,RISC_HOME_POINTS_LEGACY_RTC=1,RISC_HOME_POINTS_NATIVE_UTC=2,RISC_HOME_POINTS_CATALOG=3 };
/* Immutable deployment presentation/default-data bindings. No device identity,
 * display dimensions, pixel format, hardware callback or app controller crosses
 * this interface. A confirmed user Home-view preference overrides default_view.
 * Zero crash_namespace explicitly disables crash-report storage; never acquire
 * storage.app-data instance zero as its fallback.
 * Zero catalog_volume means the invocation's unique permitted storage volume. */
typedef struct {
 uint32_t api_version,struct_size,tag,default_view;
 uint64_t crash_namespace,catalog_volume;
 uint32_t points_schema,reserved;
} risc_home_presentation_v1;
static inline const risc_home_presentation_v1*risc_home_presentation_get_v1(const void*p,size_t bytes){
 const size_t prefix=offsetof(risc_home_presentation_v1,points_schema);
 if(!p||bytes<prefix)return NULL;
 const risc_home_presentation_v1*a=p;
 return a->api_version==1&&a->struct_size>=prefix&&a->struct_size<=bytes&&
  a->tag==RISC_HOME_PRESENTATION_TAG&&a->default_view>=RISC_HOME_VIEW_FACES&&
  a->default_view<=RISC_HOME_VIEW_OVERVIEW?a:NULL;
}

static inline unsigned risc_home_points_schema(const risc_home_presentation_v1*p){
 return p&&p->struct_size>=sizeof(*p)&&!p->reserved&&p->points_schema<=RISC_HOME_POINTS_CATALOG?p->points_schema:RISC_HOME_POINTS_UNSPECIFIED;
}
