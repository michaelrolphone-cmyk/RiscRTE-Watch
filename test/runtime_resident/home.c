#include <stdio.h>
#include "../../apps/clock/crown.c"
extern void resident_test_lifecycle(unsigned,unsigned);
extern void resident_test_set_home_observer(bool(*)(void));
__attribute__((visibility("default"))) bool resident_test_home_controls_active(void);
__attribute__((visibility("default"))) int app_module_init(void){resident_test_set_home_observer(resident_test_home_controls_active);resident_test_lifecycle(0,1);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){fprintf(stderr,"Home witness exit=%u terminal=%d enabled=%d continued=%d\n",clock_first_exit,clock_resident_terminal,clock_resident_enabled,clock_resident_continued);resident_test_lifecycle(0,2);resident_test_set_home_observer(NULL);}
__attribute__((destructor)) static void unloaded(void){resident_test_lifecycle(0,3);}
/* Read-only witness: never changes callback routing or production state. */
__attribute__((visibility("default"))) bool resident_test_home_controls_active(void){return clock_resident_dispatching&&pqa_visible(&clock_quick.ui);}
