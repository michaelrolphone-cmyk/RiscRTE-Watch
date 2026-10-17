/* Real serial protocol/session lease; platform scheduling remains modeled. */
#include "runtime/serial/Files.h"
#include <cassert>
#include <cstring>
namespace {
RiscSerial::Files wire;
uint32_t nowMs,lastRequest,expiredAt,disconnectedAt;
unsigned requests;
bool enabled,renew,drop,renewed;
bool safe(){return true;}
uint32_t millis(){return nowMs;}
void request(const char* command,const char* expected){
 assert(!wire.pending());
 for(const char*p=command;*p;++p)wire.input(uint8_t(*p));
 wire.checkpoint();assert(wire.outputSize()&&strstr(wire.output(),expected));
 wire.consumeOutput(wire.outputSize());assert(!wire.pending()&&!wire.exitSafe());
 lastRequest=nowMs;++requests;
}
}
extern "C" void serial_probe_init(int session,int renewSession,int disconnectSession){
 enabled=session;renew=renewSession;drop=disconnectSession;
 assert(wire.bind(nullptr,0,{safe,safe,millis}));
}
extern "C" int serial_probe_checkpoint(uint32_t now){
 nowMs=now;
 if(enabled&&requests<3){
  if(!requests)request("RTEF1 12345678 1 HELLO\n"," 0 HELLO ");
  else if(requests==1)request("RTEF1 12345678 2 NS 0\n","RTEF1 12345678 2 ");
  else request("RTEF1 12345678 3 PATH 0 2f7061796c6f6164\n"," 0 PATH 8");
 }else if(renew&&!renewed&&nowMs>=25000){
  request("RTEF1 12345678 4 PATH 0 2f7061796c6f6164\n"," 0 PATH 8");renewed=true;
 }
 if(drop&&!disconnectedAt&&nowMs>=15000){wire.disconnect();disconnectedAt=nowMs;}
 wire.checkpoint();assert(!wire.pending()&&wire.custodySafe());
 if(enabled&&requests>=3&&wire.exitSafe()&&!expiredAt&&!disconnectedAt)expiredAt=nowMs;
 return wire.exitSafe();
}
extern "C" unsigned serial_probe_requests(){return requests;}
extern "C" uint32_t serial_probe_last_request(){return lastRequest;}
extern "C" uint32_t serial_probe_expired_at(){return expiredAt;}
extern "C" uint32_t serial_probe_disconnected_at(){return disconnectedAt;}
extern "C" uint32_t serial_probe_timeout(){return RiscSerial::Files::TimeoutMs;}
