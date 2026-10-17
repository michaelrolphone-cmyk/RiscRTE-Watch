/* Actual shared Home with the existing fast-wake peripheral model. The runner
 * changes only the inherited fixture's main/getter names in a disposable copy.
 * A modeled RAM checkpoint client never stands in for device qualification. */
#include "home_exit_fixture.inc"
#include "home_exit_expected_lines.h"

static risc_runtime_api_v1 checkpoint_runtime;
static uint64_t checkpoint_invocation, saved_invocation;
static unsigned checkpoint_gets, checkpoint_writes, checkpoint_saves;
static unsigned diagnostic_calls, malformed_client, total_invocations;
static bool deny_getter, deny_writer, revoke_on_get, revoked, null_runtime;
static bool invocation_live, enforce_once = true;
static char saved_checkpoint[RISC_DIAGNOSTIC_CHECKPOINT_TEXT_MAX + 1u];

static bool checkpoint_diagnostic(const char *text) {
    ++diagnostic_calls;
    assert(strncmp(text, "WATCH_HOME_EXIT", 15));
    return fast_diagnostic(text);
}
static int32_t checkpoint_write(uint64_t invocation, const char *text, uint32_t length) {
    ++checkpoint_writes;
    if (enforce_once) assert(checkpoint_writes == 1);
    assert(length == 26 && text && !memcmp(text, "WATCH_HOME_EXIT line=", 21));
    for (unsigned i = 21; i < length; ++i) assert(text[i] >= '0' && text[i] <= '9');
    if (!invocation_live || revoked || retained_fence || deny_writer || invocation != checkpoint_invocation)
        return RISC_DIAGNOSTIC_CHECKPOINT_DENIED;
    memcpy(saved_checkpoint, text, length);
    saved_checkpoint[length] = 0;
    saved_invocation = invocation;
    ++checkpoint_saves;
    return RISC_DIAGNOSTIC_CHECKPOINT_OK;
}
static bool checkpoint_get(risc_diagnostic_checkpoint_client_v1 *out) {
    ++checkpoint_gets;
    if (enforce_once) assert(checkpoint_gets == 1);
    assert(out && out->struct_size == sizeof(*out));
    if (!invocation_live || revoked || retained_fence || deny_getter) return false;
    *out = (risc_diagnostic_checkpoint_client_v1){
        RISC_DIAGNOSTIC_CHECKPOINT_API_V1, sizeof(*out), checkpoint_invocation, checkpoint_write};
    if (malformed_client == 1) ++out->api_version;
    if (malformed_client == 2) --out->struct_size;
    if (malformed_client == 3) out->write = NULL;
    if (revoke_on_get) revoked = true;
    return true;
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
    assert(version == 1 && !clock_first_exit);
    ++checkpoint_invocation;
    ++total_invocations;
    checkpoint_gets = checkpoint_writes = checkpoint_saves = 0;
    invocation_live = true;
    return null_runtime ? NULL : &checkpoint_runtime;
}
static void checkpoint_configure(void) {
    assert(fixture_base_runtime_get_api(1) == &fast_runtime);
    checkpoint_runtime = fast_runtime;
    /* Keep the inherited ABI prefix; do not advertise absent sleep callbacks. */
    assert(checkpoint_runtime.struct_size >= RISC_RUNTIME_DIAGNOSTIC_CHECKPOINT_V1_SIZE);
    checkpoint_runtime.diagnostic = checkpoint_diagnostic;
    checkpoint_runtime.diagnostic_checkpoint_client = checkpoint_get;
    deny_getter = deny_writer = revoke_on_get = revoked = null_runtime = false;
    malformed_client = diagnostic_calls = 0;
    enforce_once = true;
}
static void checkpoint_reset(void) {
    reset_case();
    checkpoint_configure();
    strcpy(saved_checkpoint, "prior authorized record");
    saved_invocation = checkpoint_invocation;
}
static void expect_saved(unsigned line) {
    char expected[32];
    assert(line && line < 100000 && clock_first_exit == line);
    assert(snprintf(expected, sizeof(expected), "WATCH_HOME_EXIT line=%05u", line) == 26);
    assert(!strcmp(saved_checkpoint, expected));
    assert(checkpoint_gets == 1 && checkpoint_writes == 1 && checkpoint_saves == 1);
    assert(saved_invocation == checkpoint_invocation);
}
static void expect_unchanged(unsigned line, unsigned gets, unsigned writes) {
    if (clock_first_exit != line) fprintf(stderr, "expected exit line=%u, actual=%u, getter=%u, writes=%u, retained=%u\n", line, clock_first_exit, checkpoint_gets, checkpoint_writes, retained_fence);
    assert(clock_first_exit == line);
    assert(!strcmp(saved_checkpoint, "prior authorized record"));
    assert(saved_invocation != checkpoint_invocation);
    assert(checkpoint_gets == gets && checkpoint_writes == writes && !checkpoint_saves);
}
/* Every field is an observable existing fixture cost/output. Checkpoint calls
 * must neither enter ordinary diagnostics nor touch providers or rendering. */
struct observation {
    unsigned values[21];
    uint32_t pixels_hash;
};
static struct observation observe(void) {
    struct observation out = {{clock_ms, submitted, released, grants, ungrants,
        subscribed, unsubscribed, polls, launches, sleeps, hardware_brightness,
        first_at, input_at, confirmations, promotions, service_acquires, rtc_read_calls,
        points_loads, diagnostic_calls, allocations, retains}, UINT32_C(2166136261)};
    for (unsigned i = 0; i < TEST_WIDTH * TEST_HEIGHT; ++i)
        out.pixels_hash = (out.pixels_hash ^ completed_pixels[i]) * UINT32_C(16777619);
    return out;
}
static void expect_same(struct observation before) {
    struct observation after = observe();
    assert(!memcmp(before.values, after.values, sizeof(before.values)));
    assert(before.pixels_hash == after.pixels_hash);
}
static void expect_no_retry(unsigned later_line) {
    unsigned gets = checkpoint_gets, writes = checkpoint_writes, saves = checkpoint_saves;
    unsigned first = clock_first_exit;
    struct observation before = observe();
    char previous[sizeof(saved_checkpoint)];
    strcpy(previous, saved_checkpoint);
    clock_exit_checkpoint(later_line);
    assert(clock_first_exit == first && checkpoint_gets == gets && checkpoint_writes == writes && checkpoint_saves == saves);
    assert(!strcmp(previous, saved_checkpoint));
    expect_same(before);
}
static void unchanged_normal_case(void) {
    checkpoint_reset();
    checkpoint_runtime.diagnostic_checkpoint_client = NULL;
    app_main();
    check_clean();
    expect_unchanged(HOME_EXIT_NORMAL_END, 0, 0);
    struct observation without = observe();
    checkpoint_reset();
    app_main();
    check_clean();
    expect_saved(HOME_EXIT_NORMAL_END);
    expect_same(without);
    expect_no_retry(HOME_EXIT_NORMAL_END + 1u);
    puts("Home exit: normal first frame/input, pixels, provider counts, timing and cleanup unchanged PASS");
}
static bool fail_later_frame(void *context, uint32_t format, risc_display_surface_v1 *surface) {
    return first_seen ? false : fast_frame_acquire(context, format, surface);
}
static void exact_first_cases(void) {
    checkpoint_reset();
    fail_capability = "display.output";
    app_main();
    check_clean();
    expect_saved(HOME_EXIT_DISPLAY_ACQUIRE);
    expect_no_retry(HOME_EXIT_NORMAL_END);
    uint64_t first_invocation = saved_invocation;
    /* Do not reset the production first-reason field in the fixture. Only a
     * real second app_main entry may admit the next reason. */
    reset_case();
    diagnostic_calls = 0;
    da.base.acquire = fail_later_frame;
    app_main();
    check_clean();
    expect_saved(HOME_EXIT_RENDER_FAILURE);
    assert(saved_invocation == first_invocation + 1u);
    expect_no_retry(HOME_EXIT_NORMAL_END);
    checkpoint_reset();
    ended = true;
    app_main();
    check_clean();
    expect_saved(HOME_EXIT_INITIAL_HEALTH);
    checkpoint_reset();
    checkpoint_runtime.health = NULL;
    app_main();
    check_clean();
    expect_saved(HOME_EXIT_INVALID_RUNTIME);
    /* Successful first RAM write survives an actual later cleanup failure
     * that retains custody and returns through a different source line. */
    checkpoint_reset();
    fail_release = true;
    app_main();
    assert(retained_fence && retains && confirmations == 1);
    expect_saved(HOME_EXIT_NORMAL_END);
    expect_no_retry(HOME_EXIT_NORMAL_END + 1u);
    retained_reset();
    puts("Home exit: exact early-return/goto/loop-break lines, per-invocation reset, first reason survives real cleanup retention PASS");
}
static void refusal_cases(void) {
    for (unsigned mode = 0; mode < 10; ++mode) {
        checkpoint_reset();
        fail_capability = "display.output";
        switch (mode) {
            case 0: checkpoint_runtime.diagnostic_checkpoint_client = NULL; break;
            case 1: checkpoint_runtime.struct_size = RISC_RUNTIME_DIAGNOSTIC_CHECKPOINT_V1_SIZE - 1u; break;
            case 2: deny_getter = true; break;
            case 3: deny_writer = true; break;
            case 4: revoke_on_get = true; break;
            case 5: malformed_client = 1; break;
            case 6: malformed_client = 2; break;
            case 7: malformed_client = 3; break;
            case 8: null_runtime = true; break;
            case 9: checkpoint_runtime.api_version = 2; break;
        }
        app_main();
        check_clean();
        unsigned line = mode >= 8 ? HOME_EXIT_INVALID_RUNTIME : HOME_EXIT_DISPLAY_ACQUIRE;
        expect_unchanged(line, mode < 2 || mode >= 8 ? 0u : 1u, mode == 3 || mode == 4 ? 1u : 0u);
        deny_getter = deny_writer = revoke_on_get = revoked = false;
        malformed_client = 0;
        expect_no_retry(HOME_EXIT_NORMAL_END);
    }
    /* The real Home native-CONTEXT return has already revoked provider
     * authority before its first attempt. The RAM getter must refuse too. */
    checkpoint_reset();
    fail_late_rtc_read = late_rtc_retention = true;
    app_main();
    assert(retained_fence && retains == 1 && !first_seen && !confirmations);
    expect_unchanged(HOME_EXIT_RTC_CUSTODY, 1, 0);
    expect_no_retry(HOME_EXIT_NORMAL_END);
    retained_reset();
    puts("Home exit: absent/short Runtime, denied getter/writer, malformed client, revoked-between-get/write and retained custody preserve prior RAM record PASS");
}
static void helper_boundary_cases(void) {
    checkpoint_reset();
    clock_first_exit = 0; /* Explicit isolated-helper invocation. */
    rt = risc_runtime_get_api(1);
    struct observation before = observe();
    clock_exit_checkpoint(7);
    expect_saved(7);
    expect_same(before);
    expect_no_retry(99999);
    /* A copied callback cannot report success after revocation or after its
     * invocation ends, and must reject a token from another invocation. */
    enforce_once = false;
    unsigned saved = checkpoint_saves;
    const char text[] = "WATCH_HOME_EXIT line=00042";
    revoked = true;
    assert(checkpoint_write(checkpoint_invocation, text, sizeof(text) - 1u) == RISC_DIAGNOSTIC_CHECKPOINT_DENIED);
    revoked = false;
    invocation_live = false;
    assert(checkpoint_write(checkpoint_invocation, text, sizeof(text) - 1u) == RISC_DIAGNOSTIC_CHECKPOINT_DENIED);
    invocation_live = true;
    assert(checkpoint_write(checkpoint_invocation - 1u, text, sizeof(text) - 1u) == RISC_DIAGNOSTIC_CHECKPOINT_DENIED);
    assert(checkpoint_saves == saved && !strcmp(saved_checkpoint, "WATCH_HOME_EXIT line=00007"));
    expect_same(before);
    puts("Home exit: bounded 26-byte zero-padded RAM-only helper, no provider/diagnostic/allocation work, stale/revoked/finished invocation rejection PASS");
}
int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    checkpoint_configure();
    assert(inherited_fast_fixture_main() == 0);
    printf("Home exit: existing fast-wake lifecycle assertions with one-attempt checkpoint client across %u invocations PASS\n", total_invocations);
    unchanged_normal_case();
    exact_first_cases();
    refusal_cases();
    helper_boundary_cases();
    printf("Home exit checkpoint: %ux%u PASS (host model; physical cause remains unknown)\n", TEST_WIDTH, TEST_HEIGHT);
    return 0;
}
