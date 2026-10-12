#include "RiscRuntimeV1.h"
#include <assert.h>
/* Host-loader observation only; constructors do not call Runtime before entry. */
extern void return_test_lifetime(int loaded);
__attribute__((constructor)) static void init(void){
 return_test_lifetime(1);
}
__attribute__((destructor)) static void fini(void){
 return_test_lifetime(0);
}
__attribute__((visibility("default"))) void app_main(void){
 const risc_runtime_api_v1* a=risc_runtime_get_api(1);assert(a);
#ifdef RETURN_TEST_CHILD
 assert(a->diagnostic("return-test:child"));assert(a->request_launch("clock.elf"));
#else
 risc_runtime_health_v1 h={.struct_size=sizeof(h)};assert(a->health(&h));
 assert(a->diagnostic("return-test:home"));if(!h.uptime_ms)assert(a->request_launch("child.elf"));
#endif
}
