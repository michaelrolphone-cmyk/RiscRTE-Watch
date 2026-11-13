 if(!strncmp(host_case,"alarm-",6)) {
  risc_resident_request_v1 q={sizeof(q),RISC_RESIDENT_QUERY_ALARM_ONLY,0,0};
  risc_resident_reply_v1 r={sizeof(r),0};
  clock_resident_keys=3;resident_key_event=3;
  unsigned frames=submitted,sl=sleeps;
  if(is_case("alarm-query-flags"))q.flags=1;
  if(is_case("alarm-query-reserved"))q.reserved=1;
  if(is_case("alarm-apply-flags")){q.reason=RISC_RESIDENT_CHECKPOINT_ALARM;q.flags=1;}
  int32_t expected=(q.flags||q.reserved)?RISC_RESIDENT_INVALID:RISC_RESIDENT_BUSY;
  assert(host_callbacks.dispatch(NULL,&q,&r)==expected&&!r.flags);
  if(is_case("alarm-unavailable")){
   q.reason=RISC_RESIDENT_CHECKPOINT_ALARM;
   assert(host_callbacks.dispatch(NULL,&q,&r)==RISC_RESIDENT_BUSY&&!r.flags);
  }
  assert(clock_resident_keys==3&&resident_key_event==3&&submitted==frames&&sleeps==sl&&!host_exits);
  clock_resident_keys=resident_key_event=0;
  launched=true;result->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
 }
