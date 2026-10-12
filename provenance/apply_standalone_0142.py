"""One-time shared source migration; standalone profiles do not invent paired metadata."""
from pathlib import Path
import json
r=Path(__file__).resolve().parents[1]
if json.loads((r/'apps/clock/default.json').read_text())['version']=='0.14.2':raise SystemExit(0)
assert json.loads((r/'apps/clock/default.json').read_text())['version']=='0.14.1'
p=r/'apps/runtime_features/clock_runtime.h';s=p.read_text();old=' if(!rt->acquire(RISC_RETAINED_WAKE_CAPABILITY,1,0,&c->wake_grant))return false;';new=''' if(!rt->acquire(RISC_RETAINED_WAKE_CAPABILITY,1,0,&c->wake_grant)) {
  /* A standalone boot-test or other non-retained deployment can omit this
   * service. Start cold; never claim a restored checkpoint without a grant. */
  rt->diagnostic("WATCH_CLOCK retained-wake=unavailable resume=cold");
  return true;
 }''';assert old in s;s=s.replace(old,new);p.write_text(s)
p=r/'tests/fast_wake_clock_test.c';s=p.read_text();needle=' reset_case();fail_capability=ALARM_SERVICE_CAPABILITY;';assert needle in s;s=s.replace(needle,' reset_case();fail_capability=RISC_RETAINED_WAKE_CAPABILITY;app_main();check_clean();assert(first_seen&&confirmations==1&&!native_clock.resume);\n'+needle);p.write_text(s)
for name in ['default.json','clock.json']:
 p=r/'apps/clock'/name;m=json.loads(p.read_text());m['version']='0.14.2';p.write_text(json.dumps(m,indent=2)+'\n')
