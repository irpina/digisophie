# Sophie for Digitakt

Sophie is a metallic percussion synth machine for the original Digitakt
(Mk1), OS 1.53. It began as a fixed-point adaptation of
[Sophie for Schwung](https://github.com/mestela/schwung-sophie) and evolved
into four deliberately different, CPU-conscious models: FUSE, BOOM, PIPE
and SHARD. It uses one track per voice and the Digitakt's normal AMP,
filter, mixer and effects path. The source, not a modified Elektron OS,
is what this repository distributes.

One instance runs without FAST AUDIO; the original hardware
test found two instances practical with FAST AUDIO enabled. Eight-track
operation is **not** claimed.

## Controls

| SRC knob | Control | What it does |
| --- | --- | --- |
| A | TUNE | Pitch |
| B | MODEL | FUSE / BOOM / PIPE / SHARD |
| C | BR | Stock Digitakt bit reduction |
| D | SAMP | Stock sample selector; Sophie does not use the sample |
| E | SWEEP | Bipolar pitch sweep toward the played note |
| F | METAL | FM/ring intensity |
| G | FBK | Oscillator feedback |
| H | COLOR | Inharmonic character |

The custom controls can be parameter-locked. The normal AMP page controls
the note envelope: set HOLD to `NOTE` for TRIG LEN to determine when the
release begins. Use a finite DEC to hear that release. DEC `INF` can keep
the sound going indefinitely. Retriggering chokes the previous voice on
that track, with a brief transition to suppress a click.

SWEEP starts at zero. Turning right begins above the played pitch; turning
left begins below it. Holding FUNC while turning uses the stock TUNE
octave-step behavior (-60 to +24); ordinary turning reaches the complete
-64 to +63 range.


## Build your own OS

You need:

1. Your own original Digitakt Mk1 OS 1.53 `.syx` from
   [Elektron](https://www.elektron.se/support-downloads/digitakt).
2. A source checkout of [elekloader](https://github.com/irpina/elekloader)
   with Digitakt core 2.1 and Python 3.9 or newer.
3. A ColdFire cross-toolchain (`m68k-linux-gnu-` or `m68k-elf-`: assembler,
   GCC and linker). Set `ELEKLOADER_CROSS` if its prefix is not
   `m68k-linux-gnu-`.

From this repository:

```sh
ELEKLOADER_CROSS=m68k-elf- sh scripts/build.sh \
  /path/to/your/Digitakt_OS1.53.syx /path/to/elekloader
```

The script builds core 2.1, Sophie and the bundled digihealth diagnostic,
lints all three mods, then writes
`out/Digitakt_OS1.53_SOPHIE_S027.syx`. Its OS version is `S027`. The
stock firmware and generated `.syx` remain local and are Git-ignored.
Never commit or upload the built `.syx`: it contains Elektron's OS.

To run the host DSP tests without a firmware image:

```sh
make test
```

`make cross-check` additionally checks the ColdFire sources if you have
`m68k-elf-` tools. The optional emulator probes in `tests/` require
[digiemu](https://github.com/irpina/digiemu) and are not needed to build.

## Use and recovery

This changes firmware on the instrument. Back up projects and sounds first,
check that your stock OS is OS 1.53 for the *original* Digitakt, and keep
that stock file for recovery. Build through elekloader's verification path;
follow Elektron's normal OS-update procedure to transfer the resulting
image. Do not power off during the update. FAST AUDIO is off by default in
this diagnostic build; SYSTEM INFO and FAST AUDIO can be enabled separately
in SETTINGS. See [diagnostics](diagnostics/README.md) for the monitor.

Sophie is independent of, and not endorsed by, Elektron or the estate of
SOPHIE. It contains no Elektron firmware or samples.

## Source and licenses

The Sophie adaptation is [MIT licensed](LICENSE). The original Sophie
attribution and the separate GPL-2.0-or-later digihealth source are
documented in [THIRD_PARTY.md](THIRD_PARTY.md). The digihealth license is
also included in its source directory.

The [DSP audit](SOPHIE_DSP_AUDIT.md),
[hardware performance notes](SOPHIE_HARDWARE_LAG_AUDIT.md),
[SPICE architecture idea](SPICE_ARCHITECTURE.md) and
[oscilloscope feasibility note](OSCILLOSCOPE_FEASIBILITY.md) record the
development history; the latter two are ideas, not implemented features.
