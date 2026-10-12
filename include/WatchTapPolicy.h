#pragma once
#include "PortableTapSettings.h"
/* Existing BMA preferences keep their exact storage record and interpretation.
 * Other drivers own their encoded default/range. A shared Home never rejects a
 * valid motion provider just because its hardware family is new. */
static inline bool watch_tap_select(const portable_tap_settings *settings,
        const twatch_tap_info_v1 *info,uint16_t *value) {
    if(!settings || !info || !value || info->struct_size<sizeof(*info) || !info->profile ||
       info->minimum>info->maximum || info->default_value<info->minimum || info->default_value>info->maximum)
        return false;
    uint16_t selected=info->default_value;
    if(info->profile==TWATCH_TAP_BMA423)selected=settings->bma423;
    else if(info->profile==TWATCH_TAP_BMA456H)selected=settings->bma456h;
    if(selected<info->minimum || selected>info->maximum)return false;
    *value=selected;return true;
}
