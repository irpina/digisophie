# Sophie DSP behavior audit (S020)

This report records the S020 baseline. The implemented S021 follow-up and its
measurements are at the end of this file.

## Conclusion

The fixed-point port is generating a continuous oscillator, and the reported
pops are visible in its waveform. They are **not predominantly 32-sample
buffer seams**: in the factory-like Fuse case, only 1 of 48 large jumps falls
on a block boundary. The original floating-point Sophie oscillator also
produces bursts of sharp jumps at comparable FM settings, so some of the
metallic/crackling character is inherent to this algorithm.

The port is **not yet level- or model-faithful enough to treat S020 as the
final hardware build**. In particular, it feeds almost full-scale oscillator
output into Digitakt's stock processing, whereas original Sophie soft-clips
the voice and then applies a 0.24 mix coefficient before its output. That is
about 4.5 times less level for a single voice before accounting for the
different downstream drive/filter paths. The stock Digitakt AMP may attenuate
or amplify it further; the final hardware output has not been measured.

## Reproducible waveform measurements

`tests/waveform_probe.c` renders 0.5 seconds in 32-sample blocks at 48 kHz and
counts adjacent-sample jumps greater than 8,192 in 16-bit sample units.
Compile and run from `digisophie/`:

```sh
cc -std=c99 -O2 -Wall -Wextra -Werror -I. sophie.c tests/waveform_probe.c -lm -o /tmp/digisophie-waveform-probe
/tmp/digisophie-waveform-probe
```

| Signal | Maximum one-sample jump | Jumps > 8,192 / 24,000 samples |
| --- | ---: | ---: |
| Port, plain sine (METAL 0, FEEDBACK 0) | 612 | 0 |
| Port, light FM (METAL 32, FEEDBACK 0) | 6,165 | 0 |
| Port, Fuse, ~50 Hz, COLOR 64, METAL 40, FEEDBACK 32 | 57,535 | 48 |
| Original-style floating Fuse oscillator, same controls | 34,262 | 34 |
| Same floating oscillator after original 0.24 × 30,000 final mix gain only | 7,528 | 0 |
| Port, Fuse, ~132 Hz, METAL 90, FEEDBACK 32 | 65,494 | 12,361 |
| Original-style floating Fuse oscillator, same controls | 65,527 | 11,790 |

The original-style reference reproduces the relevant floating-point oscillator
formula from `schwung-sophie/src/dsp/sophie.c` for non-kick Fuse. It deliberately
omits its decay, drive, ring, and filter stages, so it is an oscillator
comparison, **not** a full original-plugin audio comparison. The mix-only row
applies the original final single-voice gain without those other stages.

The port's ~50 Hz burst starts around sample 416 (8.7 ms). Several adjacent
samples alternate by 30,000–57,000 units. A normal oscilloscope view or a
pre-FX digital capture would show it plainly. Increasing METAL and FEEDBACK
makes high-frequency FM and aliasing far more persistent in both engines.

The probe passed an undefined-behavior sanitizer run for these settings.
That rules out detected C arithmetic undefined behavior in this host probe;
it does not validate the Digitakt's downstream processing or hardware timing.

## Other differences from original Sophie

- **Split model:** the source adds `+0.38 sin(carrier × 1.003 − 0.71 index ×
  mod2)`. The port subtracts a differently modulated sine at the carrier's
  exact frequency. This can change cancellation, spectrum, and clipping.
- **Output limiting:** the port clamps each oscillator sample directly to
  16-bit full scale. Original Sophie applies a normalized soft-clip drive
  stage before the final mix gain. Port Split and Shard can hit their hard
  limit; the probe measured 13 clipped samples in 0.5 seconds for a
  factory-like Split setting.
- **Retrigger phase:** original Sophie zeros a voice's oscillator phases at
  each note-on. The port resets attack/pitch/feedback but leaves oscillator
  phases running. This changes repeatability and can alter attacks.
- **Live parameters:** original Sophie copies a patch into the voice at
  note-on. The port reads current Digitakt parameter values every audio
  block so knobs and locks work while sustaining. Abrupt large changes can
  introduce a discontinuity; per-control smoothing has not been measured.
- **Intentional Shard texture:** both engines quantize the carrier phase to
  32 steps and add noise. The model is expected to sound rougher than a sine.

## One-cycle ONESHOT comparison

Looping a clean one-cycle sine in ONESHOT is a useful **downstream control**:
use the same track AMP/filter/drive/BR, with BR at zero, to hear whether the
emulator or stock chain pops on a smooth continuous input. It will not test
Sophie's FM oscillators or prove phase continuity in the custom renderer.
For a fair A/B, compare recordings at matched loudness and with identical
trig timing; use a seamless sample loop.

## Recommendation

Before hardware release, bring the injected level and limiting behavior
closer to the original or establish an explicitly chosen Digitakt gain
convention, correct the Split topology, then compare pre-FX and recorded
post-FX waveforms at matched levels. A real-device timing and listening pass
is still needed. The emulator's full automated check currently stops in its
local Unicorn `SIGILL` path, so this audit is based on host DSP renders and
source/render-path inspection.

## S021 follow-up

The fixed-point engine now uses a smooth output limit with additional headroom,
damped feedback, a pitch-dependent FM depth ceiling, and sample-by-sample
smoothing for COLOR, METAL, FBK and velocity. New notes and MODEL changes reset
the oscillator phases and blend from the preceding output for 64 samples.
Stack and Split use independent detuned phase accumulators; Split's second
oscillator now has the intended positive weight and negative phase modulation.
The Shard noise bandwidth follows COLOR.

The same host probe gives these results. Each row contains 24,000 output
samples, and the jump threshold is 8,192 in 16-bit sample units.

| S021 setting | Peak level | Maximum adjacent jump | Jumps > 8,192 |
| --- | ---: | ---: | ---: |
| Plain sine | 8,191 | 275 | 0 |
| Fuse, ~50 Hz, COLOR 64, METAL 40, FBK 32 | 8,191 | 1,397 | 0 |
| Stack, same controls | 7,877 | 343 | 0 |
| Split, same controls | 8,883 | 753 | 0 |
| Shard, same controls | 7,493 | 2,740 | 0 |
| Fuse, ~132 Hz, METAL 90, FBK 32 | 8,191 | 10,413 | 36 |
| Fuse, extreme METAL/COLOR/FBK | 8,191 | 16,367 | 3,624 |

The factory-like Fuse case fell from 57,535 to 1,397 maximum jump. Relative to
its peak, that is a drop from about 1.76 to 0.17, so the change is more than
lowering gain. The more aggressive settings still produce sharp high-frequency
changes; this is a brighter, approximate implementation rather than an
alias-free reproduction. Host tests cover block-size consistency, retriggers,
model changes, endpoint settings, and C sanitizer checks. Emulator and physical
Digitakt listening remain necessary to assess the complete audio path.
