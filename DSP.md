# Digitakt Mk1 Sophie DSP reference

This note records the parts of Digitakt Mk1 OS 1.53 that Sophie actually uses,
the renderer's fixed-point choices, and the remaining performance limit. It
is for developers changing the synth, not a substitute for the stock OS or
hardware testing. Addresses below come from this project's OS 1.53 adapter
and patch manifest; they must not be reused on another firmware version.

The [RingTone DSP reference](https://github.com/DigiAlchemydsp/RingTone/blob/main/DSP.md)
inspired this layout. RingTone documents the **Digitone Mk1 OS 1.43**, whose
second CPU runs its FM voices. Its addresses, shared-memory layout and patch
sites do not describe the Digitakt or Sophie.

## Audio path

Sophie is a custom SRC machine, ID 7. Core 2.1 registers it; `glue.s` hooks
the Digitakt's audio render at `0x40077fba` and calls `ds_inject` after the
stock source work. For each Sophie track, `digitakt.c` reads its current
controls and trigger state, writes 32 mono `int32_t` source samples to the
track buffer at `0x80001a18 + 128*track`, then lets the stock AMP, filter,
mixer and sends process them. Sophie now folds that source buffer with its C
knob before the stock stages. The C knob keeps BR's persistent value slot but
is labelled FOLD; zero bypasses the fold. A fixed, interpolated gain curve
after the fold keeps the source level close to its original range. The SAMP
slot remains stock.

The audio callback runs every 32 frames at 48 kHz, about 1,500 times per
second. It is time-critical: a synth optimization must reduce work **while
the note is audible**, not just after the voice sleeps. The adapter's
per-track state is static; retriggering replaces that track's voice instead
of accumulating another one.

## Fixed-point renderer

`sophie.c` evaluates 16 synth samples at 24 kHz for each 32-frame output
block and linearly interpolates to 48 kHz. FUSE, PIPE and SHARD use two
1024-entry sine lookups per synth sample; BOOM uses a third lookup for its
clean low body. The common soft clip uses a 65-point lookup with linear
interpolation. These are intentional sound/performance tradeoffs; the
lookup quantization is part of the current sound.

The slowly moving controls, oscillator ratio, pitch step and FM bandwidth
ceiling update every four synth samples. Feedback and oscillators still
advance every synth sample. The AMP envelope remains the stock Digitakt
envelope: an active phase keeps Sophie awake even if its instantaneous
level crosses zero. After an idle/released envelope is quiet for 32 blocks,
Sophie fades its source over 128 synth samples and then skips synthesis.

The 0–127 control-to-Q15 conversion is exact using `258*x + (x == 127)`:
`32767 = 127*258 + 1`. This removes four signed divisions per active block
without changing a parameter value. This is in the post-S027 source candidate;
the tested 0.1.7 release remains available. The FM bandwidth ceiling has a
conditional signed division on slow-control ticks when the common no-clamp
case cannot be proven. Replacing it would require an error-bounded reciprocal
and listening tests at extreme pitch/METAL settings; it is not a free win.

## Verification boundaries

`make test` checks audio continuity, model differences, long-held voices,
retriggering and envelope sleep. `make cross-check` compiles the C and glue
for the MCF54455. The elekloader build checks the mod against the user's
stock OS and verifies the patched firmware. Those checks do not measure
hardware CPU use. The optional digihealth mod shows live CPU/DSP load and
FAST AUDIO in SETTINGS, and `tests/timing_probe.py` can measure an emulated
render path where a compatible digiemu runtime is available.

Do not transplant RingTone's Digitone-specific EMAC, Core B, DMA-buffer or
section-7 techniques into this Digitakt source hook. A new optimization
should preserve the audible result or have an explicit sonic reason, pass
the host and cross-build tests, and then be checked on a Digitakt.
