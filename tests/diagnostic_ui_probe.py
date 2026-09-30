"""Capture S023's SETTINGS menu in the emulator for diagnostic UI QA."""
import glob
import os
import sys

from emu import panel
from emu.session import Session


def main():
    folder = os.path.abspath(sys.argv[1])
    os.environ["DT2_SECTIONS"] = os.path.join(folder, "sections")
    os.environ["DT2_DEVICES"] = os.path.join(folder, "devices")
    syx = glob.glob(os.path.join(folder, "Digitakt_OS*.syx"))[0]
    version = os.path.basename(syx)[11:-4]
    snap = os.path.join(folder, "snapshots", "Digitakt_OS" + version,
                        "gui.snap")
    session = Session(snap, syx, hle=False)
    try:
        session.tap(session.code("GLOBAL"), hold_ms=30, after_ms=170)
        for _ in range(int(sys.argv[3]) if len(sys.argv) > 3 else 0):
            session.tap(session.code("DOWN"), hold_ms=20, after_ms=35)
        if len(sys.argv) > 4 and sys.argv[4] == "info":
            session.tap(session.code("UP"), hold_ms=20, after_ms=35)
            session.tap(session.code("YES"), hold_ms=20, after_ms=35)
            session.tap(session.code("NO"), hold_ms=20, after_ms=35)
            session.run_ms(1100)
        panel.write_png(session.screen_at(session.ms), sys.argv[2], 4)
        print("screen", sys.argv[2], "halted", session.halted)
    finally:
        session.close()


if __name__ == "__main__":
    main()
