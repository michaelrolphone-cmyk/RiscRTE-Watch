  if(is("idle-query")) {
    checkpoint(RISC_RESIDENT_QUERY_IDLE_ONLY,0,RISC_RESIDENT_OK);return;
  }
  if(is("idle-apply")) {
    callbackFlags=RISC_RESIDENT_REPLY_REDRAW|RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;
    checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY,0,RISC_RESIDENT_OK,callbackFlags);return;
  }
  if(is("idle-old-host")) {
    callbackStatus=RISC_RESIDENT_BUSY;
    checkpoint(RISC_RESIDENT_QUERY_IDLE_ONLY,0,RISC_RESIDENT_BUSY,0);
    checkpoint(RISC_RESIDENT_APPLY_IDLE_ONLY,0,RISC_RESIDENT_BUSY,0);return;
  }
  if(is("idle-flags")) {
    for(uint32_t reason:{RISC_RESIDENT_QUERY_IDLE_ONLY,RISC_RESIDENT_APPLY_IDLE_ONLY})
      for(uint32_t flags:{1u,2u,4u,UINT32_MAX})checkpoint(reason,flags,RISC_RESIDENT_INVALID);
    return;
  }
  if(is("retained-idle-query")||is("retained-idle-apply")) {
    exitSafe=false;const unsigned before=dispatches;
    checkpoint(is("retained-idle-query")?RISC_RESIDENT_QUERY_IDLE_ONLY:RISC_RESIDENT_APPLY_IDLE_ONLY,0,RISC_RESIDENT_RETAINED);
    assert(dispatches==before && runtime->retained());
    risc_resident_request_v1 request{sizeof(request),RISC_RESIDENT_QUERY_IDLE_ONLY,0,0};
    risc_resident_reply_v1 reply{sizeof(reply),0};
    assert(clients[roleNow].checkpoint(clients[roleNow].invocation,&request,&reply)==RISC_RESIDENT_DENIED);
    assert(dispatches==before);return;
  }
