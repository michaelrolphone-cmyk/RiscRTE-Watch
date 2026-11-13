#include "RiscRuntimeV1.h"
#include <assert.h>
extern void resident_test_lifecycle(unsigned,unsigned);
__attribute__((visibility("default"))) void app_main(void){const risc_runtime_api_v1*r=risc_runtime_get_api(1);assert(r);resident_test_lifecycle(2,1);assert(r->request_launch("stopwatch.elf"));resident_test_lifecycle(2,2);}
