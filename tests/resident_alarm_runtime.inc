  if(mode.find("alarm-")!=std::string::npos) {
    const uint32_t aq=RISC_RESIDENT_QUERY_ALARM_ONLY,aa=RISC_RESIDENT_CHECKPOINT_ALARM,ao=RISC_RESIDENT_QUERY_LAST_IDLE_ALARM;
    const unsigned beforeHardware=hardwareCalls;
    auto retainedAlarm=[&](uint32_t reason,bool dispatched){
      const unsigned before=dispatches;checkpoint(reason,0,RISC_RESIDENT_RETAINED);
      assert(dispatches==before+(dispatched?1u:0u));
      assert(runtime->retained()&&!runtime->residentResetSafe());
      assert(hardwareCalls==beforeHardware);
      risc_resident_request_v1 request{sizeof(request),reason,0,0};
      risc_resident_reply_v1 reply{sizeof(reply),0x1234};
      assert(clients[roleNow].checkpoint(clients[roleNow].invocation,&request,&reply)==RISC_RESIDENT_DENIED);
      assert(dispatches==before+(dispatched?1u:0u)&&reply.flags==0x1234);
    };
    if(is("alarm-query"))checkpoint(aq,0,RISC_RESIDENT_OK);
    else if(is("alarm-apply")){callbackFlags=RISC_RESIDENT_REPLY_REDRAW;checkpoint(aa,0,RISC_RESIDENT_OK,callbackFlags);}
    else if(is("alarm-outcome")){
      checkpoint(aq,0,RISC_RESIDENT_OK);callbackFlags=RISC_RESIDENT_REPLY_REDRAW;checkpoint(aa,0,RISC_RESIDENT_OK,callbackFlags);
      callbackFlags=0;checkpoint(ao,0,RISC_RESIDENT_OK,0);callbackStatus=RISC_RESIDENT_BUSY;checkpoint(ao,0,RISC_RESIDENT_BUSY,0);
      callbackStatus=RISC_RESIDENT_OK;callbackFlags=RISC_RESIDENT_REPLY_REDRAW;checkpoint(ao,0,RISC_RESIDENT_OK,callbackFlags);checkpoint(ao,0,RISC_RESIDENT_OK,callbackFlags);
    }else if(is("alarm-old-host")){callbackStatus=RISC_RESIDENT_BUSY;for(uint32_t reason:{aq,aa,ao})checkpoint(reason,0,RISC_RESIDENT_BUSY,0);}
    else if(is("alarm-flags")){
      for(uint32_t reason:{aq,aa,ao}){
        for(uint32_t flags:{1u,2u,4u,UINT32_MAX})checkpoint(reason,flags,RISC_RESIDENT_INVALID);
        risc_resident_request_v1 request{sizeof(request),reason,0,1};risc_resident_reply_v1 reply{sizeof(reply),0x1234};
        const auto old=reply;const unsigned before=dispatches;assert(invoke(request,reply)==RISC_RESIDENT_INVALID);
        assert(dispatches==before&&!std::memcmp(&reply,&old,sizeof(reply)));
      }
    }else if(is("retained-alarm-query")||is("retained-alarm-apply")||is("retained-alarm-outcome")){
      exitSafe=false;retainedAlarm(is("retained-alarm-query")?aq:is("retained-alarm-apply")?aa:ao,false);return;
    }else if(is("malformed-alarm-no-pending")){callbackStatus=RISC_RESIDENT_NO_PENDING;retainedAlarm(ao,true);return;}
    else if(is("malformed-alarm-outcome-policy")){callbackFlags=RISC_RESIDENT_REPLY_POLICY_REQUEST;retainedAlarm(ao,true);return;}
    else if(is("malformed-alarm-busy-redraw")){callbackStatus=RISC_RESIDENT_BUSY;callbackFlags=RISC_RESIDENT_REPLY_REDRAW;retainedAlarm(ao,true);return;}
    else if(is("retained-alarm-callback")){callbackStatus=RISC_RESIDENT_RETAINED;retainedAlarm(aa,true);return;}
    else assert(false);
    assert(hardwareCalls==beforeHardware);return;
  }
