/* Production crown.c is included by the inherited fast-wake fixture. Only
 * Runtime/device boundaries below are modeled; no replacement Home loop. */
#include "RiscRuntimeHostControlV1.h"

static const char *transfer_case;
static bool transfer_baseline,transfer_hello,transfer_exit,transfer_arrived_in_pacing;
static unsigned transfer_checkpoints,transfer_exit_reads,transfer_latched_frame;
static unsigned transfer_latched_phase,transfer_grants_at_exit,transfer_polls_at_exit;
static unsigned transfer_submitted_at_exit,transfer_services_at_exit,transfer_confirms_at_exit;
static unsigned transfer_acquires_after_exit,transfer_checkpoint_writes;
static unsigned transfer_terminal_grants,transfer_terminal_releases,transfer_terminal_exit_reads;
static risc_runtime_api_v1 transfer_runtime;
static const risc_runtime_api_v1 *transfer_api;

const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
    assert(version==1);return transfer_api;
}
static bool transfer_requested(void) { ++transfer_exit_reads;return transfer_exit; }
static bool transfer_health(risc_runtime_health_v1 *out) {
    check_io();out->uptime_ms=clock_ms;
    if(transfer_baseline && transfer_exit && clock_deferred_phase==3)return false;
    return !stage_calls && clock_ms<30000u;
}
static bool transfer_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant) {
    if(transfer_exit)++transfer_acquires_after_exit;
    return fast_acquire(name,version,id,grant);
}
static void transfer_yield(uint32_t ms) {
    /* The second frame has COMPLETED, so this is actual production pacing,
     * not a pending presentation callback or the foreground checkpoint. */
    if(!strcmp(transfer_case,"after-pacing") && ready && last_complete==2 &&
       !held && !pending && !owned && clock_deferred_phase==0) {
        assert(!transfer_exit);transfer_hello=transfer_arrived_in_pacing=true;
    }
    if(!strcmp(transfer_case,"during-presentation") && ready && submitted==2 && pending) {
        assert(!transfer_exit);transfer_hello=true;
    }
    yield(ms);
}
static int32_t transfer_checkpoint_write(uint64_t invocation,const char *text,uint32_t size) {
    assert(invocation==91 && text && size==26 && !memcmp(text,"WATCH_HOME_EXIT line=",21));
    assert(++transfer_checkpoint_writes==1);return RISC_DIAGNOSTIC_CHECKPOINT_OK;
}
static bool transfer_checkpoint_client(risc_diagnostic_checkpoint_client_v1 *out) {
    if(retained_fence)return false;
    check_io();assert(out && out->struct_size==sizeof(*out));
    *out=(risc_diagnostic_checkpoint_client_v1){RISC_DIAGNOSTIC_CHECKPOINT_API_V1,sizeof(*out),91,transfer_checkpoint_write};
    return true;
}
static void transfer_latch(void) {
    assert(!transfer_exit);transfer_exit=true;
    transfer_latched_frame=last_complete;transfer_latched_phase=clock_deferred_phase;
    transfer_grants_at_exit=grants;transfer_polls_at_exit=polls;
    transfer_submitted_at_exit=submitted;transfer_services_at_exit=service_acquires;
    transfer_confirms_at_exit=confirmations;
    if(!strcmp(transfer_case,"release-refusal"))fail_release=true;
    if(!strcmp(transfer_case,"pause-refusal"))fail_context_pause=true;
}
static int32_t transfer_checkpoint(void) {
    check_io();assert(first_seen && ready && !held && !pending && !owned && last_complete==submitted);
    ++transfer_checkpoints;
    if(!strncmp(transfer_case,"terminal-",9) && transfer_checkpoints==2) {
        transfer_terminal_grants=grants;transfer_terminal_releases=ungrants;
        transfer_terminal_exit_reads=transfer_exit_reads;retained_fence=true;
        return !strcmp(transfer_case,"terminal-context")?RISC_RUNTIME_CHECKPOINT_CONTEXT:
               !strcmp(transfer_case,"terminal-retained")?RISC_RUNTIME_CHECKPOINT_RETAINED:-99;
    }
    if(!strcmp(transfer_case,"before-phase0") && transfer_checkpoints==1)transfer_hello=true;
    if((!strcmp(transfer_case,"active-cleanup") || !strcmp(transfer_case,"release-refusal") ||
        !strcmp(transfer_case,"pause-refusal")) && clock_deferred_phase==3)transfer_hello=true;
    if(!strcmp(transfer_case,"pending-pair") && transfer_checkpoints==1) {
        /* Native admission refuses an unconfirmed pair. Home has no authority
         * to turn that refusal into a cancellation or fabricate confirmation. */
        transfer_hello=true;return RISC_RUNTIME_CHECKPOINT_OK;
    }
    if(transfer_hello && strcmp(transfer_case,"pending-pair") && !transfer_exit)transfer_latch();
    return transfer_exit?RISC_RUNTIME_CHECKPOINT_BUSY:RISC_RUNTIME_CHECKPOINT_OK;
}
static void transfer_helper_compatibility(void) {
    assert(!risc_runtime_exit_requested_v1(NULL));
    const size_t sizes[]={RISC_RUNTIME_CAPABILITIES_V1_SIZE,RISC_RUNTIME_MONOTONIC_V1_SIZE,
                          RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE-1};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        risc_runtime_api_v1 *legacy=malloc(sizes[i]);assert(legacy);
        memcpy(legacy,&transfer_runtime,sizes[i]);legacy->struct_size=(uint32_t)sizes[i];
        assert(!risc_runtime_exit_requested_v1(legacy));free(legacy);
    }
    transfer_runtime.exit_requested=NULL;
    assert(!risc_runtime_exit_requested_v1(&transfer_runtime) && !transfer_exit_reads);
}
int main(int argc,char **argv) {
    assert(argc==3);transfer_case=argv[1];transfer_baseline=strcmp(argv[2],"fixed")!=0;
    reset_case();probe_long=true;saved_radio(0);contexts_enabled=true;fixture_provision_mode=2;
    transfer_runtime=fast_runtime;transfer_api=&transfer_runtime;
    transfer_runtime.health=transfer_health;transfer_runtime.yield_ms=transfer_yield;
    transfer_runtime.acquire=transfer_acquire;transfer_runtime.service_checkpoint=transfer_checkpoint;
    transfer_runtime.exit_requested=transfer_requested;
    transfer_runtime.diagnostic_checkpoint_client=transfer_checkpoint_client;
    if(!strcmp(transfer_case,"helper")) {
        transfer_helper_compatibility();puts("HOME_TRANSFER_EXIT helper compatibility PASS");return 0;
    }
    if(!strcmp(transfer_case,"null-runtime")) {
        transfer_api=NULL;app_main();assert(!grants && !transfer_exit_reads && !transfer_checkpoints);
        puts("HOME_TRANSFER_EXIT null Runtime compatibility PASS");return 0;
    }
    risc_runtime_api_v1 *legacy=NULL;
    if(!strcmp(transfer_case,"old")) {
        size_t bytes=RISC_RUNTIME_MONOTONIC_V1_SIZE;
        legacy=malloc(bytes);assert(legacy);memcpy(legacy,&transfer_runtime,bytes);
        legacy->struct_size=(uint32_t)bytes;transfer_api=legacy;
    } else if(!strcmp(transfer_case,"partial")) {
        size_t bytes=RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE-1;
        legacy=malloc(bytes);assert(legacy);memcpy(legacy,&transfer_runtime,bytes);
        legacy->struct_size=(uint32_t)bytes;transfer_api=legacy;
    } else if(!strcmp(transfer_case,"null-exit"))transfer_runtime.exit_requested=NULL;
    else if(!strcmp(transfer_case,"after-pacing"))transfer_cost=1;
    app_main();
    if(!strncmp(transfer_case,"terminal-",9)) {
        assert(!transfer_baseline && retained_fence && transfer_checkpoints==2);
        assert(transfer_exit_reads==transfer_terminal_exit_reads && transfer_exit_reads==1);
        assert(grants==transfer_terminal_grants && ungrants==transfer_terminal_releases && grants>ungrants);
        assert(!held && !pending && !owned && !service_acquires && !confirmations && !promotions);
        assert(!transfer_checkpoint_writes && !retains);
        printf("HOME_TRANSFER_EXIT %ux%u %s terminal custody preserved PASS\n",TEST_WIDTH,TEST_HEIGHT,transfer_case);
        return 0;
    }
    bool refusal=!strcmp(transfer_case,"release-refusal") || !strcmp(transfer_case,"pause-refusal");
    if(transfer_exit && !transfer_baseline) {
        assert(transfer_checkpoint_writes==1 && !transfer_acquires_after_exit);
        assert(grants==transfer_grants_at_exit && polls==transfer_polls_at_exit);
        assert(submitted==transfer_submitted_at_exit && service_acquires==transfer_services_at_exit);
        assert(confirmations==transfer_confirms_at_exit && !confirmations && !promotions);
        assert(!launches && !sleeps && !fixture_provision_scans && !held && !pending && !owned);
        if(refusal) {
            assert(retained_fence && retains && grants>ungrants && transfer_latched_phase==3);
            assert(native_clock.time_live && native_clock.wake_live);
        } else {
            check_clean();assert(!native_clock.time_live && !native_clock.wake_live);
            assert(!broadcast_live && !contexts_live && !touch.subscription);
        }
        if(!strcmp(transfer_case,"before-phase0")) {
            assert(transfer_checkpoints==1 && transfer_latched_phase==0 && transfer_latched_frame==1);
            assert(!input_stages && !alarm_acquires && !service_acquires && !stage_count("radios-begin"));
        } else if(!strcmp(transfer_case,"after-pacing") || !strcmp(transfer_case,"during-presentation")) {
            assert(transfer_checkpoints==2 && transfer_latched_phase==0);
            if(!strcmp(transfer_case,"after-pacing"))assert(transfer_arrived_in_pacing);
            assert(transfer_latched_frame==2 && input_stages==1 && alarm_acquires==1);
            assert(!service_acquires && !stage_count("radios-begin"));
        } else assert(transfer_latched_phase==3 && service_acquires==4);
    } else if(transfer_exit && transfer_baseline) {
        assert(!refusal && transfer_acquires_after_exit && !confirmations && clock_deferred_phase==3);
        assert(stage_count("radios-begin")==1 && service_acquires==4);
        assert(transfer_checkpoints==submitted-1 && !transfer_exit_reads);
        if(!strcmp(transfer_case,"after-pacing"))assert(transfer_arrived_in_pacing && transfer_latched_phase==1);
        check_clean();
    } else {
        check_clean();assert(confirmations==1 && promotions==1 && clock_deferred_phase==4);
        assert(service_acquires==4 && stage_count("radios-begin")==1 && !transfer_exit);
        if(!strcmp(transfer_case,"old") || !strcmp(transfer_case,"partial"))assert(!transfer_exit_reads && !transfer_checkpoints);
        else if(!strcmp(transfer_case,"null-exit"))assert(!transfer_exit_reads && transfer_checkpoints==submitted-1);
        else if(!strcmp(transfer_case,"pending-pair"))assert(transfer_hello && transfer_exit_reads && transfer_checkpoints>1);
        else {
            assert(!strcmp(transfer_case,"no-transfer") && transfer_checkpoints>1);
            if(transfer_baseline)assert(!transfer_exit_reads && transfer_checkpoints==submitted-1);
            else assert(transfer_exit_reads);
        }
    }
    free(legacy);
    printf("HOME_TRANSFER_EXIT %ux%u variant=%s case=%s checkpoints=%u exit_reads=%u latched_phase=%u latched_frame=%u acquisitions_after_exit=%u services=%u confirm=%u grants=%u releases=%u retained=%u PASS\n",
        TEST_WIDTH,TEST_HEIGHT,argv[2],transfer_case,transfer_checkpoints,transfer_exit_reads,
        transfer_latched_phase,transfer_latched_frame,transfer_acquires_after_exit,service_acquires,
        confirmations,grants,ungrants,retains);
    return 0;
}
