#include STOPWATCH_SOURCE
#include <assert.h>
extern void resident_test_lifecycle(unsigned,unsigned);
extern void resident_test_result(unsigned,bool);
__attribute__((visibility("default"))) int app_module_init(void){resident_test_lifecycle(1,1);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){fprintf(stderr,"Stopwatch witness ui=%p terminal=%d loaded=%d notice=%s\n",(void*)ui,terminal,loaded,notice);resident_test_result(clock_state.elapsed_ms,clock_state.running);resident_test_lifecycle(1,2);}
__attribute__((destructor)) static void unloaded(void){resident_test_lifecycle(1,3);}
