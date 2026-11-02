#pragma once
/* Invocation-local ordered adapter for legacy semantic touch reducers. Canvas
 * keeps physical input/rotation ownership; navigation is never discarded.
 * The bounded consumer queue stops polling at capacity, rather than truncating
 * a provider stream. Explicit gaps cancel a gesture before snapshot adoption. */
#include "HomeCanvasSurface.h"
#define HOME_CANVAS_TOUCH_QUEUE 32u
typedef struct {
 home_canvas_surface *surface;
 risc_touch_api_v1 api;
 risc_touch_snapshot_v1 snapshot;
 risc_touch_event_v1 events[HOME_CANVAS_TOUCH_QUEUE];
 uint64_t identities[RISC_TOUCH_MAX_CONTACTS];
 unsigned head,count;
 uint8_t tag;
 bool subscribed,gap;
 risc_input_navigation_frame_v1 navigation;
 bool controls;
} home_canvas_touch;
static inline int home_canvas_touch_find(const home_canvas_touch*t,uint64_t identity){
 for(unsigned i=0;
 i<t->snapshot.contact_count;
 i++)if(t->identities[i]==identity)return (int)i;
 return -1;
}
static inline uint8_t home_canvas_touch_tag(home_canvas_touch*t){
 for(unsigned tries=0;
 tries<256;
 tries++){
  uint8_t tag=++t->tag;
 bool used=false;
  for(unsigned i=0;
 i<t->snapshot.contact_count;
 i++)used|=t->snapshot.contacts[i].id==tag;
  if(!used)return tag;
 }
 return 0;
}
static inline bool home_canvas_touch_sync(home_canvas_touch*t){
 home_canvas_surface*s=t->surface;
 if(s->retained||!home_canvas_resume(s))return false;
 size_t count=0;
 uint64_t sequence=0;
 int32_t rc=s->canvas->contacts(s->canvas->context,s->session,NULL,0,&count,&sequence);
 if(rc!=RISC_CANVAS_OK&&rc!=RISC_CANVAS_AGAIN){(void)home_canvas_result(s,rc);
 return false;}
 if(count>SIZE_MAX/sizeof(risc_canvas_event_v1)){home_canvas_retain(s);
 return false;}
 risc_canvas_event_v1 local[RISC_TOUCH_MAX_CONTACTS];
 risc_canvas_event_v1 *data=count>RISC_TOUCH_MAX_CONTACTS?calloc(count,sizeof(*data)):local;
 if(!data)return false;
 size_t capacity=count>RISC_TOUCH_MAX_CONTACTS?count:RISC_TOUCH_MAX_CONTACTS;
 for(size_t i=0;
 i<capacity;
 i++)data[i]=(risc_canvas_event_v1){.struct_size=sizeof(*data)};
 rc=s->canvas->contacts(s->canvas->context,s->session,data,capacity,&count,&sequence);
 bool valid=home_canvas_result(s,rc)&&count<=capacity;
 if(valid){
  risc_touch_snapshot_v1 next={.sequence=t->snapshot.sequence,.width=(uint16_t)s->geometry.width,.height=(uint16_t)s->geometry.height};
  uint64_t ids[RISC_TOUCH_MAX_CONTACTS]={0};
  if(count>RISC_TOUCH_MAX_CONTACTS){
   /* This consumer only recognizes a single contact. More contacts explicitly
    * cancel; they are not misrepresented as a single actionable pointer. */
   next.contact_count=2;
 next.contacts[0].id=1;
 next.contacts[1].id=2;
 t->gap=true;
  }else{
   next.contact_count=(uint8_t)count;
   for(size_t i=0;
 i<count;
 i++){
    if(data[i].struct_size<sizeof(*data)||data[i].kind!=RISC_CANVAS_CONTACT||
       data[i].geometry_revision!=s->geometry.revision||data[i].x<0||data[i].y<0||
       (uint32_t)data[i].x>=s->geometry.width||(uint32_t)data[i].y>=s->geometry.height){valid=false;
 break;}
    for(size_t j=0;
 j<i;
 j++)if(data[j].contact==data[i].contact)valid=false;
    int at=home_canvas_touch_find(t,data[i].contact);
    next.contacts[i]=(risc_touch_contact_v1){.id=at>=0?t->snapshot.contacts[at].id:home_canvas_touch_tag(t),
      .x=(uint16_t)data[i].x,.y=(uint16_t)data[i].y};
 ids[i]=data[i].contact;
   }
  }
  if(valid){t->snapshot=next;
 memcpy(t->identities,ids,sizeof(ids));}
 }
 if(data!=local&&!s->retained)free(data);
 if(!valid&&!s->retained)t->gap=true;
 return valid;
}
static uint64_t home_canvas_touch_subscribe(void*context){
 home_canvas_touch*t=context;
 if(t->subscribed||t->surface->retained)return 0;
 t->head=t->count=0;
 t->gap=false;
 t->navigation=(risc_input_navigation_frame_v1){0};
 t->controls=false;
 if(!home_canvas_touch_sync(t))return 0;
 t->subscribed=true;
 return 1;
}
static bool home_canvas_touch_unsubscribe(void*context,uint64_t id){
 home_canvas_touch*t=context;
 if(t->surface->retained||id!=1||!t->subscribed)return false;
 t->subscribed=false;
 t->head=t->count=0;
 return true;
}
static bool home_canvas_touch_poll(void*context,size_t budget){
 home_canvas_touch*t=context;
 home_canvas_surface*s=t->surface;(void)budget;
 if(s->retained||!t->subscribed||!home_canvas_resume(s))return false;
 for(unsigned n=0;
 n<HOME_CANVAS_TOUCH_QUEUE&&t->count<HOME_CANVAS_TOUCH_QUEUE;
 n++){
  risc_canvas_event_v1 e={.struct_size=sizeof(e)};
  int32_t rc=s->canvas->next(s->canvas->context,s->session,&e);
  if(rc==RISC_CANVAS_IDLE||rc==RISC_CANVAS_AGAIN)return true;
  if(!home_canvas_result(s,rc)){t->gap=true;
 return false;}
  if(e.struct_size<sizeof(e)||e.kind<RISC_CANVAS_CONTACT||e.kind>RISC_CANVAS_CONTROLS_REQUEST){home_canvas_retain(s);
 return false;}
  if(e.kind==RISC_CANVAS_NAVIGATION){
   t->navigation.buttons=e.navigation.buttons;
 t->navigation.pressed|=e.navigation.pressed;
   t->navigation.released|=e.navigation.released;
 continue;
  }
  if(e.kind==RISC_CANVAS_CONTROLS_REQUEST){t->controls=true;
 continue;}
  if(e.kind==RISC_CANVAS_INPUT_GAP||e.kind==RISC_CANVAS_GEOMETRY_CHANGED||e.phase==RISC_CANVAS_CANCEL){
   t->head=t->count=0;
 t->gap=true;
 return home_canvas_touch_sync(t);
  }
  if(e.geometry_revision!=s->geometry.revision||e.x<0||e.y<0||
     (uint32_t)e.x>=s->geometry.width||(uint32_t)e.y>=s->geometry.height){
   t->head=t->count=0;
 t->gap=true;
 return home_canvas_touch_sync(t);
  }
  int at=home_canvas_touch_find(t,e.contact);
 uint8_t tag=0;
  if(e.phase==RISC_CANVAS_DOWN){
   if(at>=0||t->snapshot.contact_count==RISC_TOUCH_MAX_CONTACTS){t->head=t->count=0;
 t->gap=true;
 return home_canvas_touch_sync(t);}
   tag=home_canvas_touch_tag(t);
 at=t->snapshot.contact_count++;
   t->identities[at]=e.contact;
 t->snapshot.contacts[at]=(risc_touch_contact_v1){.id=tag,.x=(uint16_t)e.x,.y=(uint16_t)e.y};
  }else if((e.phase==RISC_CANVAS_MOVE||e.phase==RISC_CANVAS_UP)&&at>=0){
   tag=t->snapshot.contacts[at].id;
 t->snapshot.contacts[at].x=(uint16_t)e.x;
 t->snapshot.contacts[at].y=(uint16_t)e.y;
  }else {t->head=t->count=0;
 t->gap=true;
 return home_canvas_touch_sync(t);}
  if(t->snapshot.sequence==UINT64_MAX){home_canvas_retain(s);
 return false;}
  risc_touch_event_v1 raw={.sequence=++t->snapshot.sequence,.kind=(uint8_t)e.phase,
      .id=tag,.x=(uint16_t)e.x,.y=(uint16_t)e.y};
  t->events[(t->head+t->count++)%HOME_CANVAS_TOUCH_QUEUE]=raw;
  if(e.phase==RISC_CANVAS_UP){
   for(unsigned i=(unsigned)at+1;
 i<t->snapshot.contact_count;
 i++){t->snapshot.contacts[i-1]=t->snapshot.contacts[i];
 t->identities[i-1]=t->identities[i];}
   --t->snapshot.contact_count;
  }
 }
 return true;
}
static int32_t home_canvas_touch_next(void*context,uint64_t id,risc_touch_event_v1*out){
 home_canvas_touch*t=context;
 if(t->surface->retained)return -2;
 if(!t->subscribed||id!=1||!out)return -1;
 if(t->gap){t->gap=false;
 return -1;}
 if(!t->count)return 0;
 *out=t->events[t->head];
 t->head=(t->head+1)%HOME_CANVAS_TOUCH_QUEUE;--t->count;
 return 1;
}
static bool home_canvas_touch_snapshot(void*context,risc_touch_snapshot_v1*out){
 home_canvas_touch*t=context;
 if(t->surface->retained||!t->subscribed||!out)return false;
 *out=t->snapshot;
 return true;
}
static inline void home_canvas_touch_init(home_canvas_touch*t,home_canvas_surface*s){
 *t=(home_canvas_touch){.surface=s};
 t->api=(risc_touch_api_v1){1,sizeof(t->api),t,home_canvas_touch_subscribe,
 home_canvas_touch_unsubscribe,home_canvas_touch_poll,home_canvas_touch_next,home_canvas_touch_snapshot};
}
static inline risc_input_navigation_frame_v1 home_canvas_touch_navigation(home_canvas_touch*t,bool*controls){
 risc_input_navigation_frame_v1 n=t->navigation;
 *controls=t->controls;
 t->navigation.pressed=t->navigation.released=0;
 t->controls=false;
 return n;
}
