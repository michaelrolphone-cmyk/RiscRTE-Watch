"""Controlled tagged negotiation for unchanged historical lifecycle fixture bytes."""
def tagged_alarm(text):
    edits=[
      ('static const alarm_service_sleep_v1 alarm_api={{1,sizeof(alarm_api),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_prepare,alarm_refresh},alarm_resume};',
       'static alarm_service_descriptor_v2 alarm_api={{2,sizeof(alarm_api),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_prepare,alarm_refresh},ALARM_SERVICE_DESCRIPTOR_TAG,1,ALARM_MODE_BOTH,ALARM_DESCRIPTOR_RESUME_SLEEP,alarm_resume};'),
      ('if(p){assert(version==1&&(!instance||instance==0));',
       'if(p){assert(version==(!strcmp(n,ALARM_SERVICE_CAPABILITY)?2u:1u)&&(!instance||instance==0));'),
    ]
    for before,after in edits:
        assert text.count(before)==1,(before,text.count(before))
        text=text.replace(before,after)
    return '#include "AlarmServiceV2.h"\n'+text,edits
