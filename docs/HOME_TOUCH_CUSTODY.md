# Home raw-touch custody

The shared Clock launcher preserves the raw-touch ABI distinction: failed polls
and GAP (`next == -1`) cancel observations, drain notifications and wait for a
successful sample; they do not fabricate a contact release or arm a gesture.
`next < -1` or unconfirmed touch cleanup is terminal. Snapshot refusal stays
recoverable when an immediate typed next result does not report a provider
fault; its invalid observation cancels gestures. The CST provider propagates
snapshot unlock failure, and its latched fault result makes this classification
without dependency callbacks. A notification consumed by classification is
part of the canceled observation, never fabricated contact state. The
invocation-local fence raises the existing native runtime halt /
retention marker once, then prevents further provider calls, health queries,
rendering, exit-checkpoint writes and cleanup. Ordinary no-runtime-feature
profiles use the existing Runtime retention suffix when available.

The fence covers normal foreground sampling, in-render checkpoints and pending
presentation sampling, QuickActions pre-save input, sleep/wake touch boundaries,
normal return and teardown. A retained frame/grant is not released speculatively.
The shared sleep modes, deadlines, gestures, navigation and successful cleanup
remain unchanged. This is paired with the provider's fault-versus-GAP semantics,
not a reclassification of transient I/O into terminal failure.

Owning normal/sanitized tests exercise foreground and in-render terminal events,
snapshot custody faults, wake-open and cleanup failures, failed-poll
terminal faults, recoverable GAPs, and benign snapshot refusals whose immediate
status is empty, GAP, or a queued event. The unchanged raster boundary suite
continues to require recovery after snapshot refusal. Native optional-feature and legacy
no-runtime-feature variants assert no subsequent callbacks and exactly one
retention marker. Simulated raster work is test-only, not physical timing data.
Physical cause and device acceptance remain unproven; product integration is a
separate qualification step.
