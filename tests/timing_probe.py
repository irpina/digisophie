"""Time Sophie's render path with digiemu's ColdFire cycle model.

Run from digiemu with its Python package on the import path:
    PYTHONPATH=. uv run --no-sync python ../digisophie/tests/timing_probe.py FIRMWARE_DIR PATCH_MAP_JSON

This probes emulated RAM only; it does not alter the firmware or saved project.
"""
import os
import sys
import glob
import json

from unicorn import UC_HOOK_CODE

from emu import cftiming, ssi
from emu.session import Session


MACH = 0x800018BC
TRIG_BITS = 0x80001228
PARAMS = 0x80002794
AMP_PHASE = 0x4199DF54


def main():
    folder = os.path.abspath(sys.argv[1])
    with open(sys.argv[2], encoding="utf-8") as fh:
        symbols = json.load(fh)
    voices = int(symbols["ds_voices"], 16)
    inject = int(symbols["ds_inject"], 16)
    os.environ["DT2_SECTIONS"] = os.path.join(folder, "sections")
    os.environ["DT2_DEVICES"] = os.path.join(folder, "devices")
    syx = glob.glob(os.path.join(folder, "Digitakt_OS*.syx"))[0]
    version = os.path.basename(syx)[11:-4]
    snapshot = os.path.join(folder, "snapshots", "Digitakt_OS" + version,
                            "gui.snap")
    state = {"enabled": False, "trigger": False, "calls": 0,
             "model": 0, "quiet": False}
    hot = bool(os.environ.get("DS_HOT"))
    tracks = 2 if os.environ.get("DS_TWO") else 1

    def install(session):
        def select_sophie(uc, _addr, _size, _data):
            state["calls"] += 1
            if not state["enabled"]:
                return
            for track in range(tracks):
                uc.mem_write(MACH + track, b"\x07")
                uc.mem_write(AMP_PHASE + 4 + 12 * track,
                             (0 if state["quiet"] else 1 << 28).to_bytes(4, "big"))
                for offset, value in ((0, 0x4000), (2, state["model"] << 11),
                                      (8, (0 if hot else 64) << 8),
                                      (10, (127 if hot else 64) << 8),
                                      (12, (127 if hot else 32) << 8),
                                      (14, (127 if hot else 40) << 8)):
                    uc.mem_write(PARAMS + 106 * track + offset,
                                 value.to_bytes(2, "big"))
                uc.mem_write(0x80001F18 + 2 * track,
                             (127 << 8).to_bytes(2, "big"))
            if state["trigger"]:
                bits = int.from_bytes(uc.mem_read(TRIG_BITS, 4), "big")
                uc.mem_write(TRIG_BITS,
                             (bits | ((1 << tracks) - 1)).to_bytes(4, "big"))
                state["trigger"] = False

        session.m.uc.hook_add(UC_HOOK_CODE, select_sophie,
                              begin=inject, end=inject)

    miss_penalty = int(os.environ.get("DS_ICACHE_MISS", "0"))
    timing = {"accel": "max"}
    if miss_penalty:
        timing.update(icache=True, miss_penalty=miss_penalty)
    session = Session(snapshot, syx, hle=False, timing=timing,
                      on_machine=install)
    try:
        prof = ssi.PROFILES[session.device.audio["ssi_profile"]]
        measured = 0

        def report(name):
            nonlocal measured
            period = cftiming.measured_period(session.clock, ssi.FORCE_VECTOR)
            stats = cftiming.deadline_report(session.clock, ssi.FORCE_VECTOR,
                                              period or 166667)
            walls = list(session.clock.vectors[ssi.FORCE_VECTOR].walls)[measured:]
            measured += len(walls)
            active = session.m.uc.mem_read(voices + 78, 1)[0]
            sleeping = session.m.uc.mem_read(voices + 79, 1)[0]
            amp_phase = int.from_bytes(session.m.uc.mem_read(AMP_PHASE, 4), "big")
            print(name, "ms=%.1f" % session.ms, "hooks=%d" % state["calls"],
                  "active=%d" % active, "sleeping=%d" % sleeping,
                  "amp_phase=%d" % amp_phase,
                  "window mean=%d max=%d late=%d" %
                  (sum(walls) // len(walls), max(walls),
                   sum(w > stats["period_cycles"] for w in walls)),
                  "period=%d" % stats["period_cycles"], flush=True)

        session.run_ms(200)
        report("before trigger")
        state["enabled"] = True
        state["trigger"] = True
        session.run_ms(200)
        report("after one trigger")
        if os.environ.get("DS_SHORT"):
            state["model"] = 1
            session.run_ms(200)
            report("BOOM")
            return
        for model in range(1, 4):
            state["model"] = model
            session.run_ms(200)
            report(("BOOM", "PIPE", "SHARD")[model - 1])
        state["model"] = 0
        session.run_ms(200)
        report("Fuse again, no trigger")
        state["trigger"] = True
        session.run_ms(200)
        report("Fuse after retrigger")
        state["quiet"] = True
        session.tap(session.code("STOP"), hold_ms=30, after_ms=170)
        report("after STOP")
        state["quiet"] = False
        session.run_ms(200)
        report("after AMP rises")
        print("halted", session.halted, "tx vector", prof.tx_vector,
              "decode mismatches", session.clock.decode_mismatches,
              "icache misses", session.clock.icache.misses
              if session.clock.icache else "not measured")
    finally:
        session.close()


if __name__ == "__main__":
    main()
