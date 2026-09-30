# SPICE multi-algorithm machine: feasibility report

## Executive finding

A single **SPICE** machine containing Sophie plus additional synthesis
algorithms is realistic. The DSP side is straightforward; the difficult part
is making SRC encoder D a genuine algorithm selector. D is the stock `SAMP`
slot (`0x87`), and Digitakt opens the sample browser through behavior that is
separate from the label, range and drawing hooks. Earlier Sophie experiments
proved that relabeling D does not disable that browser.

The recommended design is therefore feasible, but it needs one focused piece
of firmware work before adding algorithms: intercept the SRC parameter-D
setter/activation path only while machine 7 (or a new SPICE machine ID) is
active. It should not be implemented by merely relabeling `SAMP`.

No SPICE code or machine rename has been implemented as part of this report.

## What can be reused

The current custom-machine integration already supplies most of the required
foundation:

- Eight persistent SRC storage slots per track, with project recall, MIDI and
  parameter-lock infrastructure.
- A custom render injection point before Digitakt's stock AMP, filter, mixer,
  sends and bit reduction.
- Machine-specific labels, ranges, value text and control presentation.
- Per-track fixed-point voice state and a 32-sample render callback.
- Considerable memory headroom. The current Sophie mod uses approximately
  7 KB of linked RAM in total and only 400 bytes of private BSS.

Renaming the visible descriptor from SOPHIE to SPICE is trivial. Keeping the
same numeric machine ID would preserve the machine identity in saved sounds,
but changing the meaning of its stored parameters requires a migration policy.

## Proposed SRC layout

The most direct layout is:

| Encoder | SPICE role | Sophie algorithm role |
| --- | --- | --- |
| A | Tune | Tune |
| B | Macro 1 | Model |
| C | Stock BR | Stock BR |
| D | Algorithm | Sophie = algorithm 1 |
| E | Macro 2 | Sweep |
| F | Macro 3 | Color |
| G | Macro 4 | Feedback |
| H | Macro 5 | Metal |

This keeps C as Digitakt's native BR and reserves D for up to 12 algorithms.
The five macro labels and ranges can change with the selected algorithm. A and
C should stay consistent across algorithms.

## The D/SAMP blocker

The machine currently borrows SLICE's eight parameter descriptors. Slot D is
therefore a real sample parameter in several independent firmware paths:

1. Its label, range, value formatter and graphic are resolved for the SRC
   page.
2. Its encoder value is stored in the sound and can be locked.
3. A separate sample-browser/activation path recognizes the SAMP parameter and
   opens the chooser.
4. Core's added-machine compatibility maps sample handling through the chosen
   stock parameter machine.

Hooks for items 1 and 2 are already understood. Item 3 is why the earlier
`METAL`-on-D experiment both changed a value and opened the sample chooser.

The correct implementation is a narrow conditional hook in the SRC control
dispatch:

- If the current page belongs to SPICE and the control is `0x87`, treat the
  encoder as an ordinary bounded integer selector.
- Suppress the sample-browser action for that exact machine/control pair.
- Fall through byte-for-byte to stock behavior for every stock machine and
  every other control.
- Keep the existing parameter storage so algorithm selection remains
  recallable and parameter-lockable.

This dispatch must be traced and tested before the machine is renamed. A broad
global patch to the sample chooser would be unacceptable because it could
break normal sample tracks.

## DSP architecture

Use a small engine registry rather than growing one monolithic voice:

```text
SPICE track
  algorithm id (D)
  shared normalized controls
  union of per-algorithm voice states
  render switch -> Sophie / algorithm 2 / ... / algorithm 12
  stock Digitakt post-processing
```

Recommended properties:

- Algorithm `0` is Sophie.
- Use an explicit `switch` on the algorithm ID; it is predictable on ColdFire
  and avoids relocation/function-pointer surprises.
- Store per-track voice data in a union sized to the largest algorithm, rather
  than allocating state for all 12 engines simultaneously.
- Give every engine `init`, `trigger` and `render_block` behavior with the same
  fixed-point output contract.
- Reset or migrate voice state when D changes, preventing one engine from
  interpreting another engine's state.
- Clamp D to the number of compiled algorithms. Initially the UI could expose
  only one choice, then expand as algorithms are added.
- Keep stock AMP/filter/BR ownership common to every engine.

Twelve algorithms are plausible within the available code/RAM budgets, but
CPU is the actual constraint. Each one must be tested at eight simultaneous
tracks and with worst-case parameter settings.

## UI design

D should use a stepped selector with one position per installed algorithm and
show a short algorithm name in its value field. With 12 engines, a simple row
of 12 pixels or ticks is more useful than a circular knob.

The other macro labels can be selected dynamically from a table indexed by
algorithm. Dynamic ranges are possible through the existing parameter-range
hook, but changing range semantics between algorithms has consequences for
locks and saved values. The safest convention is to store all macros as
normalized `0..127` values and convert them inside each engine.

## Project compatibility choices

There are two viable strategies:

### Preserve machine ID 7

Rename SOPHIE to SPICE while retaining ID 7. Existing Sophie sounds continue
to identify the machine, but their D slot currently contains a sample index.
After D becomes Algorithm, that value could select an unintended engine.
Migration would need a reliable project/version marker or a conservative rule
that clamps old values to Sophie. No such marker has been established yet.

### Allocate a new SPICE machine ID

Keep SOPHIE on ID 7 and add SPICE on a new unused ID. This is the safest route
for existing projects and gives explicit opt-in migration. Sophie can later be
retired once SPICE is stable. It consumes another machine ID but avoids hidden
sound changes.

**Recommendation:** develop SPICE under a new machine ID first. If preserving
old emulator projects is unimportant, it can replace ID 7 only after the D
dispatch and migration behavior are proven.

## Suggested implementation sequence

1. Trace the exact SRC D encoder and sample-browser dispatch on OS 1.53.
2. Build a minimal test machine where D is a two-value ordinary selector and
   verify that the browser never opens.
3. Confirm storage, reload, copy/paste, sound pool, MIDI and parameter locks.
4. Add the SPICE descriptor and algorithm registry with Sophie as algorithm 0.
5. Add dynamic labels/value text while keeping macro storage normalized.
6. Add algorithms one at a time with deterministic host tests.
7. Run emulator timing checks and, before hardware release, test eight-track
   worst-case polyphony on a real Digitakt.

## Decision

Proceeding is realistic, but the D/SAMP dispatch is the gate. Solve and verify
that narrow UI behavior first; only then rename the machine and begin adding
engines. This prevents the architecture from depending on the same fragile
sample-selector override that caused the earlier Sophie UI failures.
