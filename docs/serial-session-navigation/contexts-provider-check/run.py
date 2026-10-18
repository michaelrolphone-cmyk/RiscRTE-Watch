from pathlib import Path
import hashlib,json,os,subprocess
u=Path('/workspace/shared/wave-lifetime-successor/utilities-clean');r=Path('/workspace/shared/wave-lifetime-successor/runtime-qualified');out=Path(__file__).resolve().parent
original=u/'test/native_apps/contexts_service_test.c';source=original.read_text();anchor='    assert(driver->quiesce());driver->stop();'
assert source.count(anchor)==1
source=source.replace(anchor,'''    /* A late native launch refusal must not permanently disable a supported source. */
    assert(service->request_export(NULL,CONTEXTS_RADIO));
    assert(service->begin_export(NULL,CONTEXTS_RADIO));
    assert(service->finish_export(NULL,CONTEXTS_RADIO,CONTEXTS_EXPORT_UNSUPPORTED));
    contexts_status_v1 refused=status();
    assert(!refused.export_active&&!refused.export_pending);
    assert(refused.radio.model_state==CONTEXTS_MODEL_FAILED);
    assert(refused.radio.model_state!=CONTEXTS_MODEL_UNAVAILABLE);
    assert(refused.radio.model_error==CONTEXTS_EXPORT_UNSUPPORTED);
    assert(service->request_export(NULL,CONTEXTS_RADIO));
    assert(status().radio.model_state==CONTEXTS_MODEL_REQUESTED);
    assert(service->begin_export(NULL,CONTEXTS_RADIO));
    assert(service->finish_export(NULL,CONTEXTS_RADIO,CONTEXTS_EXPORT_STORAGE));
    puts("CONTEXTS_LATE_LAUNCH_RETRY supported radio remains retryable after unsupported launch outcome PASS");
'''+anchor)
fixture=out/'contexts_retry.c';fixture.write_text(source)
receipt={'scope':'Exact production Contexts provider with existing host PCM/IQ harness; generic unsupported launch outcome and explicit retry. No device test.','utilities_head':subprocess.check_output(['git','-C',str(u),'rev-parse','HEAD'],text=True).strip(),'inputs':{},'runs':[]}
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for p in [original,fixture,Path(__file__).resolve(),out/'RiscRadioIqV1.h']:receipt['inputs'][str(p)]=digest(p)
for sanitized in [False,True]:
 binary=out/('test-san' if sanitized else 'test');dep=out/('source-san.d' if sanitized else 'source.d')
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
 command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,*['-I'+str(p) for p in [out,u/'Apps',u/'lib/Contexts/include',r/'sdk/driver']],'-MMD','-MF',str(dep),str(u/'Services/contexts/service.c'),str(fixture),'-lm','-o',str(binary)]
 result=subprocess.run(command,text=True,capture_output=True);print(result.stdout+result.stderr,end='');result.check_returncode()
 # Record the complete source trees used by the provider, including inline model code.
 for base in [u/'Apps',u/'lib/Contexts/include',u/'Services/contexts',r/'sdk/driver']:
  for p in base.rglob('*'):
   if p.is_file() and p.suffix in ['.h','.c']:receipt['inputs'][str(p)]=digest(p)
 result=subprocess.run([str(binary)],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'});print(result.stdout+result.stderr,end='')
 (out/('sanitized.log' if sanitized else 'normal.log')).write_text(result.stdout+result.stderr)
 receipt['runs'].append({'sanitized':sanitized,'command':command,'binary_sha256':digest(binary),'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr})
 (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');result.check_returncode()
