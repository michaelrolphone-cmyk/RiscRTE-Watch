// Production Runtime, ProviderGraph, dynamic loader, scene provider and Stopwatch.
// Physical display/input are deterministic doubles. Host-only, not device execution.
#include "bootstrap/Runtime.h"
#include "RuntimeFileFixture.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include "RiscRealtimeV1.h"
using RiscBoot::Runtime;
static std::string root,mode;
static unsigned width,height,format,invocations,finis,unloads,visits,frames;
static bool key_mode;static unsigned key_delivered;
extern "C" bool resident_test_key_mode(){return key_mode;}
extern "C" void resident_test_key_delivered(){++key_delivered;}
static unsigned control_frames;
static unsigned homes,home_finis,home_unloads,phase=99,phase_at,controls,child_subscriptions;
static bool ready,child_done,controls_gesture;
static uint64_t elapsed;
static unsigned writes;
static std::map<std::string,std::vector<uint8_t>> saved;
static int32_t get(void*,uint32_t ns,const char*k,void*b,uint32_t cap,uint32_t*n){auto i=saved.find(std::to_string(ns)+":"+k);if(i==saved.end()){*n=0;return RISC_KEY_VALUE_NOT_FOUND;}*n=i->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,i->second.data(),*n);return 0;}
static int32_t put(void*,uint32_t ns,const char*k,const void*b,uint32_t n){assert(ns==2&&n==20&&!strcmp(k,"stopwatch_utc"));++writes;saved[std::to_string(ns)+":"+k]=std::vector<uint8_t>((const uint8_t*)b,(const uint8_t*)b+n);return 0;}

extern "C" const void* scene_test_hardware(unsigned);
extern "C" void scene_test_hardware_closed();
extern "C" unsigned scene_test_invocation(){return invocations;}
extern "C" unsigned scene_test_width(){return width;}
extern "C" unsigned scene_test_height(){return height;}
extern "C" unsigned scene_test_format(){return format;}
extern "C" uint64_t scene_test_millis(){return elapsed;}
static bool(*home_observer)();
extern "C" void resident_test_set_home_observer(bool(*observer)()){home_observer=observer;}
extern "C" bool resident_test_started(){return writes>0;}
extern "C" void scene_test_frame(const void*p,size_t n){++frames;assert(p&&n==width*height*2);if(home_observer&&home_observer())++control_frames;}

extern "C" unsigned resident_test_phase(){return phase;}
extern "C" unsigned resident_test_age(){return (unsigned)elapsed-phase_at;}
extern "C" void resident_test_controls_gesture(){controls_gesture=true;}
extern "C" void resident_test_touch_open(){fprintf(stderr,"TOUCH OPEN time=%llu phase=%u gesture=%d\n",(unsigned long long)elapsed,phase,controls_gesture);if(invocations&&!child_done){++child_subscriptions;if(phase==1&&controls_gesture){phase=2;phase_at=elapsed;++controls;}else if(phase==2){phase=3;phase_at=elapsed;}}}
extern "C" void resident_test_touch_close(){}
extern "C" void resident_test_lifecycle(unsigned module,unsigned event){
 fprintf(stderr,"LIFE module=%u event=%u time=%llu phase=%u\n",module,event,(unsigned long long)elapsed,phase);
 if(module==0){if(event==1){++homes;phase=99;ready=false;}else if(event==2)++home_finis;else ++home_unloads;}
 else if(module==1){if(event==1){++invocations;assert(invocations==1);phase=1;phase_at=elapsed;}else if(event==2){++finis;child_done=true;}else ++unloads;}
 else {assert(module==2);if(event==1)++visits;}
}
extern "C" void resident_test_result(unsigned ms,bool running){fprintf(stderr,"RESULT writes=%u running=%d controls=%u subscriptions=%u phase=%u controls_frames=%u\n",writes,running,controls,child_subscriptions,phase,control_frames);assert(writes==1&&running&&controls==1&&child_subscriptions>=3&&control_frames>0);printf("Stopwatch preserved running state through controls elapsed=%u\n",ms);}
static bool owner(){return true;}
static bool health(risc_runtime_health_v1*h){h->uptime_ms=elapsed;return !child_done;}
static bool log_line(const char*s){fprintf(stderr,"Runtime: %s\n",s);if(!invocations&&!ready&&strstr(s,"ready")){ready=true;phase=0;phase_at=elapsed;}return true;}
static void delay(uint32_t n){elapsed+=n;assert(elapsed<30000);}
static void file(const std::string&name,const std::string&data){std::ofstream f(root+"/"+name);f<<data;assert(f.good());}
static std::string serialized(const JsonDocument&doc){std::string s;serializeJson(doc,s);return s;}
static void configure(){
 file("board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"test","revision":"unspecified","buses":[],"devices":[]})");
 JsonDocument boot;boot["board"]="board.json";boot["default_app"]="default.elf";auto shell=boot["resident_shell"].to<JsonObject>();shell["api"]=1;shell["host"]="default.elf";shell["foreground"].to<JsonArray>().add("stopwatch.elf");shell["legacy"].to<JsonArray>().add("springboard.elf");boot["provider_activation"]=mode=="retained-providers"?"demand-retained":"demand";
    auto drivers=boot["drivers"].to<JsonArray>();
    auto driver=[&](const char* id,const char* cap,unsigned api,std::initializer_list<std::pair<const char*,unsigned>> requirements){
        JsonDocument doc;doc["type"]="driver";doc["id"]=id;doc["version"]="0.1.0";doc["driver_abi"]=2;doc["architecture"]="xtensa-esp32s3";doc["file_name"]=std::string(id)+".elf";
        auto rs=doc["requires"].to<JsonArray>();for(auto r:requirements){auto row=rs.add<JsonObject>();row["capability"]=r.first;row["api"]=r.second;}
        auto p=doc["provides"].to<JsonArray>().add<JsonObject>();p["capability"]=cap;p["api"]=api;
        std::string name=std::string(id)+".json";file(name,serialized(doc));auto row=drivers.add<JsonObject>();row["manifest"]=name;return row;
    };

 if(key_mode)driver("power-key","input.power-key",1,{});
 driver("display","display.output",1,{});driver("touch","input.touch.raw",1,{});driver("navigation","input.navigation",1,{});
 driver("scene-presentation-profile","ui.presentation-profile",1,{});
 driver("scene-host","ui.scene",1,{{"display.output",1},{"input.touch.raw",1},{"input.navigation",1},{"platform.clock",1},{"ui.presentation-profile",1}});
 auto policies=boot["app_capabilities"].to<JsonArray>();
 for(const char*id:{"default","springboard","stopwatch"}){
  JsonDocument doc;doc["type"]="application";doc["id"]=id;doc["version"]="0.2.0";doc["architecture"]="xtensa-esp32s3";doc["file_name"]=std::string(id)+".elf";doc["entry"]="app_main";
  doc["requires"].to<JsonArray>();auto p=policies.add<JsonObject>();p["grants"].to<JsonArray>();p["manifest"]=std::string(id)+".json";
  if(!strcmp(id,"stopwatch")){for(const char*cap:{"ui.scene","storage.key-value","runtime.realtime"}){auto q=(doc["optional"].is<JsonArray>()?doc["optional"].as<JsonArray>():doc["optional"].to<JsonArray>()).add<JsonObject>();q["capability"]=cap;q["api"]=1;if(mode=="no-ui"&&!strcmp(cap,"ui.scene"))continue;auto g=p["grants"].is<JsonArray>()?p["grants"].as<JsonArray>().add<JsonObject>():p["grants"].to<JsonArray>().add<JsonObject>();g["capability"]=cap;g["api"]=1;g["instance_id"]=!strcmp(cap,"storage.key-value")?2:0;}}

  if(!strcmp(id,"default")){for(const char*cap:{"display.output","input.touch.raw","storage.key-value"}){auto q=(doc["optional"].is<JsonArray>()?doc["optional"].as<JsonArray>():doc["optional"].to<JsonArray>()).add<JsonObject>();q["capability"]=cap;q["api"]=1;auto g=p["grants"].as<JsonArray>().add<JsonObject>();g["capability"]=cap;g["api"]=1;g["instance_id"]=!strcmp(cap,"storage.key-value")?1:0;}}
  if(!strcmp(id,"default")&&key_mode){auto q=doc["optional"].as<JsonArray>().add<JsonObject>();q["capability"]="input.power-key";q["api"]=1;auto g=p["grants"].as<JsonArray>().add<JsonObject>();g["capability"]="input.power-key";g["api"]=1;g["instance_id"]=0;}
  file(std::string(id)+".json",serialized(doc));
 }
 file("boot.json",serialized(boot));
}
int main(int argc,char**argv){
 assert(argc==7);key_mode=!strcmp(argv[6],"power-key");root=argv[1];width=std::stoul(argv[2]);height=std::stoul(argv[3]);format=std::stoul(argv[4]);mode=argv[5];configure();
 RiscBoot::KeyValueBackend storage{nullptr,get,put};RiscBoot::Port port{owner,health,delay,log_line};port.keyValue=&storage;port.bindPlatforms=[](Runtime&r){return r.registerPlatform("platform.clock",1,Runtime::Scope::Global,0,scene_test_hardware(3))&&r.registerRealtime(static_cast<const risc_realtime_control_api_v1*>(scene_test_hardware(4)));};
 {RiscTest::FileSources files({root.c_str()});Runtime r(port);assert(files.bind(r));if(!r.prepare(root.c_str())){fprintf(stderr,"prepare: %s\n",r.error());return 2;}if(!r.run()){fprintf(stderr,"run: %s\n",r.error());return 3;}assert(!r.retained());}
 assert(frames>0&&invocations==1&&finis==1&&unloads==1&&visits==1&&homes==2&&home_finis==2&&home_unloads==2&&controls==1&&key_delivered==(key_mode?1u:0u));scene_test_hardware_closed();
 printf("actual Runtime Home resident %ux%u %s PASS homes=%u legacy=%u child-unloads=%u controls=%u frames=%u power-key=%u\n",width,height,mode.c_str(),homes,visits,unloads,controls,frames,key_delivered);
}
