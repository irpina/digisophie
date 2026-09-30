"""Time S022 with a real panel trig and the stock AMP envelope.

From digiemu: PYTHONPATH=. uv run --no-sync python ../digisophie/tests/real_trig_timing_probe.py FIRMWARE_DIR PATCH_MAP_JSON

The hook changes track 1's machine byte only in this emulated RAM session.
"""
import glob
import json
import os
import sys

from unicorn import UC_HOOK_CODE

from emu import cftiming, ssi
from emu.session import Session

AMP_LEVEL = 0x4199DF58


def main():
    folder = os.path.abspath(sys.argv[1])
    with open(sys.argv[2], encoding="utf-8") as fh:
        symbols = json.load(fh)
    select = int(symbols["ds_inject"], 16)
    voices = int(symbols["ds_voices"], 16)
    os.environ["DT2_SECTIONS"] = os.path.join(folder, "sections")
    os.environ["DT2_DEVICES"] = os.path.join(folder, "devices")
    syx = glob.glob(os.path.join(folder, "Digitakt_OS*.syx"))[0]
    version = os.path.basename(syx)[11:-4]
    snap = os.path.join(folder, "snapshots", "Digitakt_OS" + version,
                        "gui.snap")
    selections = [0]

    def install(session):
        def select_sophie(uc, _address, _size, _data):
            uc.mem_write(0x800018BC, b"\x07")
            selections[0] += 1

        session.m.uc.hook_add(UC_HOOK_CODE, select_sophie,
                              begin=select, end=select)

    s = Session(snap, syx, hle=False, timing={"accel": "max"},
                on_machine=install)
    try:
        seen = 0

        def report(label):
            nonlocal seen
            vector = ssi.FORCE_VECTOR
            walls = list(s.clock.vectors[vector].walls)[seen:]
            seen += len(walls)
            period = cftiming.measured_period(s.clock,
                ssi.PROFILES[s.device.audio["ssi_profile"]].tx_vector)
            level = int.from_bytes(s.m.uc.mem_read(AMP_LEVEL, 4), "big",
                                   signed=True)
            active = s.m.uc.mem_read(voices + 78, 1)[0]
            sleeping = s.m.uc.mem_read(voices + 79, 1)[0]
            print(label, "ms=%.0f" % s.ms, "selected=%d" % selections[0],
                  "amp=%d" % level, "active=%d" % active,
                  "sleeping=%d" % sleeping,
                  "mean=%d max=%d period=%d late=%d" %
                  (sum(walls) // len(walls), max(walls), period,
                   sum(w > period for w in walls)), flush=True)

        s.run_ms(200)
        report("idle")
        s.tap(s.code("1"), hold_ms=80, after_ms=120)
        report("manual trig")
        for n in range(8):
            s.run_ms(250)
            report("release %d" % (n + 1))
        s.tap(s.code("STOP"), hold_ms=30, after_ms=170)
        report("STOP")
        print("halted", s.halted, "decode mismatches",
              s.clock.decode_mismatches)
    finally:
        s.close()


if __name__ == "__main__":
    main()
