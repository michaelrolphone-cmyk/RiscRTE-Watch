# Elapsed animation duration and progress

Home intro (1,800 ms), final intro hold (250 ms), logo blend (the existing
PortableTransition duration), and return blend (60 ms) terminate by elapsed
monotonic time, not rendered-frame count. The prior 100-frame bound could fail
before 1,800 ms when a pending launcher gesture bypassed frame pacing and a
faster provider completed more frames.

Animations retain the existing nominal 20 ms pacing, including Contexts input
checkpoints when selected. Pending navigation remains latched; only the normal
interactive frame pacing may bypass its wait to dispatch it promptly. No intro
is skipped and no display/clock authority or custody checks are relaxed.

A shared progress reader accepts zero and uint32 wrap. Backward or ambiguous
half-range steps fail. Repeated timestamps permit at most 64 cooperative 1 ms
waits per progress observation, then fail. This is a bounded-work policy, not a
promise of 1 ms clock resolution or measured 64 ms wall-clock timeout. Valid
coarse clocks are covered at 4 and 16 ms; an unavailable clock still exits via
alive(), with no fallback callback. Slow rendering counts toward duration.

The display completion loop is deliberately unchanged: its time/poll bounds
protect an accepted transfer's custody and are not animation frame limits.

`scripts/test_animation_duration.py --output PATH` extracts the actual animation
and pacing bodies and uses the production RGB565 transition helper, with modeled
provider costs/clocks. Normal and ASan/UBSan cases cover fast/slow rendering,
pending input, quantization, wrap, backward/stalled/unavailable time and early
frame/presentation failures including retained ownership. This is software
qualification, not device timing or FPS evidence.
