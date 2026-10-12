// Runs the production Runtime and the actual zero-service clock return module.
// dlopen is the host loader boundary, not target instruction emulation.
#include "bootstrap/Runtime.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
static unsigned home_entries,child_entries;
static bool app_live;
static std::vector<std::string> events;
static bool owner(){return true;}
static bool health(risc_runtime_health_v1* h){h->uptime_ms=home_entries;return true;}
static void wait_ms(uint32_t){}
static bool log_line(const char* s){
 if(!strcmp(s,"return-test:home")){++home_entries;events.emplace_back("home");}
 if(!strcmp(s,"return-test:child")){++child_entries;events.emplace_back("child");}
 if(!strcmp(s,"return-test:init")){assert(!app_live);app_live=true;events.emplace_back("init");}
 if(!strcmp(s,"return-test:fini")){assert(app_live);app_live=false;events.emplace_back("fini");}
 return true;
}
extern "C" void return_test_lifetime(int loaded){log_line(loaded?"return-test:init":"return-test:fini");}
static void write(const std::string& path,const char* s){std::ofstream(path)<<s;}
int main(int argc,char**argv){
 assert(argc==2);std::string root=argv[1];
 write(root+"/board.json",R"({"schema":"riscrte.board-hardware","schema_version":1,"board_id":"generic-return-test","revision":"unspecified","buses":[],"devices":[]})");
 write(root+"/boot.json",R"({"board":"board.json","default_app":"default.elf","drivers":[]})");
 for(unsigned cycle=0;cycle<3;cycle++){
  home_entries=child_entries=0;app_live=false;events.clear();
  {RiscBoot::Runtime runtime({owner,health,wait_ms,log_line});
   if(!runtime.prepare(root.c_str())){std::fprintf(stderr,"%s\n",runtime.error());return 2;}
   assert(runtime.run());assert(!risc_runtime_get_api(1));}
  assert(home_entries==2&&child_entries==1&&!app_live);
  // Return module has no constructors, service grants or background clients;
  // the previous Home and child finalize before fresh Home initialization.
  const std::vector<std::string> expected={"init","home","fini","init","child","fini","init","home","fini"};
  assert(events==expected);
 }
 puts("Production Runtime: Home -> child -> actual clock trampoline -> fresh Home; teardown/restart PASS");
}
