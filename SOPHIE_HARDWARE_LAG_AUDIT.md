# Sophie hardware lag audit (through S024)

## Observation

On a Digitakt mk1, a fresh project is responsive until a Sophie voice is
triggered. One trigger causes some lag; further manual triggers eventually
make controls and STOP respond seconds late. Audio continues to sound correct.

## Finding

The strongest identified cause is persistent work in the high-priority audio
render interrupt. `ds_inject` calls `ds_voice_render` for every Sophie track on
every 32-sample audio block. `ds_voice_render` sets `voice.active = 1` on the
first trigger and never clears it while that track remains Sophie. The AMP
envelope and STOP do not gate this renderer. It therefore keeps synthesizing
32 samples every ~0.667 ms even when the audible result is silent.

The S021 hot loop also performs several 32-bit integer divisions for each
sample, notably the FM bandwidth ceiling, index phase and feedback/output
soft clipping. Those divisions are relatively costly on ColdFire. The model
types vary in cost, with Stack the heaviest in this probe.

`tests/timing_probe.py` runs the actual S021 firmware in digiemu's ColdFire
cycle model. It forces one Sophie track on and injects one trigger into
emulated RAM; it does not modify the firmware file or saved project. At the
model's assumed 250 MHz, each callback has 166,685 cycles available:

| State | Mean render cycles | Max render cycles | Voice active |
| --- | ---: | ---: | ---: |
| Before first Sophie trigger | 100,526 | 100,914 | no |
| Fuse after one trigger | 123,937 | 124,645 | yes |
| Stack | 130,413 | 131,101 | yes |
| Split | 127,151 | 128,126 | yes |
| Shard | 125,430 | 126,193 | yes |
| Fuse after retrigger | 123,870 | 124,580 | yes |
| Fuse after STOP | 123,958 | 124,667 | yes |

This is an extra 23,000–30,000 modeled cycles per audio callback from one
Sophie track, continuously after the first note. The STOP result directly
confirms the work persists. A retrigger does not accumulate another voice or
increase the steady-state cost in this test. The worsening response on the
device is consistent with lower-priority UI work queuing while the audio
interrupt occupies too much real CPU time.

## Limits of the measurement

The earlier general firmware check played the stock path and did not activate
Sophie; its reported 37.6% audio margin was therefore not a Sophie timing
result. This focused probe does exercise Sophie. It reports no missed audio
deadlines under its model, but the model assumes zero-wait memory and does not
know the real Digitakt's effective CPU clock, cache behavior or bus contention.
The device's multi-second UI lag is stronger evidence of an actual scheduling
problem than the model's remaining margin is of hardware safety. This probe
forces machine state in emulator RAM and should not be treated as a complete
hardware trace.

## S022 implementation and validation

S022 uses the stock AMP envelope level as a sleep signal, rather than a
fixed timer. Once it remains below roughly -72 dB for 32 audio blocks, the
renderer clears its tail and skips the expensive synthesis loop. A trig takes
priority and resets the same track voice, choking the previous note; there is
no stack of Sophie voices. A rising AMP level can also wake the renderer.
This gate does not change Digitakt's AMP duration control.

The ColdFire hot loop now uses a lookup/interpolation soft clip, a bounded
reciprocal approximation for FM index conversion, fewer FM-ceiling divisions,
and model-specific oscillator calculations. At centered SWEEP it bypasses
the pitch-contour calculation. The build imports no software division
helpers. Host unit, sanitizer, cross-compile, waveform-continuity, patch
linking, and emulator boot tests passed. The emulator's broader firmware
check also rated the S022 build PASS. Its stock comparison independently
failed a boot trace for a FlexBus read at address zero; the S022 build's
boot stage nevertheless passed the check. The broad check exercises mostly
stock paths and is not a substitute for the focused Sophie probe.

The focused S022 emulator cycle-model probe, with one Sophie track active,
reports 166,685 cycles available per ~0.667 ms callback:

| State | Mean cycles | Max cycles | Missed modeled deadlines |
| --- | ---: | ---: | ---: |
| Before first Sophie trig | 100,519 | 100,907 | 0 |
| Fuse | 120,207 | 121,338 | 0 |
| Stack | 127,863 | 129,105 | 0 |
| Split | 124,320 | 125,442 | 0 |
| Shard | 119,123 | 119,943 | 0 |
| Fuse after retrigger | 120,165 | 121,406 | 0 |
| After STOP with AMP silent | 102,771* | 120,947* | 0 |

*The STOP window includes the short transition into sleep. Once asleep, the
callback is close to the pre-trigger cost. A separate manual-trig probe of
the previous S022 build (before the model-specific oscillator optimization)
verified that the real stock AMP envelope naturally reaches the sleep
threshold, returning the callback to about 100,600 cycles after ~2.4 seconds
for that test note. That probe selected Sophie in emulated RAM but used a real
panel trig and stock AMP release; the final optimization does not alter the
gate or envelope.

These cycle results are only an emulator model. Its memory waits and cache
behavior may not match the actual Digitakt. Repeated trigs keep the envelope
audible and therefore keep Sophie active, which is correct musically but
still costs roughly 19,000–27,000 extra modeled cycles per callback. Hardware
testing is required before declaring the multi-second UI lag fixed. Test one
track with several manual trigs, then a running sequence, then the heaviest
Stack model; check parameter and STOP response after each.

Do not rely on S021 for a live set. Treat S022 as a candidate fix until its
responsiveness is confirmed on hardware.

## S023 active-render redesign and on-device measurement

Hardware testing showed S022 still caused severe UI and LED lag while a
Sophie note was playing. This rules out its idle-tail sleep as a sufficient
fix. The active hot loop still ran at audio rate; the emulator's default
zero-wait-memory estimate had overstated the available device margin.

S023 updates the four slow controls and oscillator ratios once every eight
samples, replaces the multi-multiply sine approximation with a direct
1024-point lookup, removes negative SWEEP's per-sample divide using a
bounded reciprocal table, and skips redundant settled-attack work. The
lookup's nearest-point quantization is a deliberate tone/performance
tradeoff. Host continuity and bounded-output tests pass, but the sound
should be auditioned on the device.

S023 includes digihealth's `SYSTEM INFO` on-device CPU/DSP/RAM readout.
`FAST AUDIO` is off at boot, so an initial reading isolates Sophie's own
cost. The diagnostic source and test protocol are in `diagnostics/`.

The focused S023 emulator timing run, with diagnostics linked but FAST
AUDIO off, measured one active Sophie track:

| State | Mean cycles | Max cycles |
| --- | ---: | ---: |
| Before first trig | 101,060 | 101,450 |
| Fuse | 115,287 | 116,697 |
| Stack | 119,933 | 121,135 |
| Split | 117,429 | 118,908 |
| Shard | 115,219 | 116,754 |
| Fuse after retrigger | 115,237 | 116,728 |
| After AMP sleep transition | 102,714 | 115,383 |

At extreme METAL, FBK, COLOR and negative SWEEP, a separate short run of
the final S023 candidate with a hypothetical 20-cycle penalty per
instruction-cache miss measured 142,340 cycles for Fuse and 147,320 for
Stack (peaks 144,175 and 149,347).
That penalty is a sensitivity test, not a measured Digitakt memory latency.
The on-device DSP percentage must decide whether S023 creates enough real
headroom. A reading close to 100% while Sophie plays, especially if LEDs or
STOP lag, means active synthesis still needs architectural reduction.

The final S023 mod links with core 2.1 and diagnostic digihealth 1.0.1
without overlapping sites; the firmware patcher verified the SysEx image.
The emulator boots to a settled UI and rates the build PASS in its broader
firmware check. Its stock comparison has the pre-existing FlexBus-at-zero
boot-trace failure; the S023 build itself passes boot and run. A separate
emulator menu probe verified that both SETTINGS rows appear, FAST AUDIO is
unchecked, and enabling SYSTEM INFO draws the RAM/sample-memory readout.

## S023 hardware result and S024 response

The on-device digihealth readout established that RAM remained at about
13.5 MB free while the lag appeared. Without an active Sophie note, the
device showed about 80% CPU and 78% DSP (84% recent DSP peak). With one
active Sophie track it showed about 97% CPU and 92% DSP (99% peak), rising
to 100% CPU and 94% DSP (100% peak) during parameter changes. Two active
Sophie tracks became unusably slow, with DSP peaks sometimes exceeding
100%. FAST AUDIO let two tracks run, but control response remained laggy.
These readings identify audio-render CPU saturation, not a RAM shortage.

S024 therefore runs the Sophie oscillator/feedback core at 24 kHz, with
linear interpolation to 48 kHz output. The slow-control and FM-depth
updates retain approximately their previous real-time rates. This reduces
the number of expensive oscillator evaluations per track by half. Its
different high-frequency character is intentional and must be auditioned;
the host continuity test found no large jumps in the stressed waveforms.

With digihealth still linked and FAST AUDIO off, the focused emulator cycle
model measured:

| Active Sophie tracks | Fuse mean/max cycles | Stack mean/max cycles |
| --- | ---: | ---: |
| none | 101,060 / 101,450 | same |
| one | 108,226 / 108,864 | 110,664 / 111,377 |
| two | 115,067 / 115,920 | 119,982 / 121,023 |

S023's one-track costs in the same model were 115,287 Fuse and 119,933
Stack. Thus S024's *incremental Sophie cost* is approximately halved: one
Fuse adds 7,166 rather than 14,227 cycles per callback, and one Stack
adds 9,604 rather than 18,873. This is a model estimate, not a device
measurement. The next hardware test should repeat the S023 CPU/DSP/peak
measurements with one and two tracks, FAST AUDIO initially off. Only those
readings can establish whether controls and STOP regain enough headroom.

## S025 two-operator experiment

S025 retains the 24 kHz synthesis rate but replaces the three-modulator
network with one inharmonic modulator and one carrier. FUSE, PIPE and SHARD
perform two sine table reads per synth sample; BOOM reads three to retain a
clean sine body under an FM impact. S024 used three, five, two and six reads
for FUSE, Split, Shard and Stack respectively. S025 also updates SWEEP's
pitch increment every four synth samples instead of every sample and adds a
separate transient-brightness contour. These are source-level work counts,
not measured whole-callback cycle reductions. Eight simultaneous tracks
remain an aspiration, not a verified hardware capability.

The host unit test, sanitizer test, waveform continuity probe and ColdFire
cross-compile passed. elekloader built and verified the S025 SysEx with core
and digihealth. An emulator first-run was attempted, but the host's patched
Unicorn library crashed with an illegal instruction in its own `mem_map`
compatibility probe, before firmware execution. Thus S025 has no valid
emulator timing or listening result yet; the S024 table above must not be
presented as an S025 measurement. Audition and hardware CPU/DSP readings
should precede any claim that this version improves playable track count.

## S027 held-note and tail candidate

Hardware listening of S025 found a ~0.8–0.9 s cutoff with AMP HOLD=NOTE,
DEC=INF and a long TRIG LEN; changing LEN did not change the cutoff. The
Sophie oscillator has no fixed duration and a host test confirms it remains
audible after two seconds when held. Therefore the cutoff is at the stock
voice/envelope handoff or Sophie's sleep decision, not the 135 ms brightness
contour. Static inspection of the stock envelope routine at `0x40073304`
shows that it maintains a separate per-track phase at `0x4199df54 + 12*t`
and level at `0x4199df58 + 12*t`. S027 now requires the phase to be
released/idle *and* the level to remain quiet before it sleeps. This prevents
a temporary low level in an active envelope from silencing a held synth.
The source then fades over about 5 ms rather than hard-zeroing its buffer.

S027 also remaps SRC F/G/H to METAL/FBK/COLOR and makes PIPE a ring-style
cross modulation with an audible inharmonic component at METAL 0. Host unit,
sanitizer, waveform, ColdFire compile, link and firmware verification passed.
The emulator remains unavailable because its native Unicorn library raises
SIGILL in `mem_map` before firmware execution. Hardware listening must verify
whether S027 actually cures the ~0.9 s cutoff and end pop; do not treat the
host test as proof of either on the device.
