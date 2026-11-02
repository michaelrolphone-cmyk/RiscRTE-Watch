#pragma once
#include "PortableQuickActions.h"
#include "PortableRadioPolicy.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
typedef struct {uint8_t flags;bool valid,available;
#if defined(PORTABLE_QUICK_CAPABILITY_SAFE) || defined(PORTABLE_QUICK_RADIO_AVAILABILITY)
 bool wifi_available,bluetooth_available;
#endif
} pqa_radios;
bool pqa_radios_load(pqa_radios*,pqa_state*,const risc_runtime_api_v1*);
bool pqa_radios_apply(pqa_radios*,pqa_state*,const risc_runtime_api_v1*,uint32_t);
bool pqa_radios_suspend(const risc_runtime_api_v1*);
bool pqa_radios_resume(pqa_radios*,pqa_state*,const risc_runtime_api_v1*);

#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
#include "PortableQuickUserPreference.h"
bool pqa_radios_apply_user(pqa_radios*,pqa_state*,const risc_runtime_api_v1*,uint32_t,
                           pqa_user_preference_save,void*,bool(*)(void*));
#endif
