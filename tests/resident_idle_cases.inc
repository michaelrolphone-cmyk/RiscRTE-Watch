 if(!strncmp(host_case,"idle-",5)) {
  risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_QUERY_IDLE_ONLY,0,0};
  risc_resident_reply_v1 reply={sizeof(reply),0};
  clock_resident_keys=3;resident_key_event=3;
  unsigned old_submitted=submitted,old_sleeps=sleeps;
  if(is_case("idle-held")) {
   held=true;assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);held=false;
  } else if(is_case("idle-unavailable")) {
   const risc_power_sleep_api_v1 *saved=device_clients.sleep;device_clients.sleep=NULL;
   assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);
   device_clients.sleep=saved;
  } else if(is_case("idle-query-flags")) {
   request.flags=1;assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_BUSY&&!reply.flags);
  } else {
   assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&!reply.flags);
   assert(resident_key_event==3&&clock_resident_keys==3&&!host_exits&&submitted==old_submitted&&sleeps==old_sleeps);
   request.reason=RISC_RESIDENT_APPLY_IDLE_ONLY;
   if(is_case("idle-not-due")) {
    assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK);
    assert(!sleeps&&!host_exits&&resident_key_event==3&&clock_resident_keys==3);
   } else if(is_case("idle-apply-flags")) {
    request.flags=1;assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_BUSY);
    assert(!sleeps&&!host_exits&&resident_key_event==3&&clock_resident_keys==3);
   } else if(is_case("idle-deep")) {
    sleep_mode=PORTABLE_SLEEP_DEEP;clock_resident_activity=clock_ms-60001u;
    assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_EXIT&&host_exits==1&&!sleeps&&clock_resident_sleep_pending&&clock_resident_keys==3&&resident_key_event==3);
    clock_resident_sleep_pending=false;
   } else if(is_case("idle-light")) {
    clock_resident_activity=clock_ms-60001u;int32_t rc=host_callbacks.dispatch(NULL,&request,&reply);
    assert(rc==RISC_RESIDENT_OK&&sleeps==1&&!host_exits&&clock_resident_keys==3);
    /* Physical wake-discard is still the existing qualified sleep lifecycle. */
    assert(!resident_key_event&&(reply.flags&RISC_RESIDENT_REPLY_REDRAW));
    assert(host_callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK&&sleeps==1&&clock_resident_keys==3);
   } else if(is_case("idle-query")) {
    assert(!sleeps&&!host_exits&&resident_key_event==3&&clock_resident_keys==3);
   } else assert(false);
  }
  clock_resident_keys=0;resident_key_event=0;
  launched=true;result->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;
 }
