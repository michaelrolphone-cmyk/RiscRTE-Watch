/* Real serial protocol/session lease; platform scheduling remains modeled. */
#include "runtime/serial/Files.h"
#include <cassert>
#include <cstring>
namespace {
RiscSerial::Files wire;
RiscBoot::RuntimeFiles files; // NS metadata only: no storage operation is dispatched.
const RiscSerial::Namespace names[]={{1,"payload",&files,true}};
uint32_t nowMs,lastRequest,expiredAt,disconnectedAt,queuedAt,maxLatency,latencies[4];
unsigned requests;
bool enabled,renew,drop,renewed,race;
int pathHold;uint32_t pathRelease;
bool safe(){return true;}
uint32_t millis(){return nowMs;}
const char*commands[]={"RTEF1 12345678 1 HELLO\n","RTEF1 12345678 2 NS 0\n","RTEF1 12345678 3 PATH 0 2f7061796c6f6164\n"};
const char*expected[]={" 0 HELLO "," 0 NS 1 1 7061796c6f6164"," 0 PATH 8"};
void queue(const char*command){
 assert(!wire.pending());queuedAt=nowMs;
 for(const char*p=command;*p;++p)wire.input(uint8_t(*p));
}
void collect(const char*reply){
 assert(wire.outputSize()&&strstr(wire.output(),reply));
 const uint32_t latency=nowMs-queuedAt;assert(latency<5000u);
 if(requests<4)latencies[requests]=latency;
 if(latency>maxLatency)maxLatency=latency;
 wire.consumeOutput(wire.outputSize());assert(!wire.pending()&&!wire.exitSafe());
 lastRequest=nowMs;++requests;
}
void request(const char*command,const char*reply){queue(command);wire.checkpoint();collect(reply);}
}
extern "C" void serial_probe_init(int session,int renewSession,int disconnectSession){
 enabled=session;renew=renewSession;drop=disconnectSession;
 assert(wire.bind(names,1,{safe,safe,millis}));
}
extern "C" void serial_probe_arrive(uint32_t now){
 assert(!enabled&&!requests&&!race&&wire.exitSafe());nowMs=now;race=enabled=true;queue(commands[0]);
}
extern "C" int serial_probe_checkpoint(uint32_t now){
 nowMs=now;
 if(pathHold){
  if(nowMs>=pathRelease){wire.checkpoint();collect(" 0 PATH 8");pathHold=0;}
  else if(pathHold==2)wire.checkpoint();
  if(pathHold)assert(wire.pending());
  return wire.exitSafe();
 }
 if(race){
  wire.checkpoint();
  if(wire.outputSize()){
   assert(requests<3);collect(expected[requests]);
   // Host queues the next request immediately after receiving this response.
   // It can only run at a later real Home settled service/sleep boundary.
   if(requests<3)queue(commands[requests]);
  }
  assert(wire.custodySafe());return wire.exitSafe();
 }
 if(enabled&&requests<3)request(commands[requests],expected[requests]);
 else if(renew&&!renewed&&nowMs>=25000){
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
extern "C" uint32_t serial_probe_max_latency(){return maxLatency;}
extern "C" uint32_t serial_probe_latency(unsigned i){assert(i<4);return latencies[i];}

extern "C" int serial_probe_exit_safe(){return wire.exitSafe();}
extern "C" int serial_probe_pending(){return wire.pending();}
extern "C" void serial_probe_path(uint32_t now,int mode){
 nowMs=now;if(!enabled)return;
 assert(requests==3&&!wire.pending());
 queue("RTEF1 12345678 4 PATH 0 2f7061796c6f6164\n");
 if(mode){pathHold=mode;pathRelease=now+1500;if(mode==2)wire.checkpoint();}
 else {wire.checkpoint();collect(" 0 PATH 8");}
}

extern "C" void serial_probe_late(uint32_t now){
 assert(!enabled&&!requests&&wire.exitSafe());nowMs=now;enabled=true;
 for(unsigned i=0;i<3;i++)request(commands[i],expected[i]);
}
