"""Inspect the Digitakt mk1's stock AMP phase around a manual note and STOP.

From digiemu: PYTHONPATH=. uv run --no-sync python ../digisophie/tests/amp_phase_probe.py FIRMWARE_DIR
"""
import os
import sys

from emu.session import Session

AMP_PHASE = 0x4199DF54
AMP_LEVEL = AMP_PHASE + 4


def main():
    folder = os.path.abspath(sys.argv[1])
    os.environ["DT2_SECTIONS"] = os.path.join(folder, "sections")
    os.environ["DT2_DEVICES"] = os.path.join(folder, "devices")
    syx = os.path.join(folder, "Digitakt_OSS021.syx")
    snap = os.path.join(folder, "snapshots", "Digitakt_OSS021", "gui.snap")
    s = Session(snap, syx)
    try:
        def report(label):
            phase = int.from_bytes(s.m.uc.mem_read(AMP_PHASE, 4), "big")
            level = int.from_bytes(s.m.uc.mem_read(AMP_LEVEL, 4), "big")
            print(label, "time=%.0fms" % s.ms, "phase=%d" % phase,
                  "level=0x%08x" % level, flush=True)

        s.run_ms(100)
        report("idle")
        s.tap(s.code("1"), hold_ms=80, after_ms=120)
        report("after trig")
        for _ in range(10):
            s.run_ms(200)
            report("later")
        s.tap(s.code("STOP"), hold_ms=30, after_ms=170)
        report("after STOP")
        print("halted", s.halted)
    finally:
        s.close()


if __name__ == "__main__":
    main()
