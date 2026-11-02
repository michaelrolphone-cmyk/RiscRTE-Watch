  if(mode.find("effects")!=std::string::npos){
    const unsigned beforeHardware=hardwareCalls;
    if(is("retained-effects")){
      callbackStatus=RISC_RESIDENT_RETAINED;checkpoint(RISC_SETTINGS_APPLY(1),0,RISC_RESIDENT_RETAINED);
      assert(runtime->retained()&&!runtime->residentResetSafe()&&hardwareCalls==beforeHardware);return;
    }
    for(uint32_t effect=1;effect<=4;effect++)for(uint32_t reason:{RISC_SETTINGS_QUERY(effect),RISC_SETTINGS_APPLY(effect),RISC_SETTINGS_PROGRESS(effect),RISC_SETTINGS_RESULT(effect,1),RISC_SETTINGS_RESULT(effect,2),RISC_SETTINGS_RESULT(effect,3)}){
      if(is("effects-flags")){for(uint32_t flags:{1u,2u,4u,UINT32_MAX})checkpoint(reason,flags,RISC_RESIDENT_INVALID);}
      else {callbackStatus=is("effects-old-host")?RISC_RESIDENT_BUSY:RISC_RESIDENT_OK;checkpoint(reason,0,callbackStatus,0);}
    }
    assert(hardwareCalls==beforeHardware);return;
  }
