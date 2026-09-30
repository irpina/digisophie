# Sophie SRC oscilloscope feasibility (Digitakt Mk1, OS 1.53)

## Verdict

This is feasible and reasonably contained as a separate UI feature. The
Digitakt's second SRC subpage is a real view state (`SamplePageView + 0x90`:
`0` parameters, `1` waveform). The installed `digislicer` mod already uses
that same subpage for a custom waveform editor. Its implementation is a
working example of intercepting the SRC key, drawing a new screen over the
stock frame, and refreshing it while audio plays.

The audio diagnosis should come first. A scope is useful for inspecting the
waveform, but it will not itself correct clicks or establish what the final
DAC output looks like.

## Evidence in this workspace

- `digislicer/slice.c`, `slc_ui_srcpost`: after the stock SRC key handler,
  reads `view + 0x90` and opens its editor on the waveform subpage.
- `digislicer/mod.json`: replaces the SRC page's key-handler pointer at
  `0x401848dc` and subscribes to core's `ev_draw`, `ev_tick`, `ev_key`, and
  `ev_enc` events.
- `digislicer/glue.s`, `slc_draw`: overlays a complete 128 × 64 screen through
  the draw hook.
- `digislicer/slice.c`, `slc_ui_draw`: uses existing firmware `Bitmap`
  primitives to draw waveform columns, markers, and text.
- `elekloader/docs/ADAPTING.md`: describes `ev_draw` after normal view
  composition and `ev_tick` at 30 Hz; audio callbacks run at 1,500 blocks/s.
- `digisophie/digitakt.c`: Sophie already renders 32 samples per track per
  block into the track buffer before stock overdrive, AMP, filter, and mixer.

These references establish a route for this exact OS version. They do not
establish compatibility with other firmware releases.

## Suggested design

1. Tap **one selected Sophie track** at the existing render hook and copy its
   32 freshly rendered, pre-FX samples into a small 16-bit circular buffer.
   A 2,048-sample buffer costs 4 KB. Capturing every track would cost 32 KB
   and is unnecessary for a first version.
2. On the UI task, snapshot the buffer using a short, bounded handoff. Never
   call drawing or allocation functions from the audio interrupt.
3. When the current track is Sophie and `view + 0x90 == 1`, use `ev_draw` to
   replace the stock waveform pane with a 128-column trace. Redraw through
   `ev_tick` at roughly 15–30 Hz.
4. Find a rising zero crossing near the middle of the snapshot for a stable
   trigger. Draw a center line, amplitude scale, and a small timebase label.
   A min/max pair per column preserves brief spikes that single-point
   decimation would miss.
5. Let SRC return to the eight-control page. Other machines retain their
   stock second page.

The first scope should show **pre-FX Sophie output**, because those exact
samples are available at the render hook. Showing the post-filter sound
would need an additional tap later in the audio chain. Showing the actual
stereo master/DAC signal would require a different capture point and more
coordination with the mix path. The UI should label its source accordingly.

## Risks and checks

- **Audio/UI concurrency:** the display must not read a half-written audio
  block. Use a short snapshot protocol or two buffers, and bound ISR work.
- **CPU cost:** copying 32 samples per block is small, but worst-case render
  timing must be checked with eight Sophie tracks before hardware use.
- **Display cost:** compose at UI rate, not audio rate; 128 min/max columns
  are sufficient on the 128 × 64 monochrome screen.
- **View lifecycle:** entering and leaving the subpage, changing tracks, and
  opening popups must not leave the scope over another screen.
- **Patch ownership:** `digislicer` already patches SRC key-handler pointer
  `0x401848dc`. A build containing both mods would need a shared hook or an
  alternative way to observe the subpage; two independent pointer patches
  would conflict. The current Sophie + core build has no such collision.
- **Verification:** test clean sine, Sophie at several METAL/FEEDBACK values,
  rapid retriggers, track changes, and transport start/stop. Compare captured
  samples against the standalone host DSP probe and a saved audio recording.

## Effort and priority

A basic live scope is a moderate firmware/UI task, not a research project on
the scale of a new audio engine. The subpage state and drawing pattern are
already demonstrated by `digislicer`; the remaining work is the safe audio
snapshot and screen-specific lifecycle. Polished triggering, zoom controls,
post-FX capture, and compatibility with other waveform-page mods would take
additional work.

Recommended order: first resolve the current audio-level and model-fidelity
questions, then build a pre-FX scope as a diagnostic and creative tool.
